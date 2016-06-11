#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

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

	PropertyType t;
	drmModePropertyPtr p = m_priv->drm_prop;
	if (drm_property_type_is(p, DRM_MODE_PROP_BITMASK))
		t = PropertyType::Bitmask;
	else if (drm_property_type_is(p, DRM_MODE_PROP_BLOB))
		t = PropertyType::Blob;
	else if (drm_property_type_is(p, DRM_MODE_PROP_ENUM))
		t = PropertyType::Enum;
	else if (drm_property_type_is(p, DRM_MODE_PROP_OBJECT))
		t = PropertyType::Object;
	else if (drm_property_type_is(p, DRM_MODE_PROP_RANGE))
		t = PropertyType::Range;
	else if (drm_property_type_is(p, DRM_MODE_PROP_SIGNED_RANGE))
		t = PropertyType::SignedRange;
	else
		throw invalid_argument("Invalid property type");

	m_type = t;
}

Property::~Property()
{
	drmModeFreeProperty(m_priv->drm_prop);
	delete m_priv;
}

const string& Property::name() const
{
	return m_name;
}

bool Property::is_immutable() const
{
	return m_priv->drm_prop->flags & DRM_MODE_PROP_IMMUTABLE;
}

bool Property::is_pending() const
{
	return m_priv->drm_prop->flags & DRM_MODE_PROP_PENDING;
}

vector<uint64_t> Property::get_values() const
{
	drmModePropertyPtr p = m_priv->drm_prop;
	return vector<uint64_t>(p->values, p->values + p->count_values);
}

map<uint64_t, string> Property::get_enums() const
{
	drmModePropertyPtr p = m_priv->drm_prop;

	map<uint64_t, string> map;

	for (int i = 0; i < p->count_enums; ++i)
		map[p->enums[i].value] = string(p->enums[i].name);

	return map;
}

vector<uint32_t> Property::get_blob_ids() const
{
	drmModePropertyPtr p = m_priv->drm_prop;
	return vector<uint32_t>(p->blob_ids, p->blob_ids + p->count_blobs);
}
}
