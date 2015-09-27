#pragma once

#include "drmobject.h"

namespace kms
{

class Framebuffer : public DrmObject
{
public:
	Framebuffer(Card& card, uint32_t width, uint32_t height, const char* fourcc);
	virtual ~Framebuffer();

	void print_short() const;

	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }
	uint32_t format() const { return m_format; }

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

	void Create(uint32_t width, uint32_t height, uint32_t format);
	void Destroy();

	unsigned m_num_planes;
	struct FramebufferPlane m_planes[4];

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_format;
};
}
