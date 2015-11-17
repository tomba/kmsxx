#include <algorithm>
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
	card.m_framebuffers.push_back(this);
}

Framebuffer::Framebuffer(Card& card, uint32_t id)
	: DrmObject(card, id, DRM_MODE_OBJECT_FB)
{
	auto fb = drmModeGetFB(card.fd(), id);

	m_width = fb->width;
	m_height = fb->height;

	drmModeFreeFB(fb);

	card.m_framebuffers.push_back(this);
}

Framebuffer::~Framebuffer()
{
	auto& fbs = card().m_framebuffers;
	auto iter = find(fbs.begin(), fbs.end(), this);
	card().m_framebuffers.erase(iter);
}


}
