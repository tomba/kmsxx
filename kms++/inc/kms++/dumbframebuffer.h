#pragma once

#include "framebuffer.h"
#include "pixelformats.h"

namespace kms
{

class DumbFramebuffer : public Framebuffer
{
public:
	DumbFramebuffer(Card& card, uint32_t width, uint32_t height, const std::string& fourcc);
	DumbFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format);
	~DumbFramebuffer() override;

	uint32_t width() const override { return Framebuffer::width(); }
	uint32_t height() const override { return Framebuffer::height(); }

	PixelFormat format() const override { return m_format; }
	unsigned num_planes() const override { return m_num_planes; }

	uint32_t handle(unsigned plane) const { return m_planes[plane].handle; }
	uint32_t stride(unsigned plane) const override { return m_planes[plane].stride; }
	uint32_t size(unsigned plane) const override { return m_planes[plane].size; }
	uint32_t offset(unsigned plane) const override { return m_planes[plane].offset; }
	uint8_t* map(unsigned plane) override;
	int prime_fd(unsigned plane) override;

private:
	struct FramebufferPlane {
		uint32_t handle;
		int prime_fd;
		uint32_t size;
		uint32_t stride;
		uint32_t offset;
		uint8_t *map;
	};

	unsigned m_num_planes;
	struct FramebufferPlane m_planes[4];

	PixelFormat m_format;
};
}
