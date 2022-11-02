#pragma once

#include "drmobject.h"
#include "pixelformats.h"

namespace kms
{
enum class CpuAccess {
	Read,
	Write,
	ReadWrite,
};

class IFramebuffer
{
public:
	virtual ~IFramebuffer() {}

	virtual uint32_t width() const = 0;
	virtual uint32_t height() const = 0;

	virtual PixelFormat format() const { throw std::runtime_error("not implemented"); }
	virtual unsigned num_planes() const { throw std::runtime_error("not implemented"); }

	virtual uint32_t stride(unsigned plane) const { throw std::runtime_error("not implemented"); }
	virtual uint32_t size(unsigned plane) const { throw std::runtime_error("not implemented"); }
	virtual uint32_t offset(unsigned plane) const { throw std::runtime_error("not implemented"); }
	virtual uint8_t* map(unsigned plane) { throw std::runtime_error("not implemented"); }
	virtual int prime_fd(unsigned plane) { throw std::runtime_error("not implemented"); }

	virtual void begin_cpu_access(CpuAccess access) {}
	virtual void end_cpu_access() {}
};

class Framebuffer : public DrmObject, public IFramebuffer
{
public:
	Framebuffer(Card& card, uint32_t id);
	~Framebuffer() override;

	uint32_t width() const override { return m_width; }
	uint32_t height() const override { return m_height; }
	PixelFormat format() const override { return m_format; }

	void flush(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
	void flush();

protected:
	Framebuffer(Card& card, uint32_t width, uint32_t height);

private:
	uint32_t m_width;
	uint32_t m_height;
	PixelFormat m_format;
};

} // namespace kms
