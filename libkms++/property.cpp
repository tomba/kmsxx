#include <xf86drm.h>
#include <xf86drmMode.h>

#include "kms++.h"

using namespace std;

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
	m_name = m_priv->drm_prop->name;
}

Property::~Property()
{
	drmModeFreeProperty(m_priv->drm_prop);
	delete m_priv;
}

void Property::print_short() const
{
	printf("Property %d, %s\n", id(), name().c_str());
}

const string& Property::name() const
{
	return m_name;
}
}
