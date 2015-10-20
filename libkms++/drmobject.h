#pragma once

#include <map>

#include "decls.h"

namespace kms
{

class DrmObject
{
	friend class Card;
public:
	DrmObject(const DrmObject& other) = delete;
	DrmObject& operator=(const DrmObject& other) = delete;

	virtual void print_short() const = 0;
	void print_props() const;

	uint32_t id() const { return m_id; }
	Card& card() const { return m_card; }

	uint32_t object_type() const { return m_object_type; }
	uint32_t idx() const { return m_idx; }

	void refresh_props();
	uint64_t get_prop_value(uint32_t id) const;
	uint64_t get_prop_value(const std::string& name) const;

	const std::map<uint32_t, uint64_t>& get_prop_map() const {
		return m_prop_values;
	}
protected:
	DrmObject(Card& card, uint32_t object_type);
	DrmObject(Card& card, uint32_t id, uint32_t object_type, uint32_t idx = 0);

	virtual ~DrmObject();

	virtual void setup() { }

	virtual void set_id(uint32_t id);

private:
	Card& m_card;

	uint32_t m_id;
	uint32_t m_object_type;
	uint32_t m_idx;

	std::map<uint32_t, uint64_t> m_prop_values;
};
}
