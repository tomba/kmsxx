#pragma once

#include <vector>

#include <kms++/framebuffer.h>
#include <kms++util/color16.h>

#include "conv-common.h"
#include "conv-raw.h"

namespace kms
{

/*
 * Raw Bayer Packed formats (MIPI CSI-2)
 */

template<BayerOrder Order, size_t BitDepth>
struct BayerPacked_Layout;

// 10-bit packed bayer formats - 4 pixels (40 bits) in 5 bytes
template<BayerOrder Order>
struct BayerPacked_Layout<Order, 10>
	: public FormatLayout<PlaneLayout<uint8_t,
		ComponentLayout<ComponentType::Y, 8, 0>>>
{
	static constexpr BayerOrder bayer_order = Order;
	static constexpr size_t bit_depth = 10;
	static constexpr size_t pixels_per_group = 4;
	static constexpr size_t bytes_per_group = 5;
};

// 12-bit packed bayer formats - 2 pixels (24 bits) in 3 bytes
template<BayerOrder Order>
struct BayerPacked_Layout<Order, 12>
	: public FormatLayout<PlaneLayout<uint8_t,
		ComponentLayout<ComponentType::Y, 8, 0>>>
{
	static constexpr BayerOrder bayer_order = Order;
	static constexpr size_t bit_depth = 12;
	static constexpr size_t pixels_per_group = 2;
	static constexpr size_t bytes_per_group = 3;
};

// Convenient aliases for different bit depths
template<BayerOrder Order> using BayerPacked10_Layout = BayerPacked_Layout<Order, 10>;
template<BayerOrder Order> using BayerPacked12_Layout = BayerPacked_Layout<Order, 12>;

// Format-specific type aliases
using SRGGB10P_Layout = BayerPacked10_Layout<BayerOrder::RGGB>;
using SGBRG10P_Layout = BayerPacked10_Layout<BayerOrder::GBRG>;
using SGRBG10P_Layout = BayerPacked10_Layout<BayerOrder::GRBG>;
using SBGGR10P_Layout = BayerPacked10_Layout<BayerOrder::BGGR>;

using SRGGB12P_Layout = BayerPacked12_Layout<BayerOrder::RGGB>;
using SGBRG12P_Layout = BayerPacked12_Layout<BayerOrder::GBRG>;
using SGRBG12P_Layout = BayerPacked12_Layout<BayerOrder::GRBG>;
using SBGGR12P_Layout = BayerPacked12_Layout<BayerOrder::BGGR>;

template<typename Layout>
class BayerPacked_Writer
{
	using Plane = typename Layout::template plane<0>;
	using TStorage = typename Plane::storage_type;

	static_assert(Layout::num_planes == 1);
	static_assert(std::is_same_v<TStorage, uint8_t>);

	static constexpr BayerOrder bayer_order = Layout::bayer_order;
	static constexpr size_t bit_depth = Layout::bit_depth;
	static constexpr size_t pixels_per_group = Layout::pixels_per_group;
	static constexpr size_t bytes_per_group = Layout::bytes_per_group;

	static constexpr ComponentType get_bayer_component(size_t x, size_t y)
	{
		const bool x_even = (x % 2) == 0;
		const bool y_even = (y % 2) == 0;

		switch (bayer_order) {
		case BayerOrder::RGGB:
			if (y_even && x_even) return ComponentType::R;
			if (y_even && !x_even) return ComponentType::G;
			if (!y_even && x_even) return ComponentType::G;
			return ComponentType::B;

		case BayerOrder::BGGR:
			if (y_even && x_even) return ComponentType::B;
			if (y_even && !x_even) return ComponentType::G;
			if (!y_even && x_even) return ComponentType::G;
			return ComponentType::R;

		case BayerOrder::GRBG:
			if (y_even && x_even) return ComponentType::G;
			if (y_even && !x_even) return ComponentType::R;
			if (!y_even && x_even) return ComponentType::B;
			return ComponentType::G;

		case BayerOrder::GBRG:
			if (y_even && x_even) return ComponentType::G;
			if (y_even && !x_even) return ComponentType::B;
			if (!y_even && x_even) return ComponentType::R;
			return ComponentType::G;
		}

		return ComponentType::Y; // fallback
	}

	static uint16_t extract_component(const RGB16& pix, ComponentType component)
	{
		switch (component) {
		case ComponentType::R:
			return pix.r;
		case ComponentType::G:
			return pix.g;
		case ComponentType::B:
			return pix.b;
		default:
			return 0;
		}
	}

	// Pack 10-bit pixels: 4 pixels (40 bits) into 5 bytes
	static void pack_10bit_group(uint8_t* dst, const std::array<uint16_t, 4>& values)
	{
		// Convert from 16-bit to 10-bit values
		const uint16_t p0 = values[0] >> 6;
		const uint16_t p1 = values[1] >> 6;
		const uint16_t p2 = values[2] >> 6;
		const uint16_t p3 = values[3] >> 6;

		// Pack MSB 8 bits of each pixel
		dst[0] = (p0 >> 2) & 0xFF;
		dst[1] = (p1 >> 2) & 0xFF;
		dst[2] = (p2 >> 2) & 0xFF;
		dst[3] = (p3 >> 2) & 0xFF;

		// Pack LSB 2 bits of each pixel into 5th byte
		dst[4] = ((p0 & 0x03) << 6) |
			 ((p1 & 0x03) << 4) |
			 ((p2 & 0x03) << 2) |
			 ((p3 & 0x03) << 0);
	}

	// Pack 12-bit pixels: 2 pixels (24 bits) into 3 bytes
	static void pack_12bit_group(uint8_t* dst, const std::array<uint16_t, 2>& values)
	{
		// Convert from 16-bit to 12-bit values
		const uint16_t p0 = values[0] >> 4;
		const uint16_t p1 = values[1] >> 4;

		// Pack MSB 8 bits of each pixel
		dst[0] = (p0 >> 4) & 0xFF;
		dst[1] = (p1 >> 4) & 0xFF;

		// Pack LSB 4 bits of each pixel into 3rd byte
		dst[2] = ((p0 & 0x0F) << 4) |
			 ((p1 & 0x0F) << 0);
	}

public:
	static void pack_line(HasIndexOperatorReturning<TStorage> auto&& dst_line,
			      HasIndexOperatorReturning<RGB16> auto&& src_line,
			      size_t num_pixels, size_t y)
	{
		if constexpr (bit_depth == 10) {
			// Process 4 pixels at a time for 10-bit packed
			for (size_t x = 0; x < num_pixels; x += pixels_per_group) {
				std::array<uint16_t, 4> pixel_values;

				for (size_t i = 0; i < pixels_per_group && (x + i) < num_pixels; i++) {
					const RGB16& pix = src_line[x + i];
					const ComponentType component = get_bayer_component(x + i, y);
					pixel_values[i] = extract_component(pix, component);
				}

				// Fill remaining pixels with 0 if at end of line
				for (size_t i = num_pixels - x; i < pixels_per_group; i++) {
					pixel_values[i] = 0;
				}

				const size_t group_idx = x / pixels_per_group;
				uint8_t* dst_bytes = reinterpret_cast<uint8_t*>(&dst_line[0]);
				pack_10bit_group(dst_bytes + group_idx * bytes_per_group, pixel_values);
			}
		} else if constexpr (bit_depth == 12) {
			// Process 2 pixels at a time for 12-bit packed
			for (size_t x = 0; x < num_pixels; x += pixels_per_group) {
				std::array<uint16_t, 2> pixel_values;

				for (size_t i = 0; i < pixels_per_group && (x + i) < num_pixels; i++) {
					const RGB16& pix = src_line[x + i];
					const ComponentType component = get_bayer_component(x + i, y);
					pixel_values[i] = extract_component(pix, component);
				}

				// Fill remaining pixels with 0 if at end of line
				for (size_t i = num_pixels - x; i < pixels_per_group; i++) {
					pixel_values[i] = 0;
				}

				const size_t group_idx = x / pixels_per_group;
				uint8_t* dst_bytes = reinterpret_cast<uint8_t*>(&dst_line[0]);
				pack_12bit_group(dst_bytes + group_idx * bytes_per_group, pixel_values);
			}
		}
	}

	static void write_pattern(IFramebuffer& fb, size_t start_y, size_t end_y,
				  auto&& generate_line)
	{
		std::vector<RGB16> linebuf(fb.width());

		// For packed formats, the stride represents bytes per line
		// We need to calculate the correct width in bytes for the view
		const size_t bytes_per_line = fb.stride(0);

		auto view = make_strided_fb_view<TStorage>(fb.map(0), fb.height(),
							   bytes_per_line, fb.stride(0));

		for (size_t y_src = start_y; y_src <= end_y; y_src++) {
			generate_line(y_src, linebuf);

			auto dst = md::submdspan(view, y_src, md::full_extent);

			pack_line(dst, linebuf, fb.width(), y_src);
		}
	}
};

} // namespace kms
