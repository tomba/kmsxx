#pragma once

#include <vector>

#include <kms++/framebuffer.h>
#include <kms++util/color16.h>

#include "conv-common.h"

namespace kms
{

/*
 * Raw Bayer formats
 */

enum class BayerOrder {
	RGGB,
	BGGR,
	GRBG,
	GBRG
};

template<BayerOrder Order, size_t BitDepth>
struct Bayer_Layout;

// 8-bit bayer formats (uint8_t storage)
template<BayerOrder Order>
struct Bayer_Layout<Order, 8>
	: public FormatLayout<PlaneLayout<uint8_t,
		ComponentLayout<ComponentType::Y, 8, 0>>>
{
	static constexpr BayerOrder bayer_order = Order;
	static constexpr size_t bit_depth = 8;
};

// 10-bit bayer formats (uint16_t storage)
template<BayerOrder Order>
struct Bayer_Layout<Order, 10>
	: public FormatLayout<PlaneLayout<uint16_t,
		ComponentLayout<ComponentType::Y, 10, 0>,
		ComponentLayout<ComponentType::X, 6, 10>>>
{
	static constexpr BayerOrder bayer_order = Order;
	static constexpr size_t bit_depth = 10;
};

// 12-bit bayer formats (uint16_t storage)
template<BayerOrder Order>
struct Bayer_Layout<Order, 12>
	: public FormatLayout<PlaneLayout<uint16_t,
		ComponentLayout<ComponentType::Y, 12, 0>,
		ComponentLayout<ComponentType::X, 4, 12>>>
{
	static constexpr BayerOrder bayer_order = Order;
	static constexpr size_t bit_depth = 12;
};

// 16-bit bayer formats (uint16_t storage)
template<BayerOrder Order>
struct Bayer_Layout<Order, 16>
	: public FormatLayout<PlaneLayout<uint16_t,
		ComponentLayout<ComponentType::Y, 16, 0>>>
{
	static constexpr BayerOrder bayer_order = Order;
	static constexpr size_t bit_depth = 16;
};

// Convenient aliases for different bit depths
template<BayerOrder Order> using Bayer8_Layout = Bayer_Layout<Order, 8>;
template<BayerOrder Order> using Bayer10_Layout = Bayer_Layout<Order, 10>;
template<BayerOrder Order> using Bayer12_Layout = Bayer_Layout<Order, 12>;
template<BayerOrder Order> using Bayer16_Layout = Bayer_Layout<Order, 16>;

// Format-specific type aliases
using SRGGB8_Layout = Bayer8_Layout<BayerOrder::RGGB>;
using SGBRG8_Layout = Bayer8_Layout<BayerOrder::GBRG>;
using SGRBG8_Layout = Bayer8_Layout<BayerOrder::GRBG>;
using SBGGR8_Layout = Bayer8_Layout<BayerOrder::BGGR>;

using SRGGB10_Layout = Bayer10_Layout<BayerOrder::RGGB>;
using SGBRG10_Layout = Bayer10_Layout<BayerOrder::GBRG>;
using SGRBG10_Layout = Bayer10_Layout<BayerOrder::GRBG>;
using SBGGR10_Layout = Bayer10_Layout<BayerOrder::BGGR>;

using SRGGB12_Layout = Bayer12_Layout<BayerOrder::RGGB>;
using SGBRG12_Layout = Bayer12_Layout<BayerOrder::GBRG>;
using SGRBG12_Layout = Bayer12_Layout<BayerOrder::GRBG>;
using SBGGR12_Layout = Bayer12_Layout<BayerOrder::BGGR>;

using SRGGB16_Layout = Bayer16_Layout<BayerOrder::RGGB>;
using SGBRG16_Layout = Bayer16_Layout<BayerOrder::GBRG>;
using SGRBG16_Layout = Bayer16_Layout<BayerOrder::GRBG>;
using SBGGR16_Layout = Bayer16_Layout<BayerOrder::BGGR>;

template<typename Layout>
class Bayer_Writer
{
	using Plane = typename Layout::template plane<0>;
	using TStorage = typename Plane::storage_type;

	static_assert(Layout::num_planes == 1);
	static_assert(Plane::template component_count<ComponentType::Y>() == 1);

	static constexpr BayerOrder bayer_order = Layout::bayer_order;
	static constexpr size_t bit_depth = Layout::bit_depth;
	static constexpr bool has_padding = Plane::template component_count<ComponentType::X>();

	static constexpr size_t y_idx = Plane::template find_pos<ComponentType::Y>();
	static constexpr size_t x_idx = has_padding ? Plane::template find_pos<ComponentType::X>() : 0;

	static constexpr size_t y_shift = 16 - Plane::template component_size<y_idx>;

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

public:
	static void pack_line(HasIndexOperatorReturning<TStorage> auto&& dst_line,
			      HasIndexOperatorReturning<RGB16> auto&& src_line,
			      size_t num_pixels, size_t y)
	{
		for (size_t x = 0; x < num_pixels; x++) {
			const RGB16& pix = src_line[x];

			const ComponentType component = get_bayer_component(x, y);
			const uint16_t value = extract_component(pix, component);

			std::array<component_storage_type, Plane::num_components> components;

			if constexpr (has_padding)
				components[x_idx] = 0;

			components[y_idx] = value >> y_shift;

			dst_line[x] = Plane::pack(components);
		}
	}

	static void write_pattern(IFramebuffer& fb, size_t start_y, size_t end_y,
				  auto&& generate_line)
	{
		std::vector<RGB16> linebuf(fb.width());

		auto view = make_strided_fb_view<TStorage>(fb.map(0), fb.height(), fb.width(),
							   fb.stride(0));

		for (size_t y_src = start_y; y_src <= end_y; y_src++) {
			generate_line(y_src, linebuf);

			auto dst = md::submdspan(view, y_src, md::full_extent);

			pack_line(dst, linebuf, fb.width(), y_src);
		}
	}
};

} // namespace kms
