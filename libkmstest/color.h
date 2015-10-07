#pragma once

#include <cstdint>

namespace kms
{
struct YUV;

struct RGB
{
	RGB();
	RGB(uint8_t r, uint8_t g, uint8_t b);

	uint32_t rgb888() const;
	uint32_t bgr888() const;
	uint16_t rgb565() const;
	YUV yuv() const;

	struct
	{
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t a;
	};
};

struct YUV
{
	YUV();
	YUV(uint8_t y, uint8_t u, uint8_t v);
	YUV(const RGB& rgb);

	struct
	{
		uint8_t v;
		uint8_t u;
		uint8_t y;
		uint8_t a;
	};
};
}
