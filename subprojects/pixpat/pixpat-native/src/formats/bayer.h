#pragma once

// Bayer raw layouts. Each pixel carries one of R/G/B selected by
// (x mod 2, y mod 2) and BayerOrder; the pattern is on the
// BayerSource/BayerSink template, not the layout itself. Storage shape
// is single-component (C::Y reused as the storage tag) so the same
// 8/10/12/16-bit shapes apply across all four phase patterns.
//
// Each format is a distinct struct (rather than a type alias of one
// another) so each format type can carry its own pattern-specific
// Source/Sink aliases. The shared bit layout lives in a base struct per
// (depth,packing) combination.
//
// ColorKind is RGB because the normalized pixel passed through ColorXfm
// is RGB16 — the sink picks one of r/g/b at write time, and the
// source nearest-neighbor demosaics into RGB16 at read time.

#include "../layout.h"
#include "../io/bayer.h"

namespace pixpat::formats
{

namespace bayer_detail
{

// Per-(depth,packing) base layouts. Every Bayer format derives from
// one of these and pins its own pattern-specific I/O templates.
using Bayer8   = Layout<ColorKind::RGB, 1, 1,
                        Plane<uint8_t,  Comp { C::Y, 8, 0 }> >;
using Bayer10  = Layout<ColorKind::RGB, 1, 1,
                        Plane<uint16_t, Comp { C::Y, 10, 0 }, Comp { C::X, 6, 10 }> >;
using Bayer12  = Layout<ColorKind::RGB, 1, 1,
                        Plane<uint16_t, Comp { C::Y, 12, 0 }, Comp { C::X, 4, 12 }> >;
using Bayer16  = Layout<ColorKind::RGB, 1, 1,
                        Plane<uint16_t, Comp { C::Y, 16, 0 }> >;
// MIPI CSI-2 packed Bayer (10P: 4 pix in 5 bytes; 12P: 2 pix in 3
// bytes). The Layout doesn't capture the packed bit layout — the
// BayerPackedSink hand-rolls the byte writes. uint8_t plane shape is
// a placeholder so the dispatch plumbing is uniform.
using Bayer10P = Layout<ColorKind::RGB, 1, 1,
                        Plane<uint8_t,  Comp { C::Y, 8, 0 }> >;
using Bayer12P = Layout<ColorKind::RGB, 1, 1,
                        Plane<uint8_t,  Comp { C::Y, 8, 0 }> >;

} // namespace bayer_detail

// Unpacked Bayer (4 patterns × 4 bit depths).
#define PIXPAT_BAYER(name, base, pat)                     \
	struct name : bayer_detail::base {                \
		using Source = BayerSource_ ## pat<name>; \
		using Sink   = BayerSink_ ## pat<name>;   \
	}

PIXPAT_BAYER(SRGGB8,  Bayer8,  RGGB);
PIXPAT_BAYER(SBGGR8,  Bayer8,  BGGR);
PIXPAT_BAYER(SGRBG8,  Bayer8,  GRBG);
PIXPAT_BAYER(SGBRG8,  Bayer8,  GBRG);

PIXPAT_BAYER(SRGGB10, Bayer10, RGGB);
PIXPAT_BAYER(SBGGR10, Bayer10, BGGR);
PIXPAT_BAYER(SGRBG10, Bayer10, GRBG);
PIXPAT_BAYER(SGBRG10, Bayer10, GBRG);

PIXPAT_BAYER(SRGGB12, Bayer12, RGGB);
PIXPAT_BAYER(SBGGR12, Bayer12, BGGR);
PIXPAT_BAYER(SGRBG12, Bayer12, GRBG);
PIXPAT_BAYER(SGBRG12, Bayer12, GBRG);

PIXPAT_BAYER(SRGGB16, Bayer16, RGGB);
PIXPAT_BAYER(SBGGR16, Bayer16, BGGR);
PIXPAT_BAYER(SGRBG16, Bayer16, GRBG);
PIXPAT_BAYER(SGBRG16, Bayer16, GBRG);

#undef PIXPAT_BAYER

// MIPI-packed Bayer: pattern + bit depth both encoded in the I/O
// template name (BayerPackedSource_RGGB10, ...).
#define PIXPAT_BAYER_PACKED(name, base, pat_depth)                    \
	struct name : bayer_detail::base {                            \
		using Source = BayerPackedSource_ ## pat_depth<name>; \
		using Sink   = BayerPackedSink_ ## pat_depth<name>;   \
	}

PIXPAT_BAYER_PACKED(SRGGB10P, Bayer10P, RGGB10);
PIXPAT_BAYER_PACKED(SBGGR10P, Bayer10P, BGGR10);
PIXPAT_BAYER_PACKED(SGRBG10P, Bayer10P, GRBG10);
PIXPAT_BAYER_PACKED(SGBRG10P, Bayer10P, GBRG10);

PIXPAT_BAYER_PACKED(SRGGB12P, Bayer12P, RGGB12);
PIXPAT_BAYER_PACKED(SBGGR12P, Bayer12P, BGGR12);
PIXPAT_BAYER_PACKED(SGRBG12P, Bayer12P, GRBG12);
PIXPAT_BAYER_PACKED(SGBRG12P, Bayer12P, GBRG12);

#undef PIXPAT_BAYER_PACKED

} // namespace pixpat::formats
