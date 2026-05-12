#pragma once

// RGB packed layouts: 8-bit / 16-bit (sub-byte) / 32-bit (10-bit) /
// 64-bit-normalized, all single-plane single-pixel-per-storage-word.
// Names follow the kms++/pixutils register-order convention (MSB-first
// in the storage word), so XRGB8888 has X at bits 31..24 and B at 7..0.

#include "../layout.h"
#include "../io/packed.h"

namespace pixpat::formats
{

// Helper: every format in this file pairs with PackedSource/PackedSink.
// Each format struct exposes Source / Sink aliases so the catalog row
// in format_catalog.h can stay name-only.
#define PIXPAT_RGB_PACKED(name, ...)                              \
	struct name : Layout<ColorKind::RGB, 1, 1, __VA_ARGS__> { \
		using Source = PackedSource<name>;                \
		using Sink   = PackedSink<name>;                  \
	}

// ---------------------------------------------------------------------
// 32-bit packed RGB, 8-bit components.
// ---------------------------------------------------------------------

PIXPAT_RGB_PACKED(XRGB8888,
                  Plane<uint32_t,
                        Comp{ C::B, 8, 0 },
                        Comp{ C::G, 8, 8 },
                        Comp{ C::R, 8, 16 },
                        Comp{ C::X, 8, 24 }>);

PIXPAT_RGB_PACKED(ARGB8888,
                  Plane<uint32_t,
                        Comp{ C::B, 8, 0 },
                        Comp{ C::G, 8, 8 },
                        Comp{ C::R, 8, 16 },
                        Comp{ C::A, 8, 24 }>);

PIXPAT_RGB_PACKED(XBGR8888,
                  Plane<uint32_t,
                        Comp{ C::R, 8, 0 },
                        Comp{ C::G, 8, 8 },
                        Comp{ C::B, 8, 16 },
                        Comp{ C::X, 8, 24 }>);

PIXPAT_RGB_PACKED(ABGR8888,
                  Plane<uint32_t,
                        Comp{ C::R, 8, 0 },
                        Comp{ C::G, 8, 8 },
                        Comp{ C::B, 8, 16 },
                        Comp{ C::A, 8, 24 }>);

PIXPAT_RGB_PACKED(RGBX8888,
                  Plane<uint32_t,
                        Comp{ C::X, 8, 0 },
                        Comp{ C::B, 8, 8 },
                        Comp{ C::G, 8, 16 },
                        Comp{ C::R, 8, 24 }>);

PIXPAT_RGB_PACKED(RGBA8888,
                  Plane<uint32_t,
                        Comp{ C::A, 8, 0 },
                        Comp{ C::B, 8, 8 },
                        Comp{ C::G, 8, 16 },
                        Comp{ C::R, 8, 24 }>);

PIXPAT_RGB_PACKED(BGRX8888,
                  Plane<uint32_t,
                        Comp{ C::X, 8, 0 },
                        Comp{ C::R, 8, 8 },
                        Comp{ C::G, 8, 16 },
                        Comp{ C::B, 8, 24 }>);

PIXPAT_RGB_PACKED(BGRA8888,
                  Plane<uint32_t,
                        Comp{ C::A, 8, 0 },
                        Comp{ C::R, 8, 8 },
                        Comp{ C::G, 8, 16 },
                        Comp{ C::B, 8, 24 }>);

// ---------------------------------------------------------------------
// 24-bit packed RGB, three bytes per pixel. storage_t is uint32_t but
// only bytes_per_pixel = 3 are read/written via memcpy.
// ---------------------------------------------------------------------

PIXPAT_RGB_PACKED(RGB888,
                  Plane<uint32_t,
                        Comp{ C::B, 8, 0 },
                        Comp{ C::G, 8, 8 },
                        Comp{ C::R, 8, 16 }>);

PIXPAT_RGB_PACKED(BGR888,
                  Plane<uint32_t,
                        Comp{ C::R, 8, 0 },
                        Comp{ C::G, 8, 8 },
                        Comp{ C::B, 8, 16 }>);

// ---------------------------------------------------------------------
// 16-bit packed RGB, sub-byte components.
// ---------------------------------------------------------------------

PIXPAT_RGB_PACKED(RGB565,
                  Plane<uint16_t,
                        Comp{ C::B, 5, 0 },
                        Comp{ C::G, 6, 5 },
                        Comp{ C::R, 5, 11 }>);

PIXPAT_RGB_PACKED(BGR565,
                  Plane<uint16_t,
                        Comp{ C::R, 5, 0 },
                        Comp{ C::G, 6, 5 },
                        Comp{ C::B, 5, 11 }>);

// 8-bit packed RGB: 3-bit R / 3-bit G / 2-bit B in a single byte.

PIXPAT_RGB_PACKED(RGB332,
                  Plane<uint8_t,
                        Comp{ C::B, 2, 0 },
                        Comp{ C::G, 3, 2 },
                        Comp{ C::R, 3, 5 }>);

PIXPAT_RGB_PACKED(XRGB1555,
                  Plane<uint16_t,
                        Comp{ C::B, 5, 0 },
                        Comp{ C::G, 5, 5 },
                        Comp{ C::R, 5, 10 },
                        Comp{ C::X, 1, 15 }>);

PIXPAT_RGB_PACKED(ARGB1555,
                  Plane<uint16_t,
                        Comp{ C::B, 5, 0 },
                        Comp{ C::G, 5, 5 },
                        Comp{ C::R, 5, 10 },
                        Comp{ C::A, 1, 15 }>);

PIXPAT_RGB_PACKED(XBGR1555,
                  Plane<uint16_t,
                        Comp{ C::R, 5, 0 },
                        Comp{ C::G, 5, 5 },
                        Comp{ C::B, 5, 10 },
                        Comp{ C::X, 1, 15 }>);

PIXPAT_RGB_PACKED(ABGR1555,
                  Plane<uint16_t,
                        Comp{ C::R, 5, 0 },
                        Comp{ C::G, 5, 5 },
                        Comp{ C::B, 5, 10 },
                        Comp{ C::A, 1, 15 }>);

PIXPAT_RGB_PACKED(XRGB4444,
                  Plane<uint16_t,
                        Comp{ C::B, 4, 0 },
                        Comp{ C::G, 4, 4 },
                        Comp{ C::R, 4, 8 },
                        Comp{ C::X, 4, 12 }>);

PIXPAT_RGB_PACKED(ARGB4444,
                  Plane<uint16_t,
                        Comp{ C::B, 4, 0 },
                        Comp{ C::G, 4, 4 },
                        Comp{ C::R, 4, 8 },
                        Comp{ C::A, 4, 12 }>);

PIXPAT_RGB_PACKED(XBGR4444,
                  Plane<uint16_t,
                        Comp{ C::R, 4, 0 },
                        Comp{ C::G, 4, 4 },
                        Comp{ C::B, 4, 8 },
                        Comp{ C::X, 4, 12 }>);

PIXPAT_RGB_PACKED(ABGR4444,
                  Plane<uint16_t,
                        Comp{ C::R, 4, 0 },
                        Comp{ C::G, 4, 4 },
                        Comp{ C::B, 4, 8 },
                        Comp{ C::A, 4, 12 }>);

PIXPAT_RGB_PACKED(RGBX4444,
                  Plane<uint16_t,
                        Comp{ C::X, 4, 0 },
                        Comp{ C::B, 4, 4 },
                        Comp{ C::G, 4, 8 },
                        Comp{ C::R, 4, 12 }>);

PIXPAT_RGB_PACKED(RGBA4444,
                  Plane<uint16_t,
                        Comp{ C::A, 4, 0 },
                        Comp{ C::B, 4, 4 },
                        Comp{ C::G, 4, 8 },
                        Comp{ C::R, 4, 12 }>);

// ---------------------------------------------------------------------
// 32-bit packed RGB, 10-bit components.
// ---------------------------------------------------------------------

PIXPAT_RGB_PACKED(XRGB2101010,
                  Plane<uint32_t,
                        Comp{ C::B, 10, 0 },
                        Comp{ C::G, 10, 10 },
                        Comp{ C::R, 10, 20 },
                        Comp{ C::X, 2, 30 }>);

PIXPAT_RGB_PACKED(ARGB2101010,
                  Plane<uint32_t,
                        Comp{ C::B, 10, 0 },
                        Comp{ C::G, 10, 10 },
                        Comp{ C::R, 10, 20 },
                        Comp{ C::A, 2, 30 }>);

PIXPAT_RGB_PACKED(XBGR2101010,
                  Plane<uint32_t,
                        Comp{ C::R, 10, 0 },
                        Comp{ C::G, 10, 10 },
                        Comp{ C::B, 10, 20 },
                        Comp{ C::X, 2, 30 }>);

PIXPAT_RGB_PACKED(ABGR2101010,
                  Plane<uint32_t,
                        Comp{ C::R, 10, 0 },
                        Comp{ C::G, 10, 10 },
                        Comp{ C::B, 10, 20 },
                        Comp{ C::A, 2, 30 }>);

PIXPAT_RGB_PACKED(RGBX1010102,
                  Plane<uint32_t,
                        Comp{ C::X, 2, 0 },
                        Comp{ C::B, 10, 2 },
                        Comp{ C::G, 10, 12 },
                        Comp{ C::R, 10, 22 }>);

PIXPAT_RGB_PACKED(RGBA1010102,
                  Plane<uint32_t,
                        Comp{ C::A, 2, 0 },
                        Comp{ C::B, 10, 2 },
                        Comp{ C::G, 10, 12 },
                        Comp{ C::R, 10, 22 }>);

PIXPAT_RGB_PACKED(BGRX1010102,
                  Plane<uint32_t,
                        Comp{ C::X, 2, 0 },
                        Comp{ C::R, 10, 2 },
                        Comp{ C::G, 10, 12 },
                        Comp{ C::B, 10, 22 }>);

PIXPAT_RGB_PACKED(BGRA1010102,
                  Plane<uint32_t,
                        Comp{ C::A, 2, 0 },
                        Comp{ C::R, 10, 2 },
                        Comp{ C::G, 10, 12 },
                        Comp{ C::B, 10, 22 }>);

// ---------------------------------------------------------------------
// 64-bit normalized wide RGB (16 bits per component).
// ---------------------------------------------------------------------

PIXPAT_RGB_PACKED(XRGB16161616,
                  Plane<uint64_t,
                        Comp{ C::B, 16, 0 },
                        Comp{ C::G, 16, 16 },
                        Comp{ C::R, 16, 32 },
                        Comp{ C::X, 16, 48 }>);

PIXPAT_RGB_PACKED(XBGR16161616,
                  Plane<uint64_t,
                        Comp{ C::R, 16, 0 },
                        Comp{ C::G, 16, 16 },
                        Comp{ C::B, 16, 32 },
                        Comp{ C::X, 16, 48 }>);

PIXPAT_RGB_PACKED(ARGB16161616,
                  Plane<uint64_t,
                        Comp{ C::B, 16, 0 },
                        Comp{ C::G, 16, 16 },
                        Comp{ C::R, 16, 32 },
                        Comp{ C::A, 16, 48 }>);

PIXPAT_RGB_PACKED(ABGR16161616,
                  Plane<uint64_t,
                        Comp{ C::R, 16, 0 },
                        Comp{ C::G, 16, 16 },
                        Comp{ C::B, 16, 32 },
                        Comp{ C::A, 16, 48 }>);

#undef PIXPAT_RGB_PACKED

} // namespace pixpat::formats
