#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <algorithm>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{

struct PlanePriv
{
	drmModePlanePtr drm_plane;
};

Plane::Plane(Card &card, uint32_t id, uint32_t idx)
	:DrmPropObject(card, id, DRM_MODE_OBJECT_PLANE, idx)
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
	if (card().has_has_universal_planes()) {
		switch (get_prop_value("type")) {
		case DRM_PLANE_TYPE_OVERLAY:
			return PlaneType::Overlay;
		case DRM_PLANE_TYPE_PRIMARY:
			return PlaneType::Primary;
		case DRM_PLANE_TYPE_CURSOR:
			return PlaneType::Cursor;
		default:
			throw invalid_argument("Bad plane type");
		}
	} else {
		return PlaneType::Overlay;
	}
}

vector<Crtc*> Plane::get_possible_crtcs() const
{
	unsigned idx = 0;
	vector<Crtc*> v;
	auto crtcs = card().get_crtcs();

	for (uint32_t crtc_mask = m_priv->drm_plane->possible_crtcs;
	     crtc_mask;
	     idx++, crtc_mask >>= 1) {

		if ((crtc_mask & 1) == 0)
			continue;

		auto iter = find_if(crtcs.begin(), crtcs.end(), [idx](Crtc* crtc) { return crtc->idx() == idx; });

		if (iter == crtcs.end())
			throw runtime_error("get_possible_crtcs: crtc missing");

		v.push_back(*iter);
	}

	return v;
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
