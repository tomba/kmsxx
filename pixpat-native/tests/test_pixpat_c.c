/*
 * Native C smoke test for libpixpat.
 *
 * Sole purpose: prove that <pixpat/pixpat.h> compiles as C and that the
 * library links and runs from a C consumer. Behavioral coverage lives
 * in the Python suite; this file exists to catch C-only regressions in
 * the public header (stray C++-isms, missing extern "C", etc.) that a
 * C++ test cannot detect.
 */

#include <pixpat/pixpat.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CHECK always evaluates its expression so the test still runs in
 * release builds where assert() is compiled out. */
#define CHECK(expr) do {                                                \
		if (!(expr)) {                                          \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n",    \
				__FILE__, __LINE__, #expr);             \
			return 1;                                       \
		}                                                       \
} while (0)

int main(void)
{
	CHECK(pixpat_format_count() > 0);
	CHECK(pixpat_format_supported("XRGB8888") == 1);

	const uint32_t w = 32, h = 16;
	uint8_t *rgb = (uint8_t *)calloc((size_t)w * h * 4, 1);
	uint8_t *wide = (uint8_t *)calloc((size_t)w * h * 8, 1);
	CHECK(rgb && wide);

	pixpat_buffer fb;
	memset(&fb, 0, sizeof(fb));
	fb.format = "XRGB8888";
	fb.width = w;
	fb.height = h;
	fb.num_planes = 1;
	fb.planes[0] = rgb;
	fb.strides[0] = w * 4;

	pixpat_pattern_opts pat;
	memset(&pat, 0, sizeof(pat));
	pat.rec = PIXPAT_REC_BT709;
	pat.range = PIXPAT_RANGE_LIMITED;
	CHECK(pixpat_draw_pattern(&fb, "smpte", &pat) == 0);
	/* Cover the NULL-opts path. */
	CHECK(pixpat_draw_pattern(&fb, NULL, NULL) == 0);

	pixpat_buffer dst;
	memset(&dst, 0, sizeof(dst));
	dst.format = "ABGR16161616";
	dst.width = w;
	dst.height = h;
	dst.num_planes = 1;
	dst.planes[0] = wide;
	dst.strides[0] = w * 8;
	CHECK(pixpat_convert(&dst, &fb, NULL) == 0);

	free(rgb);
	free(wide);
	return 0;
}
