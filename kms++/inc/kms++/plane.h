#pragma once

#include "drmpropobject.h"

namespace kms
{

enum class PlaneType
{
	None = 0,
	Overlay = 1 << 0,
	Primary = 1 << 1,
	Cursor = 1 << 2,
};

inline constexpr PlaneType operator&(PlaneType a, PlaneType b)
{
	return static_cast<PlaneType>(static_cast<uint>(a) &
				      static_cast<uint>(b));
}

inline constexpr PlaneType operator|(PlaneType a, PlaneType b)
{
	return static_cast<PlaneType> (static_cast<uint>(a) |
				       static_cast<uint>(b));
}

inline constexpr PlaneType operator~(PlaneType a)
{
	return static_cast<PlaneType> (~static_cast<uint>(a));
}

struct PlanePriv;

class Plane : public DrmPropObject
{
	friend class Card;
public:
	bool supports_crtc(Crtc* crtc) const;
	bool supports_format(PixelFormat fmt) const;

	PlaneType plane_type() const;

	std::vector<Crtc*> get_possible_crtcs() const;
	std::vector<PixelFormat> get_formats() const;
	uint32_t crtc_id() const;
	uint32_t fb_id() const;

	uint32_t crtc_x() const;
	uint32_t crtc_y() const;
	uint32_t x() const;
	uint32_t y() const;
	uint32_t gamma_size() const;
private:
	Plane(Card& card, uint32_t id, uint32_t idx);
	~Plane();

	PlanePriv* m_priv;
};
}
