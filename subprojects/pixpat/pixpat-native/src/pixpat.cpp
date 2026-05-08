// pixpat: extern "C" entry points + runtime format dispatch.
//
// The format catalog (X-macro PIXPAT_FORMAT_LIST + FormatId enum +
// s_format_table) is hand-written in format_catalog.h. The generator
// (pixpat-native/codegen/gen_pixpat.py) reads the same X-macro and
// the user TOML and emits the per-config bits:
//
//   pixpat_config.h — PIXPAT_FEATURE_PATTERN / _CONVERT
//   pixpat_caps.inc — s_format_caps[] (per-format readable / writable /
//                     hot_src / hot_dst, indexed by FormatId) and
//                     s_pattern_caps[] (per-pattern enabled flag).
//
// The convert and pattern dispatch (dispatch_convert in
// pixpat_convert.cpp, dispatch_draw_pattern in pixpat_pattern.cpp) is
// hand-written and consumes s_format_caps / s_pattern_caps via
// `if constexpr` on the per-row constexpr fields.
//
// s_format_info is built here, once, by re-expanding the catalog
// X-macro through unpack_for / pack_for / snk_block_h_for /
// snk_block_w_for. Those constexpr helpers use `if constexpr` on the
// per-format readable / writable flags from s_format_caps to either
// take the address of unpack_to_norm / pack_from_norm or fall back to
// nullptr (or 0). Because they're function templates, the discarded
// branch is never instantiated, so disabled-direction templates
// produce no code.
//
// Feature gating is meson-side: pixpat_pattern.cpp / pixpat_convert.cpp
// are added to the source list only when their feature is enabled. This
// file's entry points always exist; they call the bridge functions
// dispatch_draw_pattern / dispatch_convert under `if constexpr
// (kFeatureXxx)`. The discarded if-constexpr branch produces no symbol
// reference, so when the matching TU is absent the link still succeeds
// and the entry point returns -1 instead.

#include <pixpat/pixpat.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "pixpat_config.h"

#include "color.h"
#include "error.h"
#include "format_catalog.h"
#include "formats.h"
#include "io.h"
#include "layout.h"
#include "params.h"
#include "pattern.h"
#include "pixpat_internal.h"
#include "threading.h"

namespace pixpat
{

inline constexpr bool kFeaturePattern = PIXPAT_FEATURE_PATTERN;
inline constexpr bool kFeatureConvert = PIXPAT_FEATURE_CONVERT;

static FormatId lookup_format(const char* name) noexcept
{
	if (!name)
		return FormatId::Unknown;
	for (const auto& e : s_format_table)
		if (std::strcmp(e.name, name) == 0)
			return e.id;
	return FormatId::Unknown;
}

// Per-source: fill `bh` rows of normalized pixels by calling Src::read.
// Address is taken in s_format_info[] for every readable format. When
// no format is readable (convert disabled) no specialization is
// instantiated, so this template emits no code.
template <typename Src>
static void unpack_to_norm(uint8_t* norm, const pixpat_buffer* src,
                           size_t by, size_t bh, size_t W) noexcept
{
	using P = typename Src::Pixel;
	auto sb = make_buffer<typename Src::Layout>(src);
	auto* dst = reinterpret_cast<P*>(norm);
	const size_t H = src->height;
	for (size_t dy = 0; dy < bh; ++dy)
		for (size_t x = 0; x < W; ++x)
			dst[dy * W + x] = Src::read(sb, x, by + dy, W, H);
}

// Per-sink: re-block `Snk::block_h × W` of normalized pixels and call
// Sink::write_block. Snk's block_h dictates how many normalized rows
// the caller has to have prepared. Used by the normalized pivot for
// both convert (cold path) and pattern.
template <typename Snk>
static void pack_from_norm(const pixpat_buffer* dst,
                           const uint8_t* norm,
                           size_t by, size_t W) noexcept
{
	using P = typename Snk::Pixel;
	constexpr size_t bh = Snk::block_h;
	constexpr size_t bw = Snk::block_w;
	auto db = make_buffer<typename Snk::Layout>(dst);
	auto* src = reinterpret_cast<const P*>(norm);
	for (size_t bx = 0; bx < W; bx += bw) {
		P block[bh][bw];
		for (size_t dy = 0; dy < bh; ++dy)
			for (size_t dx = 0; dx < bw; ++dx)
				block[dy][dx] = src[dy * W + bx + dx];
		Snk::write_block(db, bx, by, block);
	}
}

// Generated: s_format_caps[] indexed by FormatId, plus s_pattern_* /
// DefaultPattern (used only by pixpat_pattern.cpp; harmless here).
#include "pixpat_caps.inc"

static_assert(sizeof(s_format_caps) / sizeof(s_format_caps[0]) == s_format_catalog_count,
              "s_format_caps must cover the full catalog");

// `if constexpr` keeps disabled-direction function-template bodies
// uninstantiated. Taking `&unpack_to_norm<Src>` / `&pack_from_norm<Snk>`
// forces the function body to be emitted; without the gate every
// catalog format would carry unpack and pack code regardless of its
// readable / writable bit. Snk::block_h / Snk::block_w are constexpr
// scalars — no body, no emission — so they're inlined directly in the
// initializer below, without a helper.
template <bool Read, typename Src>
static constexpr UnpackFn unpack_for() noexcept
{
	if constexpr (Read)
		return &unpack_to_norm<Src>;
	else
		return nullptr;
}

template <bool Write, typename Snk>
static constexpr PackFn pack_for() noexcept
{
	if constexpr (Write)
		return &pack_from_norm<Snk>;
	else
		return nullptr;
}

const FormatInfo s_format_info[] = {
#define CAPS(name) s_format_caps[size_t(FormatId::name)]
#define X(name)                                                           \
	{                                                                 \
		unpack_for<CAPS(name).readable, formats::name::Source>(), \
		pack_for<CAPS(name).writable, formats::name::Sink>(),     \
		formats::name::kind,                                      \
		uint8_t(formats::name::h_sub),                            \
		uint8_t(formats::name::v_sub),                            \
		uint8_t(formats::name::Sink::block_h),                    \
		uint8_t(formats::name::Sink::block_w),                    \
	},
	PIXPAT_FORMAT_LIST(X)
#undef X
#undef CAPS
};
static_assert(sizeof(s_format_info) / sizeof(s_format_info[0]) == s_format_catalog_count,
              "s_format_info must cover the full catalog");

// validate_* / parse_spec are only reached from inside the entry points'
// `if constexpr (kFeatureXxx)` true branches. With a feature disabled,
// its caller's branch is discarded and the helper becomes unreferenced;
// require_readable is convert-only. [[maybe_unused]] keeps
// -Wunused-function (and clang's -Wunneeded-internal-declaration) quiet.
[[maybe_unused]] static void validate_buffer(const pixpat_buffer* b)
{
	if (!b)
		throw invalid_argument("null buffer");
	if (b->width == 0 || b->height == 0)
		throw invalid_argument("zero-sized buffer");
}

[[maybe_unused]] static FormatId validate_format(const char* name)
{
	auto id = lookup_format(name);
	if (id == FormatId::Unknown)
		throw invalid_argument("unknown format");
	return id;
}

[[maybe_unused]] static void require_writable(FormatId id)
{
	if (s_format_info[size_t(id)].pack == nullptr)
		throw invalid_argument("format not enabled as a sink in this build");
}

[[maybe_unused]] static void require_readable(FormatId id)
{
	if (s_format_info[size_t(id)].unpack == nullptr)
		throw invalid_argument("format not enabled as a source in this build");
}

[[maybe_unused]] static unsigned validate_thread_count(int n)
{
	if (n < 0)
		throw invalid_argument("negative num_threads");
	return n > 0 ? static_cast<unsigned>(n) : default_thread_count();
}

// Map the C-side pixpat_rec / pixpat_range enums (defined in
// pixpat.h with explicit values 0/1/2 for rec, 0/1 for range) onto
// the internal pixpat::Rec / pixpat::Range. Out-of-range values fall
// back to BT.601 / Limited — matching the zero-initialised opts
// struct and kDefaultColorSpec.
[[maybe_unused]] static ColorSpec parse_spec(int rec_in, int range_in) noexcept
{
	Rec rec;
	switch (rec_in) {
	case PIXPAT_REC_BT709:  rec = Rec::BT709;  break;
	case PIXPAT_REC_BT2020: rec = Rec::BT2020; break;
	default:                rec = Rec::BT601;  break;
	}
	Range range = (range_in == PIXPAT_RANGE_FULL) ? Range::Full : Range::Limited;
	return ColorSpec{ rec, range };
}

} // namespace pixpat

// Marks the C entry points as part of the public ABI: restores default
// visibility against the build-wide -fvisibility=hidden, so they are
// exported from libpixpat.so.
#define PIXPAT_API __attribute__((visibility("default")))

extern "C" {

PIXPAT_API int pixpat_draw_pattern(const pixpat_buffer* dst,
                                   const char* pattern,
                                   const pixpat_pattern_opts* opts)
{
	if constexpr (pixpat::kFeaturePattern) {
		try {
			pixpat::validate_buffer(dst);
			auto id = pixpat::validate_format(dst->format);
			pixpat::require_writable(id);
			const auto& di = pixpat::s_format_info[size_t(id)];
			if (dst->width % di.snk_block_w != 0 ||
			    dst->height % di.snk_block_h != 0)
				throw pixpat::invalid_argument(
					      "dimensions not aligned to format block");
			const unsigned n_threads = opts
			        ? pixpat::validate_thread_count(opts->num_threads)
			        : pixpat::default_thread_count();
			const pixpat::ColorSpec spec = opts
			        ? pixpat::parse_spec(opts->rec, opts->range)
			        : pixpat::kDefaultColorSpec;
			const pixpat::Params params(opts ? opts->params : nullptr);
			if (!params.ok())
				throw pixpat::invalid_argument("malformed opts->params");

			pixpat::run_stripes(dst->height, di.snk_block_h, n_threads,
			                    [&](size_t y0, size_t y1) {
					pixpat::dispatch_draw_pattern(
						id, pattern, params, dst,
						dst->width, dst->height, y0, y1, spec);
				});
			return 0;
		} catch (const std::exception&) {
			return -1;
		}
	} else {
		(void)dst;
		(void)pattern;
		(void)opts;
		return -1;
	}
}

PIXPAT_API int pixpat_convert(const pixpat_buffer* dst,
                              const pixpat_buffer* src,
                              const pixpat_convert_opts* opts)
{
	if constexpr (pixpat::kFeatureConvert) {
		try {
			pixpat::validate_buffer(dst);
			pixpat::validate_buffer(src);
			if (src->width != dst->width || src->height != dst->height)
				throw pixpat::invalid_argument("src/dst dimensions differ");

			auto src_id = pixpat::validate_format(src->format);
			auto dst_id = pixpat::validate_format(dst->format);
			pixpat::require_readable(src_id);
			pixpat::require_writable(dst_id);

			const auto& si = pixpat::s_format_info[size_t(src_id)];
			const auto& di = pixpat::s_format_info[size_t(dst_id)];
			// Each constraint must hold independently — checking only
			// max() would miss e.g. h_sub=2 vs snk_block_w=3 with W=3.
			if (src->width % si.h_sub != 0 || src->height % si.v_sub != 0 ||
			    src->width % di.h_sub != 0 || src->height % di.v_sub != 0 ||
			    src->width % di.snk_block_w != 0 || src->height % di.snk_block_h != 0)
				throw pixpat::invalid_argument(
					      "dimensions not aligned to format subsampling");
			// run_stripes only needs the v dimension. Stripes must align
			// to si.v_sub (source reads) and di.snk_block_h (sink block
			// loop); for pixpat's catalog these are powers-of-two and
			// max == LCM.
			const unsigned vs = std::max({ unsigned(si.v_sub),
			                               unsigned(di.v_sub),
			                               unsigned(di.snk_block_h) });
			const unsigned n_threads = opts
			        ? pixpat::validate_thread_count(opts->num_threads)
			        : pixpat::default_thread_count();
			const pixpat::ColorSpec spec = opts
			        ? pixpat::parse_spec(opts->rec, opts->range)
			        : pixpat::kDefaultColorSpec;

			pixpat::run_stripes(src->height, vs, n_threads,
			                    [&](size_t y0, size_t y1) {
					pixpat::dispatch_convert(src_id, dst_id, src, dst,
					                         src->width, src->height,
					                         y0, y1, spec);
				});
			return 0;
		} catch (const std::exception&) {
			return -1;
		}
	} else {
		(void)dst;
		(void)src;
		(void)opts;
		return -1;
	}
}

PIXPAT_API int pixpat_format_supported(const char* format)
{
	auto id = pixpat::lookup_format(format);
	if (id == pixpat::FormatId::Unknown)
		return 0;
	return pixpat::s_format_caps[size_t(id)].enabled() ? 1 : 0;
}

PIXPAT_API size_t pixpat_format_count(void)
{
	size_t n = 0;
	for (const auto& c : pixpat::s_format_caps)
		if (c.enabled())
			++n;
	return n;
}

PIXPAT_API const char* pixpat_format_name(size_t idx)
{
	size_t n = 0;
	for (size_t i = 0; i < pixpat::s_format_catalog_count; ++i) {
		if (!pixpat::s_format_caps[i].enabled())
			continue;
		if (n++ == idx)
			return pixpat::s_format_table[i].name;
	}
	return nullptr;
}

} // extern "C"
