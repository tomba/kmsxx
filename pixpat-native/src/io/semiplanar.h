#pragma once

// 2-plane semiplanar YUV. Two flavours:
//
//   SemiplanarSource / SemiplanarSink — NV12/NV21/NV16/NV61, single
//     pixel per Y storage word, single chroma pair per chroma word.
//
//   MultiPixelSemiplanarSource / MultiPixelSemiplanarSink — P030/P230,
//     multiple Y pixels per Y word and multiple chroma pairs per
//     chroma word. The Y plane has `ppw_y = component_count<Y>()` Y
//     samples per storage word; the chroma plane has `pairs =
//     component_count<U>()` U/V pairs per storage word. block_w =
//     pairs × h_sub, block_h = v_sub — each block exactly fills one
//     chroma word.

#include <array>

#include "../layout.h"
#include "detail.h"

namespace pixpat
{

template <typename L>
struct SemiplanarSource {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 2);

	using YP = typename L::template plane<0>;
	using CP = typename L::template plane<1>;
	static constexpr size_t y_idx = YP::template find_pos<C::Y>();
	static constexpr size_t u_idx = CP::template find_pos<C::U>();
	static constexpr size_t v_idx = CP::template find_pos<C::V>();

	static YUV16 read(const Buffer<2>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		const uint8_t* yp = buf.data[0] + y * buf.stride[0] + x * YP::bytes_per_pixel;
		const auto y_vals = YP::unpack(detail::load_word<YP>(yp));

		const size_t cx = x / L::h_sub;
		const size_t cy = y / L::v_sub;
		const uint8_t* cp = buf.data[1] + cy * buf.stride[1] + cx * CP::bytes_per_pixel;
		const auto c_vals = CP::unpack(detail::load_word<CP>(cp));

		return YUV16{
		        detail::decode_norm(YP::comps[y_idx].bits, y_vals[y_idx]),
		        detail::decode_norm(CP::comps[u_idx].bits, c_vals[u_idx]),
		        detail::decode_norm(CP::comps[v_idx].bits, c_vals[v_idx]),
		        uint16_t(0),
		};
	}
};

template <typename L>
struct SemiplanarSink {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 2);

	using YP = typename L::template plane<0>;
	using CP = typename L::template plane<1>;
	static constexpr size_t y_idx = YP::template find_pos<C::Y>();
	static constexpr size_t u_idx = CP::template find_pos<C::U>();
	static constexpr size_t v_idx = CP::template find_pos<C::V>();

	static constexpr size_t block_h = L::v_sub;
	static constexpr size_t block_w = L::h_sub;

	static void write_block(Buffer<2>& buf, size_t bx, size_t by,
	                        const YUV16 (&block)[block_h][block_w]) noexcept
	{
		// Y per pixel.
		for (size_t dy = 0; dy < block_h; ++dy) {
			uint8_t* y_row = buf.data[0] + (by + dy) * buf.stride[0];
			for (size_t dx = 0; dx < block_w; ++dx) {
				std::array<uint16_t, YP::num_comps> v{};
				v[y_idx] = detail::encode_norm(YP::comps[y_idx].bits,
				                               block[dy][dx].y);
				detail::store_word<YP>(
					y_row + (bx + dx) * YP::bytes_per_pixel,
					YP::pack(v));
			}
		}

		// One averaged UV pair for the whole block. Integer truncation
		// (no round-half-up).
		uint32_t u_sum = 0, v_sum = 0;
		for (size_t dy = 0; dy < block_h; ++dy) {
			for (size_t dx = 0; dx < block_w; ++dx) {
				u_sum += block[dy][dx].u;
				v_sum += block[dy][dx].v;
			}
		}
		constexpr uint32_t n = block_h * block_w;
		const uint16_t u_avg = uint16_t(u_sum / n);
		const uint16_t v_avg = uint16_t(v_sum / n);

		std::array<uint16_t, CP::num_comps> uv{};
		uv[u_idx] = detail::encode_norm(CP::comps[u_idx].bits, u_avg);
		uv[v_idx] = detail::encode_norm(CP::comps[v_idx].bits, v_avg);

		const size_t cx = bx / L::h_sub;
		const size_t cy = by / L::v_sub;
		uint8_t* cp = buf.data[1] + cy * buf.stride[1] + cx * CP::bytes_per_pixel;
		detail::store_word<CP>(cp, CP::pack(uv));
	}
};

// Multi-pixel-per-word semiplanar (P030: 4:2:0, P230: 4:2:2). All Y
// components share the same bit width; same for U and V.
template <typename L>
struct MultiPixelSemiplanarSource {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 2);

	using YP = typename L::template plane<0>;
	using CP = typename L::template plane<1>;
	static constexpr size_t ppw_y = YP::template component_count<C::Y>();
	static constexpr size_t pairs = CP::template component_count<C::U>();
	static_assert(ppw_y >= 1 && pairs >= 1);
	static_assert(pairs == CP::template component_count<C::V>());

	// All same-tag positions share the same bit width.
	static constexpr unsigned y_bits = YP::comps[YP::template find_pos<C::Y>(0)].bits;
	static constexpr unsigned u_bits = CP::comps[CP::template find_pos<C::U>(0)].bits;
	static constexpr unsigned v_bits = CP::comps[CP::template find_pos<C::V>(0)].bits;

	static YUV16 read(const Buffer<2>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		// Y read.
		const size_t y_gx  = x / ppw_y;
		const size_t y_off = x % ppw_y;
		const uint8_t* yp = buf.data[0] + y * buf.stride[0]
		                    + y_gx * YP::bytes_per_pixel;
		const auto y_vals = YP::unpack(detail::load_word<YP>(yp));

		// Chroma read.
		const size_t cx    = x / L::h_sub;
		const size_t cy    = y / L::v_sub;
		const size_t c_gx  = cx / pairs;
		const size_t c_off = cx % pairs;
		const uint8_t* cp = buf.data[1] + cy * buf.stride[1]
		                    + c_gx * CP::bytes_per_pixel;
		const auto c_vals = CP::unpack(detail::load_word<CP>(cp));

		return YUV16{
		        detail::decode_norm(y_bits, y_vals[YP::template find_pos<C::Y>(y_off)]),
		        detail::decode_norm(u_bits, c_vals[CP::template find_pos<C::U>(c_off)]),
		        detail::decode_norm(v_bits, c_vals[CP::template find_pos<C::V>(c_off)]),
		        uint16_t(0),
		};
	}
};

template <typename L>
struct MultiPixelSemiplanarSink {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 2);

	using YP = typename L::template plane<0>;
	using CP = typename L::template plane<1>;
	static constexpr size_t ppw_y = YP::template component_count<C::Y>();
	static constexpr size_t pairs = CP::template component_count<C::U>();
	static_assert(ppw_y >= 1 && pairs >= 1);

	// One block exactly fills one chroma word: `pairs` chroma pairs,
	// each covering h_sub luma columns × v_sub rows.
	static constexpr size_t block_w = pairs * L::h_sub;
	static constexpr size_t block_h = L::v_sub;
	static_assert(block_w % ppw_y == 0,
	              "block width must be a multiple of Y-pixels-per-word");
	static constexpr size_t y_words_per_row = block_w / ppw_y;

	// All same-tag positions share the same bit width.
	static constexpr unsigned y_bits = YP::comps[YP::template find_pos<C::Y>(0)].bits;
	static constexpr unsigned u_bits = CP::comps[CP::template find_pos<C::U>(0)].bits;
	static constexpr unsigned v_bits = CP::comps[CP::template find_pos<C::V>(0)].bits;

	static void write_block(Buffer<2>& buf, size_t bx, size_t by,
	                        const YUV16 (&block)[block_h][block_w]) noexcept
	{
		// Y plane: y_words_per_row Y-words per row, block_h rows.
		for (size_t dy = 0; dy < block_h; ++dy) {
			uint8_t* y_row = buf.data[0]
			                 + (by + dy) * buf.stride[0];
			for (size_t w = 0; w < y_words_per_row; ++w) {
				std::array<uint16_t, YP::num_comps> v{};
				for (size_t i = 0; i < ppw_y; ++i) {
					const size_t pos = YP::template find_pos<C::Y>(i);
					v[pos] = detail::encode_norm(
						y_bits, block[dy][w * ppw_y + i].y);
				}
				detail::store_word<YP>(
					y_row + (bx / ppw_y + w)
					* YP::bytes_per_pixel,
					YP::pack(v));
			}
		}

		// One UV-word: `pairs` chroma pairs. Each pair averages h_sub
		// horizontally × v_sub vertically luma values.
		std::array<uint16_t, CP::num_comps> uv{};
		constexpr uint32_t n = L::h_sub * L::v_sub;
		for (size_t p = 0; p < pairs; ++p) {
			uint32_t u_sum = 0, v_sum = 0;
			for (size_t dy = 0; dy < block_h; ++dy) {
				for (size_t dx = 0; dx < L::h_sub; ++dx) {
					u_sum += block[dy][p * L::h_sub + dx].u;
					v_sum += block[dy][p * L::h_sub + dx].v;
				}
			}
			uv[CP::template find_pos<C::U>(p)] =
				detail::encode_norm(u_bits, uint16_t(u_sum / n));
			uv[CP::template find_pos<C::V>(p)] =
				detail::encode_norm(v_bits, uint16_t(v_sum / n));
		}

		const size_t cy = by / L::v_sub;
		const size_t uv_word_idx = bx / block_w;
		detail::store_word<CP>(
			buf.data[1] + cy * buf.stride[1]
			+ uv_word_idx * CP::bytes_per_pixel,
			CP::pack(uv));
	}
};

} // namespace pixpat
