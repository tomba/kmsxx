#pragma once

#include <kms++/kms++.h>

namespace kms
{

class CPUFramebuffer : public IFramebuffer {
public:
	CPUFramebuffer(uint32_t width, uint32_t height, PixelFormat format);

	virtual ~CPUFramebuffer();

	CPUFramebuffer(const CPUFramebuffer& other) = delete;
	CPUFramebuffer& operator=(const CPUFramebuffer& other) = delete;

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
