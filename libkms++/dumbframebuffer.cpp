
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <drm.h>
#include <drm_mode.h>

#include "kms++.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

using namespace std;

namespace kms
{

DumbFramebuffer::DumbFramebuffer(Card &card, uint32_t width, uint32_t height, const string& fourcc)
	:Framebuffer(card, width, height)
{
	uint32_t a, b, c, d;
	a = fourcc[0];
	b = fourcc[1];
	c = fourcc[2];
	d = fourcc[3];

	uint32_t code = ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24));

	Create(width, height, code);
}

DumbFramebuffer::~DumbFramebuffer()
{
	Destroy();
}

void DumbFramebuffer::print_short() const
{
	printf("DumbFramebuffer %d\n", id());
}

struct FormatPlaneInfo
{
	uint8_t bitspp;	/* bits per (macro) pixel */
	uint8_t xsub;
	uint8_t ysub;
};

struct FormatInfo
{
	uint32_t format;
	uint8_t num_planes;
	struct FormatPlaneInfo planes[4];
};

static const FormatInfo format_info_array[] = {
	/* YUV packed */
	{ DRM_FORMAT_UYVY, 1, { { 32, 2, 1 } }, },
	{ DRM_FORMAT_YUYV, 1, { { 32, 2, 1 } }, },
	/* YUV semi-planar */
	{ DRM_FORMAT_NV12, 2, { { 8, 1, 1, }, { 16, 2, 2 } }, },
	/* RGB16 */
	{ DRM_FORMAT_RGB565, 1, { { 16, 1, 1 } }, },
	/* RGB32 */
	{ DRM_FORMAT_XRGB8888, 1, { { 32, 1, 1 } }, },
};

static const FormatInfo& find_format(uint32_t format)
{
	for (uint i = 0; i < ARRAY_SIZE(format_info_array); ++i) {
		if (format == format_info_array[i].format)
			return format_info_array[i];
	}

	throw std::invalid_argument("foo");
}

void DumbFramebuffer::Create(uint32_t width, uint32_t height, uint32_t format)
{
	int r;

	m_format = format;

	const FormatInfo& format_info = find_format(format);

	m_num_planes = format_info.num_planes;

	for (int i = 0; i < format_info.num_planes; ++i) {
		const FormatPlaneInfo& pi = format_info.planes[i];
		FramebufferPlane& plane = m_planes[i];

		/* create dumb buffer */
		struct drm_mode_create_dumb creq = drm_mode_create_dumb();
		creq.width = width / pi.xsub;
		creq.height = height / pi.ysub;
		creq.bpp = pi.bitspp;
		r = drmIoctl(card().fd(), DRM_IOCTL_MODE_CREATE_DUMB, &creq);
		if (r)
			throw std::invalid_argument("foo");

		plane.handle = creq.handle;
		plane.stride = creq.pitch;
		plane.size = creq.height * creq.pitch;

		/*
		printf("buf %d: %dx%d, bitspp %d, stride %d, size %d\n",
			i, creq.width, creq.height, pi->bitspp, plane->stride, plane->size);
		*/

		/* prepare buffer for memory mapping */
		struct drm_mode_map_dumb mreq = drm_mode_map_dumb();
		mreq.handle = plane.handle;
		r = drmIoctl(card().fd(), DRM_IOCTL_MODE_MAP_DUMB, &mreq);
		if (r)
			throw std::invalid_argument("foo");

		/* perform actual memory mapping */
		m_planes[i].map = (uint8_t *)mmap(0, plane.size, PROT_READ | PROT_WRITE, MAP_SHARED,
						  card().fd(), mreq.offset);
		if (plane.map == MAP_FAILED)
			throw std::invalid_argument("foo");

		/* clear the framebuffer to 0 */
		memset(plane.map, 0, plane.size);
	}

	/* create framebuffer object for the dumb-buffer */
	uint32_t bo_handles[4] = { m_planes[0].handle, m_planes[1].handle };
	uint32_t pitches[4] = { m_planes[0].stride, m_planes[1].stride };
	uint32_t offsets[4] = { 0 };
	uint32_t id;
	r = drmModeAddFB2(card().fd(), width, height, format,
			  bo_handles, pitches, offsets, &id, 0);
	if (r)
		throw std::invalid_argument("foo");

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

	}
}

void DumbFramebuffer::clear()
{
	for (unsigned i = 0; i < m_num_planes; ++i)
		memset(m_planes[i].map, 0, m_planes[i].size);
}

}
