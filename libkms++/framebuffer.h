#pragma once

#include "drmobject.h"
#include "pixelformats.h"

namespace kms
{
class Framebuffer : public DrmObject
{
public:
	Framebuffer(Card& card, uint32_t id);
	virtual ~Framebuffer();

	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }
protected:
	Framebuffer(Card& card, int width, int height);

private:
	uint32_t m_width;
	uint32_t m_height;
};

class IMappedFramebuffer {
public:
	virtual ~IMappedFramebuffer() { }

	virtual uint32_t width() const = 0;
	virtual uint32_t height() const = 0;

	virtual PixelFormat format() const = 0;
	virtual unsigned num_planes() const = 0;

	virtual uint32_t stride(unsigned plane) const = 0;
	virtual uint32_t size(unsigned plane) const = 0;
	virtual uint32_t offset(unsigned plane) const = 0;
	virtual uint8_t* map(unsigned plane) = 0;
};

}
