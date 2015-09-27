#include <xf86drm.h>
#include <xf86drmMode.h>

#include "kms++.h"

namespace kms
{

struct PropertyPriv
{
	drmModePropertyPtr drm_prop;
};

Property::Property(Card& card, uint32_t id)
	: DrmObject(card, id, DRM_MODE_OBJECT_PROPERTY)
{
	m_priv = new PropertyPriv();
	m_priv->drm_prop = drmModeGetProperty(card.fd(), id);
}

Property::~Property()
{
	drmModeFreeProperty(m_priv->drm_prop);
	delete m_priv;
}

void Property::print_short() const
{
	printf("Property %d, %s\n", id(), name());
}

const char *Property::name() const
{
	return m_priv->drm_prop->name;
}
}
