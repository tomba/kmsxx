
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

#include <kms++/kms++.h>
#include <kms++/omap/omapkms++.h>

extern "C" {
#include <omap_drmif.h>
}

using namespace std;

namespace kms
{

OmapFramebuffer::OmapFramebuffer(OmapCard& card, uint32_t width, uint32_t height, const string& fourcc, bool tiled)
	: OmapFramebuffer(card, width, height, FourCCToPixelFormat(fourcc), tiled)
{
}

OmapFramebuffer::OmapFramebuffer(OmapCard& card, uint32_t width, uint32_t height, PixelFormat format, bool tiled)
	:MappedFramebuffer(card, width, height), m_omap_card(card), m_format(format)
{
	Create(tiled);
}

OmapFramebuffer::~OmapFramebuffer()
{
	Destroy();
}

void OmapFramebuffer::Create(bool tiled)
{
	int r;

	const PixelFormatInfo& format_info = get_pixel_format_info(m_format);

	m_num_planes = format_info.num_planes;

	for (int i = 0; i < format_info.num_planes; ++i) {
		const PixelFormatPlaneInfo& pi = format_info.planes[i];
		FramebufferPlane& plane = m_planes[i];

		uint32_t flags = OMAP_BO_SCANOUT | OMAP_BO_WC;

		struct omap_bo* bo;

		uint32_t stride;

		if (!tiled) {
			stride = width() * pi.bitspp / 8;

			uint32_t size = stride * height() / pi.ysub;

			bo = omap_bo_new(m_omap_card.dev(), size, flags);
			if (!bo)
				throw invalid_argument(string("omap_bo_new failed: ") + strerror(errno));

			printf("omap_bo_new: %s plane %d, %ux%u, size %u, stride %u\n",
			       PixelFormatToFourCC(m_format).c_str(), i, width(), height(), omap_bo_size(bo), stride);
		} else {
			unsigned bitspermacro;

			switch (m_format) {
			case PixelFormat::NV12:
				bitspermacro = i == 0 ? 8 : 16; break;
			case PixelFormat::YUYV:
				bitspermacro = 16; break;
			case PixelFormat::XRGB8888:
				bitspermacro = 32; break;
			case PixelFormat::RGB565:
				bitspermacro = 16; break;
			default:
				throw invalid_argument("unimplemented format");
			}

			switch (bitspermacro) {
			case 8: flags |= OMAP_BO_TILED_8; break;
			case 16: flags |= OMAP_BO_TILED_16; break;
			case 32: flags |= OMAP_BO_TILED_32; break;
			default:
				throw invalid_argument("bad bitspermacro");
			}

			bo = omap_bo_new_tiled(m_omap_card.dev(), width(), height(), flags);
			if (!bo)
				throw invalid_argument(string("omap_bo_new_tiled failed: ") + strerror(errno));

			// XXX how does this go...
			stride = width() * pi.bitspp / 8;
			if (stride > 4096)
				stride = 4096 * 2;
			else
				stride = 4096;

			printf("omap_bo_new_tiled: %s plane %d, %ux%u, OMAP_BO_TILED_%u, size %u, stride %u\n",
			       PixelFormatToFourCC(m_format).c_str(), i, width(), height(), bitspermacro, omap_bo_size(bo), stride);
		}

		plane.omap_bo = bo;
		plane.handle = omap_bo_handle(bo);
		plane.stride = stride;
		plane.size = omap_bo_size(bo);
		plane.offset = 0;
		plane.map = 0;
		plane.prime_fd = -1;
	}

	/* create framebuffer object for the dumb-buffer */
	uint32_t bo_handles[4] = { m_planes[0].handle, m_planes[1].handle };
	uint32_t pitches[4] = { m_planes[0].stride, m_planes[1].stride };
	uint32_t offsets[4] = { m_planes[0].offset, m_planes[1].offset };
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
