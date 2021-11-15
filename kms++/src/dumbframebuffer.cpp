
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <drm.h>
#include <drm_mode.h>

#include <kms++/kms++.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

using namespace std;

namespace kms
{
DumbFramebuffer::DumbFramebuffer(Card& card, uint32_t width, uint32_t height, const string& fourcc)
	: DumbFramebuffer(card, width, height, FourCCToPixelFormat(fourcc))
{
}

DumbFramebuffer::DumbFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format)
	: Framebuffer(card, width, height), m_format(format)
{
	int r;

	const PixelFormatInfo& format_info = get_pixel_format_info(m_format);

	m_num_planes = format_info.num_planes;

	for (int i = 0; i < format_info.num_planes; ++i) {
		const PixelFormatPlaneInfo& pi = format_info.planes[i];
		FramebufferPlane& plane = m_planes.at(i);

		/* create dumb buffer */
		struct drm_mode_create_dumb creq = drm_mode_create_dumb();
		creq.width = width;
		creq.height = height / pi.ysub;
		/*
		 * For fully planar YUV buffers, the chroma planes don't combine
		 * U and V components, their width must thus be divided by the
		 * horizontal subsampling factor.
		 */
		if (format_info.type == PixelColorType::YUV &&
		    format_info.num_planes == 3)
			creq.width /= pi.xsub;
		creq.bpp = pi.bitspp;
		r = drmIoctl(card.fd(), DRM_IOCTL_MODE_CREATE_DUMB, &creq);
		if (r)
			__throw_exception_again invalid_argument(string("DRM_IOCTL_MODE_CREATE_DUMB failed: ") + strerror(errno));

		plane.handle = creq.handle;
		plane.stride = creq.pitch;
		plane.size = creq.height * creq.pitch;
		plane.offset = 0;
		plane.map = 0;
		plane.prime_fd = -1;
	}

	/* create framebuffer object for the dumb-buffer */
	uint32_t bo_handles[4] = {
		m_planes[0].handle,
		m_planes[1].handle,
		m_planes[2].handle,
		m_planes[3].handle,
	};
	uint32_t pitches[4] = {
		m_planes[0].stride,
		m_planes[1].stride,
		m_planes[2].stride,
		m_planes[3].stride,
	};
	uint32_t offsets[4] = {
		m_planes[0].offset,
		m_planes[1].offset,
		m_planes[2].offset,
		m_planes[3].offset,
	};
	uint32_t id;
	r = drmModeAddFB2(card.fd(), width, height, (uint32_t)format,
			  bo_handles, pitches, offsets, &id, 0);
	if (r)
		__throw_exception_again invalid_argument(string("drmModeAddFB2 failed: ") + strerror(errno));

	set_id(id);
}

DumbFramebuffer::~DumbFramebuffer()
{
	/* delete framebuffer */
	drmModeRmFB(card().fd(), id());

	for (uint i = 0; i < m_num_planes; ++i) {
		FramebufferPlane& plane = m_planes.at(i);

		/* unmap buffer */
		if (plane.map)
			munmap(plane.map, plane.size);

		/* delete dumb buffer */
		struct drm_mode_destroy_dumb dreq = drm_mode_destroy_dumb();
		dreq.handle = plane.handle;
		drmIoctl(card().fd(), DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
		if (plane.prime_fd >= 0)
			::close(plane.prime_fd);
	}
}

uint8_t* DumbFramebuffer::map(unsigned plane)
{
	FramebufferPlane& p = m_planes.at(plane);

	if (p.map)
		return p.map;

	/* prepare buffer for memory mapping */
	struct drm_mode_map_dumb mreq = drm_mode_map_dumb();
	mreq.handle = p.handle;
	int r = drmIoctl(card().fd(), DRM_IOCTL_MODE_MAP_DUMB, &mreq);
	if (r)
		__throw_exception_again invalid_argument(string("DRM_IOCTL_MODE_MAP_DUMB failed: ") + strerror(errno));

	/* perform actual memory mapping */
	p.map = (uint8_t*)mmap(0, p.size, PROT_READ | PROT_WRITE, MAP_SHARED,
			       card().fd(), mreq.offset);
	if (p.map == MAP_FAILED)
		__throw_exception_again invalid_argument(string("mmap failed: ") + strerror(errno));

	return p.map;
}

int DumbFramebuffer::prime_fd(unsigned int plane)
{
	if (m_planes.at(plane).prime_fd >= 0)
		return m_planes.at(plane).prime_fd;

	int r = drmPrimeHandleToFD(card().fd(), m_planes.at(plane).handle,
				   DRM_CLOEXEC | O_RDWR, &m_planes.at(plane).prime_fd);
	if (r)
		__throw_exception_again std::runtime_error("drmPrimeHandleToFD failed");

	return m_planes.at(plane).prime_fd;
}

} // namespace kms
