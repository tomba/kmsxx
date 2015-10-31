
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <drm.h>
#include <drm_mode.h>

#include "kms++.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

using namespace std;

namespace kms
{

DumbFramebuffer::DumbFramebuffer(Card &card, uint32_t width, uint32_t height, const string& fourcc)
	:DumbFramebuffer(card, width, height, FourCCToPixelFormat(fourcc))
{
}

DumbFramebuffer::DumbFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format)
	:Framebuffer(card, width, height), m_format(format)
{
	Create();
}

DumbFramebuffer::~DumbFramebuffer()
{
	Destroy();
}

uint32_t DumbFramebuffer::prime_fd(unsigned int plane)
{
	if (m_planes[plane].prime_fd >= 0)
		return m_planes[plane].prime_fd;

	int r = drmPrimeHandleToFD(card().fd(), m_planes[plane].handle,
				   DRM_CLOEXEC, &m_planes[plane].prime_fd);
	if (r)
		throw std::runtime_error("drmPrimeHandleToFD failed\n");

	return m_planes[plane].prime_fd;
}

struct FormatPlaneInfo
{
	uint8_t bitspp;	/* bits per (macro) pixel */
	uint8_t xsub;
	uint8_t ysub;
};

struct FormatInfo
{
	uint8_t num_planes;
	struct FormatPlaneInfo planes[4];
};

static const map<PixelFormat, FormatInfo> format_info_array = {
	/* YUV packed */
	{ PixelFormat::UYVY, { 1, { { 32, 2, 1 } }, } },
	{ PixelFormat::YUYV, { 1, { { 32, 2, 1 } }, } },
	{ PixelFormat::YVYU, { 1, { { 32, 2, 1 } }, } },
	{ PixelFormat::VYUY, { 1, { { 32, 2, 1 } }, } },
	/* YUV semi-planar */
	{ PixelFormat::NV12, { 2, { { 8, 1, 1, }, { 16, 2, 2 } }, } },
	{ PixelFormat::NV21, { 2, { { 8, 1, 1, }, { 16, 2, 2 } }, } },
	/* RGB16 */
	{ PixelFormat::RGB565, { 1, { { 16, 1, 1 } }, } },
	/* RGB32 */
	{ PixelFormat::XRGB8888, { 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::XBGR8888, { 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::ARGB8888, { 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::ABGR8888, { 1, { { 32, 1, 1 } }, } },
};

void DumbFramebuffer::Create()
{
	int r;

	const FormatInfo& format_info = format_info_array.at(m_format);

	m_num_planes = format_info.num_planes;

	for (int i = 0; i < format_info.num_planes; ++i) {
		const FormatPlaneInfo& pi = format_info.planes[i];
		FramebufferPlane& plane = m_planes[i];

		/* create dumb buffer */
		struct drm_mode_create_dumb creq = drm_mode_create_dumb();
		creq.width = width() / pi.xsub;
		creq.height = height() / pi.ysub;
		creq.bpp = pi.bitspp;
		r = drmIoctl(card().fd(), DRM_IOCTL_MODE_CREATE_DUMB, &creq);
		if (r)
			throw invalid_argument(string("DRM_IOCTL_MODE_CREATE_DUMB failed") + strerror(errno));

		plane.handle = creq.handle;
		plane.stride = creq.pitch;
		plane.size = creq.height * creq.pitch;
		plane.offset = 0;
		plane.map = 0;
		plane.prime_fd = -1;
	}

	/* create framebuffer object for the dumb-buffer */
	uint32_t bo_handles[4] = { m_planes[0].handle, m_planes[1].handle };
	uint32_t pitches[4] = { m_planes[0].stride, m_planes[1].stride };
	uint32_t offsets[4] = {  m_planes[0].offset, m_planes[1].offset };
	uint32_t id;
	r = drmModeAddFB2(card().fd(), width(), height(), (uint32_t)format(),
			  bo_handles, pitches, offsets, &id, 0);
	if (r)
		throw invalid_argument(string("drmModeAddFB2 failed: ") + strerror(errno));

	set_id(id);
}

void DumbFramebuffer::Destroy()
{
	/* delete framebuffer */
	drmModeRmFB(card().fd(), id());

	for (uint i = 0; i < m_num_planes; ++i) {
		FramebufferPlane& plane = m_planes[i];

		/* unmap buffer */
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
	FramebufferPlane& p = m_planes[plane];

	if (p.map)
		return p.map;

	/* prepare buffer for memory mapping */
	struct drm_mode_map_dumb mreq = drm_mode_map_dumb();
	mreq.handle = p.handle;
	int r = drmIoctl(card().fd(), DRM_IOCTL_MODE_MAP_DUMB, &mreq);
	if (r)
		throw invalid_argument(string("DRM_IOCTL_MODE_MAP_DUMB failed") + strerror(errno));

	/* perform actual memory mapping */
	p.map = (uint8_t *)mmap(0, p.size, PROT_READ | PROT_WRITE, MAP_SHARED,
					  card().fd(), mreq.offset);
	if (p.map == MAP_FAILED)
		throw invalid_argument(string("mmap failed: ") + strerror(errno));

	return p.map;
}

}
