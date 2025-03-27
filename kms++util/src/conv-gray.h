#pragma once

#include <vector>

#include <kms++/framebuffer.h>
#include <kms++util/color16.h>

#include "conv-common.h"

namespace kms
{

/*
 * Grayscale formats
 */

using Y8_Layout = FormatLayout<PlaneLayout<uint8_t,
	ComponentLayout<ComponentType::Y, 8, 0>>>;

using Y10_Layout = FormatLayout<PlaneLayout<uint16_t,
	ComponentLayout<ComponentType::Y, 10, 0>,
	ComponentLayout<ComponentType::X, 4, 10>>>;

using Y12_Layout = FormatLayout<PlaneLayout<uint16_t,
	ComponentLayout<ComponentType::Y, 12, 0>,
	ComponentLayout<ComponentType::X, 4, 12>>>;

using Y16_Layout = FormatLayout<PlaneLayout<uint16_t,
	ComponentLayout<ComponentType::Y, 16, 0>>>;

using Y10_P32_Layout = FormatLayout<PlaneLayout<uint32_t,
	ComponentLayout<ComponentType::Y, 10, 0>,   // Y0
	ComponentLayout<ComponentType::Y, 10, 10>,  // Y1
	ComponentLayout<ComponentType::Y, 10, 20>,  // Y2
	ComponentLayout<ComponentType::X, 2, 30>
>>;

template<typename Layout> class Y_Writer
{
	using Plane = Layout::template plane<0>;
	using TStorage = Plane::storage_type;

	static_assert(Layout::num_planes == 1);
	static_assert(Plane::template component_count<ComponentType::Y>() >= 1);

	static constexpr bool has_padding =
		Plane::template component_count<ComponentType::X>();

	static constexpr size_t y_idx = Plane::template find_pos<ComponentType::Y>();
	static constexpr size_t x_idx =
		has_padding ? Plane::template find_pos<ComponentType::X>() : 0;

	static constexpr size_t y_shift = 16 - Plane::template component_size<y_idx>;

	static constexpr size_t pixels_in_group =
		Plane::template component_count<ComponentType::Y>();
	static constexpr bool is_packed_format = pixels_in_group > 1;

public:
	// Pack and write num_pixels pixels from src_line to dst_line
	static void pack_line(HasIndexOperatorReturning<TStorage> auto&& dst_line,
			      HasIndexOperatorReturning<YUV16> auto&& src_line,
			      size_t num_pixels)
	{
		if constexpr (!is_packed_format) {
			// Simple case: one Y component per storage unit
			for (size_t x = 0; x < num_pixels; x++) {
				const YUV16& pix = src_line[x];

				std::array<component_storage_type, Plane::num_components>
					components;

				if constexpr (has_padding)
					components[x_idx] = 0;

				components[y_idx] = pix.y >> y_shift;

				dst_line[x] = Plane::pack(components);
			}
		} else {
			write_y_samples(dst_line, src_line, num_pixels);
		}
	}

	static void write_pattern(IFramebuffer& fb, size_t start_y, size_t end_y,
				  auto&& generate_line)
	{
		std::vector<YUV16> linebuf(fb.width());

		// View to the plane
		auto view = make_strided_fb_view<TStorage>(fb.map(0), fb.height(),
							   fb.width() / pixels_in_group,
							   fb.stride(0));

		for (size_t y_src = start_y; y_src <= end_y; y_src++) {
			generate_line(y_src, linebuf);

			auto dst = md::submdspan(view, y_src, md::full_extent);

			pack_line(dst, linebuf, fb.width());
		}
	}

private:
	template<typename YBuf>
	static void write_y_samples(YBuf&& y_view, auto&& linebuf, size_t num_pixels)
	{
		for (size_t x_src = 0; x_src < num_pixels; x_src += pixels_in_group) {
			auto x_dst = x_src / pixels_in_group;

			write_y_group(y_view, linebuf, x_src, x_dst,
				      std::make_index_sequence<pixels_in_group>{});
		}
	}

	template<typename YBuf, size_t... I>
	static void write_y_group(YBuf&& y_view, auto&& linebuf, size_t x_src,
				  size_t x_dst, std::index_sequence<I...>)
	{
		std::array<component_storage_type, Plane::num_components> y_values{
			static_cast<component_storage_type>(
				(linebuf[x_src + I].y >>
				 (16 - Plane::template component_size<I>)))...
		};

		y_view[x_dst] = Plane::pack(y_values);
	}
};

} // namespace kms