#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * libpixpat — draw test patterns and convert pixel data in a wide range
 * of pixel formats (planar / semi-planar / packed YUV, RGB, raw, ...).
 *
 * The library writes directly into caller-owned pixel buffers. The
 * caller is responsible for allocating each plane with the correct size
 * and stride for the chosen format; pixpat does not own or allocate any
 * pixel memory itself.
 *
 * Format names follow the convention used by kms++ and pixutils
 * (e.g. "XRGB8888", "NV12", "YUYV") — not the DRM/V4L2 four-character
 * codes ("XR24", etc.). Use pixpat_format_count() / pixpat_format_name()
 * to enumerate the names accepted by pixpat_buffer::format, and
 * pixpat_format_supported() to test a single name.
 *
 * All functions are thread-safe in the sense that independent calls on
 * disjoint buffers may run concurrently from different threads. Pixpat
 * does not spawn threads internally.
 */

/* Maximum number of planes any pixpat_buffer may reference. */
#define PIXPAT_MAX_PLANES 4

/*
 * Description of a pixel buffer passed to or from the library.
 *
 *   format     - Pixel-format name (NUL-terminated ASCII), e.g.
 *                "XRGB8888", "NV12", "YUYV". Must be a name accepted by
 *                pixpat_format_supported(). The string is read during
 *                the call and need not outlive it.
 *   width      - Image width in pixels. Some formats constrain this:
 *                chroma-subsampled YUV formats require a multiple of
 *                the horizontal subsampling factor (2 for NV12 /
 *                YUV420 / YUYV), and packed formats add a further
 *                multiple from their pixel-group size (e.g. 6 for
 *                P030, 4 for SBGGR10P).
 *   height     - Image height in pixels. Vertically-subsampled formats
 *                (e.g. NV12, YUV420) require a multiple of the vertical
 *                subsampling factor.
 *   num_planes - Number of planes the format uses. Must be in
 *                [1, PIXPAT_MAX_PLANES] and match `format`.
 *   planes     - Per-plane base pointers. Each plane must have at least
 *                strides[i] * plane_height bytes addressable, where
 *                plane_height is `height` for the main plane and
 *                height / vertical_subsampling for chroma planes (e.g.
 *                height / 2 for the UV plane of NV12). Entries beyond
 *                num_planes are ignored.
 *   strides    - Per-plane row stride in bytes. Strides larger than the
 *                minimum row size are allowed — useful for hardware
 *                that requires aligned rows. Entries beyond num_planes
 *                are ignored.
 */
typedef struct {
	const char* format;
	uint32_t width;
	uint32_t height;
	uint32_t num_planes;
	void* planes[PIXPAT_MAX_PLANES];
	uint32_t strides[PIXPAT_MAX_PLANES];
} pixpat_buffer;

/*
 * YCbCr color encoding standard.
 *
 * Selects the matrix used to convert between RGB and YUV. Has no effect
 * when the operation does not cross the RGB/YUV boundary (e.g. drawing
 * into an RGB or raw format, or converting between two formats of the
 * same color kind).
 *
 *   PIXPAT_REC_BT601   - ITU-R BT.601 (standard definition).
 *   PIXPAT_REC_BT709   - ITU-R BT.709 (HD).
 *   PIXPAT_REC_BT2020  - ITU-R BT.2020 (UHD / HDR, non-constant
 *                        luminance).
 */
typedef enum {
	PIXPAT_REC_BT601 = 0,
	PIXPAT_REC_BT709 = 1,
	PIXPAT_REC_BT2020 = 2,
} pixpat_rec;

/*
 * Quantization range for YUV components.
 *
 *   PIXPAT_RANGE_LIMITED - "TV" / studio range: Y in [16, 235], C in
 *                          [16, 240] (scaled to bit depth). What most
 *                          video pipelines expect.
 *   PIXPAT_RANGE_FULL    - "PC" / full range: every component uses the
 *                          full code range (e.g. [0, 255] for 8-bit).
 */
typedef enum {
	PIXPAT_RANGE_LIMITED = 0,
	PIXPAT_RANGE_FULL = 1,
} pixpat_range;

/*
 * Optional settings for pixpat_draw_pattern().
 *
 *   rec      - YCbCr matrix used when drawing into a YUV format.
 *              Ignored for RGB / raw formats.
 *   range    - Quantization range used when drawing into a YUV format.
 *              Ignored for RGB / raw formats.
 *   num_threads - Worker-thread count. Zero selects a sensible default
 *              (one per online CPU, capped to a sane maximum); one runs
 *              single-threaded with no thread-spawn overhead; N > 1 uses
 *              exactly N workers. Negative values are rejected. Output
 *              is bit-identical regardless of the chosen count.
 *   params   - Pattern-specific parameters as a NUL-terminated ASCII
 *              string of comma-separated "key=value" items, e.g.
 *              "color=ff0000". Whitespace around tokens is trimmed;
 *              keys and values are case-insensitive and may not contain
 *              ',' or '='. NULL or "" selects per-pattern defaults.
 *              Unknown keys are silently ignored; malformed input
 *              (missing '=', empty key, …) makes the call fail with
 *              -1. Per-pattern keys are documented per pattern.
 */
typedef struct {
	pixpat_rec rec;
	pixpat_range range;
	int num_threads;
	const char* params;
} pixpat_pattern_opts;

/*
 * Draw a test pattern into `dst`.
 *
 * `dst` must be non-NULL; `dst->format` must name a supported format,
 * and `dst->width` / `dst->height` / strides must satisfy the format's
 * constraints (see pixpat_buffer).
 *
 * `pattern` selects the pattern to draw (NUL-terminated ASCII). NULL
 * selects the default ("kmstest"); any other unrecognized name returns
 * -1. Recognized values:
 *   "kmstest"  - default test pattern (color gradients with ramps),
 *                originally from kmstest.
 *   "smpte"    - SMPTE RP 219-1 color bars. Pixel values are spec-
 *                defined in BT.709 / Limited; pass rec=BT709,
 *                range=Limited for spec-correct output. Other settings
 *                are accepted and produce visibly-wrong colors when
 *                drawing into RGB sinks (the caller's matrix is applied
 *                to BT.709-encoded values). Callers are trusted.
 *   "plain"    - solid color fill from opts->params. Reads "color=<hex>"
 *                (hex, case-insensitive, optional "0x" prefix). The
 *                number of hex digits selects the layout:
 *                  6  digits: 8-bit  RRGGBB
 *                  8  digits: 8-bit  AARRGGBB           (alpha first)
 *                  12 digits: 16-bit RRRRGGGGBBBB
 *                  16 digits: 16-bit AAAARRRRGGGGBBBB   (alpha first)
 *                8-bit components are byte-replicated to the internal
 *                16-bit normalized form (0xFF -> 0xFFFF). Missing or
 *                malformed `color` returns -1.
 *   "checker"  - black/white checkerboard. Reads optional "cell=<N>"
 *                (positive integer; default 8) for cell size in pixels.
 *                "cell=1" gives the 1-pixel chroma-subsampling stress
 *                test. A non-positive or non-numeric value returns -1.
 *   "hramp"    - four horizontal stripes (R, G, B, gray), each a
 *                0->max ramp along x. Per-channel and luma
 *                quantization in one pattern.
 *   "vramp"    - same as hramp rotated 90°: four vertical columns
 *                (R, G, B, gray), each a 0->max ramp along y.
 *   "hbar"     - horizontal bar (full image width, narrow along y)
 *                over a black background. Reads required "pos=<N>"
 *                (signed integer, top edge in pixels; negative values
 *                clip at the top) and optional "width=<N>" (positive
 *                integer, bar thickness in pixels; default 32). The
 *                bar is split into seven equal-width regions colored
 *                white/red/white/green/white/blue/white. Missing or
 *                non-numeric pos, or non-positive width, returns -1.
 *   "vbar"     - same as hbar rotated 90°: vertical bar (full image
 *                height, narrow along x), with "pos" measured along x
 *                and the bar split into seven equal-height regions
 *                with the same color sequence.
 *   "dramp"    - diagonal RGB ramp (R sweeps with x, G with y,
 *                B with x+y).
 *   "zoneplate"- centered radial cosine pattern; spatial frequency
 *                ramps from DC at the center to Nyquist at the longer
 *                edge. Useful for spotting scaling/aliasing artifacts.
 *
 * `opts` may be NULL: equivalent to passing a zero-initialised
 * pixpat_pattern_opts. That is BT.601 / limited-range color math, the
 * auto thread count (one worker per online CPU, capped), and no
 * pattern parameters.
 *
 * Returns 0 on success, -1 on failure — typically a NULL `dst`, an
 * unknown format name, or zero-sized dimensions. Strides and plane
 * sizes are not validated against the buffers; an undersized plane
 * leads to undefined behavior.
 */
int pixpat_draw_pattern(const pixpat_buffer* dst,
                        const char* pattern,
                        const pixpat_pattern_opts* opts);

/*
 * Options for pixpat_convert().
 *
 *   rec      - YCbCr matrix used when the conversion crosses the
 *              RGB/YUV boundary. Ignored when src and dst share the
 *              same color kind.
 *   range    - Quantization range used when the conversion crosses the
 *              RGB/YUV boundary. Ignored when src and dst share the
 *              same color kind.
 *   num_threads - Worker-thread count. Zero selects a sensible default
 *              (one per online CPU, capped to a sane maximum); one runs
 *              single-threaded with no thread-spawn overhead; N > 1 uses
 *              exactly N workers. Negative values are rejected. Output
 *              is bit-identical regardless of the chosen count.
 */
typedef struct {
	pixpat_rec rec;
	pixpat_range range;
	int num_threads;
} pixpat_convert_opts;

/*
 * Convert pixel data from src into dst. Both buffers must have matching
 * width and height.
 *
 * Cross-color-kind conversions (RGB <-> YUV) use opts->rec/range for
 * the color-space math; same-color-kind conversions ignore both fields.
 *
 * Bayer sources are decoded with a 3x3 bilinear demosaic.
 *
 * `opts` may be NULL: equivalent to passing a zero-initialised
 * pixpat_convert_opts. That is BT.601 / limited-range color math and
 * the auto thread count (one worker per online CPU, capped).
 *
 * Returns 0 on success, -1 on failure.
 */
int pixpat_convert(const pixpat_buffer* dst,
                   const pixpat_buffer* src,
                   const pixpat_convert_opts* opts);

/*
 * Return non-zero if `format` is a known pixel-format name accepted by
 * pixpat_buffer::format, zero otherwise. Passing NULL returns zero.
 */
int pixpat_format_supported(const char* format);

/*
 * Return the number of pixel-format names known to the library. Use
 * with pixpat_format_name() to enumerate the formats.
 */
size_t pixpat_format_count(void);

/*
 * Return the name of the format at index `idx`, or NULL if `idx` is
 * out of range (>= pixpat_format_count()).
 *
 * The returned pointer references storage owned by the library and is
 * valid for the lifetime of the process; the caller must not free it.
 */
const char* pixpat_format_name(size_t idx);

#ifdef __cplusplus
}
#endif
