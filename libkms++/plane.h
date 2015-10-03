#pragma once

#include "drmobject.h"

namespace kms
{

enum class PlaneType
{
	Overlay = 0,
	Primary = 1,
	Cursor = 2,
};

struct PlanePriv;

class Plane : public DrmObject
{
	friend class Card;
public:
	void print_short() const;

	bool supports_crtc(Crtc* crtc) const;

	PlaneType plane_type() const;

private:
	Plane(Card& card, uint32_t id);
	~Plane();

	PlanePriv* m_priv;
};
}
