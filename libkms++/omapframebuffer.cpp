
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

extern "C" {
#include <omap_drmif.h>
}

#if 0
struct omap_bo * omap_bo_new(struct omap_device *dev,
		uint32_t size, uint32_t flags);
struct omap_bo * omap_bo_new_tiled(struct omap_device *dev,
		uint32_t width, uint32_t height, uint32_t flags);
struct omap_bo * omap_bo_ref(struct omap_bo *bo);
struct omap_bo * omap_bo_from_name(struct omap_device *dev, uint32_t name);
struct omap_bo * omap_bo_from_dmabuf(struct omap_device *dev, int fd);
void omap_bo_del(struct omap_bo *bo);
int omap_bo_get_name(struct omap_bo *bo, uint32_t *name);
uint32_t omap_bo_handle(struct omap_bo *bo);
int omap_bo_dmabuf(struct omap_bo *bo);
uint32_t omap_bo_size(struct omap_bo *bo);
void * omap_bo_map(struct omap_bo *bo);
int omap_bo_cpu_prep(struct omap_bo *bo, enum omap_gem_op op);
int omap_bo_cpu_fini(struct omap_bo *bo, enum omap_gem_op op);
#endif

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

using namespace std;

namespace kms
{

OmapFramebuffer::OmapFramebuffer(OmapCard &card, uint32_t width, uint32_t height, const string& fourcc)
	:OmapFramebuffer(card, width, height, FourCCToPixelFormat(fourcc))
{
}

OmapFramebuffer::OmapFramebuffer(OmapCard& card, uint32_t width, uint32_t height, PixelFormat format)
        :Framebuffer(card, width, height), m_omap_card(card), m_format(format)
{
	Create();
}

OmapFramebuffer::~OmapFramebuffer()
{
	Destroy();
}

void OmapFramebuffer::Create()
{
	int r;

	const PixelFormatInfo& format_info = get_pixel_format_info(m_format);

	m_num_planes = format_info.num_planes;

	for (int i = 0; i < format_info.num_planes; ++i) {
		const PixelFormatPlaneInfo& pi = format_info.planes[i];
		FramebufferPlane& plane = m_planes[i];

		uint32_t size = width() * height() * pi.bitspp / 8;
		uint32_t flags = OMAP_BO_SCANOUT | OMAP_BO_WC;

		struct omap_bo* bo;

		bo = omap_bo_new(m_omap_card.dev(), size, flags);
		if (!bo)
			throw invalid_argument(string("omap_bo_new failed") + strerror(errno));

		plane.omap_bo = bo;
		plane.handle = omap_bo_handle(bo);
		plane.stride = width() * pi.bitspp / 8;
		plane.size = omap_bo_size(bo);
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

void OmapFramebuffer::Destroy()
{
	/* delete framebuffer */
	drmModeRmFB(card().fd(), id());

	for (uint i = 0; i < m_num_planes; ++i) {
		FramebufferPlane& plane = m_planes[i];

		/* unmap buffer */
		if (plane.map)
			munmap(plane.map, plane.size);

		omap_bo_del(plane.omap_bo);

		if (plane.prime_fd >= 0)
			::close(plane.prime_fd);
	}
}

uint8_t* OmapFramebuffer::map(unsigned plane)
{
	FramebufferPlane& p = m_planes[plane];

	if (p.map)
		return p.map;

	p.map = (uint8_t*)omap_bo_map(p.omap_bo);
	if (p.map == MAP_FAILED)
		throw invalid_argument(string("mmap failed: ") + strerror(errno));

	return p.map;
}

int OmapFramebuffer::prime_fd(unsigned int plane)
{
	FramebufferPlane& p = m_planes[plane];

	if (p.prime_fd >= 0)
		return p.prime_fd;

	int fd = omap_bo_dmabuf(p.omap_bo);
	if (fd < 0)
		throw std::runtime_error("omap_bo_dmabuf failed\n");

	p.prime_fd = fd;

	return fd;
}

void OmapFramebuffer::prep()
{
	//OMAP_GEM_READ = 0x01,
	//OMAP_GEM_WRITE = 0x02,

	for (uint i = 0; i < m_num_planes; ++i) {
		FramebufferPlane& plane = m_planes[i];

		printf("prep %d\n", i);
		int r = omap_bo_cpu_prep(plane.omap_bo, (omap_gem_op)(OMAP_GEM_WRITE | OMAP_GEM_READ));
		if (r)
			throw std::runtime_error("prep failed");
	}
}

void OmapFramebuffer::unprep()
{
	for (uint i = 0; i < m_num_planes; ++i) {
		FramebufferPlane& plane = m_planes[i];

		int r = omap_bo_cpu_fini(plane.omap_bo, (omap_gem_op)(OMAP_GEM_WRITE | OMAP_GEM_READ));
		if (r)
			throw std::runtime_error("unprep failed");
	}
}

}
