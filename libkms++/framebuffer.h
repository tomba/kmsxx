#pragma once

#include "drmobject.h"

namespace kms
{
class Framebuffer : public DrmObject
{
public:
	Framebuffer(Card& card, uint32_t id);
	virtual ~Framebuffer() { }

	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }
protected:
	Framebuffer(Card& card, int width, int height);

private:
	uint32_t m_width;
	uint32_t m_height;
};
}
