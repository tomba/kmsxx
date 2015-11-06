#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "kms++.h"

using namespace std;

namespace kms
{

struct PlanePriv
{
	drmModePlanePtr drm_plane;
};

Plane::Plane(Card &card, uint32_t id)
	:DrmObject(card, id, DRM_MODE_OBJECT_PLANE)
{
	m_priv = new PlanePriv();
	m_priv->drm_plane = drmModeGetPlane(this->card().fd(), this->id());
	assert(m_priv->drm_plane);
}

Plane::~Plane()
{
	drmModeFreePlane(m_priv->drm_plane);
	delete m_priv;
}

bool Plane::supports_crtc(Crtc* crtc) const
{
	return m_priv->drm_plane->possible_crtcs & (1 << crtc->idx());
}

bool Plane::supports_format(PixelFormat fmt) const
{
	auto p = m_priv->drm_plane;

	for (unsigned i = 0; i < p->count_formats; ++i)
		if ((uint32_t)fmt == p->formats[i])
			return true;

	return false;
}

PlaneType Plane::plane_type() const
{
	if (card().has_has_universal_planes())
		return (PlaneType)get_prop_value("type");
	else
		return PlaneType::Overlay;
}

vector<PixelFormat> Plane::get_formats() const
{
	auto p = m_priv->drm_plane;
	vector<PixelFormat> r;

	for (unsigned i = 0; i < p->count_formats; ++i)
		r.push_back((PixelFormat) p->formats[i]);

	return r;
}

uint32_t Plane::crtc_id() const
{
	return m_priv->drm_plane->crtc_id;
}

uint32_t Plane::fb_id() const
{
	return m_priv->drm_plane->fb_id;
}

uint32_t Plane::crtc_x() const
{
	return m_priv->drm_plane->crtc_x;
}

uint32_t Plane::crtc_y() const
{
	return m_priv->drm_plane->crtc_y;
}

uint32_t Plane::x() const
{
	return m_priv->drm_plane->x;
}

uint32_t Plane::y() const
{
	return m_priv->drm_plane->y;
}

uint32_t Plane::gamma_size() const
{
	return m_priv->drm_plane->gamma_size;
}

}
