#pragma once

#include "framebuffer.h"
#include "pixelformats.h"

namespace kms
{

class ExtFramebuffer : public Framebuffer
{
public:
	ExtFramebuffer(Card& card, uint32_t width, uint32_t height, uint32_t depth, uint32_t bpp, uint32_t stride, uint32_t handle);
	ExtFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format,
		       uint32_t handles[4], uint32_t pitches[4], uint32_t offsets[4]);
	virtual ~ExtFramebuffer();

	void print_short() const;

private:
	uint32_t m_pitch;
	uint32_t m_bpp;
	uint32_t m_depth;
};
}
