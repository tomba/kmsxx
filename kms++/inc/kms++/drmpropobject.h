#pragma once

#include <map>
#include <memory>

#include "drmobject.h"
#include "decls.h"

namespace kms
{

class DrmPropObject : public DrmObject
{
	friend class Card;
public:
	void refresh_props();

	Property* get_prop(const std::string& name) const;

	uint64_t get_prop_value(uint32_t id) const;
	uint64_t get_prop_value(const std::string& name) const;
	std::unique_ptr<Blob> get_prop_value_as_blob(const std::string& name) const;

	const std::map<uint32_t, uint64_t>& get_prop_map() const { return m_prop_values; }

	int set_prop_value(Property* prop, uint64_t value);
	int set_prop_value(uint32_t id, uint64_t value);
	int set_prop_value(const std::string& name, uint64_t value);

protected:
	DrmPropObject(Card& card, uint32_t object_type);
	DrmPropObject(Card& card, uint32_t id, uint32_t object_type, uint32_t idx = 0);

	virtual ~DrmPropObject();

private:
	std::map<uint32_t, uint64_t> m_prop_values;
};
}
