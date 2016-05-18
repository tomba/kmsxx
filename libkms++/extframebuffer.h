#pragma once

#include "framebuffer.h"
#include "pixelformats.h"

namespace kms
{

class ExtFramebuffer : public Framebuffer
{
public:
	ExtFramebuffer(Card& card, uint32_t width, uint32_t height, uint8_t depth, uint8_t bpp, uint32_t stride, uint32_t handle);
	ExtFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format,
		       uint32_t handles[4], uint32_t pitches[4], uint32_t offsets[4]);
	ExtFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format,
		       int fds[4], uint32_t pitches[4], uint32_t offsets[4]);
	virtual ~ExtFramebuffer();

private:
};
}
