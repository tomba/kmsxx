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
	virtual ~DumbFramebuffer();

	void print_short() const;

	PixelFormat format() const { return m_format; }

	uint8_t* map(unsigned plane) const { return m_planes[plane].map; }
	uint32_t stride(unsigned plane) const { return m_planes[plane].stride; }
	uint32_t size(unsigned plane) const { return m_planes[plane].size; }

	void clear();

private:
	struct FramebufferPlane {
		uint32_t handle;
		uint32_t size;
		uint32_t stride;
		uint8_t *map;
	};

	void Create();
	void Destroy();

	unsigned m_num_planes;
	struct FramebufferPlane m_planes[4];

	PixelFormat m_format;
};
}