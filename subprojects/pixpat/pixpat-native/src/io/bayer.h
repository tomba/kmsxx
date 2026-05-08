#pragma once

// Bayer raw read/write support.
//
// Write side: each pixel carries one of R/G/B selected by (x mod 2,
// y mod 2) and a fixed BayerOrder. Two missing channels per pixel are
// dropped on encode.
//
// Read side: bilinear demosaic over a 3x3 window. The pixel's own
// channel comes from self; missing channels are averaged from the
// same-channel neighbours that the Bayer phase guarantees to exist:
//
//   * At an R or B pixel, all four cardinal (N, E, S, W) neighbours
//     carry G and all four diagonal (NE, NW, SE, SW) neighbours carry
//     the other colour, so each missing channel averages four samples.
//   * At a G pixel, one missing colour sits in the row neighbours
//     (W, E) and the other in the column neighbours (N, S), so each
//     missing channel averages two samples.
//
// Sampled coordinates are clamped to the image bounds.
//
// The Layout shape is the same as a Y-only single-plane format
// (storage carries one component plus optional X padding); the
// BayerOrder is a separate template parameter on the Source / Sink.

#include <array>
#include <cstdint>

#include "../layout.h"
#include "csi2.h"
#include "detail.h"

namespace pixpat
{

enum class BayerOrder { RGGB, BGGR, GRBG, GBRG };

namespace detail
{
constexpr C bayer_pick(BayerOrder o, bool x_even, bool y_even) noexcept
{
	switch (o) {
	case BayerOrder::RGGB:
		return y_even ? (x_even ? C::R : C::G)
		              : (x_even ? C::G : C::B);
	case BayerOrder::BGGR:
		return y_even ? (x_even ? C::B : C::G)
		              : (x_even ? C::G : C::R);
	case BayerOrder::GRBG:
		return y_even ? (x_even ? C::G : C::R)
		              : (x_even ? C::B : C::G);
	case BayerOrder::GBRG:
		return y_even ? (x_even ? C::G : C::B)
		              : (x_even ? C::R : C::G);
	}
	return C::G;
}

constexpr size_t clamp_coord(int v, size_t max_excl) noexcept
{
	if (v < 0)
		return 0;
	if (size_t(v) >= max_excl)
		return max_excl - 1;
	return size_t(v);
}
} // namespace detail

template <typename L, BayerOrder Order>
struct BayerSource {
	using Layout = L;
	using Pixel  = RGB16;

	static_assert(L::kind == ColorKind::RGB);
	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t y_idx = P::template find_pos<C::Y>();
	static_assert(y_idx < P::num_comps);

	static uint16_t read_sample(const Buffer<1>& buf, size_t x, size_t y) noexcept
	{
		const uint8_t* p = buf.data[0] + y * buf.stride[0]
		                   + x * P::bytes_per_pixel;
		const auto vals = P::unpack(detail::load_word<P>(p));
		return detail::decode_norm(P::comps[y_idx].bits, vals[y_idx]);
	}

	static RGB16 read(const Buffer<1>& buf, size_t x, size_t y,
	                  size_t W, size_t H) noexcept
	{
		const bool x_even = (x & 1) == 0;
		const bool y_even = (y & 1) == 0;
		const C self = detail::bayer_pick(Order, x_even, y_even);

		const size_t xL = detail::clamp_coord(int(x) - 1, W);
		const size_t xR = detail::clamp_coord(int(x) + 1, W);
		const size_t yT = detail::clamp_coord(int(y) - 1, H);
		const size_t yB = detail::clamp_coord(int(y) + 1, H);

		const uint16_t s = read_sample(buf, x, y);

		uint16_t r = 0, g = 0, b = 0;

		if (self == C::G) {
			const C h_color = detail::bayer_pick(Order, !x_even, y_even);
			const uint16_t h_avg = uint16_t(
				(uint32_t(read_sample(buf, xL, y))
				 + read_sample(buf, xR, y) + 1u) >> 1);
			const uint16_t v_avg = uint16_t(
				(uint32_t(read_sample(buf, x, yT))
				 + read_sample(buf, x, yB) + 1u) >> 1);
			g = s;
			if (h_color == C::R) { r = h_avg; b = v_avg; }
			else                 { b = h_avg; r = v_avg; }
		} else {
			const uint16_t g_avg = uint16_t(
				(uint32_t(read_sample(buf, x,  yT))
				 + read_sample(buf, x,  yB)
				 + read_sample(buf, xL, y)
				 + read_sample(buf, xR, y) + 2u) >> 2);
			const uint16_t o_avg = uint16_t(
				(uint32_t(read_sample(buf, xL, yT))
				 + read_sample(buf, xR, yT)
				 + read_sample(buf, xL, yB)
				 + read_sample(buf, xR, yB) + 2u) >> 2);
			g = g_avg;
			if (self == C::R) { r = s;     b = o_avg; }
			else              { b = s;     r = o_avg; }
		}

		return RGB16{ r, g, b, uint16_t(0) };
	}
};

template <typename L, BayerOrder Order>
struct BayerSink {
	using Layout = L;
	using Pixel  = RGB16;

	static_assert(L::kind == ColorKind::RGB);
	static_assert(L::num_planes == 1);

	using P = typename L::template plane<0>;
	static constexpr size_t y_idx = P::template find_pos<C::Y>();
	static constexpr size_t x_idx = P::template find_pos<C::X>();
	static constexpr bool has_x = (x_idx < P::num_comps);
	static_assert(y_idx < P::num_comps);

	static constexpr size_t block_h = 1;
	static constexpr size_t block_w = 1;

	static void write_block(Buffer<1>& buf, size_t bx, size_t by,
	                        const RGB16 (&block)[1][1]) noexcept
	{
		const C pick = detail::bayer_pick(Order, (bx & 1) == 0,
		                                  (by & 1) == 0);
		const uint16_t val = pick == C::R ? block[0][0].r
		                   : pick == C::G ? block[0][0].g
		                                  : block[0][0].b;

		std::array<uint16_t, P::num_comps> v{};
		v[y_idx] = detail::encode_norm(P::comps[y_idx].bits, val);
		if constexpr (has_x)
			v[x_idx] = 0;

		uint8_t* p = buf.data[0] + by * buf.stride[0]
		             + bx * P::bytes_per_pixel;
		detail::store_word<P>(p, P::pack(v));
	}
};

// Aliases so X-macro can register without nested template-template params.
template <typename L> using BayerSource_RGGB = BayerSource<L, BayerOrder::RGGB>;
template <typename L> using BayerSource_BGGR = BayerSource<L, BayerOrder::BGGR>;
template <typename L> using BayerSource_GRBG = BayerSource<L, BayerOrder::GRBG>;
template <typename L> using BayerSource_GBRG = BayerSource<L, BayerOrder::GBRG>;

template <typename L> using BayerSink_RGGB = BayerSink<L, BayerOrder::RGGB>;
template <typename L> using BayerSink_BGGR = BayerSink<L, BayerOrder::BGGR>;
template <typename L> using BayerSink_GRBG = BayerSink<L, BayerOrder::GRBG>;
template <typename L> using BayerSink_GBRG = BayerSink<L, BayerOrder::GBRG>;

// MIPI CSI-2 packed Bayer. The bit layout doesn't fit
// `Plane<Storage, Comp...>` because each pixel's bits span two
// non-contiguous bytes, so we use the shared CSI-2 helper (io/csi2.h)
// to (un)pack samples.
//
// The Layout slot is a placeholder (matches the unpacked Bayer of the
// same bit-depth so the user-facing API can pick the right buffer
// shape); bytes_per_pixel from the Plane is unused.
template <typename L, BayerOrder Order, size_t BitDepth>
struct BayerPackedSource {
	using Layout = L;
	using Pixel  = RGB16;

	static_assert(L::kind == ColorKind::RGB);
	static_assert(L::num_planes == 1);
	static_assert(BitDepth == 10 || BitDepth == 12);

	using Traits = detail::csi2::packed_traits<BitDepth>;
	static constexpr size_t ppg = Traits::ppg;
	static constexpr size_t bpg = Traits::bpg;

	// Stored N-bit value upshifts to normalized-16 by `<< (16-N)`,
	// matching the unpacked Bayer source.
	static constexpr unsigned shift = 16 - BitDepth;

	static uint16_t read_sample(const Buffer<1>& buf, size_t x, size_t y) noexcept
	{
		const uint8_t* src = buf.data[0] + y * buf.stride[0]
		                     + (x / ppg) * bpg;
		const uint16_t val = detail::csi2::unpack_sample<BitDepth>(src, x % ppg);
		return uint16_t(val << shift);
	}

	static RGB16 read(const Buffer<1>& buf, size_t x, size_t y,
	                  size_t W, size_t H) noexcept
	{
		const bool x_even = (x & 1) == 0;
		const bool y_even = (y & 1) == 0;
		const C self = detail::bayer_pick(Order, x_even, y_even);

		const size_t xL = detail::clamp_coord(int(x) - 1, W);
		const size_t xR = detail::clamp_coord(int(x) + 1, W);
		const size_t yT = detail::clamp_coord(int(y) - 1, H);
		const size_t yB = detail::clamp_coord(int(y) + 1, H);

		const uint16_t s = read_sample(buf, x, y);

		uint16_t r = 0, g = 0, b = 0;

		if (self == C::G) {
			const C h_color = detail::bayer_pick(Order, !x_even, y_even);
			const uint16_t h_avg = uint16_t(
				(uint32_t(read_sample(buf, xL, y))
				 + read_sample(buf, xR, y) + 1u) >> 1);
			const uint16_t v_avg = uint16_t(
				(uint32_t(read_sample(buf, x, yT))
				 + read_sample(buf, x, yB) + 1u) >> 1);
			g = s;
			if (h_color == C::R) { r = h_avg; b = v_avg; }
			else                 { b = h_avg; r = v_avg; }
		} else {
			const uint16_t g_avg = uint16_t(
				(uint32_t(read_sample(buf, x,  yT))
				 + read_sample(buf, x,  yB)
				 + read_sample(buf, xL, y)
				 + read_sample(buf, xR, y) + 2u) >> 2);
			const uint16_t o_avg = uint16_t(
				(uint32_t(read_sample(buf, xL, yT))
				 + read_sample(buf, xR, yT)
				 + read_sample(buf, xL, yB)
				 + read_sample(buf, xR, yB) + 2u) >> 2);
			g = g_avg;
			if (self == C::R) { r = s;     b = o_avg; }
			else              { b = s;     r = o_avg; }
		}

		return RGB16{ r, g, b, uint16_t(0) };
	}
};

template <typename L, BayerOrder Order, size_t BitDepth>
struct BayerPackedSink {
	using Layout = L;
	using Pixel  = RGB16;

	static_assert(L::kind == ColorKind::RGB);
	static_assert(L::num_planes == 1);
	static_assert(BitDepth == 10 || BitDepth == 12);

	using Traits = detail::csi2::packed_traits<BitDepth>;
	static constexpr size_t ppg = Traits::ppg;
	static constexpr size_t bpg = Traits::bpg;

	static constexpr size_t block_h = 1;
	static constexpr size_t block_w = ppg;

	static void write_block(Buffer<1>& buf, size_t bx, size_t by,
	                        const RGB16 (&block)[1][ppg]) noexcept
	{
		std::array<uint16_t, ppg> vals{};
		for (size_t i = 0; i < ppg; ++i) {
			const C pick = detail::bayer_pick(
				Order, ((bx + i) & 1) == 0, (by & 1) == 0);
			const uint16_t norm =
				pick == C::R ? block[0][i].r
				: pick == C::G ? block[0][i].g
				: block[0][i].b;
			vals[i] = uint16_t(norm >> (16 - BitDepth));
		}

		uint8_t* dst = buf.data[0] + by * buf.stride[0]
		               + (bx / ppg) * bpg;
		detail::csi2::pack_group<BitDepth>(dst, vals);
	}
};

template <typename L> using BayerPackedSource_RGGB10 = BayerPackedSource<L, BayerOrder::RGGB, 10>;
template <typename L> using BayerPackedSource_BGGR10 = BayerPackedSource<L, BayerOrder::BGGR, 10>;
template <typename L> using BayerPackedSource_GRBG10 = BayerPackedSource<L, BayerOrder::GRBG, 10>;
template <typename L> using BayerPackedSource_GBRG10 = BayerPackedSource<L, BayerOrder::GBRG, 10>;
template <typename L> using BayerPackedSource_RGGB12 = BayerPackedSource<L, BayerOrder::RGGB, 12>;
template <typename L> using BayerPackedSource_BGGR12 = BayerPackedSource<L, BayerOrder::BGGR, 12>;
template <typename L> using BayerPackedSource_GRBG12 = BayerPackedSource<L, BayerOrder::GRBG, 12>;
template <typename L> using BayerPackedSource_GBRG12 = BayerPackedSource<L, BayerOrder::GBRG, 12>;

template <typename L> using BayerPackedSink_RGGB10 = BayerPackedSink<L, BayerOrder::RGGB, 10>;
template <typename L> using BayerPackedSink_BGGR10 = BayerPackedSink<L, BayerOrder::BGGR, 10>;
template <typename L> using BayerPackedSink_GRBG10 = BayerPackedSink<L, BayerOrder::GRBG, 10>;
template <typename L> using BayerPackedSink_GBRG10 = BayerPackedSink<L, BayerOrder::GBRG, 10>;
template <typename L> using BayerPackedSink_RGGB12 = BayerPackedSink<L, BayerOrder::RGGB, 12>;
template <typename L> using BayerPackedSink_BGGR12 = BayerPackedSink<L, BayerOrder::BGGR, 12>;
template <typename L> using BayerPackedSink_GRBG12 = BayerPackedSink<L, BayerOrder::GRBG, 12>;
template <typename L> using BayerPackedSink_GBRG12 = BayerPackedSink<L, BayerOrder::GBRG, 12>;

} // namespace pixpat
