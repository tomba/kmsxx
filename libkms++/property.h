#pragma once

#include "drmobject.h"

namespace kms
{

struct PropertyPriv;

class Property : public DrmObject
{
	friend class Card;
public:
	void print_short() const;

	const char *name() const;

private:
	Property(Card& card, uint32_t id);
	~Property();

	PropertyPriv* m_priv;
};
}
