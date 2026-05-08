#pragma once

// 3-plane planar YUV. Two flavours:
//
//   PlanarSource / PlanarSink — YUV/YVU 420/422/444, single Y per word,
//     single chroma per word. Chroma is averaged over h_sub × v_sub
//     on write.
//
//   MultiPixelPlanarSource / MultiPixelPlanarSink — T430, multi-pixel-
//     per-word planar 4:4:4 (3 samples per uint32_t in each of 3
//     planes, plus 2-bit X padding). block_w = ppw, block_h = 1.
//
// Plane indices for Y / U / V are looked up via Layout::find_plane<C>(),
// so swap_uv layouts (YVU vs YUV) work without separate templates.

#include <array>

#include "../layout.h"
#include "detail.h"

namespace pixpat
{

template <typename L>
struct PlanarSource {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 3);

	static constexpr size_t y_plane = L::template find_plane<C::Y>();
	static constexpr size_t u_plane = L::template find_plane<C::U>();
	static constexpr size_t v_plane = L::template find_plane<C::V>();

	using YP = typename L::template plane<y_plane>;
	using UP = typename L::template plane<u_plane>;
	using VP = typename L::template plane<v_plane>;

	static YUV16 read(const Buffer<3>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		const uint8_t* yp = buf.data[y_plane] + y * buf.stride[y_plane]
		                    + x * YP::bytes_per_pixel;
		const auto y_vals = YP::unpack(detail::load_word<YP>(yp));

		const size_t cx = x / L::h_sub;
		const size_t cy = y / L::v_sub;
		const uint8_t* up = buf.data[u_plane] + cy * buf.stride[u_plane]
		                    + cx * UP::bytes_per_pixel;
		const uint8_t* vp = buf.data[v_plane] + cy * buf.stride[v_plane]
		                    + cx * VP::bytes_per_pixel;
		const auto u_vals = UP::unpack(detail::load_word<UP>(up));
		const auto v_vals = VP::unpack(detail::load_word<VP>(vp));

		return YUV16{
		        detail::decode_norm(YP::comps[0].bits, y_vals[0]),
		        detail::decode_norm(UP::comps[0].bits, u_vals[0]),
		        detail::decode_norm(VP::comps[0].bits, v_vals[0]),
		        uint16_t(0),
		};
	}
};

template <typename L>
struct PlanarSink {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 3);

	static constexpr size_t y_plane = L::template find_plane<C::Y>();
	static constexpr size_t u_plane = L::template find_plane<C::U>();
	static constexpr size_t v_plane = L::template find_plane<C::V>();

	using YP = typename L::template plane<y_plane>;
	using UP = typename L::template plane<u_plane>;
	using VP = typename L::template plane<v_plane>;

	static constexpr size_t block_h = L::v_sub;
	static constexpr size_t block_w = L::h_sub;

	static void write_block(Buffer<3>& buf, size_t bx, size_t by,
	                        const YUV16 (&block)[block_h][block_w]) noexcept
	{
		// Y per pixel.
		for (size_t dy = 0; dy < block_h; ++dy) {
			uint8_t* y_row = buf.data[y_plane]
			                 + (by + dy) * buf.stride[y_plane];
			for (size_t dx = 0; dx < block_w; ++dx) {
				std::array<uint16_t, YP::num_comps> v{};
				v[0] = detail::encode_norm(YP::comps[0].bits, block[dy][dx].y);
				detail::store_word<YP>(
					y_row + (bx + dx) * YP::bytes_per_pixel,
					YP::pack(v));
			}
		}

		// One averaged U and V sample per block. Integer truncation
		// (no round-half-up).
		uint32_t u_sum = 0, v_sum = 0;
		for (size_t dy = 0; dy < block_h; ++dy) {
			for (size_t dx = 0; dx < block_w; ++dx) {
				u_sum += block[dy][dx].u;
				v_sum += block[dy][dx].v;
			}
		}
		constexpr uint32_t n = block_h * block_w;

		const size_t cx = bx / L::h_sub;
		const size_t cy = by / L::v_sub;

		std::array<uint16_t, UP::num_comps> uw{};
		uw[0] = detail::encode_norm(UP::comps[0].bits, uint16_t(u_sum / n));
		detail::store_word<UP>(
			buf.data[u_plane] + cy * buf.stride[u_plane]
			+ cx * UP::bytes_per_pixel,
			UP::pack(uw));

		std::array<uint16_t, VP::num_comps> vw{};
		vw[0] = detail::encode_norm(VP::comps[0].bits, uint16_t(v_sum / n));
		detail::store_word<VP>(
			buf.data[v_plane] + cy * buf.stride[v_plane]
			+ cx * VP::bytes_per_pixel,
			VP::pack(vw));
	}
};

// T430-style 3-plane multi-pixel-per-word planar 4:4:4. Each plane has
// `ppw` samples of the same component (Y in plane 0, U in 1, V in 2 —
// or whichever ordering find_plane resolves) packed into a single
// storage word. block_w = ppw, block_h = 1. No chroma subsampling.
template <typename L>
struct MultiPixelPlanarSource {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 3);
	static_assert(L::h_sub == 1 && L::v_sub == 1);

	static constexpr size_t y_plane = L::template find_plane<C::Y>();
	static constexpr size_t u_plane = L::template find_plane<C::U>();
	static constexpr size_t v_plane = L::template find_plane<C::V>();

	using YP = typename L::template plane<y_plane>;
	using UP = typename L::template plane<u_plane>;
	using VP = typename L::template plane<v_plane>;

	static constexpr size_t ppw = YP::template component_count<C::Y>();
	static_assert(ppw == UP::template component_count<C::U>());
	static_assert(ppw == VP::template component_count<C::V>());

	// All same-tag positions share the same bit width.
	static constexpr unsigned y_bits = YP::comps[YP::template find_pos<C::Y>(0)].bits;
	static constexpr unsigned u_bits = UP::comps[UP::template find_pos<C::U>(0)].bits;
	static constexpr unsigned v_bits = VP::comps[VP::template find_pos<C::V>(0)].bits;

	static YUV16 read(const Buffer<3>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		const size_t gx  = x / ppw;
		const size_t off = x % ppw;

		const uint8_t* yp = buf.data[y_plane] + y * buf.stride[y_plane]
		                    + gx * YP::bytes_per_pixel;
		const uint8_t* up = buf.data[u_plane] + y * buf.stride[u_plane]
		                    + gx * UP::bytes_per_pixel;
		const uint8_t* vp = buf.data[v_plane] + y * buf.stride[v_plane]
		                    + gx * VP::bytes_per_pixel;

		const auto y_vals = YP::unpack(detail::load_word<YP>(yp));
		const auto u_vals = UP::unpack(detail::load_word<UP>(up));
		const auto v_vals = VP::unpack(detail::load_word<VP>(vp));

		return YUV16{
		        detail::decode_norm(y_bits, y_vals[YP::template find_pos<C::Y>(off)]),
		        detail::decode_norm(u_bits, u_vals[UP::template find_pos<C::U>(off)]),
		        detail::decode_norm(v_bits, v_vals[VP::template find_pos<C::V>(off)]),
		        uint16_t(0),
		};
	}
};

template <typename L>
struct MultiPixelPlanarSink {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 3);
	static_assert(L::h_sub == 1 && L::v_sub == 1);

	static constexpr size_t y_plane = L::template find_plane<C::Y>();
	static constexpr size_t u_plane = L::template find_plane<C::U>();
	static constexpr size_t v_plane = L::template find_plane<C::V>();

	using YP = typename L::template plane<y_plane>;
	using UP = typename L::template plane<u_plane>;
	using VP = typename L::template plane<v_plane>;

	static constexpr size_t ppw = YP::template component_count<C::Y>();

	static constexpr size_t y_x_idx = YP::template find_pos<C::X>();
	static constexpr size_t u_x_idx = UP::template find_pos<C::X>();
	static constexpr size_t v_x_idx = VP::template find_pos<C::X>();
	static constexpr bool y_has_x = (y_x_idx < YP::num_comps);
	static constexpr bool u_has_x = (u_x_idx < UP::num_comps);
	static constexpr bool v_has_x = (v_x_idx < VP::num_comps);

	static constexpr size_t block_h = 1;
	static constexpr size_t block_w = ppw;

	static void write_block(Buffer<3>& buf, size_t bx, size_t by,
	                        const YUV16 (&block)[1][ppw]) noexcept
	{
		std::array<uint16_t, YP::num_comps> yv{};
		std::array<uint16_t, UP::num_comps> uv{};
		std::array<uint16_t, VP::num_comps> vv{};

		// All same-tag positions share the same bit width.
		constexpr unsigned y_bits = YP::comps[YP::template find_pos<C::Y>(0)].bits;
		constexpr unsigned u_bits = UP::comps[UP::template find_pos<C::U>(0)].bits;
		constexpr unsigned v_bits = VP::comps[VP::template find_pos<C::V>(0)].bits;
		for (size_t i = 0; i < ppw; ++i) {
			yv[YP::template find_pos<C::Y>(i)] =
				detail::encode_norm(y_bits, block[0][i].y);
			uv[UP::template find_pos<C::U>(i)] =
				detail::encode_norm(u_bits, block[0][i].u);
			vv[VP::template find_pos<C::V>(i)] =
				detail::encode_norm(v_bits, block[0][i].v);
		}

		if constexpr (y_has_x) yv[y_x_idx] = 0;
		if constexpr (u_has_x) uv[u_x_idx] = 0;
		if constexpr (v_has_x) vv[v_x_idx] = 0;

		const size_t gx = bx / ppw;
		detail::store_word<YP>(
			buf.data[y_plane] + by * buf.stride[y_plane]
			+ gx * YP::bytes_per_pixel,
			YP::pack(yv));
		detail::store_word<UP>(
			buf.data[u_plane] + by * buf.stride[u_plane]
			+ gx * UP::bytes_per_pixel,
			UP::pack(uv));
		detail::store_word<VP>(
			buf.data[v_plane] + by * buf.stride[v_plane]
			+ gx * VP::bytes_per_pixel,
			VP::pack(vv));
	}
};

} // namespace pixpat
