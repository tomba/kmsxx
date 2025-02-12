
#include <cstring>
#include <fmt/format.h>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#ifdef HAS_PTHREAD
#include <thread>
#endif

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

#include "conv.h"

using namespace std;

namespace kms
{
static RGB16 get_test_pattern_pixel_16(size_t w, size_t h, size_t x, size_t y)
{
	const unsigned mw = 20;

	const unsigned xm1 = mw;
	const unsigned xm2 = w - mw - 1;
	const unsigned ym1 = mw;
	const unsigned ym2 = h - mw - 1;

	// white margin lines
	if (x == xm1 || x == xm2 || y == ym1 || y == ym2)
		return RGB16::from_8(255, 255, 255);
	// white box in top left corner
	else if (x < xm1 && y < ym1)
		return RGB16::from_8(255, 255, 255);
	// white box outlines to corners
	else if ((x == 0 || x == w - 1) && (y < ym1 || y > ym2))
		return RGB16::from_8(255, 255, 255);
	// white box outlines to corners
	else if ((y == 0 || y == h - 1) && (x < xm1 || x > xm2))
		return RGB16::from_8(255, 255, 255);
	// blue bar on the left
	else if (x < xm1 && (y > ym1 && y < ym2))
		return RGB16::from_8(0, 0, 255);
	// blue bar on the top
	else if (y < ym1 && (x > xm1 && x < xm2))
		return RGB16::from_8(0, 0, 255);
	// red bar on the right
	else if (x > xm2 && (y > ym1 && y < ym2))
		return RGB16::from_8(255, 0, 0);
	// red bar on the bottom
	else if (y > ym2 && (x > xm1 && x < xm2))
		return RGB16::from_8(255, 0, 0);
	// inside the margins
	else if (x > xm1 && x < xm2 && y > ym1 && y < ym2) {
		// diagonal line
		if (x == y || w - x == h - y)
			return RGB16::from_8(255, 255, 255);
		// diagonal line
		else if (w - x - 1 == y || x == h - y - 1)
			return RGB16::from_8(255, 255, 255);
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

			return RGB16::from_8(r, g, b);
		}
	} else {
		// black corners
		return RGB16::from_8(0, 0, 0);
	}
}

static void get_test_pattern_line(size_t w, size_t h, size_t row, std::span<RGB16> buf)
{
	for (size_t x = 0; x < w; ++x)
		buf[x] = get_test_pattern_pixel_16(w, h, x, row);
}

static void get_test_pattern_line_yuv(size_t w, size_t h, size_t row,
				      std::span<YUV16> buf,
				      const TestPatternOptions& options)
{
	for (size_t x = 0; x < w; ++x)
		buf[x] = get_test_pattern_pixel_16(w, h, x, row)
				 .to_yuv(options.rec, options.range);
}

// SMPTE RP 219-1:2014
// High-Definition, Standard-Definition Compatible Color Bar Signal
// Limited range YUV
static YUV16 get_smpte_pixel(size_t w, size_t h, size_t x, size_t y)
{
	// Pattern colors (12-bit values, limited range)
	constexpr YUV16 gray40 = YUV16::from_12(1658, 2048, 2048); // 40% Gray
	constexpr YUV16 white75 = YUV16::from_12(2884, 2048, 2048); // 75% White
	constexpr YUV16 yellow75 = YUV16::from_12(2694, 704, 2171); // 75% Yellow
	constexpr YUV16 cyan75 = YUV16::from_12(2325, 2356, 704); // 75% Cyan
	constexpr YUV16 green75 = YUV16::from_12(2136, 1012, 827); // 75% Green
	constexpr YUV16 magenta75 = YUV16::from_12(1004, 3084, 3269); // 75% Magenta
	constexpr YUV16 red75 = YUV16::from_12(815, 1740, 3392); // 75% Red
	constexpr YUV16 blue75 = YUV16::from_12(446, 3392, 1925); // 75% Blue
	constexpr YUV16 cyan100 = YUV16::from_12(3015, 2459, 256); // 100% Cyan
	constexpr YUV16 blue100 = YUV16::from_12(509, 3840, 1884); // 100% Blue
	constexpr YUV16 yellow100 = YUV16::from_12(3507, 256, 2212); // 100% Yellow
	constexpr YUV16 black = YUV16::from_12(256, 2048, 2048); // 0% Black
	constexpr YUV16 white100 = YUV16::from_12(3760, 2048, 2048); // 100% White
	constexpr YUV16 red100 = YUV16::from_12(1001, 1637, 3840); // 100% Red
	constexpr YUV16 gray15 = YUV16::from_12(782, 2048, 2048); // 15% Gray

	// PLUGE steps
	constexpr YUV16 black_m2 = YUV16::from_12(186, 2048, 2048); // -2% Black
	constexpr YUV16 black_p2 = YUV16::from_12(326, 2048, 2048); // +2% Black
	constexpr YUV16 black_p4 = YUV16::from_12(396, 2048, 2048); // +4% Black

	// High precision for x-axis calculations
	constexpr size_t M = 1024;
	x = x * M;
	const size_t a = w * M;
	const size_t c = (a * 3 / 4) / 7;
	const size_t d = a / 8;

	// Pattern heights
	const size_t pattern1_height = (h * 7) / 12;
	const size_t pattern2_height = pattern1_height + (h / 12);
	const size_t pattern3_height = pattern2_height + (h / 12);

	// Pattern 1 (75% color bars)
	if (y < pattern1_height) {
		if (x < d || x >= (a - d))
			return gray40;

		size_t bar_index = (x - d) / c;
		switch (bar_index) {
		case 0:
			return white75;
		case 1:
			return yellow75;
		case 2:
			return cyan75;
		case 3:
			return green75;
		case 4:
			return magenta75;
		case 5:
			return red75;
		default:
			return blue75;
		}
	}

	// Pattern 2 (Color difference reference)
	if (y >= pattern1_height && y < pattern2_height) {
		if (x < d)
			return cyan100;

		if (x >= (a - d))
			return blue100;

		return white75;
	}

	// Pattern 3 (Ramp)
	if (y >= pattern2_height && y < pattern3_height) {
		if (x < d)
			return yellow100;

		if (x >= (a - d))
			return red100;

		const size_t ramp_width = a - 2 * d;
		const size_t ramp_x = x - d;

		uint16_t y = 256 + (3760 - 256) * ramp_x / ramp_width;
		return YUV16::from_12(y, 2048, 2048);
	}

	// Pattern 4 (PLUGE)
	if (y >= pattern3_height) {
		const size_t c0 = d;
		const size_t c1 = c0 + c * 3 / 2;
		const size_t c2 = c1 + 2 * c;
		const size_t c3 = c2 + c * 5 / 6;

		if (x < c0)
			return gray15;

		if (x < c1)
			return black;

		if (x < c2)
			return white100;

		if (x < c3)
			return black;

		if (x >= a - d)
			return gray15;

		if (x >= a - d - c)
			return black;

		const size_t step = (x - c3) / (c / 3);

		switch (step) {
		case 0:
			return black_m2; // -2%
		case 1:
			return black; // 0%
		case 2:
			return black_p2; // +2%
		case 3:
			return black; // 0%
		default:
			return black_p4; // +4%
		}
	}

	return black;
}

static void get_smpte_line_rgb(size_t w, size_t h, size_t row, std::span<RGB16> buf,
			       const TestPatternOptions& options)
{
	for (size_t x = 0; x < w; ++x)
		buf[x] = get_smpte_pixel(w, h, x, row)
				 .to_rgb(RecStandard::BT709, ColorRange::Limited);
}

static void get_smpte_line_yuv(size_t w, size_t h, size_t row, std::span<YUV16> buf,
                               const TestPatternOptions& options)
{
	for (size_t x = 0; x < w; ++x)
		buf[x] = get_smpte_pixel(w, h, x, row);
}

static void get_plain_line_rgb(size_t w, const RGB16& color, std::span<RGB16> buf)
{
	for (size_t x = 0; x < w; ++x)
		buf[x] = color;
}

static void get_plain_line_yuv(size_t w, const YUV16& color, std::span<YUV16> buf)
{
	for (size_t x = 0; x < w; ++x)
		buf[x] = color;
}

static void draw_test_pattern_part(IFramebuffer& fb, size_t start_y, size_t end_y,
				   const TestPatternOptions& options)
{
	std::optional<RGB16> solid;

	if (options.pattern == "red")
		solid = RGB16(0xffff, 0, 0);
	else if (options.pattern == "green")
		solid = RGB16(0, 0xffff, 0);
	else if (options.pattern == "blue")
		solid = RGB16(0, 0, 0xffff);
	else if (options.pattern == "white")
		solid = RGB16(0xffff, 0xffff, 0xffff);
	else if (options.pattern == "black")
		solid = RGB16(0, 0, 0);

	std::function<void(size_t y, std::span<RGB16> span)> generate_line_rgb;
	std::function<void(size_t y, std::span<YUV16> span)> generate_line_yuv;

	if (solid.has_value()) {
		generate_line_rgb = [&fb, rgb = solid.value()](size_t y,
							     std::span<RGB16> span) {
			get_plain_line_rgb(fb.width(), rgb, span);
		};

		generate_line_yuv = [&fb, rgb = solid.value(),
				     &options](size_t y, std::span<YUV16> span) {
			get_plain_line_yuv(fb.width(),
					   rgb.to_yuv(options.rec, options.range), span);
		};
	} else if (options.pattern == "smpte") {
		generate_line_rgb = [&fb, &options](size_t y, std::span<RGB16> span) {
			get_smpte_line_rgb(fb.width(), fb.height(), y, span, options);
		};

		generate_line_yuv = [&fb, &options](size_t y, std::span<YUV16> span) {
			get_smpte_line_yuv(fb.width(), fb.height(), y, span, options);
		};
	} else {
		generate_line_rgb = [&fb](size_t y, std::span<RGB16> span) {
			get_test_pattern_line(fb.width(), fb.height(), y, span);
		};

		generate_line_yuv = [&fb, &options](size_t y, std::span<YUV16> span) {
			get_test_pattern_line_yuv(fb.width(), fb.height(), y, span,
						   options);
		};
	}

#define CASE_ARGB(x)                                                                     \
	case PixelFormat::x:                                                             \
		ARGB_Writer<x##_Layout>::write_pattern(fb, start_y, end_y,               \
						       generate_line_rgb);               \
		break;

#define CASE_YUV(x)                                                                      \
	case PixelFormat::x:                                                             \
		YUV_Writer<x##_Layout>::write_pattern(fb, start_y, end_y,                \
						      generate_line_yuv);                \
		break;

#define CASE_YUV_PACKED(x)                                                               \
	case PixelFormat::x:                                                             \
		YUVPackedWriter<x##_Layout>::write_pattern(fb, start_y, end_y,           \
							   generate_line_yuv);           \
		break;

#define CASE_YUV_SEMI(x)                                                                 \
	case PixelFormat::x:                                                             \
		YUVSemiPlanarWriter<x##_Layout>::write_pattern(fb, start_y, end_y,       \
							       generate_line_yuv);       \
		break;

#define CASE_YUV_PLANAR(x)                                                               \
	case PixelFormat::x:                                                             \
		YUVPlanarWriter<x##_Layout>::write_pattern(fb, start_y, end_y,           \
							   generate_line_yuv);           \
		break;

	switch (fb.format()) {
		CASE_YUV_SEMI(XV20);
		CASE_YUV_SEMI(XV15);
		CASE_YUV_SEMI(NV12);
		CASE_YUV_SEMI(NV21);
		CASE_YUV_SEMI(NV16);
		CASE_YUV_SEMI(NV61);

		CASE_ARGB(RGB565);
		CASE_ARGB(BGR565);

		CASE_ARGB(XRGB1555);
		CASE_ARGB(ARGB1555);
		CASE_ARGB(XRGB4444);
		CASE_ARGB(ARGB4444);

		CASE_ARGB(RGB888);
		CASE_ARGB(BGR888);

		CASE_ARGB(XRGB8888);
		CASE_ARGB(ARGB8888);
		CASE_ARGB(XBGR8888);
		CASE_ARGB(ABGR8888);
		CASE_ARGB(RGBX8888);
		CASE_ARGB(RGBA8888);
		CASE_ARGB(BGRX8888);
		CASE_ARGB(BGRA8888);
		CASE_ARGB(XRGB2101010);
		CASE_ARGB(ARGB2101010);
		CASE_ARGB(XBGR2101010);
		CASE_ARGB(ABGR2101010);
		CASE_ARGB(RGBX1010102);
		CASE_ARGB(RGBA1010102);
		CASE_ARGB(BGRX1010102);
		CASE_ARGB(BGRA1010102);

		CASE_YUV_PACKED(YUYV);
		CASE_YUV_PACKED(YVYU);
		CASE_YUV_PACKED(UYVY);
		CASE_YUV_PACKED(VYUY);

		CASE_YUV(XVUY2101010);

		CASE_YUV_PLANAR(YUV444);
		CASE_YUV_PLANAR(YVU444);
		CASE_YUV_PLANAR(YUV422);
		CASE_YUV_PLANAR(YVU422);
		CASE_YUV_PLANAR(YUV420);
		CASE_YUV_PLANAR(YVU420);

	default:
		break;
	}
}

void draw_test_pattern_multi(IFramebuffer& fb, const TestPatternOptions& options)
{
	auto& info = get_pixel_format_info(fb.format());
	uint8_t v_sub = 0;
	for (size_t p = 0; p < info.num_planes; ++p)
		v_sub = max(v_sub, info.planes[p].vsub);

	if (fb.height() % v_sub)
		throw invalid_argument("FB height must be divisible with vsub");

	// Create the mmaps before starting the threads
	for (size_t i = 0; i < fb.num_planes(); ++i)
		fb.map(i);

	size_t num_threads = thread::hardware_concurrency();

	size_t part_height = fb.height() / num_threads;

	// round up to v_sub
	part_height = (part_height + v_sub - 1) / v_sub * v_sub;

	vector<thread> workers;

	std::vector<std::exception_ptr> errors(num_threads);

	for (size_t n = 0; n < num_threads; ++n) {
		size_t start = n * part_height;
		size_t end = start + part_height - 1;

		if (n == num_threads - 1)
			end = fb.height() - 1;

		workers.push_back(thread([&fb, start, end, &options, &error = errors[n]]() {
			try {
				draw_test_pattern_part(fb, start, end, options);
			} catch (...) {
				error = std::current_exception();
			}
		}));
	}

	for (thread& t : workers)
		t.join();

	auto i = std::find_if(errors.begin(), errors.end(),
			      [](auto& e) { return e != nullptr; });
	if (i != errors.end())
		std::rethrow_exception(*i);
}

void draw_test_pattern_single(IFramebuffer& fb, const TestPatternOptions& options)
{
	draw_test_pattern_part(fb, 0, fb.height() - 1, options);
}

void draw_test_pattern(IFramebuffer& fb, const TestPatternOptions& options)
{
#ifdef HAS_PTHREAD
	draw_test_pattern_multi(fb, options);
#else
	draw_test_pattern_single(fb, options);
#endif
}

} // namespace kms
