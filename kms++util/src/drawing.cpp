
#include <cmath>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#include <kms++util/endian.h>

using namespace std;

namespace kms
{
void draw_rgb_pixel(IFramebuffer& buf, unsigned x, unsigned y, RGB color)
{
	if (x >= buf.width() || y >= buf.height())
		throw runtime_error("attempt to draw outside the buffer");

	switch (buf.format()) {
	case PixelFormat::XRGB8888:
	case PixelFormat::ARGB8888: {
		uint32_t* p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.argb8888();
		break;
	}
	case PixelFormat::XBGR8888:
	case PixelFormat::ABGR8888: {
		uint32_t* p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.abgr8888();
		break;
	}
	case PixelFormat::RGBX8888:
	case PixelFormat::RGBA8888: {
		uint32_t* p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.rgba8888();
		break;
	}
	case PixelFormat::BGRX8888:
	case PixelFormat::BGRA8888: {
		uint32_t* p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.bgra8888();
		break;
	}
	case PixelFormat::XRGB2101010:
	case PixelFormat::ARGB2101010: {
		uint32_t* p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.argb2101010();
		break;
	}
	case PixelFormat::XBGR2101010:
	case PixelFormat::ABGR2101010: {
		uint32_t* p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.abgr2101010();
		break;
	}
	case PixelFormat::RGBX1010102:
	case PixelFormat::RGBA1010102: {
		uint32_t* p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.rgba1010102();
		break;
	}
	case PixelFormat::BGRX1010102:
	case PixelFormat::BGRA1010102: {
		uint32_t* p = (uint32_t*)(buf.map(0) + buf.stride(0) * y + x * 4);
		*p = color.bgra1010102();
		break;
	}
	case PixelFormat::RGB888: {
		uint8_t* p = buf.map(0) + buf.stride(0) * y + x * 3;
		p[0] = color.b;
		p[1] = color.g;
		p[2] = color.r;
		break;
	}
	case PixelFormat::BGR888: {
		uint8_t* p = buf.map(0) + buf.stride(0) * y + x * 3;
		p[0] = color.r;
		p[1] = color.g;
		p[2] = color.b;
		break;
	}
	case PixelFormat::RGB332: {
		uint8_t* p = (uint8_t*)(buf.map(0) + buf.stride(0) * y + x);
		*p = color.rgb332();
		break;
	}
	case PixelFormat::RGB565: {
		uint16_t* p = (uint16_t*)(buf.map(0) + buf.stride(0) * y + x * 2);
		*p = color.rgb565();
		break;
	}
	case PixelFormat::BGR565: {
		uint16_t* p = (uint16_t*)(buf.map(0) + buf.stride(0) * y + x * 2);
		*p = color.bgr565();
		break;
	}
	case PixelFormat::XRGB4444:
	case PixelFormat::ARGB4444: {
		uint16_t* p = (uint16_t*)(buf.map(0) + buf.stride(0) * y + x * 2);
		*p = color.argb4444();
		break;
	}
	case PixelFormat::XRGB1555:
	case PixelFormat::ARGB1555: {
		uint16_t* p = (uint16_t*)(buf.map(0) + buf.stride(0) * y + x * 2);
		*p = color.argb1555();
		break;
	}
	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

void draw_yuv444_pixel(IFramebuffer& buf, unsigned x, unsigned y, YUV yuv)
{
	if (x >= buf.width() || y >= buf.height())
		throw runtime_error("attempt to draw outside the buffer");

	uint8_t* py = (uint8_t*)(buf.map(0) + buf.stride(0) * y + x);
	uint8_t* pu = (uint8_t*)(buf.map(1) + buf.stride(1) * y + x);
	uint8_t* pv = (uint8_t*)(buf.map(2) + buf.stride(2) * y + x);

	switch (buf.format()) {
	case PixelFormat::YUV444:
		py[0] = yuv.y;
		pu[0] = yuv.u;
		pv[0] = yuv.v;
		break;

	case PixelFormat::YVU444:
		py[0] = yuv.y;
		pu[0] = yuv.v;
		pv[0] = yuv.u;
		break;

	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

static void draw_yuv422_packed_macropixel(IFramebuffer& buf, unsigned x, unsigned y,
					  YUV yuv1, YUV yuv2)
{
	uint8_t* p = (uint8_t*)(buf.map(0) + buf.stride(0) * y + x * 2);

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

static void draw_y2xx_packed_macropixel(IFramebuffer& buf, unsigned x, unsigned y,
					YUV yuv1, YUV yuv2)
{
	const uint32_t macro_size = 4;
	uint16_t* p = (uint16_t*)(buf.map(0) + buf.stride(0) * y + x * macro_size);

	switch (buf.format()) {
	case PixelFormat::Y210: {
		// XXX naive expansion to 10 bits, similar to 10-bit funcs in class RGB
		uint16_t y0 = yuv1.y << 2;
		uint16_t y1 = yuv2.y << 2;
		uint16_t cb = ((yuv1.u << 2) + (yuv2.u << 2)) / 2;
		uint16_t cr = ((yuv1.v << 2) + (yuv2.v << 2)) / 2;

		// The 10 bits occupy the msb, so we shift left by 16-10 = 6
		write16le(&p[0], y0 << 6);
		write16le(&p[1], cb << 6);
		write16le(&p[2], y1 << 6);
		write16le(&p[3], cr << 6);
		break;
	}

	case PixelFormat::Y212: {
		// XXX naive expansion to 12 bits
		uint16_t y0 = yuv1.y << 4;
		uint16_t y1 = yuv2.y << 4;
		uint16_t cb = ((yuv1.u << 4) + (yuv2.u << 4)) / 2;
		uint16_t cr = ((yuv1.v << 4) + (yuv2.v << 4)) / 2;

		// The 10 bits occupy the msb, so we shift left by 16-12 = 4
		write16le(&p[0], y0 << 4);
		write16le(&p[1], cb << 4);
		write16le(&p[2], y1 << 4);
		write16le(&p[3], cr << 4);
		break;
	}

	case PixelFormat::Y216: {
		// XXX naive expansion to 16 bits
		uint16_t y0 = yuv1.y << 8;
		uint16_t y1 = yuv2.y << 8;
		uint16_t cb = ((yuv1.u << 8) + (yuv2.u << 8)) / 2;
		uint16_t cr = ((yuv1.v << 8) + (yuv2.v << 8)) / 2;

		write16le(&p[0], y0);
		write16le(&p[1], cb);
		write16le(&p[2], y1);
		write16le(&p[3], cr);
		break;
	}

	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

static void draw_yuv422_semiplanar_macropixel(IFramebuffer& buf, unsigned x, unsigned y,
					      YUV yuv1, YUV yuv2)
{
	uint8_t* py = (uint8_t*)(buf.map(0) + buf.stride(0) * y + x);
	uint8_t* puv = (uint8_t*)(buf.map(1) + buf.stride(1) * y + x);

	uint8_t y0 = yuv1.y;
	uint8_t y1 = yuv2.y;
	uint8_t u = (yuv1.u + yuv2.u) / 2;
	uint8_t v = (yuv1.v + yuv2.v) / 2;

	switch (buf.format()) {
	case PixelFormat::NV16:
		py[0] = y0;
		py[1] = y1;
		puv[0] = u;
		puv[1] = v;
		break;

	case PixelFormat::NV61:
		py[0] = y0;
		py[1] = y1;
		puv[0] = v;
		puv[1] = u;
		break;

	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

static void draw_yuv422_planar_macropixel(IFramebuffer& buf, unsigned x, unsigned y,
					  YUV yuv1, YUV yuv2)
{
	uint8_t* py = (uint8_t*)(buf.map(0) + buf.stride(0) * y + x);
	uint8_t* pu = (uint8_t*)(buf.map(1) + buf.stride(1) * y + x / 2);
	uint8_t* pv = (uint8_t*)(buf.map(2) + buf.stride(2) * y + x / 2);

	uint8_t y0 = yuv1.y;
	uint8_t y1 = yuv2.y;
	uint8_t u = (yuv1.u + yuv2.u) / 2;
	uint8_t v = (yuv1.v + yuv2.v) / 2;

	switch (buf.format()) {
	case PixelFormat::YUV422:
		py[0] = y0;
		py[1] = y1;
		pu[0] = u;
		pv[0] = v;
		break;

	case PixelFormat::YVU422:
		py[0] = y0;
		py[1] = y1;
		pu[0] = v;
		pv[0] = u;
		break;

	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

void draw_yuv422_macropixel(IFramebuffer& buf, unsigned x, unsigned y, YUV yuv1, YUV yuv2)
{
	if ((x + 1) >= buf.width() || y >= buf.height())
		throw runtime_error("attempt to draw outside the buffer");

	ASSERT((x & 1) == 0);

	switch (buf.format()) {
	case PixelFormat::UYVY:
	case PixelFormat::YUYV:
	case PixelFormat::YVYU:
	case PixelFormat::VYUY:
		draw_yuv422_packed_macropixel(buf, x, y, yuv1, yuv2);
		break;

	case PixelFormat::Y210:
	case PixelFormat::Y212:
	case PixelFormat::Y216:
		draw_y2xx_packed_macropixel(buf, x, y, yuv1, yuv2);
		break;

	case PixelFormat::NV16:
	case PixelFormat::NV61:
		draw_yuv422_semiplanar_macropixel(buf, x, y, yuv1, yuv2);
		break;

	case PixelFormat::YUV422:
	case PixelFormat::YVU422:
		draw_yuv422_planar_macropixel(buf, x, y, yuv1, yuv2);
		break;

	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

static void draw_yuv420_semiplanar_macropixel(IFramebuffer& buf, unsigned x, unsigned y,
					      YUV yuv1, YUV yuv2, YUV yuv3, YUV yuv4)
{
	uint8_t* py1 = (uint8_t*)(buf.map(0) + buf.stride(0) * (y + 0) + x);
	uint8_t* py2 = (uint8_t*)(buf.map(0) + buf.stride(0) * (y + 1) + x);

	uint8_t* puv = (uint8_t*)(buf.map(1) + buf.stride(1) * (y / 2) + x);

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

static void draw_yuv420_planar_macropixel(IFramebuffer& buf, unsigned x, unsigned y,
					  YUV yuv1, YUV yuv2, YUV yuv3, YUV yuv4)
{
	uint8_t* py1 = (uint8_t*)(buf.map(0) + buf.stride(0) * (y + 0) + x);
	uint8_t* py2 = (uint8_t*)(buf.map(0) + buf.stride(0) * (y + 1) + x);

	uint8_t* pu = (uint8_t*)(buf.map(1) + buf.stride(1) * (y / 2) + x / 2);
	uint8_t* pv = (uint8_t*)(buf.map(2) + buf.stride(2) * (y / 2) + x / 2);

	uint8_t y0 = yuv1.y;
	uint8_t y1 = yuv2.y;
	uint8_t y2 = yuv3.y;
	uint8_t y3 = yuv4.y;
	uint8_t u = (yuv1.u + yuv2.u + yuv3.u + yuv4.u) / 4;
	uint8_t v = (yuv1.v + yuv2.v + yuv3.v + yuv4.v) / 4;

	switch (buf.format()) {
	case PixelFormat::YUV420:
		py1[0] = y0;
		py1[1] = y1;
		py2[0] = y2;
		py2[1] = y3;
		pu[0] = u;
		pv[0] = v;
		break;

	case PixelFormat::YVU420:
		py1[0] = y0;
		py1[1] = y1;
		py2[0] = y2;
		py2[1] = y3;
		pu[0] = v;
		pv[0] = u;
		break;

	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

void draw_yuv420_macropixel(IFramebuffer& buf, unsigned x, unsigned y,
			    YUV yuv1, YUV yuv2, YUV yuv3, YUV yuv4)
{
	if ((x + 1) >= buf.width() || (y + 1) >= buf.height())
		throw runtime_error("attempt to draw outside the buffer");

	ASSERT((x & 1) == 0);
	ASSERT((y & 1) == 0);

	switch (buf.format()) {
	case PixelFormat::NV12:
	case PixelFormat::NV21:
		draw_yuv420_semiplanar_macropixel(buf, x, y, yuv1, yuv2, yuv3, yuv4);
		break;

	case PixelFormat::YUV420:
	case PixelFormat::YVU420:
		draw_yuv420_planar_macropixel(buf, x, y, yuv1, yuv2, yuv3, yuv4);
		break;

	default:
		throw std::invalid_argument("invalid pixelformat");
	}
}

void draw_rect(IFramebuffer& fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color)
{
	unsigned i, j;
	YUV yuvcolor = color.yuv();

	switch (fb.format()) {
	case PixelFormat::XRGB8888:
	case PixelFormat::XBGR8888:
	case PixelFormat::ARGB8888:
	case PixelFormat::ABGR8888:
	case PixelFormat::RGB888:
	case PixelFormat::BGR888:
	case PixelFormat::RGB565:
	case PixelFormat::BGR565:
	case PixelFormat::XRGB4444:
	case PixelFormat::XRGB1555:
	case PixelFormat::ARGB4444:
	case PixelFormat::ARGB1555:
	case PixelFormat::RGB332:
		for (j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				draw_rgb_pixel(fb, x + i, y + j, color);
			}
		}
		break;

	case PixelFormat::YUV444:
	case PixelFormat::YVU444:
		for (j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				draw_yuv444_pixel(fb, x + i, y + j, yuvcolor);
			}
		}
		break;

	case PixelFormat::UYVY:
	case PixelFormat::YUYV:
	case PixelFormat::YVYU:
	case PixelFormat::VYUY:
	case PixelFormat::NV16:
	case PixelFormat::NV61:
	case PixelFormat::YUV422:
	case PixelFormat::YVU422:
		for (j = 0; j < h; j++) {
			for (i = 0; i < w; i += 2) {
				draw_yuv422_macropixel(fb, x + i, y + j, yuvcolor, yuvcolor);
			}
		}
		break;

	case PixelFormat::NV12:
	case PixelFormat::NV21:
	case PixelFormat::YUV420:
	case PixelFormat::YVU420:
		for (j = 0; j < h; j += 2) {
			for (i = 0; i < w; i += 2) {
				draw_yuv420_macropixel(fb, x + i, y + j,
						       yuvcolor, yuvcolor, yuvcolor, yuvcolor);
			}
		}
		break;
	default:
		throw std::invalid_argument("draw_rect: unknown pixelformat");
	}
}

void draw_horiz_line(IFramebuffer& fb, uint32_t x1, uint32_t x2, uint32_t y, RGB color)
{
	for (uint32_t x = x1; x <= x2; ++x)
		draw_rgb_pixel(fb, x, y, color);
}

void draw_circle(IFramebuffer& fb, int32_t xCenter, int32_t yCenter, int32_t radius, RGB color)
{
	int32_t r2 = radius * radius;

	for (int y = -radius; y <= radius; y++) {
		int32_t x = (int)(sqrt(r2 - y * y) + 0.5);
		draw_horiz_line(fb, xCenter - x, xCenter + x, yCenter - y, color);
	}
}

static bool get_char_pixel(char c, uint32_t x, uint32_t y)
{
#include "font_8x8.h"

	uint8_t bits = fontdata_8x8[8 * c + y];
	bool bit = (bits >> (7 - x)) & 1;

	return bit;
}

static void draw_char(IFramebuffer& buf, uint32_t xpos, uint32_t ypos, char c, RGB color)
{
	unsigned x, y;
	YUV yuvcolor = color.yuv();

	switch (buf.format()) {
	case PixelFormat::XRGB8888:
	case PixelFormat::XBGR8888:
	case PixelFormat::ARGB8888:
	case PixelFormat::ABGR8888:
	case PixelFormat::RGB888:
	case PixelFormat::BGR888:
	case PixelFormat::RGB565:
	case PixelFormat::BGR565:
	case PixelFormat::XRGB4444:
	case PixelFormat::XRGB1555:
	case PixelFormat::ARGB4444:
	case PixelFormat::ARGB1555:
	case PixelFormat::RGB332:
		for (y = 0; y < 8; y++) {
			for (x = 0; x < 8; x++) {
				bool b = get_char_pixel(c, x, y);

				draw_rgb_pixel(buf, xpos + x, ypos + y, b ? color : RGB());
			}
		}
		break;

	case PixelFormat::YUV444:
	case PixelFormat::YVU444:
		for (y = 0; y < 8; y++) {
			for (x = 0; x < 8; x++) {
				bool b = get_char_pixel(c, x, y);

				draw_yuv444_pixel(buf, xpos + x, ypos + y, b ? yuvcolor : YUV(RGB()));
			}
		}
		break;

	case PixelFormat::UYVY:
	case PixelFormat::YUYV:
	case PixelFormat::YVYU:
	case PixelFormat::VYUY:
	case PixelFormat::NV16:
	case PixelFormat::NV61:
	case PixelFormat::YUV422:
	case PixelFormat::YVU422:
		for (y = 0; y < 8; y++) {
			for (x = 0; x < 8; x += 2) {
				bool b0 = get_char_pixel(c, x, y);
				bool b1 = get_char_pixel(c, x + 1, y);

				draw_yuv422_macropixel(buf, xpos + x, ypos + y,
						       b0 ? yuvcolor : YUV(RGB()), b1 ? yuvcolor : YUV(RGB()));
			}
		}
		break;

	case PixelFormat::NV12:
	case PixelFormat::NV21:
	case PixelFormat::YUV420:
	case PixelFormat::YVU420:
		for (y = 0; y < 8; y += 2) {
			for (x = 0; x < 8; x += 2) {
				bool b00 = get_char_pixel(c, x, y);
				bool b10 = get_char_pixel(c, x + 1, y);
				bool b01 = get_char_pixel(c, x, y + 1);
				bool b11 = get_char_pixel(c, x + 1, y + 1);

				draw_yuv420_macropixel(buf, xpos + x, ypos + y,
						       b00 ? yuvcolor : YUV(RGB()), b10 ? yuvcolor : YUV(RGB()),
						       b01 ? yuvcolor : YUV(RGB()), b11 ? yuvcolor : YUV(RGB()));
			}
		}
		break;
	default:
		throw std::invalid_argument("draw_char: unknown pixelformat");
	}
}

void draw_text(IFramebuffer& buf, uint32_t x, uint32_t y, const string& str, RGB color)
{
	for (unsigned i = 0; i < str.size(); i++)
		draw_char(buf, (x + 8 * i), y, str[i], color);
}

} // namespace kms
