#pragma once

#include "drmobject.h"
#include "pixelformats.h"

namespace kms
{
class IFramebuffer {
public:
	virtual ~IFramebuffer() { }

	virtual uint32_t width() const = 0;
	virtual uint32_t height() const = 0;

	virtual PixelFormat format() const { throw std::runtime_error("not implemented"); }
	virtual unsigned num_planes() const { throw std::runtime_error("not implemented"); }

	virtual uint32_t stride(unsigned plane) const { throw std::runtime_error("not implemented"); }
	virtual uint32_t size(unsigned plane) const { throw std::runtime_error("not implemented"); }
	virtual uint32_t offset(unsigned plane) const { throw std::runtime_error("not implemented"); }
	virtual uint8_t* map(unsigned plane) { throw std::runtime_error("not implemented"); }
	virtual int prime_fd(unsigned plane) { throw std::runtime_error("not implemented"); }
};

class Framebuffer : public DrmObject, public IFramebuffer
{
public:
	Framebuffer(Card& card, uint32_t id);
	virtual ~Framebuffer();

	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }

	void flush();
protected:
	Framebuffer(Card& card, uint32_t width, uint32_t height);

private:
	uint32_t m_width;
	uint32_t m_height;
};

}
