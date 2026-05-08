/*
 * Native C++ smoke test for libpixpat.
 *
 * Scope is narrow: prove that the public header, the shared library,
 * and the C ABI surface are wired up correctly when consumed from C++.
 * Behavioral coverage (formats, patterns, error paths, threading,
 * conversion matrix) lives in the Python test suite, which exercises
 * the same C ABI with much less boilerplate.
 */

#include <pixpat/pixpat.h>

#include <cstdint>
#include <cstdio>
#include <vector>

// CHECK always evaluates its expression so the test still runs in
// release builds where assert() is compiled out.
#define CHECK(expr) do {                                                  \
		if (!(expr)) {                                            \
			std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", \
				     __FILE__, __LINE__, #expr);          \
			return 1;                                         \
		}                                                         \
} while (0)

int main()
{
	// Format introspection.
	CHECK(pixpat_format_count() > 0);
	CHECK(pixpat_format_name(0) != nullptr);
	CHECK(pixpat_format_supported("XRGB8888") == 1);
	CHECK(pixpat_format_supported("NOT_A_REAL_FORMAT") == 0);

	// Draw a pattern into an RGB buffer.
	const uint32_t w = 64, h = 32;
	std::vector<uint8_t> rgb(w * h * 4, 0);
	pixpat_buffer fb{};
	fb.format = "XRGB8888";
	fb.width = w;
	fb.height = h;
	fb.num_planes = 1;
	fb.planes[0] = rgb.data();
	fb.strides[0] = w * 4;

	pixpat_pattern_opts pat{};
	pat.rec = PIXPAT_REC_BT709;
	pat.range = PIXPAT_RANGE_LIMITED;
	CHECK(pixpat_draw_pattern(&fb, "smpte", &pat) == 0);
	// Cover the NULL-opts path.
	CHECK(pixpat_draw_pattern(&fb, nullptr, nullptr) == 0);

	// Plain pattern: hex color via opts->params. BGR888-style byte
	// order means the first pixel of XRGB8888 stores B at byte 0,
	// G at byte 1, R at byte 2.
	pixpat_pattern_opts plain_opts{};
	plain_opts.params = "color=ff0000";
	CHECK(pixpat_draw_pattern(&fb, "plain", &plain_opts) == 0);
	CHECK(rgb[0] == 0x00 && rgb[1] == 0x00 && rgb[2] == 0xFF);
	// Missing/malformed params for `plain` must fail.
	plain_opts.params = nullptr;
	CHECK(pixpat_draw_pattern(&fb, "plain", &plain_opts) == -1);
	plain_opts.params = "color=zzzzzz";
	CHECK(pixpat_draw_pattern(&fb, "plain", &plain_opts) == -1);
	// Top-level params parse failure also fails the call.
	plain_opts.params = "foo,bar";
	CHECK(pixpat_draw_pattern(&fb, "plain", &plain_opts) == -1);

	// Cover the rest of the pattern catalog. None of these accept
	// failure at the C-API entry point with valid inputs.
	pixpat_pattern_opts po{};
	CHECK(pixpat_draw_pattern(&fb, "checker", nullptr) == 0);     // default cell
	po.params = "cell=1";
	CHECK(pixpat_draw_pattern(&fb, "checker", &po) == 0);
	po.params = "cell=0";
	CHECK(pixpat_draw_pattern(&fb, "checker", &po) == -1);        // non-positive
	po.params = "cell=oops";
	CHECK(pixpat_draw_pattern(&fb, "checker", &po) == -1);        // non-numeric
	CHECK(pixpat_draw_pattern(&fb, "hramp",     nullptr) == 0);
	CHECK(pixpat_draw_pattern(&fb, "vramp",     nullptr) == 0);
	CHECK(pixpat_draw_pattern(&fb, "dramp",     nullptr) == 0);
	CHECK(pixpat_draw_pattern(&fb, "zoneplate", nullptr) == 0);

	// Convert into the normalized wide RGB format.
	std::vector<uint8_t> wide(w * h * 8, 0);
	pixpat_buffer dst{};
	dst.format = "ABGR16161616";
	dst.width = w;
	dst.height = h;
	dst.num_planes = 1;
	dst.planes[0] = wide.data();
	dst.strides[0] = w * 8;
	CHECK(pixpat_convert(&dst, &fb, nullptr) == 0);

	// Error path: unknown format must return -1, not crash.
	uint8_t dummy[4]{};
	pixpat_buffer bad{};
	bad.format = "NOT_A_REAL_FORMAT";
	bad.width = 1;
	bad.height = 1;
	bad.num_planes = 1;
	bad.planes[0] = dummy;
	bad.strides[0] = 4;
	CHECK(pixpat_draw_pattern(&bad, "smpte", &pat) == -1);

	return 0;
}
