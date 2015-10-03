
#include <chrono>
#include <cstring>
#include <cassert>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <drm.h>
#include <drm_mode.h>

#include "kms++.h"
#include "color.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

namespace kms
{
static void draw_pixel(DumbFramebuffer& buf, unsigned x, unsigned y, RGB color)
{
	static RGB c1;

	switch (buf.format()) {
	case PixelFormat::XRGB8888:
	{
		uint32_t *p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.raw;
		break;
	}
	case PixelFormat::RGB565:
	{
		uint16_t *p = (uint16_t*)(buf.map(0) + buf.stride(0) * y + x * 2);
		*p = color.rgb565();
		break;
	}
	case PixelFormat::UYVY:
	{
		if ((x & 1) == 0) {
			c1 = color;
			return;
		}

		uint8_t *p = (uint8_t*)(buf.map(0) + buf.stride(0) * y + x * 2);

		YUV yuv1 = c1.yuv();
		YUV yuv2 = color.yuv();

		p[0] = (yuv1.u + yuv2.u) / 2;
		p[1] = yuv1.y;
		p[2] = (yuv1.v + yuv2.v) / 2;
		p[3] = yuv2.y;
		break;
	}
	case PixelFormat::YUYV:
	{
		if ((x & 1) == 0) {
			c1 = color;
			return;
		}

		uint8_t *p = (uint8_t*)(buf.map(0) + buf.stride(0) * y + x * 2);

		YUV yuv1 = c1.yuv();
		YUV yuv2 = color.yuv();

		p[0] = yuv1.y;
		p[1] = (yuv1.u + yuv2.u) / 2;
		p[2] = yuv2.y;
		p[3] = (yuv1.v + yuv2.v) / 2;
		break;
	}
	default:
		throw std::invalid_argument("unknown pixelformat");
	}
}

static void draw_rgb_test_pattern(DumbFramebuffer& fb)
{
	unsigned x, y;
	unsigned w = fb.width();
	unsigned h = fb.height();

	const unsigned mw = 20;

	const unsigned xm1 = mw;
	const unsigned xm2 = w - mw - 1;
	const unsigned ym1 = mw;
	const unsigned ym2 = h - mw - 1;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			// white margin lines
			if (x == xm1 || x == xm2 || y == ym1 || y == ym2)
				draw_pixel(fb, x, y, RGB(255, 255, 255));
			// white box outlines to corners
			else if ((x == 0 || x == w - 1) && (y < ym1 || y > ym2))
				draw_pixel(fb, x, y, RGB(255, 255, 255));
			// white box outlines to corners
			else if ((y == 0 || y == h - 1) && (x < xm1 || x > xm2))
				draw_pixel(fb, x, y, RGB(255, 255, 255));
			// blue bar on the left
			else if (x < xm1 && (y > ym1 && y < ym2))
				draw_pixel(fb, x, y, RGB(0, 0, 255));
			// blue bar on the top
			else if (y < ym1 && (x > xm1 && x < xm2))
				draw_pixel(fb, x, y, RGB(0, 0, 255));
			// red bar on the right
			else if (x > xm2 && (y > ym1 && y < ym2))
				draw_pixel(fb, x, y, RGB(255, 0, 0));
			// red bar on the bottom
			else if (y > ym2 && (x > xm1 && x < xm2))
				draw_pixel(fb, x, y, RGB(255, 0, 0));
			// inside the margins
			else if (x > xm1 && x < xm2 && y > ym1 && y < ym2) {
				// diagonal line
				if (x == y || w - x == h - y)
					draw_pixel(fb, x, y, RGB(255, 255, 255));
				// diagonal line
				else if (w - x == y || x == h - y)
					draw_pixel(fb, x, y, RGB(255, 255, 255));
				else {
					int t = (x - xm1 - 1) * 3 / (xm2 - xm1 - 1);
					unsigned r = 0, g = 0, b = 0;

					unsigned c = (y - ym1 - 1) % 256;

					switch (t) {
					case 0:
						r = c;
						break;
					case 1:
						g = c;
						break;
					case 2:
						b = c;
						break;
					}

					draw_pixel(fb, x, y, RGB(r, g, b));
				}
				// black corners
			} else {
				draw_pixel(fb, x, y, RGB(0, 0, 0));
			}
		}
	}
}

void draw_test_pattern(DumbFramebuffer& fb)
{
	using namespace std::chrono;

	auto t1 = high_resolution_clock::now();

	draw_rgb_test_pattern(fb);

	auto t2 = high_resolution_clock::now();
	auto time_span = duration_cast<microseconds>(t2 - t1);

	printf("draw took %u us\n", (unsigned)time_span.count());
}
}
