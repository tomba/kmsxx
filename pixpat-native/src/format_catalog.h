#pragma once

// Catalog of every pixel format the C++ side handles. The X-macro is a
// flat list of names:
//
//   X(name)
//
// `name` is the canonical format identifier — both an internal FormatId
// enum entry and the public string accepted by pixpat_buffer::format.
// Each name resolves to a struct in `pixpat::formats::` (defined under
// pixpat-native/src/formats/) that carries:
//
//   - the layout (subsampling, planes, components)
//   - nested `Source` / `Sink` aliases for the matching I/O templates
//
// Adding a format = a row here AND a struct in the right
// pixpat-native/src/formats/*.h. The codegen
// (pixpat-native/codegen/gen_pixpat.py) parses this X-macro to learn
// the format set; pixpat.cpp re-expands it to build s_format_info via
// `formats::name::Source` / `formats::name::Sink`.
//
// FormatId is internal — the public C ABI deals in format names only.

#include <cstddef>

namespace pixpat
{

#define PIXPAT_FORMAT_LIST(X) \
	X(XRGB8888)           \
	X(ARGB8888)           \
	X(XBGR8888)           \
	X(ABGR8888)           \
	X(RGBX8888)           \
	X(RGBA8888)           \
	X(BGRX8888)           \
	X(BGRA8888)           \
	X(RGB888)             \
	X(BGR888)             \
	X(RGB332)             \
	X(RGB565)             \
	X(BGR565)             \
	X(XRGB1555)           \
	X(ARGB1555)           \
	X(XBGR1555)           \
	X(ABGR1555)           \
	X(XRGB4444)           \
	X(ARGB4444)           \
	X(XBGR4444)           \
	X(ABGR4444)           \
	X(RGBX4444)           \
	X(RGBA4444)           \
	X(XRGB2101010)        \
	X(ARGB2101010)        \
	X(XBGR2101010)        \
	X(ABGR2101010)        \
	X(RGBX1010102)        \
	X(RGBA1010102)        \
	X(BGRX1010102)        \
	X(BGRA1010102)        \
	X(ABGR16161616)       \
	X(NV12)               \
	X(NV21)               \
	X(NV16)               \
	X(NV61)               \
	X(P030)               \
	X(P230)               \
	X(YUV420)             \
	X(YVU420)             \
	X(YUV422)             \
	X(YVU422)             \
	X(YUV444)             \
	X(YVU444)             \
	X(T430)               \
	X(VUY888)             \
	X(XVUY8888)           \
	X(XVUY2101010)        \
	X(AVUY16161616)       \
	X(YUYV)               \
	X(YVYU)               \
	X(UYVY)               \
	X(VYUY)               \
	X(Y210)               \
	X(Y212)               \
	X(Y216)               \
	X(Y8)                 \
	X(Y10)                \
	X(Y12)                \
	X(Y16)                \
	X(R8)                 \
	X(XYYY2101010)        \
	X(Y10P)               \
	X(Y12P)               \
	X(SRGGB8)             \
	X(SBGGR8)             \
	X(SGRBG8)             \
	X(SGBRG8)             \
	X(SRGGB10)            \
	X(SBGGR10)            \
	X(SGRBG10)            \
	X(SGBRG10)            \
	X(SRGGB12)            \
	X(SBGGR12)            \
	X(SGRBG12)            \
	X(SGBRG12)            \
	X(SRGGB16)            \
	X(SBGGR16)            \
	X(SGRBG16)            \
	X(SGBRG16)            \
	X(SRGGB10P)           \
	X(SBGGR10P)           \
	X(SGRBG10P)           \
	X(SGBRG10P)           \
	X(SRGGB12P)           \
	X(SBGGR12P)           \
	X(SGRBG12P)           \
	X(SGBRG12P)

enum class FormatId {
#define X(name) name,
	PIXPAT_FORMAT_LIST(X)
#undef X
	Unknown,
};

struct FormatEntry {
	const char* name;
	FormatId id;
};

inline constexpr FormatEntry s_format_table[] = {
#define X(name) { #name, FormatId::name },
	PIXPAT_FORMAT_LIST(X)
#undef X
};

inline constexpr size_t s_format_catalog_count =
	sizeof(s_format_table) / sizeof(s_format_table[0]);

} // namespace pixpat
