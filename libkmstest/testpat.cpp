
//#define DRAW_PERF_PRINT

#include <chrono>
#include <cstring>
#include <cassert>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <drm.h>
#include <drm_mode.h>

#include "kms++.h"
#include "test.h"
#include "mappedbuffer.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

namespace kms
{
static void draw_rgb_pixel(MappedBuffer& buf, unsigned x, unsigned y, RGB color)
{
	switch (buf.format()) {
	case PixelFormat::XRGB8888:
	case PixelFormat::ARGB8888:
	{
		uint32_t *p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.argb8888();
		break;
	}
	case PixelFormat::XBGR8888:
	case PixelFormat::ABGR8888:
	{
		uint32_t *p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.abgr8888();
		break;
	}
	case PixelFormat::RGB565:
	{
		uint16_t *p = (uint16_t*)(buf.map(0) + buf.stride(0) * y + x * 2);
		*p = color.rgb565();
		break;
	}
	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

static void draw_yuv422_macropixel(MappedBuffer& buf, unsigned x, unsigned y, YUV yuv1, YUV yuv2)
{
	ASSERT((x & 1) == 0);

	uint8_t *p = (uint8_t*)(buf.map(0) + buf.stride(0) * y + x * 2);

	uint8_t y0 = yuv1.y;
	uint8_t y1 = yuv2.y;
	uint8_t u = (yuv1.u + yuv2.u) / 2;
	uint8_t v = (yuv1.v + yuv2.v) / 2;

	switch (buf.format()) {
	case PixelFormat::UYVY:
		p[0] = u;
		p[1] = y0;
		p[2] = v;
		p[3] = y1;
		break;

	case PixelFormat::YUYV:
		p[0] = y0;
		p[1] = u;
		p[2] = y1;
		p[3] = v;
		break;

	case PixelFormat::YVYU:
		p[0] = y0;
		p[1] = v;
		p[2] = y1;
		p[3] = u;
		break;

	case PixelFormat::VYUY:
		p[0] = v;
		p[1] = y0;
		p[2] = u;
		p[3] = y1;
		break;

	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

static void draw_yuv420_macropixel(MappedBuffer& buf, unsigned x, unsigned y,
				   YUV yuv1, YUV yuv2, YUV yuv3, YUV yuv4)
{
	ASSERT((x & 1) == 0);
	ASSERT((y & 1) == 0);

	uint8_t *py1 = (uint8_t*)(buf.map(0) + buf.stride(0) * (y + 0) + x);
	uint8_t *py2 = (uint8_t*)(buf.map(0) + buf.stride(0) * (y + 1) + x);

	uint8_t *puv = (uint8_t*)(buf.map(1) + buf.stride(1) * (y / 2) + x);

	uint8_t y0 = yuv1.y;
	uint8_t y1 = yuv2.y;
	uint8_t y2 = yuv3.y;
	uint8_t y3 = yuv4.y;
	uint8_t u = (yuv1.u + yuv2.u + yuv3.u + yuv4.u) / 4;
	uint8_t v = (yuv1.v + yuv2.v + yuv3.v + yuv4.v) / 4;

	switch (buf.format()) {
	case PixelFormat::NV12:
		py1[0] = y0;
		py1[1] = y1;
		py2[0] = y2;
		py2[1] = y3;
		puv[0] = u;
		puv[1] = v;
		break;

	case PixelFormat::NV21:
		py1[0] = y0;
		py1[1] = y1;
		py2[0] = y2;
		py2[1] = y3;
		puv[0] = v;
		puv[1] = u;
		break;

	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

static RGB get_test_pattern_pixel(MappedBuffer& fb, unsigned x, unsigned y)
{
	const unsigned w = fb.width();
	const unsigned h = fb.height();

	const unsigned mw = 20;

	const unsigned xm1 = mw;
	const unsigned xm2 = w - mw - 1;
	const unsigned ym1 = mw;
	const unsigned ym2 = h - mw - 1;

	// white margin lines
	if (x == xm1 || x == xm2 || y == ym1 || y == ym2)
		return RGB(255, 255, 255);
	// white box outlines to corners
	else if ((x == 0 || x == w - 1) && (y < ym1 || y > ym2))
		return RGB(255, 255, 255);
	// white box outlines to corners
	else if ((y == 0 || y == h - 1) && (x < xm1 || x > xm2))
		return RGB(255, 255, 255);
	// blue bar on the left
	else if (x < xm1 && (y > ym1 && y < ym2))
		return RGB(0, 0, 255);
	// blue bar on the top
	else if (y < ym1 && (x > xm1 && x < xm2))
		return RGB(0, 0, 255);
	// red bar on the right
	else if (x > xm2 && (y > ym1 && y < ym2))
		return RGB(255, 0, 0);
	// red bar on the bottom
	else if (y > ym2 && (x > xm1 && x < xm2))
		return RGB(255, 0, 0);
	// inside the margins
	else if (x > xm1 && x < xm2 && y > ym1 && y < ym2) {
		// diagonal line
		if (x == y || w - x == h - y)
			return RGB(255, 255, 255);
		// diagonal line
		else if (w - x == y || x == h - y)
			return RGB(255, 255, 255);
		else {
			int t = (x - xm1 - 1) * 8 / (xm2 - xm1 - 1);
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
			case 3:
				g = b = c;
				break;
			case 4:
				r = b = c;
				break;
			case 5:
				r = g = c;
				break;
			case 6:
				r = g = b = c;
				break;
			case 7:
				break;
			}

			return RGB(r, g, b);
		}
	} else {
		// black corners
		return RGB(0, 0, 0);
	}
}

static void draw_test_pattern_impl(MappedBuffer& fb)
{
	unsigned x, y;
	unsigned w = fb.width();
	unsigned h = fb.height();

	switch (fb.format()) {
	case PixelFormat::XRGB8888:
	case PixelFormat::XBGR8888:
	case PixelFormat::ARGB8888:
	case PixelFormat::ABGR8888:
	case PixelFormat::RGB565:
		for (y = 0; y < h; y++) {
			for (x = 0; x < w; x++) {
				RGB pixel = get_test_pattern_pixel(fb, x, y);
				draw_rgb_pixel(fb, x, y, pixel);
			}
		}
		break;

	case PixelFormat::UYVY:
	case PixelFormat::YUYV:
	case PixelFormat::YVYU:
	case PixelFormat::VYUY:
		for (y = 0; y < h; y++) {
			for (x = 0; x < w; x += 2) {
				RGB pixel1 = get_test_pattern_pixel(fb, x, y);
				RGB pixel2 = get_test_pattern_pixel(fb, x + 1, y);
				draw_yuv422_macropixel(fb, x, y, pixel1.yuv(), pixel2.yuv());
			}
		}
		break;

	case PixelFormat::NV12:
	case PixelFormat::NV21:
		for (y = 0; y < h; y += 2) {
			for (x = 0; x < w; x += 2) {
				RGB pixel00 = get_test_pattern_pixel(fb, x, y);
				RGB pixel10 = get_test_pattern_pixel(fb, x + 1, y);
				RGB pixel01 = get_test_pattern_pixel(fb, x, y + 1);
				RGB pixel11 = get_test_pattern_pixel(fb, x + 1, y + 1);
				draw_yuv420_macropixel(fb, x, y,
						       pixel00.yuv(), pixel10.yuv(),
						       pixel01.yuv(), pixel11.yuv());
			}
		}
		break;
	default:
		throw std::invalid_argument("unknown pixelformat");
	}
}

void draw_test_pattern(DumbFramebuffer &fb)
{
	MappedDumbBuffer mfb(fb);
	draw_test_pattern(mfb);
}

void draw_test_pattern(MappedBuffer &fb)
{
#ifdef DRAW_PERF_PRINT
	using namespace std::chrono;

	auto t1 = high_resolution_clock::now();
#endif

	draw_test_pattern_impl(fb);

#ifdef DRAW_PERF_PRINT
	auto t2 = high_resolution_clock::now();
	auto time_span = duration_cast<microseconds>(t2 - t1);

	printf("draw took %u us\n", (unsigned)time_span.count());
#endif
}

void draw_rect(MappedBuffer &fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color)
{
	for (unsigned i = x; i < x + w; ++i) {
		for (unsigned j = y; j < y + h; ++j) {
			draw_rgb_pixel(fb, i, j, color);
		}
	}
}

void draw_rect(DumbFramebuffer &fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color)
{
	MappedDumbBuffer mfb(fb);
	draw_rect(mfb, x, y, w, h, color);
}

}
