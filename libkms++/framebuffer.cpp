#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "kms++.h"

using namespace std;

namespace kms
{

Framebuffer::Framebuffer(Card& card, int width, int height)
	: DrmObject(card, DRM_MODE_OBJECT_FB), m_width(width), m_height(height)
{
}

Framebuffer::Framebuffer(Card& card, uint32_t id)
	: DrmObject(card, id, DRM_MODE_OBJECT_FB)
{
	auto fb = drmModeGetFB(card.fd(), id);

	m_width = fb->width;
	m_height = fb->height;

	drmModeFreeFB(fb);
}

}
