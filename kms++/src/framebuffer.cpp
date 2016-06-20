#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{

Framebuffer::Framebuffer(Card& card, uint32_t width, uint32_t height)
	: DrmObject(card, DRM_MODE_OBJECT_FB), m_width(width), m_height(height)
{
	card.m_framebuffers.push_back(this);
}

Framebuffer::Framebuffer(Card& card, uint32_t id)
	: DrmObject(card, id, DRM_MODE_OBJECT_FB)
{
	auto fb = drmModeGetFB(card.fd(), id);

	if (fb) {
		m_width = fb->width;
		m_height = fb->height;

		drmModeFreeFB(fb);
	} else {
		m_width = m_height = 0;
	}

	card.m_framebuffers.push_back(this);
}

void Framebuffer::flush()
{
	drmModeClip clip { };
	clip.x1 = clip.y1 = 0;
	clip.x2 = width();
	clip.y2 = height();

	drmModeDirtyFB(card().fd(), id(), &clip, 1);
}

Framebuffer::~Framebuffer()
{
	auto& fbs = card().m_framebuffers;
	auto iter = find(fbs.begin(), fbs.end(), this);
	card().m_framebuffers.erase(iter);
}


}
