#pragma once

#include <kms++/framebuffer.h>
#include <kms++/pixelformats.h>

struct omap_bo;

namespace kms
{
class OmapCard;

class OmapFramebuffer : public Framebuffer
{
public:
	enum Flags
	{
		None = 0,
		Tiled = 1 << 0,
		MemContig = 1 << 1,
		MemTiler = 1 << 2,
		MemPin = 1 << 3,
	};

	OmapFramebuffer(OmapCard& card, uint32_t width, uint32_t height, const std::string& fourcc, Flags flags = Flags::None);
	OmapFramebuffer(OmapCard& card, uint32_t width, uint32_t height, PixelFormat format, Flags flags = Flags::None);
	virtual ~OmapFramebuffer();

	uint32_t width() const { return Framebuffer::width(); }
	uint32_t height() const { return Framebuffer::height(); }

	PixelFormat format() const { return m_format; }
	unsigned num_planes() const { return m_num_planes; }

	uint32_t handle(unsigned plane) const { return m_planes[plane].handle; }
	uint32_t stride(unsigned plane) const { return m_planes[plane].stride; }
	uint32_t size(unsigned plane) const { return m_planes[plane].size; }
	uint32_t offset(unsigned plane) const { return m_planes[plane].offset; }
	uint8_t* map(unsigned plane);
	int prime_fd(unsigned plane);

private:
	OmapCard& m_omap_card;

	struct FramebufferPlane {
		struct omap_bo* omap_bo;
		uint32_t handle;
		int prime_fd;
		uint32_t size;
		uint32_t stride;
		uint32_t offset;
		uint8_t* map;
	};

	void Create(Flags buffer_flags);
	void Destroy();

	unsigned m_num_planes;
	struct FramebufferPlane m_planes[3];

	PixelFormat m_format;
};
}
