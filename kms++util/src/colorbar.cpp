#include <cstdint>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

namespace kms
{
static const RGB colors32[] = {
	RGB(255, 255, 255),
	RGB(255, 0, 0),
	RGB(255, 255, 255),
	RGB(0, 255, 0),
	RGB(255, 255, 255),
	RGB(0, 0, 255),
	RGB(255, 255, 255),
	RGB(200, 200, 200),
	RGB(255, 255, 255),
	RGB(100, 100, 100),
	RGB(255, 255, 255),
	RGB(50, 50, 50),
};

static const uint16_t colors16[] = {
	colors32[0].rgb565(),
	colors32[1].rgb565(),
	colors32[2].rgb565(),
	colors32[3].rgb565(),
	colors32[4].rgb565(),
	colors32[5].rgb565(),
	colors32[6].rgb565(),
	colors32[7].rgb565(),
	colors32[8].rgb565(),
	colors32[9].rgb565(),
	colors32[10].rgb565(),
	colors32[11].rgb565(),
};

static void drm_draw_color_bar_rgb888(IFramebuffer& buf, int old_xpos, int xpos, int width)
{
	for (unsigned y = 0; y < buf.height(); ++y) {
		RGB bcol = colors32[y * ARRAY_SIZE(colors32) / buf.height()];
		uint32_t *line = (uint32_t*)(buf.map(0) + buf.stride(0) * y);

		if (old_xpos >= 0) {
			for (int x = old_xpos; x < old_xpos + width; ++x)
				line[x] = 0;
		}

		for (int x = xpos; x < xpos + width; ++x)
			line[x] = bcol.argb8888();
	}
}

static void drm_draw_color_bar_rgb565(IFramebuffer& buf, int old_xpos, int xpos, int width)
{
	static_assert(ARRAY_SIZE(colors32) == ARRAY_SIZE(colors16), "bad colors arrays");

	for (unsigned y = 0; y < buf.height(); ++y) {
		uint16_t bcol = colors16[y * ARRAY_SIZE(colors16) / buf.height()];
		uint16_t *line = (uint16_t*)(buf.map(0) + buf.stride(0) * y);

		if (old_xpos >= 0) {
			for (int x = old_xpos; x < old_xpos + width; ++x)
				line[x] = 0;
		}

		for (int x = xpos; x < xpos + width; ++x)
			line[x] = bcol;
	}
}

static void drm_draw_color_bar_semiplanar_yuv(IFramebuffer& buf, int old_xpos, int xpos, int width)
{
	const uint8_t colors[] = {
		0xff,
		0x00,
		0xff,
		0x20,
		0xff,
		0x40,
		0xff,
		0x80,
		0xff,
	};

	for (unsigned y = 0; y < buf.height(); ++y) {
		unsigned int bcol = colors[y * ARRAY_SIZE(colors) / buf.height()];
		uint8_t *line = (uint8_t*)(buf.map(0) + buf.stride(0) * y);

		if (old_xpos >= 0) {
			for (int x = old_xpos; x < old_xpos + width; ++x)
				line[x] = 0;
		}

		for (int x = xpos; x < xpos + width; ++x)
			line[x] = bcol;
	}
}

void draw_color_bar(IFramebuffer& buf, int old_xpos, int xpos, int width)
{
	switch (buf.format()) {
	case PixelFormat::NV12:
	case PixelFormat::NV21:
		// XXX not right but gets something on the screen
		drm_draw_color_bar_semiplanar_yuv(buf, old_xpos, xpos, width);
		break;

	case PixelFormat::YUYV:
	case PixelFormat::UYVY:
		// XXX not right but gets something on the screen
		drm_draw_color_bar_rgb565(buf, old_xpos, xpos, width);
		break;

	case PixelFormat::RGB565:
		drm_draw_color_bar_rgb565(buf, old_xpos, xpos, width);
		break;

	case PixelFormat::BGR565:
		// XXX not right, red and blue are reversed
		drm_draw_color_bar_rgb565(buf, old_xpos, xpos, width);
		break;

	case PixelFormat::XRGB8888:
		drm_draw_color_bar_rgb888(buf, old_xpos, xpos, width);
		break;

	default:
		ASSERT(false);
	}
}
}
