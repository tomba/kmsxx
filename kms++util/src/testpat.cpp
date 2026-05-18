#include <format>
#include <stdexcept>
#include <string>

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

// Point a pixpat_buffer at a vertical strip of 'width' width, starting at
// column `x` of `fb`. Each plane pointer is offset by the byte distance to
// column `x` in that plane; strides, height, and format are unchanged. `x` must
// be aligned to every plane's hsub and pixels_per_block.
static void fill_pixpat_sub_buffer(pixpat_buffer& buf, IFramebuffer& fb,
				   unsigned x, unsigned width)
{
	const auto& info = get_pixel_format_info(fb.format());

	buf.format = info.name.c_str();
	buf.width = width;
	buf.height = fb.height();
	buf.num_planes = fb.num_planes();

	if (buf.num_planes > 4)
		throw std::runtime_error("Too many planes");

	for (unsigned i = 0; i < buf.num_planes; ++i) {
		const auto& p = info.planes[i];

		if (x % p.hsub != 0)
			throw std::runtime_error("vbar x not aligned to plane hsub");
		const unsigned plane_x = x / p.hsub;
		if (plane_x % p.pixels_per_block != 0)
			throw std::runtime_error("vbar x not aligned to plane pixels_per_block");
		const unsigned x_bytes = (plane_x / p.pixels_per_block) * p.bytes_per_block;

		buf.planes[i] = fb.map(i) + x_bytes;
		buf.strides[i] = fb.stride(i);
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

// Repaint a moving vbar incrementally: erase the column at `old_x` (skipped
// when `old_x` is negative or partially off-screen) with solid black, then
// draw a fresh vbar into the column at `new_x`. Both strips are `width`
// pixels wide and span the full image height. x positions are rounded down
// to the format's horizontal pixel alignment so callers don't have to know
// per-format sub-sampling; `width` must already be a multiple of that
// alignment.
void draw_vbar_pattern(IFramebuffer& fb, int old_x, int new_x, unsigned width,
		       const TestPatternOptions& options)
{
	pixpat_pattern_opts popts{};
	fill_pattern_opts(popts, options);

	const auto& info = get_pixel_format_info(fb.format());
	const unsigned x_align = std::get<0>(info.pixel_align);
	const unsigned fb_w = fb.width();

	if (width % x_align != 0)
		throw std::runtime_error("vbar width not aligned to format");

	auto snap = [x_align](int x) { return x - (x % int(x_align)); };

	if (old_x >= 0) {
		const unsigned x = unsigned(snap(old_x));
		if (x + width <= fb_w) {
			pixpat_buffer buf{};
			fill_pixpat_sub_buffer(buf, fb, x, width);

			popts.params = "color=000000";
			if (pixpat_draw_pattern(&buf, "plain", &popts) != 0)
				throw std::runtime_error("pixpat_draw_pattern failed");
		}
	}

	if (new_x >= 0) {
		const unsigned x = unsigned(snap(new_x));
		if (x + width <= fb_w) {
			pixpat_buffer buf{};
			fill_pixpat_sub_buffer(buf, fb, x, width);

			std::string params = std::format("pos=0,width={}", width);
			popts.params = params.c_str();
			if (pixpat_draw_pattern(&buf, "vbar", &popts) != 0)
				throw std::runtime_error("pixpat_draw_pattern failed");
		}
	}
}

} // namespace kms
