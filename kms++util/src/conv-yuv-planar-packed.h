#pragma once

#include <vector>

#include <kms++/framebuffer.h>
#include <kms++util/color16.h>

#include "conv-common.h"

namespace kms
{
/* YUV Planar Packed (only X403 for now) */

struct X403_Layout : FormatLayout <
	PlaneLayout<uint32_t,
		ComponentLayout<ComponentType::Y, 10, 0>,
		ComponentLayout<ComponentType::Y, 10, 10>,
		ComponentLayout<ComponentType::Y, 10, 20>,
		ComponentLayout<ComponentType::X, 2, 30>>,
	PlaneLayout<uint32_t,
		ComponentLayout<ComponentType::Cb, 10, 0>,
		ComponentLayout<ComponentType::Cb, 10, 10>,
		ComponentLayout<ComponentType::Cb, 10, 20>,
		ComponentLayout<ComponentType::X, 2, 30>>,
	PlaneLayout<uint32_t,
		ComponentLayout<ComponentType::Cr, 10, 0>,
		ComponentLayout<ComponentType::Cr, 10, 10>,
		ComponentLayout<ComponentType::Cr, 10, 20>,
		ComponentLayout<ComponentType::X, 2, 30>>
	>
{
	static constexpr size_t y_plane = 0;
	static constexpr size_t cb_plane = 1;
	static constexpr size_t cr_plane = 2;

};

template<typename Format>
class YUVPlanarPackedWriter
{
	using YLayout = typename Format::template plane<Format::y_plane>;
	using CbLayout = typename Format::template plane<Format::cb_plane>;
	using CrLayout = typename Format::template plane<Format::cr_plane>;

	using TY = typename YLayout::storage_type;
	using TCb = typename CbLayout::storage_type;
	using TCr = typename CrLayout::storage_type;

	static constexpr size_t pixels_in_group = 3;

	// Helper to extract the correct component based on component type
	template<ComponentType CType>
	static uint16_t extract_component(const YUV16& pixel)
	{
		if constexpr (CType == ComponentType::Y)
			return pixel.y;
		else if constexpr (CType == ComponentType::Cb)
			return pixel.u;
		else if constexpr (CType == ComponentType::Cr)
			return pixel.v;
		else
			return 0; // For padding or other component types
	}

public:
	static void write_pattern(IFramebuffer& fb, size_t start_y, size_t end_y,
				  auto&& generate_line)
	{
		// Line buffers
		std::vector<YUV16> linebuf(fb.width());

		// Views to all planes
		auto y_buf = make_strided_fb_view<TY>(fb.map(Format::y_plane),
						      fb.height(), fb.width(),
						      fb.stride(Format::y_plane));

		auto cb_buf = make_strided_fb_view<TCb>(fb.map(Format::cb_plane),
							fb.height(), fb.width(),
							fb.stride(Format::cb_plane));

		auto cr_buf = make_strided_fb_view<TCr>(fb.map(Format::cr_plane),
							fb.height(), fb.width(),
							fb.stride(Format::cr_plane));

		for (size_t y_src = start_y; y_src <= end_y; y_src++) {
			generate_line(y_src, linebuf);

			write_samples<YLayout, ComponentType::Y>(md::submdspan(y_buf, y_src, md::full_extent), linebuf, fb.width());
			write_samples<CbLayout, ComponentType::Cb>(md::submdspan(cb_buf, y_src, md::full_extent), linebuf, fb.width());
			write_samples<CrLayout, ComponentType::Cr>(md::submdspan(cr_buf, y_src, md::full_extent), linebuf, fb.width());
		}
	}


private:
	template<typename Plane, ComponentType CType, typename Buf>
	static void write_samples(Buf&& view, auto&& linebuf, size_t num_pixels)
	{
		for (size_t x_src = 0; x_src < num_pixels; x_src += pixels_in_group) {
			auto x_dst = x_src / pixels_in_group;

			write_group<Plane, CType>(view, linebuf, x_src, x_dst,
				std::make_index_sequence<pixels_in_group>{});
		}
	}

	template<typename Plane, ComponentType CType, typename YBuf, size_t... I>
	static void write_group(YBuf&& view, auto&& linebuf, size_t x_src,
				size_t x_dst, std::index_sequence<I...>)
	{
		std::array<component_storage_type, Plane::num_components> values{
			static_cast<component_storage_type>(
				(extract_component<CType>(linebuf[x_src + I]) >>
				(16 - Plane::template component_size<I>)))...
		};

		// Set padding bits to 0
		if constexpr (Plane::template component_count<ComponentType::X>())
		       values[Plane::template find_pos<ComponentType::X>()] = 0;

		view[x_dst] = Plane::pack(values);
	}
};

} // namespace kms
