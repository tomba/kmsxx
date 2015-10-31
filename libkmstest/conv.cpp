#include <drm_fourcc.h>
#include <stdexcept>

#include "kms++.h"
#include "color.h"

namespace kms
{
static RGB read_rgb(DumbFramebuffer& fb, int x, int y)
{
	uint32_t *pc = (uint32_t *)(fb.map(0) + fb.stride(0) * y);

	uint32_t c = pc[x];

	return RGB((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
}

static YUV read_rgb_as_yuv(DumbFramebuffer& fb, int x, int y)
{
	RGB rgb = read_rgb(fb, x, y);
	return YUV(rgb);
}

static void fb_rgb_to_packed_yuv(DumbFramebuffer& dst_fb, DumbFramebuffer& src_fb)
{
	unsigned w = src_fb.width();
	unsigned h = src_fb.height();

	uint8_t *dst = dst_fb.map(0);

	for (unsigned y = 0; y < h; ++y) {
		for (unsigned x = 0; x < w; x += 2) {
			YUV yuv1 = read_rgb_as_yuv(src_fb, x + 0, y);
			YUV yuv2 = read_rgb_as_yuv(src_fb, x + 1, y);

			switch (dst_fb.format()) {
			case PixelFormat::UYVY:
				dst[x * 2 + 0] = (yuv1.u + yuv2.u) / 2;
				dst[x * 2 + 1] = yuv1.y;
				dst[x * 2 + 2] = (yuv1.v + yuv2.v) / 2;
				dst[x * 2 + 3] = yuv2.y;
				break;
			case PixelFormat::YUYV:
				dst[x * 2 + 0] = yuv1.y;
				dst[x * 2 + 1] = (yuv1.u + yuv2.u) / 2;
				dst[x * 2 + 2] = yuv2.y;
				dst[x * 2 + 3] = (yuv1.v + yuv2.v) / 2;
				break;

			default:
				throw std::invalid_argument("fo");
			}
		}

		dst += dst_fb.stride(0);
	}
}

static void fb_rgb_to_semiplanar_yuv(DumbFramebuffer& dst_fb, DumbFramebuffer& src_fb)
{
	unsigned w = src_fb.width();
	unsigned h = src_fb.height();

	uint8_t *dst_y = dst_fb.map(0);
	uint8_t *dst_uv = dst_fb.map(1);

	for (unsigned y = 0; y < h; ++y) {
		for (unsigned x = 0; x < w; ++x) {
			YUV yuv = read_rgb_as_yuv(src_fb, x, y);
			dst_y[x] = yuv.y;
		}

		dst_y += dst_fb.stride(0);
	}

	for (unsigned y = 0; y < h; y += 2) {
		for (unsigned x = 0; x < w; x += 2) {
			YUV yuv00 = read_rgb_as_yuv(src_fb, x + 0, y + 0);
			YUV yuv01 = read_rgb_as_yuv(src_fb, x + 1, y + 0);
			YUV yuv10 = read_rgb_as_yuv(src_fb, x + 0, y + 1);
			YUV yuv11 = read_rgb_as_yuv(src_fb, x + 1, y + 1);

			unsigned u = (yuv00.u + yuv01.u + yuv10.u + yuv11.u) / 4;
			unsigned v = (yuv00.v + yuv01.v + yuv10.v + yuv11.v) / 4;

			dst_uv[x + 0] = u;
			dst_uv[x + 1] = v;
		}

		dst_uv += dst_fb.stride(1);
	}
}

static void fb_rgb_to_rgb565(DumbFramebuffer& dst_fb, DumbFramebuffer& src_fb)
{
	unsigned w = src_fb.width();
	unsigned h = src_fb.height();

	uint8_t *dst = dst_fb.map(0);

	for (unsigned y = 0; y < h; ++y) {
		for (unsigned x = 0; x < w; ++x) {
			RGB rgb = read_rgb(src_fb, x, y);

			unsigned r = rgb.r * 32 / 256;
			unsigned g = rgb.g * 64 / 256;
			unsigned b = rgb.b * 32 / 256;

			((uint16_t *)dst)[x] = (r << 11) | (g << 5) | (b << 0);
		}

		dst += dst_fb.stride(0);
	}
}

void color_convert(DumbFramebuffer& dst, DumbFramebuffer &src)
{
	switch (dst.format()) {
	case PixelFormat::NV12:
	case PixelFormat::NV21:
		fb_rgb_to_semiplanar_yuv(dst, src);
		break;

	case PixelFormat::YUYV:
	case PixelFormat::UYVY:
		fb_rgb_to_packed_yuv(dst, src);
		break;

	case PixelFormat::RGB565:
		fb_rgb_to_rgb565(dst, src);
		break;

	default:
		throw std::invalid_argument("fo");
	}
}
}
