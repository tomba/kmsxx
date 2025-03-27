#pragma once

#include <vector>

#include <kms++/framebuffer.h>
#include <kms++util/color16.h>

#include "conv-common.h"

namespace kms
{

/*
 * RGB
 */

template<ComponentType C3, size_t size3, size_t offset3,
	 ComponentType C2, size_t size2, size_t offset2,
	 ComponentType C1, size_t size1, size_t offset1,
	 ComponentType C0, size_t size0, size_t offset0>
struct RGB_16_Layout
	: public FormatLayout<PlaneLayout<uint16_t,
		ComponentLayout<C0, size0, offset0>,
		ComponentLayout<C1, size1, offset1>,
		ComponentLayout<C2, size2, offset2>,
		ComponentLayout<C3, size3, offset3>>>
{
};

using XRGB4444_Layout = RGB_16_Layout<
	ComponentType::X, 4, 12,
	ComponentType::R, 4, 8,
	ComponentType::G, 4, 4,
	ComponentType::B, 4, 0
>;

using ARGB4444_Layout = RGB_16_Layout<
	ComponentType::A, 4, 12,
	ComponentType::R, 4, 8,
	ComponentType::G, 4, 4,
	ComponentType::B, 4, 0
>;

using XRGB1555_Layout = RGB_16_Layout<
	ComponentType::X, 1, 15,
	ComponentType::R, 5, 10,
	ComponentType::G, 5, 5,
	ComponentType::B, 5, 0
>;

using ARGB1555_Layout = RGB_16_Layout<
	ComponentType::A, 1, 15,
	ComponentType::R, 5, 10,
	ComponentType::G, 5, 5,
	ComponentType::B, 5, 0
>;

using RGB565_Layout = FormatLayout<PlaneLayout<uint16_t,
	ComponentLayout<ComponentType::R, 5, 11>,
	ComponentLayout<ComponentType::G, 6, 5>,
	ComponentLayout<ComponentType::B, 5, 0>>
>;

using BGR565_Layout = FormatLayout<PlaneLayout<uint16_t,
	ComponentLayout<ComponentType::B, 5, 11>,
	ComponentLayout<ComponentType::G, 6, 5>,
	ComponentLayout<ComponentType::R, 5, 0>>
>;

using RGB888_Layout = FormatLayout<PlaneLayout<uint32_t,
	ComponentLayout<ComponentType::R, 8, 16>,
	ComponentLayout<ComponentType::G, 8, 8>,
	ComponentLayout<ComponentType::B, 8, 0>>
>;

using BGR888_Layout = FormatLayout<PlaneLayout<uint32_t,
	ComponentLayout<ComponentType::B, 8, 16>,
	ComponentLayout<ComponentType::G, 8, 8>,
	ComponentLayout<ComponentType::R, 8, 0>>
>;

template<ComponentType C3, size_t size3, size_t offset3,
	 ComponentType C2, size_t size2, size_t offset2,
	 ComponentType C1, size_t size1, size_t offset1,
	 ComponentType C0, size_t size0, size_t offset0>
struct RGB_32_Layout
	: public FormatLayout<PlaneLayout<uint32_t,
		ComponentLayout<C0, size0, offset0>,
		ComponentLayout<C1, size1, offset1>,
		ComponentLayout<C2, size2, offset2>,
		ComponentLayout<C3, size3, offset3>>>
{
};

template<ComponentType C3, ComponentType C2, ComponentType C1, ComponentType C0>
struct RGB_8_32_Layout
	: public RGB_32_Layout<
		C0, 8, 0,
		C1, 8, 8,
		C2, 8, 16,
		C3, 8, 24>
{
};

using XRGB8888_Layout = RGB_8_32_Layout<ComponentType::X, ComponentType::R, ComponentType::G, ComponentType::B>;
using ARGB8888_Layout = RGB_8_32_Layout<ComponentType::A, ComponentType::R, ComponentType::G, ComponentType::B>;

using XBGR8888_Layout = RGB_8_32_Layout<ComponentType::X, ComponentType::B, ComponentType::G, ComponentType::R>;
using ABGR8888_Layout = RGB_8_32_Layout<ComponentType::A, ComponentType::B, ComponentType::G, ComponentType::R>;

using RGBX8888_Layout = RGB_8_32_Layout<ComponentType::R, ComponentType::G, ComponentType::B, ComponentType::X>;
using RGBA8888_Layout = RGB_8_32_Layout<ComponentType::R, ComponentType::G, ComponentType::B, ComponentType::A>;

using BGRX8888_Layout = RGB_8_32_Layout<ComponentType::B, ComponentType::G, ComponentType::R, ComponentType::X>;
using BGRA8888_Layout = RGB_8_32_Layout<ComponentType::B, ComponentType::G, ComponentType::R, ComponentType::A>;

using XRGB2101010_Layout = RGB_32_Layout<
	ComponentType::X, 2, 30,
	ComponentType::R, 10, 20,
	ComponentType::G, 10, 10,
	ComponentType::B, 10, 0
>;

using ARGB2101010_Layout = RGB_32_Layout<
	ComponentType::A, 2, 30,
	ComponentType::R, 10, 20,
	ComponentType::G, 10, 10,
	ComponentType::B, 10, 0
>;

using XBGR2101010_Layout = RGB_32_Layout<
	ComponentType::X, 2, 30,
	ComponentType::B, 10, 20,
	ComponentType::G, 10, 10,
	ComponentType::R, 10, 0
>;

using ABGR2101010_Layout = RGB_32_Layout<
	ComponentType::A, 2, 30,
	ComponentType::B, 10, 20,
	ComponentType::G, 10, 10,
	ComponentType::R, 10, 0
>;

using RGBX1010102_Layout = RGB_32_Layout<
	ComponentType::R, 10, 22,
	ComponentType::G, 10, 12,
	ComponentType::B, 10, 2,
	ComponentType::X, 2, 0
>;

using RGBA1010102_Layout = RGB_32_Layout<
	ComponentType::R, 10, 22,
	ComponentType::G, 10, 12,
	ComponentType::B, 10, 2,
	ComponentType::A, 2, 0
>;

using BGRX1010102_Layout = RGB_32_Layout<
	ComponentType::B, 10, 22,
	ComponentType::G, 10, 12,
	ComponentType::R, 10, 2,
	ComponentType::X, 2, 0
>;

using BGRA1010102_Layout = RGB_32_Layout<
	ComponentType::B, 10, 22,
	ComponentType::G, 10, 12,
	ComponentType::R, 10, 2,
	ComponentType::A, 2, 0
>;

template<typename Layout>
class ARGB_Writer
{
	using Plane = Layout::template plane<0>;
	using TStorage = Plane::storage_type;

	static_assert(Layout::num_planes == 1);
	static_assert(Plane::num_components == 3 || Plane::num_components == 4);

	static_assert(Plane::template component_count<ComponentType::R>() == 1);
	static_assert(Plane::template component_count<ComponentType::G>() == 1);
	static_assert(Plane::template component_count<ComponentType::B>() == 1);

	static constexpr bool has_alpha = Plane::template component_count<ComponentType::A>();
	static constexpr bool has_padding = Plane::template component_count<ComponentType::X>();

	static constexpr bool needs_packed_access = Plane::total_bits != Plane::storage_bits;

	static constexpr size_t a_idx = Plane::template find_pos<ComponentType::A>();
	static constexpr size_t x_idx = Plane::template find_pos<ComponentType::X>();
	static constexpr size_t r_idx = Plane::template find_pos<ComponentType::R>();
	static constexpr size_t g_idx = Plane::template find_pos<ComponentType::G>();
	static constexpr size_t b_idx = Plane::template find_pos<ComponentType::B>();

	static constexpr size_t a_shift = has_alpha ? 16 - Plane::template component_size<a_idx> : 0;
	static constexpr size_t r_shift = 16 - Plane::template component_size<r_idx>;
	static constexpr size_t g_shift = 16 - Plane::template component_size<g_idx>;
	static constexpr size_t b_shift = 16 - Plane::template component_size<b_idx>;

	static_assert(Plane::total_bits % 8 == 0);
	static constexpr size_t bytes_per_pixel = Plane::total_bits / 8;

public:
	// Pack and write num_pixels pixels from src_line to dst_line
	static void pack_line(HasIndexOperatorReturning<TStorage> auto&& dst_line,
			      HasIndexOperatorReturning<RGB16> auto&& src_line,
			      size_t num_pixels)
	{
		for (size_t x = 0; x < num_pixels; x++) {
			const RGB16& pix = src_line[x];

			std::array<component_storage_type, Plane::num_components>
				components;

			if constexpr (has_alpha)
				components[a_idx] = pix.a >> a_shift;

			if constexpr (has_padding)
				components[x_idx] = 0;

			components[r_idx] = pix.r >> r_shift;
			components[g_idx] = pix.g >> g_shift;
			components[b_idx] = pix.b >> b_shift;

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
	static void unpack_line(HasIndexOperatorReturning<RGB16> auto&& dst_line,
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

			dst_line[x] = RGB16 {
				static_cast<uint16_t>(components[r_idx] << r_shift),
				static_cast<uint16_t>(components[g_idx] << g_shift),
				static_cast<uint16_t>(components[b_idx] << b_shift),
				static_cast<uint16_t>(has_alpha ? components[a_idx] << a_shift : 0),
			};
		}
	}

	static void write_pattern(IFramebuffer& fb, size_t start_y, size_t end_y,
				  auto&& generate_line)
	{
		std::vector<RGB16> linebuf(fb.width());

		// View to the plane
		auto view = make_strided_fb_view<TStorage>(fb.map(0), fb.height(), fb.width(),
							   fb.stride(0));

		for (size_t y_src = start_y; y_src <= end_y; y_src++) {
			generate_line(y_src, linebuf);

			auto dst = md::submdspan(view, y_src, md::full_extent);

			pack_line(dst, linebuf, fb.width());
		}
	}

	static void get_line(IFramebuffer& fb, size_t w, size_t h, size_t row, std::span<RGB16> linebuf)
	{
		auto view = make_strided_fb_view<const TStorage>(fb.map(0), fb.height(), fb.width(),
								 fb.stride(0));

		auto src = md::submdspan(view, row, md::full_extent);

		unpack_line(linebuf, src);
	}
};

} // namespace kms
