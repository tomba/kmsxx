#pragma once

// Packed YUV layouts:
//   VUY888        — 1 pixel / 24-bit, 8-bit Y/U/V (storage uint32_t,
//                   bytes_per_pixel = 3; parallels BGR888 in the YUV
//                   register order)
//   XVUY8888      — 1 pixel / 32-bit word, 8-bit Y/U/V + 8-bit padding
//   XVUY2101010   — 1 pixel / 32-bit word, 10-bit Y/U/V + 2-bit padding
//   AVUY16161616  — 1 pixel / 64-bit word, 16-bit Y/U/V/A (normalized)
//   YUYV / YVYU / UYVY / VYUY — 4:2:2, 2 pixels / 32-bit word
//   Y210 / Y212 / Y216        — 4:2:2, 2 pixels / 64-bit word, with
//                   each component MSB-aligned in a 16-bit slot
//
// XVUY/AVUY name is register MSB-first (X/A in the top bits). The
// YUYV names follow V4L2 / pixpat memory-byte order (Y0 in byte 0),
// so shifts ascend in name order — opposite of XRGB-style.

#include "../layout.h"
#include "../io/packed.h"
#include "../io/packed_yuv.h"

namespace pixpat::formats
{

// 1-pixel-per-word packed (single Pixel/Word; uses PackedSource/Sink).

struct VUY888 : Layout<ColorKind::YUV, 1, 1,
	               Plane<uint32_t,
	                     Comp{ C::Y, 8, 0 },
	                     Comp{ C::U, 8, 8 },
	                     Comp{ C::V, 8, 16 }> > {
	using Source = PackedSource<VUY888>;
	using Sink   = PackedSink<VUY888>;
};

struct XVUY8888 : Layout<ColorKind::YUV, 1, 1,
	                 Plane<uint32_t,
	                       Comp{ C::Y, 8, 0 },
	                       Comp{ C::U, 8, 8 },
	                       Comp{ C::V, 8, 16 },
	                       Comp{ C::X, 8, 24 }> > {
	using Source = PackedSource<XVUY8888>;
	using Sink   = PackedSink<XVUY8888>;
};

struct XVUY2101010 : Layout<ColorKind::YUV, 1, 1,
	                    Plane<uint32_t,
	                          Comp{ C::Y, 10, 0 },
	                          Comp{ C::U, 10, 10 },
	                          Comp{ C::V, 10, 20 },
	                          Comp{ C::X, 2,  30 }> > {
	using Source = PackedSource<XVUY2101010>;
	using Sink   = PackedSink<XVUY2101010>;
};

struct AVUY16161616 : Layout<ColorKind::YUV, 1, 1,
	                     Plane<uint64_t,
	                           Comp{ C::Y, 16, 0 },
	                           Comp{ C::U, 16, 16 },
	                           Comp{ C::V, 16, 32 },
	                           Comp{ C::A, 16, 48 }> > {
	using Source = PackedSource<AVUY16161616>;
	using Sink   = PackedSink<AVUY16161616>;
};

// 2-pixel-per-word 4:2:2 (uses PackedYUVSource/Sink).

#define PIXPAT_PACKED_YUV422(name, ...)                       \
	struct name : Layout<ColorKind::YUV, 2, 1,            \
			     Plane<uint32_t, __VA_ARGS__> > { \
		using Source = PackedYUVSource<name>;         \
		using Sink   = PackedYUVSink<name>;           \
	}

PIXPAT_PACKED_YUV422(YUYV,
                     Comp{ C::Y, 8, 0 }, Comp{ C::U, 8, 8 },
                     Comp{ C::Y, 8, 16 }, Comp{ C::V, 8, 24 });

PIXPAT_PACKED_YUV422(YVYU,
                     Comp{ C::Y, 8, 0 }, Comp{ C::V, 8, 8 },
                     Comp{ C::Y, 8, 16 }, Comp{ C::U, 8, 24 });

PIXPAT_PACKED_YUV422(UYVY,
                     Comp{ C::U, 8, 0 }, Comp{ C::Y, 8, 8 },
                     Comp{ C::V, 8, 16 }, Comp{ C::Y, 8, 24 });

PIXPAT_PACKED_YUV422(VYUY,
                     Comp{ C::V, 8, 0 }, Comp{ C::Y, 8, 8 },
                     Comp{ C::U, 8, 16 }, Comp{ C::Y, 8, 24 });

#undef PIXPAT_PACKED_YUV422

// Y210 / Y212 / Y216: 4:2:2, 2 pixels per 64-bit word, MSB-aligned in
// 16-bit slots. Y210 has 6 unused LSBs per slot, Y212 has 4, Y216 has
// none. The X padding entries pad total_bits to 64 so bytes_per_pixel
// resolves to 8; PackedYUVSink leaves their slots zero via the
// value-array zero-init (see io/packed_yuv.h).
struct Y210 : Layout<ColorKind::YUV, 2, 1,
	             Plane<uint64_t,
	                   Comp{ C::X,  6,  0 },
	                   Comp{ C::Y, 10,  6 },
	                   Comp{ C::X,  6, 16 },
	                   Comp{ C::U, 10, 22 },
	                   Comp{ C::X,  6, 32 },
	                   Comp{ C::Y, 10, 38 },
	                   Comp{ C::X,  6, 48 },
	                   Comp{ C::V, 10, 54 }> > {
	using Source = PackedYUVSource<Y210>;
	using Sink   = PackedYUVSink<Y210>;
};

struct Y212 : Layout<ColorKind::YUV, 2, 1,
	             Plane<uint64_t,
	                   Comp{ C::X,  4,  0 },
	                   Comp{ C::Y, 12,  4 },
	                   Comp{ C::X,  4, 16 },
	                   Comp{ C::U, 12, 20 },
	                   Comp{ C::X,  4, 32 },
	                   Comp{ C::Y, 12, 36 },
	                   Comp{ C::X,  4, 48 },
	                   Comp{ C::V, 12, 52 }> > {
	using Source = PackedYUVSource<Y212>;
	using Sink   = PackedYUVSink<Y212>;
};

struct Y216 : Layout<ColorKind::YUV, 2, 1,
	             Plane<uint64_t,
	                   Comp{ C::Y, 16,  0 },
	                   Comp{ C::U, 16, 16 },
	                   Comp{ C::Y, 16, 32 },
	                   Comp{ C::V, 16, 48 }> > {
	using Source = PackedYUVSource<Y216>;
	using Sink   = PackedYUVSink<Y216>;
};

} // namespace pixpat::formats
