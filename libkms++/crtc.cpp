#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>

#include "kms++.h"
#include "helpers.h"

using namespace std;

namespace kms
{

struct CrtcPriv
{
	drmModeCrtcPtr drm_crtc;
};

Crtc::Crtc(Card &card, uint32_t id, uint32_t idx)
	:DrmObject(card, id, DRM_MODE_OBJECT_CRTC, idx)
{
	m_priv = new CrtcPriv();
	m_priv->drm_crtc = drmModeGetCrtc(this->card().fd(), this->id());
	assert(m_priv->drm_crtc);
}

Crtc::~Crtc()
{
	drmModeFreeCrtc(m_priv->drm_crtc);
	delete m_priv;
}

void Crtc::setup()
{
	for (Plane* plane : card().get_planes()) {
		if (plane->supports_crtc(this))
			m_possible_planes.push_back(plane);
	}
}

void Crtc::print_short() const
{
	auto c  = m_priv->drm_crtc;

	printf("Crtc %d, %d,%d %dx%d\n", id(),
	       c->x, c->y, c->width, c->height);
}

int Crtc::set_mode(Connector* conn, Framebuffer& fb, const Videomode& mode)
{
	uint32_t conns[] = { conn->id() };
	drmModeModeInfo drmmode = video_mode_to_drm_mode(mode);

	return drmModeSetCrtc(card().fd(), id(), fb.id(),
			      0, 0,
			      conns, 1, &drmmode);
}

static inline uint32_t conv(float x)
{
	// XXX fix the conversion for fractional part
	return ((uint32_t)x) << 16;
}

int Crtc::set_plane(Plane* plane, Framebuffer& fb,
		    int32_t dst_x, int32_t dst_y, uint32_t dst_w, uint32_t dst_h,
		    float src_x, float src_y, float src_w, float src_h)
{
	return drmModeSetPlane(card().fd(), plane->id(), id(), fb.id(), 0,
			       dst_x, dst_y, dst_w, dst_h,
			       conv(src_x), conv(src_y), conv(src_w), conv(src_h));
}
}
