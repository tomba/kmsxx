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

const std::string Property::to_str(uint64_t val) const
{
	drmModePropertyPtr p = m_priv->drm_prop;
	string ret;

	if (p->flags & DRM_MODE_PROP_ENUM) {
		for (int i = 0; i < p->count_enums; i++) {
			if (p->enums[i].value == val) {
				ret += string("\"") + p->enums[i].name + "\"";
				break;
			}
		}
		ret += " (enum: " + to_string(val) + ")";
	} else if (p->flags & DRM_MODE_PROP_RANGE) {
		ret += to_string(val);
		if (p->count_values == 2)
			ret += " [" + to_string(p->values[0]) + "-" +
				to_string(p->values[1]) + "]";
		else
			ret += " <broken range>";
	} else if (p->flags & DRM_MODE_PROP_BLOB) {
		ret += "Blob id: " + to_string(val);

		auto blob = drmModeGetPropertyBlob(card().fd(), (uint32_t) val);
		if (blob) {
			ret += " length: " + to_string(blob->length);
			drmModeFreePropertyBlob(blob);
		}
	} else {
		ret += to_string(val);
	}

	if (p->flags & DRM_MODE_PROP_PENDING)
		ret += " (pendig)";
	if (p->flags & DRM_MODE_PROP_IMMUTABLE)
		ret += " (immutable)";

	return ret;
}
}
