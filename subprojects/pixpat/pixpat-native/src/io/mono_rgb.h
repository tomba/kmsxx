#pragma once

// Single-channel RGB formats (R8). Storage carries one R component;
// MonoRGBSource synthesizes G=B=R on read so cross-color-kind ColorXfm
// produces sensible Y from R alone. MonoRGBSink encodes R and ignores
// G/B/A (and zeroes any X padding). Symmetric to GraySource/GraySink
// (io/gray.h) but for ColorKind::RGB on C::R.

#include <array>

#include "../layout.h"
#include "detail.h"

namespace pixpat
{

template <typename L>
struct MonoRGBSource {
	using Layout = L;
	using Pixel  = RGB16;

	static_assert(L::kind == ColorKind::RGB);
	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t r_idx = P::template find_pos<C::R>();
	static_assert(r_idx < P::num_comps);

	static RGB16 read(const Buffer<1>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		const uint8_t* p = buf.data[0] + y * buf.stride[0]
		                   + x * P::bytes_per_pixel;
		const auto vals = P::unpack(detail::load_word<P>(p));
		const uint16_t r = detail::decode_norm(P::comps[r_idx].bits, vals[r_idx]);
		return RGB16{ r, r, r, uint16_t(0) };
	}
};

template <typename L>
struct MonoRGBSink {
	using Layout = L;
	using Pixel  = RGB16;

	static_assert(L::kind == ColorKind::RGB);
	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t r_idx = P::template find_pos<C::R>();
	static constexpr size_t x_idx = P::template find_pos<C::X>();
	static constexpr bool has_x = (x_idx < P::num_comps);
	static_assert(r_idx < P::num_comps);

	static constexpr size_t block_h = 1;
	static constexpr size_t block_w = 1;

	static void write_block(Buffer<1>& buf, size_t bx, size_t by,
	                        const RGB16 (&block)[1][1]) noexcept
	{
		std::array<uint16_t, P::num_comps> v{};
		v[r_idx] = detail::encode_norm(P::comps[r_idx].bits, block[0][0].r);
		if constexpr (has_x)
			v[x_idx] = 0;

		uint8_t* p = buf.data[0] + by * buf.stride[0]
		             + bx * P::bytes_per_pixel;
		detail::store_word<P>(p, P::pack(v));
	}
};

} // namespace pixpat
