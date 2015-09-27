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

void Plane::print_short() const
{
	auto p = m_priv->drm_plane;

	printf("Plane %d, %d modes, %d,%d -> %dx%d\n", id(),
	       p->count_formats,
	       p->crtc_x, p->crtc_y, p->x, p->y);

	printf("\t");
	for (unsigned i = 0; i < p->count_formats; ++i) {
		uint32_t f = p->formats[i];
		printf("%c%c%c%c ",
		       (f >> 0) & 0xff,
		       (f >> 8) & 0xff,
		       (f >> 16) & 0xff,
		       (f >> 24) & 0xff);
	}
	printf("\n");
}

bool Plane::supports_crtc(Crtc* crtc) const
{
	return m_priv->drm_plane->possible_crtcs & (1 << crtc->idx());
}

PlaneType Plane::plane_type() const
{
	return (PlaneType)get_prop_value("type");
}
}
