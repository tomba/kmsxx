// Pattern-feature TU: built only when PIXPAT_FEATURE_PATTERN is on
// (controlled by the meson source list). pixpat.cpp's pixpat_draw_pattern
// entry calls into dispatch_draw_pattern() below via if-constexpr; when
// the feature is off this file isn't compiled, the discarded if-constexpr
// branch emits no symbol reference, and the .so simply lacks these
// symbols.

#include <cassert>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

#include "color.h"
#include "error.h"
#include "params.h"
#include "pattern.h"
#include "pattern_catalog.h"
#include "pipeline.h"
#include "pixpat_internal.h"

namespace pixpat
{

// Generated: s_pattern_* enable flags + DefaultPattern alias. Included
// inside namespace pixpat so the unqualified FormatId / s_format_catalog_count
// references resolve.
#include "pixpat_caps.inc"

// Cold pattern path: fill a per-thread normalized line buffer with
// Pattern samples in the pattern's native color kind, run a cross-
// color-kind pass over the buffer if the sink wants the other kind,
// then hand the buffer to the destination's per-format pack via
// s_format_info. Same shape as run_norm in pixpat_convert.cpp.
template <typename Pattern>
static void run_pattern_norm(const Pattern& pat,
                             FormatId dst_id, const pixpat_buffer* dst,
                             size_t W, size_t H,
                             size_t by_start, size_t by_end,
                             ColorSpec spec)
{
	using P = typename Pattern::Pixel;
	constexpr bool pat_is_rgb = std::is_same_v<P, RGB16>;

	const auto& di = s_format_info[size_t(dst_id)];
	const size_t bh = di.snk_block_h;
	// Entry point (pixpat_draw_pattern) validates W%bw / H%bh.
	assert(W % di.snk_block_w == 0 && H % bh == 0);

	thread_local std::vector<uint8_t> norm;
	norm.resize(bh * W * sizeof(RGB16));   // RGB16 / YUV16 same size

	const ColorCoeffs c = coeffs_for(spec);
	const bool need_xfm = (pat_is_rgb && di.kind == ColorKind::YUV) ||
	                      (!pat_is_rgb && di.kind == ColorKind::RGB);

	for (size_t by = by_start; by < by_end; by += bh) {
		auto* px = reinterpret_cast<P*>(norm.data());
		for (size_t dy = 0; dy < bh; ++dy)
			for (size_t x = 0; x < W; ++x)
				px[dy * W + x] = pat.sample(x, by + dy, W, H);
		if (need_xfm) {
			const size_t n = bh * W;
			if constexpr (pat_is_rgb)
				norm_rgb_to_yuv(norm.data(), n, c);
			else
				norm_yuv_to_rgb(norm.data(), n, c);
		}
		di.pack(dst, norm.data(), by, W);
	}
}

// Construct, ready-check, and run a pattern. Patterns whose colors
// depend on the call's ColorSpec (e.g. native-YUV bar variants) opt
// in by exposing a (Params, ColorSpec) constructor; the rest take
// Params only and stay unchanged.
template <typename Pattern>
static void run_one_pattern(const Params& params,
                            FormatId id, const pixpat_buffer* dst,
                            size_t W, size_t H,
                            size_t by_start, size_t by_end,
                            ColorSpec spec)
{
	auto pat = [&] {
			   if constexpr (std::is_constructible_v<
						 Pattern, const Params&, ColorSpec>)
				   return Pattern(params, spec);
			   else
				   return Pattern(params);
		   }();
	if constexpr (requires { pat.ready(); }) {
		if (!pat.ready())
			throw invalid_argument("pattern parameters not accepted");
	}
	run_pattern_norm(pat, id, dst, W, H, by_start, by_end, spec);
}

// Per-pattern dispatch arm. Templated on the catalog row's RGB and
// YUV variants (either may be `void` if the pattern has no variant
// in that kind). When both are present, the sink kind picks the
// matching variant so the cross-kind pass is a no-op; when only one
// is present, the pipeline runs the cross-kind pass for opposite-
// kind sinks.
//
// Wrapping in a templated helper is what keeps the binary size down:
// `if constexpr (Enabled = false)` discards the run_pattern_norm
// reference, and because try_pattern is itself a template, the
// discarded branch is *not instantiated* — so disabled patterns
// emit no code, and the `void` arms of partial patterns never
// instantiate `Pattern::Pixel` or run_pattern_norm<void>.
template <bool Enabled, typename Rgb, typename Yuv>
static bool try_pattern(std::string_view name, std::string_view want,
                        const Params& params,
                        FormatId id, ColorKind sink_kind,
                        const pixpat_buffer* dst,
                        size_t W, size_t H,
                        size_t by_start, size_t by_end,
                        ColorSpec spec)
{
	if constexpr (Enabled) {
		if (name == want) {
			constexpr bool has_rgb = !std::is_void_v<Rgb>;
			constexpr bool has_yuv = !std::is_void_v<Yuv>;
			static_assert(has_rgb || has_yuv,
			              "pattern needs at least one variant");
			if constexpr (has_rgb && has_yuv) {
				if (sink_kind == ColorKind::YUV)
					run_one_pattern<Yuv>(params, id, dst, W, H,
					                     by_start, by_end, spec);
				else
					run_one_pattern<Rgb>(params, id, dst, W, H,
					                     by_start, by_end, spec);
			} else if constexpr (has_rgb) {
				run_one_pattern<Rgb>(params, id, dst, W, H,
				                     by_start, by_end, spec);
			} else {
				run_one_pattern<Yuv>(params, id, dst, W, H,
				                     by_start, by_end, spec);
			}
			return true;
		}
	}
	return false;
}

void dispatch_draw_pattern(FormatId id, const char* pattern_name,
                           const Params& params,
                           const pixpat_buffer* dst,
                           size_t W, size_t H,
                           size_t by_start, size_t by_end,
                           ColorSpec spec)
{
	using namespace patterns;
	// NULL pattern_name selects the default ("kmstest"); see pixpat.h.
	const std::string_view name = pattern_name ? pattern_name : "kmstest";
	const ColorKind kind = s_format_info[size_t(id)].kind;

#define X(label, rgb, yuv, str)                                                      \
	if (try_pattern<s_pattern_caps[size_t(PatternId::label)].enabled, rgb, yuv>( \
		    name, str, params, id, kind, dst, W, H, by_start, by_end, spec)) \
	return;
	PIXPAT_PATTERN_LIST(X)
#undef X

	throw invalid_argument("unknown or disabled pattern name");
}

} // namespace pixpat
