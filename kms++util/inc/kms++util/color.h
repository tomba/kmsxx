#pragma once

#include <cstdint>

namespace kms
{
struct YUV;

enum class YUVType {
	BT601_Lim = 0,
	BT601_Full,
	BT709_Lim,
	BT709_Full,
	MAX,
};

struct RGB
{
	RGB();
	RGB(uint8_t r, uint8_t g, uint8_t b);
	RGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b);
	RGB(uint32_t argb);

	uint32_t rgb888() const;
	uint32_t bgr888() const;
	uint32_t argb8888() const;
	uint32_t abgr8888() const;
	uint32_t rgba8888() const;
	uint32_t bgra8888() const;

	// XXX these functions leave the lowest 2 bits zero
	uint32_t argb2101010() const;
	uint32_t abgr2101010() const;
	uint32_t rgba1010102() const;
	uint32_t bgra1010102() const;

	uint16_t rgb565() const;
	uint16_t bgr565() const;
	uint16_t argb4444() const;
	uint16_t argb1555() const;
	YUV yuv(YUVType type = YUVType::BT601_Lim) const;

	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a;
};

struct YUV
{
	YUV();
	YUV(uint8_t y, uint8_t u, uint8_t v);
	YUV(const RGB& rgb, YUVType type = YUVType::BT601_Lim);

	uint8_t v;
	uint8_t u;
	uint8_t y;
	uint8_t a;
};
}
