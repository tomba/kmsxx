#pragma once

#include <cstdint>

namespace kms
{
struct YUV;

struct RGB
{
	RGB();
	RGB(uint8_t r, uint8_t g, uint8_t b);

	uint16_t rgb565() const;
	YUV yuv() const;

	union {
		struct
		{
			uint8_t b;
			uint8_t g;
			uint8_t r;
			uint8_t a;
		};

		uint32_t raw;
	};
};

struct YUV
{
	YUV();
	YUV(uint8_t y, uint8_t u, uint8_t v);
	YUV(const RGB& rgb);

	union {
		struct
		{
			uint8_t v;
			uint8_t u;
			uint8_t y;
			uint8_t a;
		};

		uint32_t raw;
	};
};
}
