#pragma once

// YUV planar layouts: 3 separate planes (Y, then U/V or V/U), 8-bit
// components.
//   YUV420/YVU420 — h_sub=2, v_sub=2  (a.k.a. I420 / YV12)
//   YUV422/YVU422 — h_sub=2, v_sub=1
//   YUV444/YVU444 — h_sub=1, v_sub=1
//   T430          — multi-pixel-per-word planar 4:4:4.

#include "../layout.h"
#include "../io/planar.h"

namespace pixpat::formats
{

#define PIXPAT_PLANAR(name, ...)                            \
	struct name : Layout<ColorKind::YUV, __VA_ARGS__> { \
		using Source = PlanarSource<name>;          \
		using Sink   = PlanarSink<name>;            \
	}

PIXPAT_PLANAR(YUV420, 2, 2,
              Plane<uint8_t, Comp{ C::Y, 8, 0 }>,
              Plane<uint8_t, Comp{ C::U, 8, 0 }>,
              Plane<uint8_t, Comp{ C::V, 8, 0 }>);

PIXPAT_PLANAR(YVU420, 2, 2,
              Plane<uint8_t, Comp{ C::Y, 8, 0 }>,
              Plane<uint8_t, Comp{ C::V, 8, 0 }>,
              Plane<uint8_t, Comp{ C::U, 8, 0 }>);

PIXPAT_PLANAR(YUV422, 2, 1,
              Plane<uint8_t, Comp{ C::Y, 8, 0 }>,
              Plane<uint8_t, Comp{ C::U, 8, 0 }>,
              Plane<uint8_t, Comp{ C::V, 8, 0 }>);

PIXPAT_PLANAR(YVU422, 2, 1,
              Plane<uint8_t, Comp{ C::Y, 8, 0 }>,
              Plane<uint8_t, Comp{ C::V, 8, 0 }>,
              Plane<uint8_t, Comp{ C::U, 8, 0 }>);

PIXPAT_PLANAR(YUV444, 1, 1,
              Plane<uint8_t, Comp{ C::Y, 8, 0 }>,
              Plane<uint8_t, Comp{ C::U, 8, 0 }>,
              Plane<uint8_t, Comp{ C::V, 8, 0 }>);

PIXPAT_PLANAR(YVU444, 1, 1,
              Plane<uint8_t, Comp{ C::Y, 8, 0 }>,
              Plane<uint8_t, Comp{ C::V, 8, 0 }>,
              Plane<uint8_t, Comp{ C::U, 8, 0 }>);

#undef PIXPAT_PLANAR

// T430: 3-plane multi-pixel-per-word planar 4:4:4. Each plane carries
// 3 × 10-bit samples per uint32_t plus a 2-bit X padding bit-field.
struct T430 : Layout<ColorKind::YUV, 1, 1,
	             Plane<uint32_t,
	                   Comp{ C::Y, 10, 0 },
	                   Comp{ C::Y, 10, 10 },
	                   Comp{ C::Y, 10, 20 },
	                   Comp{ C::X, 2,  30 }>,
	             Plane<uint32_t,
	                   Comp{ C::U, 10, 0 },
	                   Comp{ C::U, 10, 10 },
	                   Comp{ C::U, 10, 20 },
	                   Comp{ C::X, 2,  30 }>,
	             Plane<uint32_t,
	                   Comp{ C::V, 10, 0 },
	                   Comp{ C::V, 10, 10 },
	                   Comp{ C::V, 10, 20 },
	                   Comp{ C::X, 2,  30 }> > {
	using Source = MultiPixelPlanarSource<T430>;
	using Sink   = MultiPixelPlanarSink<T430>;
};

} // namespace pixpat::formats
