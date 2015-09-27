#pragma once

#include "drmobject.h"

namespace kms
{

struct PropertyPriv;

class Property : public DrmObject
{
public:
	Property(Card& card, uint32_t id);
	~Property();

	void print_short() const;

	const char *name() const;

private:
	PropertyPriv* m_priv;
};
}
