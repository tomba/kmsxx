#pragma once

// YUV semiplanar layouts: Y plane + interleaved UV plane.
//   NV12/NV21 — 4:2:0 (h_sub=2, v_sub=2)
//   NV16/NV61 — 4:2:2 (h_sub=2, v_sub=1)
//   P030/P230 — multi-pixel-per-word semiplanar (10-bit Y triplets).

#include "../layout.h"
#include "../io/semiplanar.h"

namespace pixpat::formats
{

struct NV12 : Layout<ColorKind::YUV, 2, 2,
	             Plane<uint8_t,  Comp{ C::Y, 8, 0 }>,
	             Plane<uint16_t, Comp{ C::U, 8, 0 }, Comp{ C::V, 8, 8 }> > {
	using Source = SemiplanarSource<NV12>;
	using Sink   = SemiplanarSink<NV12>;
};

struct NV21 : Layout<ColorKind::YUV, 2, 2,
	             Plane<uint8_t,  Comp{ C::Y, 8, 0 }>,
	             Plane<uint16_t, Comp{ C::V, 8, 0 }, Comp{ C::U, 8, 8 }> > {
	using Source = SemiplanarSource<NV21>;
	using Sink   = SemiplanarSink<NV21>;
};

struct NV16 : Layout<ColorKind::YUV, 2, 1,
	             Plane<uint8_t,  Comp{ C::Y, 8, 0 }>,
	             Plane<uint16_t, Comp{ C::U, 8, 0 }, Comp{ C::V, 8, 8 }> > {
	using Source = SemiplanarSource<NV16>;
	using Sink   = SemiplanarSink<NV16>;
};

struct NV61 : Layout<ColorKind::YUV, 2, 1,
	             Plane<uint8_t,  Comp{ C::Y, 8, 0 }>,
	             Plane<uint16_t, Comp{ C::V, 8, 0 }, Comp{ C::U, 8, 8 }> > {
	using Source = SemiplanarSource<NV61>;
	using Sink   = SemiplanarSink<NV61>;
};

// Multi-pixel-per-word semiplanar (P030: 4:2:0, P230: 4:2:2). Y plane
// holds 3 × 10-bit Y samples per uint32_t (top 2 bits unused). UV plane
// holds 3 × (Cb,Cr) pairs per uint64_t (10 bits each, with 2-bit gaps
// at bits 30-31 and 62-63 — left implicit, no X declared).

struct P030 : Layout<ColorKind::YUV, 2, 2,
	             Plane<uint32_t,
	                   Comp{ C::Y, 10, 0 },
	                   Comp{ C::Y, 10, 10 },
	                   Comp{ C::Y, 10, 20 }>,
	             Plane<uint64_t,
	                   Comp{ C::U, 10, 0 },
	                   Comp{ C::V, 10, 10 },
	                   Comp{ C::U, 10, 20 },
	                   Comp{ C::V, 10, 32 },
	                   Comp{ C::U, 10, 42 },
	                   Comp{ C::V, 10, 52 }> > {
	using Source = MultiPixelSemiplanarSource<P030>;
	using Sink   = MultiPixelSemiplanarSink<P030>;
};

struct P230 : Layout<ColorKind::YUV, 2, 1,
	             Plane<uint32_t,
	                   Comp{ C::Y, 10, 0 },
	                   Comp{ C::Y, 10, 10 },
	                   Comp{ C::Y, 10, 20 }>,
	             Plane<uint64_t,
	                   Comp{ C::U, 10, 0 },
	                   Comp{ C::V, 10, 10 },
	                   Comp{ C::U, 10, 20 },
	                   Comp{ C::V, 10, 32 },
	                   Comp{ C::U, 10, 42 },
	                   Comp{ C::V, 10, 52 }> > {
	using Source = MultiPixelSemiplanarSource<P230>;
	using Sink   = MultiPixelSemiplanarSink<P230>;
};

} // namespace pixpat::formats
