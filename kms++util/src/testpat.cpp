#include <algorithm>
#include <format>
#include <stdexcept>
#include <string>
#include <tuple>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

#include <pixpat/pixpat.h>

namespace kms
{

static void fill_pixpat_buffer(pixpat_buffer& buf, IFramebuffer& fb)
{
	const auto& info = get_pixel_format_info(fb.format());

	buf.format = info.name.c_str();
	buf.width = fb.width();
	buf.height = fb.height();
	buf.num_planes = fb.num_planes();

	if (buf.num_planes > 4)
		throw std::runtime_error("Too many planes");

	for (unsigned i = 0; i < buf.num_planes; ++i) {
		buf.planes[i] = fb.map(i);
		buf.strides[i] = fb.stride(i);
	}
}

/*
 * Describe a width-pixel-wide window of fb starting at pixel column x.
 * Each plane pointer is advanced to the window's first block and the
 * full-buffer strides are kept, so pixpat writes only the window's
 * pixels on every row. x must be block-aligned in every plane; callers
 * snap it to the format's pixel alignment first.
 */
static void fill_pixpat_window(pixpat_buffer& buf, IFramebuffer& fb, unsigned x, unsigned width)
{
	const auto& info = get_pixel_format_info(fb.format());

	fill_pixpat_buffer(buf, fb);
	buf.width = width;

	for (unsigned i = 0; i < buf.num_planes; ++i) {
		const auto& pi = info.planes[i];

		if (x % pi.hsub != 0)
			throw std::invalid_argument(std::format("vbar x={} not aligned to plane {} hsub={}",
								x, i, pi.hsub));

		unsigned plane_x = x / pi.hsub;

		if (plane_x % pi.pixels_per_block != 0)
			throw std::invalid_argument(std::format("vbar x={} not aligned to plane {} pixels_per_block={}",
								x, i, pi.pixels_per_block));

		unsigned x_bytes = plane_x / pi.pixels_per_block * pi.bytes_per_block;

		buf.planes[i] = static_cast<uint8_t*>(buf.planes[i]) + x_bytes;
	}
}

static void fill_pattern_opts(pixpat_pattern_opts& popts, const TestPatternOptions& options)
{
	switch (options.rec) {
	case RecStandard::BT601: popts.rec = PIXPAT_REC_BT601; break;
	case RecStandard::BT709: popts.rec = PIXPAT_REC_BT709; break;
	case RecStandard::BT2020: popts.rec = PIXPAT_REC_BT2020; break;
	}
	popts.range = options.range == ColorRange::Full ? PIXPAT_RANGE_FULL
							: PIXPAT_RANGE_LIMITED;
	popts.num_threads = 0;
}

void draw_test_pattern(IFramebuffer& fb, const TestPatternOptions& options)
{
	pixpat_buffer buf{};
	fill_pixpat_buffer(buf, fb);

	pixpat_pattern_opts popts{};
	fill_pattern_opts(popts, options);

	const char* pattern = options.pattern.empty() ? nullptr : options.pattern.c_str();
	std::string params;

	if (pattern) {
		struct {
			const char* alias;
			const char* color;
		} static const solid_aliases[] = {
			{ "red",   "ff0000" },
			{ "green", "00ff00" },
			{ "blue",  "0000ff" },
			{ "white", "ffffff" },
			{ "black", "000000" },
		};

		for (const auto& a : solid_aliases) {
			if (options.pattern == a.alias) {
				pattern = "plain";
				params = std::string("color=") + a.color;
				popts.params = params.c_str();
				break;
			}
		}
	}

	if (pixpat_draw_pattern(&buf, pattern, &popts) != 0)
		throw std::runtime_error("pixpat_draw_pattern failed");
}

void draw_vbar_pattern(IFramebuffer& fb, unsigned x, unsigned width,
		       const TestPatternOptions& options)
{
	pixpat_buffer buf{};
	fill_pixpat_buffer(buf, fb);

	pixpat_pattern_opts popts{};
	fill_pattern_opts(popts, options);

	std::string params = std::format("pos={},width={}", x, width);
	popts.params = params.c_str();

	if (pixpat_draw_pattern(&buf, "vbar", &popts) != 0)
		throw std::runtime_error("pixpat_draw_pattern failed");
}

/*
 * Horizontal alignment a window's x offset needs so that it starts on a
 * whole block in every plane. Derived from the plane geometry rather than
 * trusting pixel_align alone, as the two disagree for some table entries
 * (e.g. YUV420 has pixel_align {1,1} but 2x subsampled chroma planes).
 */
static unsigned window_x_align(const PixelFormatInfo& info)
{
	unsigned align = std::get<0>(info.pixel_align);

	for (const auto& pi : info.planes)
		align = std::max<unsigned>(align, pi.hsub * pi.pixels_per_block);

	return align;
}

void draw_moving_vbar_pattern(IFramebuffer& fb, int old_x, int new_x, unsigned width,
			      const TestPatternOptions& options)
{
	const auto& info = get_pixel_format_info(fb.format());
	const unsigned x_align = window_x_align(info);

	if (width % x_align != 0)
		throw std::invalid_argument(std::format("vbar width={} not aligned to format x_align={}",
							width, x_align));

	auto snap = [x_align](int x) { return unsigned(x) - unsigned(x) % x_align; };

	pixpat_pattern_opts popts{};
	fill_pattern_opts(popts, options);
	// The windows are only `width` pixels wide; spawning worker threads
	// would cost more than the drawing itself.
	popts.num_threads = 1;

	if (old_x >= 0) {
		unsigned x = snap(old_x);

		if (x + width <= fb.width()) {
			pixpat_buffer buf{};
			fill_pixpat_window(buf, fb, x, width);

			popts.params = "color=000000";

			if (pixpat_draw_pattern(&buf, "plain", &popts) != 0)
				throw std::runtime_error("pixpat_draw_pattern failed");
		}
	}

	if (new_x >= 0) {
		unsigned x = snap(new_x);

		if (x + width <= fb.width()) {
			pixpat_buffer buf{};
			fill_pixpat_window(buf, fb, x, width);

			std::string params = std::format("pos=0,width={}", width);
			popts.params = params.c_str();

			if (pixpat_draw_pattern(&buf, "vbar", &popts) != 0)
				throw std::runtime_error("pixpat_draw_pattern failed");
		}
	}
}

} // namespace kms
