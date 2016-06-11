#pragma once

#include "drmobject.h"
#include <map>
#include <vector>

namespace kms
{

struct PropertyPriv;

enum class PropertyType
{
	Range,
	Enum,
	Blob,
	Bitmask,
	Object,
	SignedRange,
};

class Property : public DrmObject
{
	friend class Card;
public:
	const std::string& name() const;

	bool is_immutable() const;
	bool is_pending() const;

	PropertyType type() const { return m_type; }
	std::map<uint64_t, std::string> get_enums() const;
	std::vector<uint64_t> get_values() const;
	std::vector<uint32_t> get_blob_ids() const;
private:
	Property(Card& card, uint32_t id);
	~Property();

	PropertyType m_type;

	PropertyPriv* m_priv;
	std::string m_name;
};
}
