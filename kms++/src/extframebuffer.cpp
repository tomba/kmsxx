
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <unistd.h>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{

ExtFramebuffer::ExtFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format,
			       vector<uint32_t> handles, vector<uint32_t> pitches, vector<uint32_t> offsets)
	: Framebuffer(card, width, height)
{
	m_format = format;

	const PixelFormatInfo& format_info = get_pixel_format_info(format);

	m_num_planes = format_info.num_planes;

	for (int i = 0; i < format_info.num_planes; ++i) {
		FramebufferPlane& plane = m_planes[i];

		plane.handle = handles[i];
		plane.prime_fd = 0;

		plane.stride = pitches[i];
		plane.offset = offsets[i];
		plane.size = plane.stride * height;
		plane.map = 0;
	}

	uint32_t id;
	int r = drmModeAddFB2(card.fd(), width, height, (uint32_t)format, handles.data(), pitches.data(), offsets.data(), &id, 0);
	if (r)
		throw std::invalid_argument(string("Failed to create ExtFramebuffer: ") + strerror(r));

	set_id(id);
}

ExtFramebuffer::ExtFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format,
			       vector<int> fds, vector<uint32_t> pitches, vector<uint32_t> offsets)
	: Framebuffer(card, width, height)
{
	int r;

	m_format = format;

	const PixelFormatInfo& format_info = get_pixel_format_info(format);

	m_num_planes = format_info.num_planes;

	for (int i = 0; i < format_info.num_planes; ++i) {
		FramebufferPlane& plane = m_planes[i];

		plane.prime_fd = fds[i];

		r = drmPrimeFDToHandle(card.fd(), fds[i], &plane.handle);
		if (r)
			throw invalid_argument(string("drmPrimeFDToHandle: ") + strerror(errno));

		plane.stride = pitches[i];
		plane.offset = offsets[i];
		plane.size = plane.stride * height;
		plane.map = 0;
	}

	uint32_t id;
	uint32_t bo_handles[4] = { m_planes[0].handle, m_planes[1].handle };
	r = drmModeAddFB2(card.fd(), width, height, (uint32_t)format,
			  bo_handles, pitches.data(), offsets.data(), &id, 0);
	if (r)
		throw invalid_argument(string("drmModeAddFB2 failed: ") + strerror(errno));

	set_id(id);
}

ExtFramebuffer::~ExtFramebuffer()
{
	drmModeRmFB(card().fd(), id());

	for (unsigned i = 0; i < m_num_planes; ++i) {
		FramebufferPlane& plane = m_planes[i];

		/* unmap buffer */
		if (plane.map)
			munmap(plane.map, plane.size);

		if (plane.prime_fd && plane.handle) {
			drm_gem_close closeReq {};
			int ret;

			closeReq.handle = plane.handle;
			ret = drmIoctl(card().fd(), DRM_IOCTL_GEM_CLOSE, &closeReq);
			/*
			 * do not throw an exception now, DRM core will
			 * make proper cleanup, but still print an error
			 */
			if (ret)
				printf("Error closing GEM %d (%s)\n", ret, strerror(errno));
		}
	}

}

uint8_t* ExtFramebuffer::map(unsigned plane)
{
	FramebufferPlane& p = m_planes[plane];

	if (!p.prime_fd)
		throw invalid_argument("cannot mmap non-dmabuf fb");

	if (p.map)
		return p.map;

	p.map = (uint8_t *)mmap(0, p.size, PROT_READ | PROT_WRITE, MAP_SHARED,
					  p.prime_fd, 0);
	if (p.map == MAP_FAILED)
		throw invalid_argument(string("mmap failed: ") + strerror(errno));

	return p.map;
}

int ExtFramebuffer::prime_fd(unsigned plane)
{
	FramebufferPlane& p = m_planes[plane];

	if (!p.prime_fd)
		throw invalid_argument("no primefb for non-dmabuf fb");

	return p.prime_fd;
}

}
