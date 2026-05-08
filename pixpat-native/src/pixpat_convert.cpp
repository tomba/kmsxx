// Convert-feature TU: built only when PIXPAT_FEATURE_CONVERT is on
// (controlled by the meson source list). pixpat.cpp's pixpat_convert
// entry calls into dispatch_convert() below via if-constexpr; when the
// feature is off this file isn't compiled, the discarded if-constexpr
// branch emits no symbol reference, and the .so simply lacks these
// symbols.

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "color.h"
#include "error.h"
#include "format_catalog.h"
#include "formats.h"
#include "io.h"
#include "layout.h"
#include "pattern.h"
#include "pipeline.h"
#include "pixpat_internal.h"

namespace pixpat
{

template <typename Src, typename Snk>
static void run_convert_impl(const pixpat_buffer* src, const pixpat_buffer* dst,
                             size_t W, size_t H,
                             size_t by_start, size_t by_end,
                             ColorSpec spec)
{
	using SL = typename Src::Layout;
	using DL = typename Snk::Layout;
	// Entry point (pixpat_convert) validates W/H against each layout's
	// h_sub / v_sub, plus the sink's block dims.
	assert(W % SL::h_sub == 0 && W % DL::h_sub == 0);
	assert(H % SL::v_sub == 0 && H % DL::v_sub == 0);

	auto sb = make_buffer<SL>(src);
	auto db = make_buffer<DL>(dst);
	Converter<Src, Snk>::run(sb, db, W, H, by_start, by_end, spec);
}

static void run_norm(FormatId src_id, FormatId dst_id,
                     const pixpat_buffer* src, const pixpat_buffer* dst,
                     size_t W, size_t H,
                     size_t by_start, size_t by_end,
                     ColorSpec spec)
{
	const auto& si = s_format_info[size_t(src_id)];
	const auto& di = s_format_info[size_t(dst_id)];

	const size_t bh = di.snk_block_h;
	// Entry point (pixpat_convert) guarantees W/H alignment to each
	// of si.h_sub / si.v_sub and di.snk_block_w / di.snk_block_h.
	assert(W % si.h_sub == 0 && W % di.snk_block_w == 0);
	assert(H % si.v_sub == 0 && H % bh == 0);

	// Per-thread normalized line buffer. RGB16 and YUV16 are both 8
	// bytes, so one allocation works for both. thread_local gives each
	// worker its own buffer when called from run_stripes.
	thread_local std::vector<uint8_t> norm;
	norm.resize(bh * W * sizeof(RGB16));

	const ColorCoeffs c = coeffs_for(spec);
	for (size_t by = by_start; by < by_end; by += bh) {
		si.unpack(norm.data(), src, by, bh, W);
		if (si.kind != di.kind) {
			const size_t n = bh * W;
			if (si.kind == ColorKind::RGB)
				norm_rgb_to_yuv(norm.data(), n, c);
			else
				norm_yuv_to_rgb(norm.data(), n, c);
		}
		di.pack(dst, norm.data(), by, W);
	}
}

// Generated: FormatCaps + s_format_caps[] (per-format readable/writable
// + hot_src/hot_dst), plus s_pattern_* / DefaultPattern.
#include "pixpat_caps.inc"

// Per-Src dispatch: pick the right Sink for `dst_id` and call
// run_convert_impl. The X-macro emits one case per catalog format;
// `if constexpr (...writable)` discards the body for non-writable
// formats — those cases fall to the trailing throw.
template <typename Src>
static void dispatch_dst_convert(FormatId dst_id,
                                 const pixpat_buffer* src, const pixpat_buffer* dst,
                                 size_t W, size_t H,
                                 size_t by_start, size_t by_end,
                                 ColorSpec spec)
{
	switch (dst_id) {
#define CAPS(name) s_format_caps[size_t(FormatId::name)]
#define X(name)                                                          \
	case FormatId::name:                                             \
		if constexpr (CAPS(name).writable) {                     \
			run_convert_impl<Src, formats::name::Sink>(      \
				src, dst, W, H, by_start, by_end, spec); \
			return;                                          \
		}                                                        \
		break;
	PIXPAT_FORMAT_LIST(X)
#undef X
#undef CAPS
	default:
		break;
	}
	throw invalid_argument("destination format not enabled in this build");
}

// Per-Snk dispatch: mirror of dispatch_dst_convert.
template <typename Snk>
static void dispatch_src_convert(FormatId src_id,
                                 const pixpat_buffer* src, const pixpat_buffer* dst,
                                 size_t W, size_t H,
                                 size_t by_start, size_t by_end,
                                 ColorSpec spec)
{
	switch (src_id) {
#define CAPS(name) s_format_caps[size_t(FormatId::name)]
#define X(name)                                                          \
	case FormatId::name:                                             \
		if constexpr (CAPS(name).readable) {                     \
			run_convert_impl<formats::name::Source, Snk>(    \
				src, dst, W, H, by_start, by_end, spec); \
			return;                                          \
		}                                                        \
		break;
	PIXPAT_FORMAT_LIST(X)
#undef X
#undef CAPS
	default:
		break;
	}
	throw invalid_argument("source format not enabled in this build");
}

// Hot-pivot probes. The wrapper has to be a template so that the
// discarded `if constexpr` branch is not instantiated — otherwise
// dispatch_dst_convert<formats::X::Source> would be instantiated for
// every catalog format, not just hot pivots.
template <bool HotSrc, FormatId Id, typename Source>
static bool try_hot_src(FormatId src_id, FormatId dst_id,
                        const pixpat_buffer* src, const pixpat_buffer* dst,
                        size_t W, size_t H,
                        size_t by_start, size_t by_end,
                        ColorSpec spec)
{
	if constexpr (HotSrc) {
		if (src_id == Id) {
			dispatch_dst_convert<Source>(
				dst_id, src, dst, W, H, by_start, by_end, spec);
			return true;
		}
	}
	return false;
}

template <bool HotDst, FormatId Id, typename Sink>
static bool try_hot_dst(FormatId src_id, FormatId dst_id,
                        const pixpat_buffer* src, const pixpat_buffer* dst,
                        size_t W, size_t H,
                        size_t by_start, size_t by_end,
                        ColorSpec spec)
{
	if constexpr (HotDst) {
		if (dst_id == Id) {
			dispatch_src_convert<Sink>(
				src_id, src, dst, W, H, by_start, by_end, spec);
			return true;
		}
	}
	return false;
}

void dispatch_convert(FormatId src_id, FormatId dst_id,
                      const pixpat_buffer* src, const pixpat_buffer* dst,
                      size_t W, size_t H,
                      size_t by_start, size_t by_end,
                      ColorSpec spec)
{
#define CAPS(name) s_format_caps[size_t(FormatId::name)]
#define X(name)                                                              \
	if (try_hot_src<CAPS(name).hot_src, FormatId::name,                  \
			formats::name::Source>(                              \
		    src_id, dst_id, src, dst, W, H, by_start, by_end, spec)) \
	return;                                                              \
	if (try_hot_dst<CAPS(name).hot_dst, FormatId::name,                  \
			formats::name::Sink>(                                \
		    src_id, dst_id, src, dst, W, H, by_start, by_end, spec)) \
	return;
	PIXPAT_FORMAT_LIST(X)
#undef X
#undef CAPS

	run_norm(src_id, dst_id, src, dst, W, H, by_start, by_end, spec);
}

} // namespace pixpat
