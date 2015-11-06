#pragma once

#include "drmobject.h"

namespace kms
{

struct PropertyPriv;

class Property : public DrmObject
{
	friend class Card;
public:
	const std::string& name() const;

	const std::string to_str(uint64_t val) const;
private:
	Property(Card& card, uint32_t id);
	~Property();

	PropertyPriv* m_priv;
	std::string m_name;
};
}
