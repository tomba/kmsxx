#pragma once

// Catalog of every named pattern the C++ side knows. Mirrors the
// shape of format_catalog.h. The X-macro is a list of
// (Label, RgbType, YuvType, "name") rows:
//
//   X(Label, RgbType, YuvType, "name")
//
// `Label` is the C++ identifier doubling as the PatternId enum value
// and the s_pattern_caps[] index. `RgbType` and `YuvType` resolve to
// classes in `pixpat::patterns::` (defined in pattern.h) that satisfy
// the pattern interface (sample(), Pixel) — one per color kind. Use
// `void` if the pattern has no variant in that kind. At least one
// must be non-void. When both are present, dispatch_draw_pattern
// picks the variant matching the sink's color kind so the cross-kind
// pass is a no-op; when only one is present, the pipeline runs the
// cross-kind pass for the opposite-kind sinks. `name` is the
// lowercase identifier exposed via the C ABI.
//
// Adding a pattern = a row here AND its class(es) in pattern.h. The
// codegen (pixpat-native/codegen/gen_pixpat.py) parses this X-macro
// to learn the pattern set; pixpat_pattern.cpp re-expands it to build
// the dispatch arms and the default-pattern fallback.

#include <cstddef>
#include <cstdint>

namespace pixpat
{

#define PIXPAT_PATTERN_LIST(X)                        \
	X(Kmstest,   Kmstest,   void,      "kmstest") \
	X(Smpte,     void,      Smpte,     "smpte")   \
	X(Plain,     Plain,     void,      "plain")   \
	X(Checker,   Checker,   void,      "checker") \
	X(Hramp,     Hramp,     void,      "hramp")   \
	X(Vramp,     Vramp,     void,      "vramp")   \
	X(HBar,      HBarRGB,   HBarYUV,   "hbar")    \
	X(VBar,      VBarRGB,   VBarYUV,   "vbar")    \
	X(Dramp,     Dramp,     void,      "dramp")   \
	X(Zoneplate, Zoneplate, void,      "zoneplate")

enum class PatternId : uint8_t {
#define X(label, rgb, yuv, name) label,
	PIXPAT_PATTERN_LIST(X)
#undef X
	Unknown,
};

struct PatternEntry {
	const char* name;
	PatternId id;
};

inline constexpr PatternEntry s_pattern_table[] = {
#define X(label, rgb, yuv, name) { name, PatternId::label },
	PIXPAT_PATTERN_LIST(X)
#undef X
};

inline constexpr size_t s_pattern_catalog_count =
	sizeof(s_pattern_table) / sizeof(s_pattern_table[0]);

} // namespace pixpat
