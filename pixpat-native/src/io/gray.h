#pragma once

// Grayscale (Y8 / Y10 / Y12 / Y16) and multi-pixel-per-word grayscale
// (XYYY2101010: 3 Y components in one uint32_t). Modeled as a YUV format
// with neutral chroma synthesized on read so cross-color-kind ColorXfm
// produces R=G=B=Y'. The sink encodes Y from YUV16 and ignores U/V.
// Y10/Y12 carry an X padding bitfield which we zero out on write.
// Neutral chroma in normalized-16 is 0x8000 (the midpoint of [0, 0xFFFF]).

#include <array>

#include "../layout.h"
#include "detail.h"

namespace pixpat
{

template <typename L>
struct GraySource {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t y_idx = P::template find_pos<C::Y>();
	static_assert(y_idx < P::num_comps);

	static YUV16 read(const Buffer<1>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		const uint8_t* p = buf.data[0] + y * buf.stride[0]
		                   + x * P::bytes_per_pixel;
		const auto vals = P::unpack(detail::load_word<P>(p));
		return YUV16{
		        detail::decode_norm(P::comps[y_idx].bits, vals[y_idx]),
		        0x8000, 0x8000, uint16_t(0),
		};
	}
};

template <typename L>
struct GraySink {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t y_idx = P::template find_pos<C::Y>();
	static constexpr size_t x_idx = P::template find_pos<C::X>();
	static constexpr bool has_x = (x_idx < P::num_comps);
	static_assert(y_idx < P::num_comps);

	static constexpr size_t block_h = 1;
	static constexpr size_t block_w = 1;

	static void write_block(Buffer<1>& buf, size_t bx, size_t by,
	                        const YUV16 (&block)[1][1]) noexcept
	{
		std::array<uint16_t, P::num_comps> v{};
		v[y_idx] = detail::encode_norm(P::comps[y_idx].bits, block[0][0].y);
		if constexpr (has_x)
			v[x_idx] = 0;

		uint8_t* p = buf.data[0] + by * buf.stride[0]
		             + bx * P::bytes_per_pixel;
		detail::store_word<P>(p, P::pack(v));
	}
};

// Multi-pixel-per-word grayscale. The Layout carries one C::Y entry per
// pixel in the group; pixels_per_word is derived from how many C::Y
// entries the layout has. All Y components must share the same bit width
// (so the encode/decode shift is shared). block_w = ppw so the sink
// writes one storage word per block.
template <typename L>
struct MultiPixelGraySource {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t ppw = P::template component_count<C::Y>();
	static_assert(ppw >= 1);

	// All Y positions share the same bit width.
	static constexpr unsigned y_bits = P::comps[P::template find_pos<C::Y>(0)].bits;

	static YUV16 read(const Buffer<1>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		const size_t gx  = x / ppw;
		const size_t off = x % ppw;
		const uint8_t* p = buf.data[0] + y * buf.stride[0]
		                   + gx * P::bytes_per_pixel;
		const auto vals = P::unpack(detail::load_word<P>(p));

		// find_pos walks the comps array at runtime; comps is constexpr
		// and num_comps is small (≤4 for these formats), so it inlines.
		const size_t y_pos = P::template find_pos<C::Y>(off);

		return YUV16{
		        detail::decode_norm(y_bits, vals[y_pos]),
		        0x8000, 0x8000, uint16_t(0),
		};
	}
};

template <typename L>
struct MultiPixelGraySink {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t ppw = P::template component_count<C::Y>();
	static constexpr size_t x_idx = P::template find_pos<C::X>();
	static constexpr bool has_x = (x_idx < P::num_comps);
	static_assert(ppw >= 1);

	static constexpr size_t block_h = 1;
	static constexpr size_t block_w = ppw;

	static void write_block(Buffer<1>& buf, size_t bx, size_t by,
	                        const YUV16 (&block)[1][ppw]) noexcept
	{
		std::array<uint16_t, P::num_comps> v{};
		// All Y slots share the same bit width.
		constexpr unsigned y_bits = P::comps[P::template find_pos<C::Y>(0)].bits;
		for (size_t i = 0; i < ppw; ++i) {
			const size_t pos = P::template find_pos<C::Y>(i);
			v[pos] = detail::encode_norm(y_bits, block[0][i].y);
		}

		if constexpr (has_x)
			v[x_idx] = 0;

		uint8_t* p = buf.data[0] + by * buf.stride[0]
		             + (bx / ppw) * P::bytes_per_pixel;
		detail::store_word<P>(p, P::pack(v));
	}
};

} // namespace pixpat
