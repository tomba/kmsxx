#pragma once

// Single-plane, single-pixel-per-storage-word formats. Works for both
// RGB layouts (XRGB8888, RGB565, ABGR16161616, ...) and YUV
// single-pixel layouts (XVUY2101010, AVUY16161616). Pixel type follows
// L::kind; the three mandatory components are R/G/B for RGB or Y/U/V
// for YUV. Both `RGB16` and `YUV16` are 4 uint16_t with the alpha last,
// so aggregate-init by position works for either.

#include <array>
#include <type_traits>

#include "../layout.h"
#include "detail.h"

namespace pixpat
{

template <typename L>
struct PackedSource {
	using Layout = L;
	using Pixel  = std::conditional_t<L::kind == ColorKind::RGB, RGB16, YUV16>;

	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr C c0 = (L::kind == ColorKind::RGB) ? C::R : C::Y;
	static constexpr C c1 = (L::kind == ColorKind::RGB) ? C::G : C::U;
	static constexpr C c2 = (L::kind == ColorKind::RGB) ? C::B : C::V;

	static constexpr size_t i0 = P::template find_pos<c0>();
	static constexpr size_t i1 = P::template find_pos<c1>();
	static constexpr size_t i2 = P::template find_pos<c2>();
	static constexpr size_t a_idx = P::template find_pos<C::A>();
	static constexpr bool has_a = (a_idx < P::num_comps);
	static_assert(i0 < P::num_comps && i1 < P::num_comps && i2 < P::num_comps);

	static Pixel read(const Buffer<1>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		const uint8_t* p = buf.data[0] + y * buf.stride[0] + x * P::bytes_per_pixel;
		const auto vals = P::unpack(detail::load_word<P>(p));
		Pixel out{
			detail::decode_norm(P::comps[i0].bits, vals[i0]),
			detail::decode_norm(P::comps[i1].bits, vals[i1]),
			detail::decode_norm(P::comps[i2].bits, vals[i2]),
			uint16_t(0),
		};
		if constexpr (has_a)
			out.a = detail::decode_norm(P::comps[a_idx].bits, vals[a_idx]);
		return out;
	}
};

template <typename L>
struct PackedSink {
	using Layout = L;
	using Pixel  = std::conditional_t<L::kind == ColorKind::RGB, RGB16, YUV16>;

	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr C c0 = (L::kind == ColorKind::RGB) ? C::R : C::Y;
	static constexpr C c1 = (L::kind == ColorKind::RGB) ? C::G : C::U;
	static constexpr C c2 = (L::kind == ColorKind::RGB) ? C::B : C::V;

	static constexpr size_t i0 = P::template find_pos<c0>();
	static constexpr size_t i1 = P::template find_pos<c1>();
	static constexpr size_t i2 = P::template find_pos<c2>();
	static constexpr size_t x_idx = P::template find_pos<C::X>();
	static constexpr size_t a_idx = P::template find_pos<C::A>();
	static constexpr bool has_x = (x_idx < P::num_comps);
	static constexpr bool has_a = (a_idx < P::num_comps);
	static_assert(i0 < P::num_comps && i1 < P::num_comps && i2 < P::num_comps);

	static constexpr size_t block_h = 1;
	static constexpr size_t block_w = 1;

	// Aggregate-init access to RGB16/YUV16 by position: .r/.y, .g/.u, .b/.v.
	// We use the field names corresponding to L::kind.
	static void write_block(Buffer<1>& buf, size_t bx, size_t by,
	                        const Pixel (&block)[1][1]) noexcept
	{
		const Pixel& pix = block[0][0];
		std::array<uint16_t, P::num_comps> v{};
		if constexpr (L::kind == ColorKind::RGB) {
			v[i0] = detail::encode_norm(P::comps[i0].bits, pix.r);
			v[i1] = detail::encode_norm(P::comps[i1].bits, pix.g);
			v[i2] = detail::encode_norm(P::comps[i2].bits, pix.b);
		} else {
			v[i0] = detail::encode_norm(P::comps[i0].bits, pix.y);
			v[i1] = detail::encode_norm(P::comps[i1].bits, pix.u);
			v[i2] = detail::encode_norm(P::comps[i2].bits, pix.v);
		}
		if constexpr (has_x)
			v[x_idx] = 0;
		if constexpr (has_a)
			v[a_idx] = detail::encode_norm(P::comps[a_idx].bits, pix.a);

		uint8_t* p = buf.data[0] + by * buf.stride[0] + bx * P::bytes_per_pixel;
		detail::store_word<P>(p, P::pack(v));
	}
};

} // namespace pixpat
