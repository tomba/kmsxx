#pragma once

// Packed YUV 4:2:2 (YUYV / YVYU / UYVY / VYUY): two pixels per 32-bit
// word, one shared chroma pair. The Layout uses two C::Y entries plus
// one each of C::U / C::V; we resolve the duplicate Y via
// find_pos<C::Y>(n).

#include <array>

#include "../layout.h"
#include "detail.h"

namespace pixpat
{

template <typename L>
struct PackedYUVSource {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 1);
	static_assert(L::h_sub == 2 && L::v_sub == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t y0_idx = P::template find_pos<C::Y>(0);
	static constexpr size_t y1_idx = P::template find_pos<C::Y>(1);
	static constexpr size_t u_idx  = P::template find_pos<C::U>();
	static constexpr size_t v_idx  = P::template find_pos<C::V>();

	static YUV16 read(const Buffer<1>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		const uint8_t* p = buf.data[0] + y * buf.stride[0]
		                   + (x / 2) * P::bytes_per_pixel;
		const auto vals = P::unpack(detail::load_word<P>(p));
		const size_t y_pick = (x & 1) ? y1_idx : y0_idx;
		// Both Y components share the same bit width, so the bit-width
		// for y0 and y1 is identical — pick either.
		return YUV16{
		        detail::decode_norm(P::comps[y0_idx].bits, vals[y_pick]),
		        detail::decode_norm(P::comps[u_idx].bits, vals[u_idx]),
		        detail::decode_norm(P::comps[v_idx].bits, vals[v_idx]),
		        uint16_t(0),
		};
	}
};

template <typename L>
struct PackedYUVSink {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 1);
	static_assert(L::h_sub == 2 && L::v_sub == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t y0_idx = P::template find_pos<C::Y>(0);
	static constexpr size_t y1_idx = P::template find_pos<C::Y>(1);
	static constexpr size_t u_idx  = P::template find_pos<C::U>();
	static constexpr size_t v_idx  = P::template find_pos<C::V>();

	static constexpr size_t block_h = 1;
	static constexpr size_t block_w = 2;

	static void write_block(Buffer<1>& buf, size_t bx, size_t by,
	                        const YUV16 (&block)[1][2]) noexcept
	{
		std::array<uint16_t, P::num_comps> v{};
		v[y0_idx] = detail::encode_norm(P::comps[y0_idx].bits, block[0][0].y);
		v[y1_idx] = detail::encode_norm(P::comps[y1_idx].bits, block[0][1].y);
		// Integer chroma averaging in normalized-16 space. Truncates
		// (no round-half-up).
		v[u_idx]  = detail::encode_norm(P::comps[u_idx].bits, uint16_t(
							(uint32_t(block[0][0].u) +
							 uint32_t(block[0][1].u)) / 2));
		v[v_idx]  = detail::encode_norm(P::comps[v_idx].bits, uint16_t(
							(uint32_t(block[0][0].v) +
							 uint32_t(block[0][1].v)) / 2));

		uint8_t* p = buf.data[0] + by * buf.stride[0]
		             + (bx / 2) * P::bytes_per_pixel;
		detail::store_word<P>(p, P::pack(v));
	}
};

} // namespace pixpat
