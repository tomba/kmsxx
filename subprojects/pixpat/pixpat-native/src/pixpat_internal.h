#pragma once

// Internal interface shared between the always-built pixpat.cpp and the
// optional pixpat_pattern.cpp / pixpat_convert.cpp TUs. The feature
// gate is meson-side: pixpat_pattern.cpp is in the source list iff
// PIXPAT_FEATURE_PATTERN, and likewise for convert. The bridge
// declarations below are unconditional; pixpat.cpp's entry points call
// them inside `if constexpr (kFeatureXxx)`, and the discarded branch
// emits no symbol reference, so absent definitions don't cause link
// failures.

#include <cstddef>
#include <cstdint>

#include <pixpat/pixpat.h>

#include "color.h"
#include "format_catalog.h"
#include "layout.h"
#include "pattern_catalog.h"

namespace pixpat
{

template <typename Layout>
inline Buffer<Layout::num_planes> make_buffer(const pixpat_buffer* b) noexcept
{
	Buffer<Layout::num_planes> out{};
	for (size_t i = 0; i < Layout::num_planes; ++i) {
		out.data[i] = static_cast<uint8_t*>(b->planes[i]);
		out.stride[i] = b->strides[i];
	}
	return out;
}

using UnpackFn = void (*)(uint8_t*, const pixpat_buffer*, size_t, size_t, size_t);
using PackFn   = void (*)(const pixpat_buffer*, const uint8_t*, size_t, size_t);

struct FormatInfo {
	UnpackFn unpack;
	PackFn pack;
	ColorKind kind;
	uint8_t h_sub;
	uint8_t v_sub;
	uint8_t snk_block_h;
	uint8_t snk_block_w;
};

extern const FormatInfo s_format_info[];

// Per-format build capabilities. Defined once per build by the
// generator into s_format_caps[] (in pixpat_caps.inc); the schema is
// here so that file is pure data.
struct FormatCaps {
	bool readable;
	bool writable;
	bool hot_src;
	bool hot_dst;

	constexpr bool enabled() const noexcept
	{
		return readable || writable;
	}
};

// Per-pattern build capabilities. Generator emits s_pattern_caps[]
// indexed by PatternId, plus a separate s_default_pattern_id singleton
// (the fallback when pattern_name doesn't match any enabled arm).
// Used only when PIXPAT_FEATURE_PATTERN — pixpat_pattern.cpp consumes
// both.
struct PatternCaps {
	bool enabled;
};

class Params;

// Bridge into pixpat_pattern.cpp (defined there iff PIXPAT_FEATURE_PATTERN).
void dispatch_draw_pattern(FormatId id, const char* pattern_name,
                           const Params& params,
                           const pixpat_buffer* dst, size_t W, size_t H,
                           size_t by_start, size_t by_end, ColorSpec spec);

// Bridge into pixpat_convert.cpp (defined there iff PIXPAT_FEATURE_CONVERT).
void dispatch_convert(FormatId src_id, FormatId dst_id,
                      const pixpat_buffer* src, const pixpat_buffer* dst,
                      size_t W, size_t H,
                      size_t by_start, size_t by_end, ColorSpec spec);

} // namespace pixpat
