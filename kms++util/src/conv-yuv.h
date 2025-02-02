#pragma once

#include <vector>

#include <kms++/framebuffer.h>
#include <kms++util/color16.h>

#include "conv-common.h"

namespace kms
{

using XVUY2101010_Layout =
	FormatLayout<PlaneLayout<uint32_t,
		ComponentLayout<ComponentType::Y, 10, 0>,
		ComponentLayout<ComponentType::Cb, 10, 10>,
		ComponentLayout<ComponentType::Cr, 10, 20>,
		ComponentLayout<ComponentType::X, 2, 30>>>;

template<typename Layout>
class YUV_Writer
{
	using Plane = Layout::template plane<0>;
	using TStorage = Plane::storage_type;

	static_assert(Layout::num_planes == 1);
	static_assert(Plane::num_components == 3 || Plane::num_components == 4);

	static_assert(Plane::template component_count<ComponentType::Y>() == 1);
	static_assert(Plane::template component_count<ComponentType::Cb>() == 1);
	static_assert(Plane::template component_count<ComponentType::Cr>() == 1);

	static constexpr bool has_alpha = Plane::template component_count<ComponentType::A>();
	static constexpr bool has_padding = Plane::template component_count<ComponentType::X>();

	static constexpr bool needs_packed_access = Plane::total_bits != Plane::storage_bits;

	static constexpr size_t a_idx = Plane::template find_pos<ComponentType::A>();
	static constexpr size_t x_idx = Plane::template find_pos<ComponentType::X>();
	static constexpr size_t y_idx = Plane::template find_pos<ComponentType::Y>();
	static constexpr size_t cb_idx = Plane::template find_pos<ComponentType::Cb>();
	static constexpr size_t cr_idx = Plane::template find_pos<ComponentType::Cr>();

	static constexpr size_t a_shift = has_alpha ? 16 - Plane::template component_size<a_idx> : 0;
	static constexpr size_t x_shift = has_padding ? 16 - Plane::template component_size<x_idx> : 0;
	static constexpr size_t y_shift = 16 - Plane::template component_size<y_idx>;
	static constexpr size_t cb_shift = 16 - Plane::template component_size<cb_idx>;
	static constexpr size_t cr_shift = 16 - Plane::template component_size<cr_idx>;

	static_assert(Plane::total_bits % 8 == 0);
	static constexpr size_t bytes_per_pixel = Plane::total_bits / 8;

public:
	// Pack and write num_pixels pixels from src_line to dst_line
	static void pack_line(HasIndexOperatorReturning<TStorage> auto&& dst_line,
			      HasIndexOperatorReturning<YUV16> auto&& src_line,
			      size_t num_pixels)
	{
		for (size_t x = 0; x < num_pixels; x++) {
			const YUV16& pix = src_line[x];

			std::array<component_storage_type, Plane::num_components>
				components;

			if constexpr (has_alpha)
				components[a_idx] = pix.a >> a_shift;

			if constexpr (has_padding)
				components[x_idx] = 0;

			components[y_idx] = pix.y >> y_shift;
			components[cb_idx] = pix.u >> cb_shift;
			components[cr_idx] = pix.v >> cr_shift;

			if constexpr (!needs_packed_access) {
				dst_line[x] = Plane::pack(components);
			} else {
				auto dst_bytes = reinterpret_cast<uint8_t*>(&dst_line[0]);

				TStorage packed = Plane::pack(components);

				memcpy(dst_bytes + x * bytes_per_pixel, &packed,
				       bytes_per_pixel);
			}
		}
	}

	// Read and unpack num_pixels pixels from src_line to dst_line
	static void unpack_line(HasIndexOperatorReturning<YUV16> auto&& dst_line,
				HasIndexOperatorReturning<TStorage> auto&& src_line,
				size_t num_pixels)
	{
		for (size_t x = 0; x < num_pixels; x++) {
			decltype(Plane::unpack(src_line[x])) components;

			if constexpr (!needs_packed_access) {
				components = Plane::unpack(src_line[x]);
			} else {
				auto src_bytes =
					reinterpret_cast<const uint8_t*>(&src_line[0]);
				TStorage packed;

				memcpy(&packed, src_bytes + x * bytes_per_pixel,
				       bytes_per_pixel);

				components = Plane::unpack(packed);
			}

			dst_line[x] = YUV16 {
				static_cast<uint16_t>(components[y_idx] << y_shift),
				static_cast<uint16_t>(components[cb_idx] << cb_shift),
				static_cast<uint16_t>(components[cr_idx] << cr_shift),
				static_cast<uint16_t>(has_alpha ? components[a_idx] << a_shift : 0),
			};
		}
	}

	static void write_pattern(IFramebuffer& fb, size_t start_y, size_t end_y,
				  auto&& generate_line)
	{
		std::vector<YUV16> linebuf(fb.width());

		// View to the plane
		auto view = make_strided_fb_view<TStorage>(fb.map(0), fb.height(), fb.width(),
							   fb.stride(0));

		for (size_t y_src = start_y; y_src <= end_y; y_src++) {
			generate_line(y_src, linebuf);

			auto dst = md::submdspan(view, y_src, md::full_extent);

			pack_line(dst, linebuf, fb.width());
		}
	}

	static void get_line(IFramebuffer& fb, size_t w, size_t h, size_t row, std::span<YUV16> linebuf)
	{
		auto view = make_strided_fb_view<const TStorage>(fb.map(0), fb.height(), fb.width(),
								 fb.stride(0));

		auto src = md::submdspan(view, row, md::full_extent);

		unpack_line(linebuf, src);
	}
};

} // namespace kms
