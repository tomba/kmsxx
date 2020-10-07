#pragma once

#include <array>
#include <vector>

#include "framebuffer.h"
#include "pixelformats.h"

namespace kms
{
class ExtFramebuffer : public Framebuffer
{
public:
	ExtFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format,
		       std::vector<uint32_t> handles, std::vector<uint32_t> pitches, std::vector<uint32_t> offsets, std::vector<uint64_t> modifiers = {});
	~ExtFramebuffer() override;

	uint32_t width() const override { return Framebuffer::width(); }
	uint32_t height() const override { return Framebuffer::height(); }

	PixelFormat format() const override { return m_format; }
	unsigned num_planes() const override { return m_num_planes; }

	uint32_t handle(unsigned plane) const { return m_planes.at(plane).handle; }
	uint32_t stride(unsigned plane) const override { return m_planes.at(plane).stride; }
	uint32_t size(unsigned plane) const override { return m_planes.at(plane).size; }
	uint32_t offset(unsigned plane) const override { return m_planes.at(plane).offset; }

private:
	struct FramebufferPlane {
		uint32_t handle;
		uint32_t size;
		uint32_t stride;
		uint32_t offset;
		uint64_t modifier;
		uint8_t* map;
	};

	unsigned m_num_planes;
	std::array<FramebufferPlane, 4> m_planes;

	PixelFormat m_format;
};

} // namespace kms
