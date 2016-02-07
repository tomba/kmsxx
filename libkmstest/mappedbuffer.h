#pragma once

#include "kms++.h"

namespace kms
{

class MappedBuffer {
public:
	MappedBuffer()
	{
	}

	virtual ~MappedBuffer()
	{
	}

	virtual uint32_t width() const = 0;
	virtual uint32_t height() const = 0;

	virtual PixelFormat format() const = 0;
	virtual unsigned num_planes() const = 0;

	virtual uint32_t stride(unsigned plane) const = 0;
	virtual uint32_t size(unsigned plane) const = 0;
	virtual uint32_t offset(unsigned plane) const = 0;
	virtual uint8_t* map(unsigned plane) = 0;
};

class MappedDumbBuffer : public MappedBuffer {
public:
	MappedDumbBuffer(DumbFramebuffer& dumbfb)
		: m_fb(dumbfb)
	{

	}

	virtual ~MappedDumbBuffer()
	{

	}

	uint32_t width() const { return m_fb.width(); }
	uint32_t height() const { return m_fb.height(); }

	PixelFormat format() const { return m_fb.format(); }
	unsigned num_planes() const { return m_fb.num_planes(); }

	uint32_t stride(unsigned plane) const { return m_fb.stride(plane); }
	uint32_t size(unsigned plane) const { return m_fb.size(plane); }
	uint32_t offset(unsigned plane) const { return m_fb.offset(plane); }
	uint8_t* map(unsigned plane) { return m_fb.map(plane); }

private:
	DumbFramebuffer& m_fb;
};

class MappedCPUBuffer : public MappedBuffer {
public:
	MappedCPUBuffer(uint32_t width, uint32_t height, PixelFormat format);

	virtual ~MappedCPUBuffer();

	MappedCPUBuffer(const MappedCPUBuffer& other) = delete;
	MappedCPUBuffer& operator=(const MappedCPUBuffer& other) = delete;

	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }

	PixelFormat format() const { return m_format; }
	unsigned num_planes() const { return m_num_planes; }

	uint32_t stride(unsigned plane) const { return m_planes[plane].stride; }
	uint32_t size(unsigned plane) const { return m_planes[plane].size; }
	uint32_t offset(unsigned plane) const { return m_planes[plane].offset; }
	uint8_t* map(unsigned plane) { return m_planes[plane].map; }


private:
	struct FramebufferPlane {
		uint32_t size;
		uint32_t stride;
		uint32_t offset;
		uint8_t *map;
	};

	uint32_t m_width;
	uint32_t m_height;
	PixelFormat m_format;

	unsigned m_num_planes;
	struct FramebufferPlane m_planes[4];
};

}
