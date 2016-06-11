
#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

using namespace std;

namespace kms
{
void draw_rgb_pixel(IMappedFramebuffer& buf, unsigned x, unsigned y, RGB color)
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

void draw_yuv422_macropixel(IMappedFramebuffer& buf, unsigned x, unsigned y, YUV yuv1, YUV yuv2)
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

void draw_yuv420_macropixel(IMappedFramebuffer& buf, unsigned x, unsigned y,
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

void draw_rect(IMappedFramebuffer &fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color)
{
	for (unsigned i = x; i < x + w; ++i) {
		for (unsigned j = y; j < y + h; ++j) {
			draw_rgb_pixel(fb, i, j, color);
		}
	}
}

static void draw_char(IMappedFramebuffer& buf, uint32_t xpos, uint32_t ypos, char c, RGB color)
{
#include "font_8x8.h"

	for (uint32_t y = 0; y < 8; y++) {
		uint8_t bits = fontdata_8x8[8 * c + y];

		for (uint32_t x = 0; x < 8; x++) {
			bool bit = (bits >> (7 - x)) & 1;

			draw_rgb_pixel(buf, xpos + x, ypos + y, bit ? color : RGB());
		}
	}
}

void draw_text(IMappedFramebuffer& buf, uint32_t x, uint32_t y, const string& str, RGB color)
{
	for(unsigned i = 0; i < str.size(); i++)
		draw_char(buf, (x + 8 * i), y, str[i], color);
}

}
