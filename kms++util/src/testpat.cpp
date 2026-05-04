#include <stdexcept>
#include <string>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

#include <pixpat/pixpat.h>

namespace kms
{

void draw_test_pattern(IFramebuffer& fb, const TestPatternOptions& options)
{
	const auto& info = get_pixel_format_info(fb.format());

	pixpat_buffer buf{};
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

	pixpat_pattern_opts popts{};
	switch (options.rec) {
	case RecStandard::BT601: popts.rec = PIXPAT_REC_BT601; break;
	case RecStandard::BT709: popts.rec = PIXPAT_REC_BT709; break;
	case RecStandard::BT2020: popts.rec = PIXPAT_REC_BT2020; break;
	}
	popts.range = options.range == ColorRange::Full ? PIXPAT_RANGE_FULL
							: PIXPAT_RANGE_LIMITED;
	popts.num_threads = 0;

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

} // namespace kms
