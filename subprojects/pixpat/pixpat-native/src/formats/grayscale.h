#pragma once

// Single-component-per-pixel formats. Most are grayscale (Y) modeled as
// a YUV format with synthesized neutral chroma; R8 is the RGB-kind
// counterpart, modeled grey-style with G=B=R on read. Y10/Y12 carry an
// explicit X padding bitfield. XYYY2101010 is multi-pixel-per-word: 3 Y
// samples in 32 bits.

#include "../layout.h"
#include "../io/gray.h"
#include "../io/gray_packed.h"
#include "../io/mono_rgb.h"

namespace pixpat::formats
{

#define PIXPAT_GRAY(name, ...)                                    \
	struct name : Layout<ColorKind::YUV, 1, 1, __VA_ARGS__> { \
		using Source = GraySource<name>;                  \
		using Sink   = GraySink<name>;                    \
	}

PIXPAT_GRAY(Y8,
            Plane<uint8_t,  Comp{ C::Y, 8, 0 }>);

PIXPAT_GRAY(Y10,
            Plane<uint16_t, Comp{ C::Y, 10, 0 }, Comp{ C::X, 6, 10 }>);

PIXPAT_GRAY(Y12,
            Plane<uint16_t, Comp{ C::Y, 12, 0 }, Comp{ C::X, 4, 12 }>);

PIXPAT_GRAY(Y16,
            Plane<uint16_t, Comp{ C::Y, 16, 0 }>);

#undef PIXPAT_GRAY

// R8: single 8-bit R channel. Read synthesizes G=B=R; write encodes R
// and drops G/B/A. Symmetric to Y8 but ColorKind::RGB so cross-pipeline
// conversions go through the RGB->YUV ColorXfm direction.
struct R8 : Layout<ColorKind::RGB, 1, 1,
	           Plane<uint8_t, Comp{ C::R, 8, 0 }> > {
	using Source = MonoRGBSource<R8>;
	using Sink   = MonoRGBSink<R8>;
};

struct XYYY2101010 : Layout<ColorKind::YUV, 1, 1,
	                    Plane<uint32_t,
	                          Comp{ C::Y, 10, 0 },
	                          Comp{ C::Y, 10, 10 },
	                          Comp{ C::Y, 10, 20 },
	                          Comp{ C::X, 2,  30 }> > {
	using Source = MultiPixelGraySource<XYYY2101010>;
	using Sink   = MultiPixelGraySink<XYYY2101010>;
};

// MIPI CSI-2 packed grayscale (Y10P / Y12P). The Layout doesn't capture
// the packed bit layout — GrayPackedSource/Sink delegate to the shared
// CSI-2 helper (io/csi2.h). uint8_t plane shape is a placeholder so
// dispatch plumbing is uniform (mirrors bayer_detail::Bayer10P/12P).
namespace gray_csi2_detail
{
using Gray10P = Layout<ColorKind::YUV, 1, 1,
                       Plane<uint8_t, Comp { C::Y, 8, 0 }> >;
using Gray12P = Layout<ColorKind::YUV, 1, 1,
                       Plane<uint8_t, Comp { C::Y, 8, 0 }> >;
} // namespace gray_csi2_detail

struct Y10P : gray_csi2_detail::Gray10P {
	using Source = GrayPackedSource<Y10P, 10>;
	using Sink   = GrayPackedSink<Y10P, 10>;
};

struct Y12P : gray_csi2_detail::Gray12P {
	using Source = GrayPackedSource<Y12P, 12>;
	using Sink   = GrayPackedSink<Y12P, 12>;
};

} // namespace pixpat::formats
