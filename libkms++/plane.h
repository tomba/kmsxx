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
public:
	Plane(Card& card, uint32_t id);
	~Plane();

	void print_short() const;

	bool supports_crtc(Crtc* crtc) const;

	PlaneType plane_type() const;

private:
	PlanePriv* m_priv;
};
}
