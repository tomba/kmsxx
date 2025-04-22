#pragma once

#include <vector>

#include <kms++/framebuffer.h>
#include <kms++util/color16.h>

#include "conv-common.h"

namespace kms
{

/* Semiplanar YUV */

template<ComponentType C0, ComponentType C1, size_t HSub, size_t VSub>
struct NV12_Family_Layout
	: public FormatLayout<
		PlaneLayout<uint8_t,
			ComponentLayout<ComponentType::Y, 8, 0>
		>,
		PlaneLayout<uint16_t,
			ComponentLayout<C0, 8, 0>,
			ComponentLayout<C1, 8, 8>
		>
	>
{
	static constexpr size_t h_sub = HSub;
	static constexpr size_t v_sub = VSub;
};

using NV12_Layout = NV12_Family_Layout<ComponentType::Cb, ComponentType::Cr, 2, 2>;
using NV21_Layout = NV12_Family_Layout<ComponentType::Cr, ComponentType::Cb, 2, 2>;

using NV16_Layout = NV12_Family_Layout<ComponentType::Cb, ComponentType::Cr, 2, 1>;
using NV61_Layout = NV12_Family_Layout<ComponentType::Cr, ComponentType::Cb, 2, 1>;

template<size_t HSub, size_t VSub>
struct XV15_Family_Layout
	: public FormatLayout<
		PlaneLayout<uint32_t,
		    ComponentLayout<ComponentType::Y, 10, 0>,
		    ComponentLayout<ComponentType::Y, 10, 10>,
		    ComponentLayout<ComponentType::Y, 10, 20>
		>,
		PlaneLayout<uint64_t,
		    ComponentLayout<ComponentType::Cb, 10, 0>,
		    ComponentLayout<ComponentType::Cr, 10, 10>,
		    ComponentLayout<ComponentType::Cb, 10, 20>,
		    ComponentLayout<ComponentType::Cr, 10, 32>,
		    ComponentLayout<ComponentType::Cb, 10, 42>,
		    ComponentLayout<ComponentType::Cr, 10, 52>
		>
	>
{
	static constexpr size_t h_sub = HSub;
	static constexpr size_t v_sub = VSub;
};

using XV15_Layout = XV15_Family_Layout<2, 2>;
using XV20_Layout = XV15_Family_Layout<2, 1>;

template<size_t HSub, size_t VSub>
struct SubsampleHelper {
	template<typename View> static constexpr auto subsample(const View& view, size_t group_idx)
	{
		uint32_t sum = 0;

		static_for<0, HSub>([&](auto x) { sum += view(0, group_idx + x); });

		if constexpr (VSub > 1) {
			static_for<1, VSub>([&](auto y) {
				static_for<0, HSub>([&](auto x) { sum += view(y, group_idx + x); });
			});
		}

		return sum / (HSub * VSub);
	}
};

template<typename Layout>
class YUVSemiPlanarWriter
{
	static_assert(Layout::num_planes == 2);

	static constexpr size_t h_sub = Layout::h_sub;
	static constexpr size_t v_sub = Layout::v_sub;

	using YLayout = typename Layout::template plane<0>;
	using UVLayout = typename Layout::template plane<1>;

	using TY = typename YLayout::storage_type;
	using TCrCb = typename UVLayout::storage_type;

	static constexpr size_t pixels_in_group = YLayout::template component_count<ComponentType::Y>();
	static_assert(pixels_in_group == UVLayout::template component_count<ComponentType::Cb>());
	static_assert(pixels_in_group == UVLayout::template component_count<ComponentType::Cr>());

public:
	static void write_pattern(IFramebuffer& fb, size_t start_y, size_t end_y,
				  auto&& generate_line)
	{
		assert(start_y % v_sub == 0);
		assert((end_y + 1) % v_sub == 0);
		if (fb.width() % pixels_in_group != 0)
			throw std::invalid_argument("FB width doesn't align to pixel format");

		// Line buffers
		std::vector<YUV16> linebuf_storage(fb.width() * v_sub);
		auto linebuf = md::mdspan(linebuf_storage.data(), v_sub, fb.width());

		// Views to the planes
		auto y_view = make_strided_fb_view<TY>(fb.map(0), fb.height(),
						       fb.width() / pixels_in_group, fb.stride(0));

		auto uv_view = make_strided_fb_view<TCrCb>(fb.map(1), fb.height() / v_sub,
							   fb.width() / pixels_in_group / h_sub,
							   fb.stride(1));

		for (size_t y_src = start_y; y_src <= end_y; y_src++) {
			size_t y_offset = y_src % v_sub;

			if (y_offset == 0) {
				// Fill line buffers
				for (size_t y_offset = 0; y_offset < v_sub; y_offset++) {
					auto line = md::submdspan(linebuf, y_offset, md::full_extent);
					std::span<YUV16> span(line.data_handle(), line.size());
					generate_line(y_src + y_offset, span);
				}
			}

			// Write Y values from the line buffer
			write_y_samples(md::submdspan(y_view, y_src, md::full_extent),
					md::submdspan(linebuf, y_offset, md::full_extent));

			if (y_offset == 0) {
				// Write UV values from the line buffers
				write_uv_samples(uv_view, linebuf, y_src);
			}
		}
	}

private:

	template<typename YBuf>
	static void write_y_samples(YBuf&& y_view, auto&& linebuf)
	{
		for (size_t x_src = 0; x_src < linebuf.extent(1); x_src += pixels_in_group) {
			auto x_dst = x_src / pixels_in_group;

			write_y_group(y_view, linebuf, x_src, x_dst,
				      std::make_index_sequence<pixels_in_group>{});
		}
	}

	template<typename YBuf, size_t... I>
	static void write_y_group(YBuf&& y_view, auto&& linebuf, size_t x_src,
				 size_t x_dst, std::index_sequence<I...>)
	{
		std::array<component_storage_type, YLayout::num_components> y_values{
			static_cast<component_storage_type>((linebuf(x_src + I).y >> (16 - YLayout::template component_size<I>)))...
		};

		y_view(x_dst) = YLayout::pack(y_values);
	}

	template<typename UVBuf>
	static void write_uv_samples(UVBuf& uv_view, auto& linebuf, size_t y_src)
	{
		for (size_t x_src = 0; x_src < linebuf.extent(1); x_src += pixels_in_group * h_sub) {
			const size_t y_offset = 0;
			auto y_dst = (y_src + y_offset) / v_sub;
			auto x_dst = x_src / (pixels_in_group * h_sub);

			auto group_view = md::submdspan(linebuf, std::tuple(y_offset, y_offset + v_sub),
						   std::tuple(x_src, x_src + h_sub * pixels_in_group));

			write_uv_group(uv_view, group_view, y_dst, x_dst,
				       std::make_index_sequence<pixels_in_group>{});
		}
	}

	template<typename UVBuf, size_t... I>
	static void write_uv_group(UVBuf& uv_view, auto& group_view, size_t y_dst, size_t x_dst,
				   std::index_sequence<I...>)
	{
		std::array<component_storage_type, UVLayout::num_components> uv_values;

		(
			[&]<size_t i>() {
				constexpr size_t group_idx = i * h_sub;

				constexpr size_t u_idx = UVLayout::template find_nth_pos<ComponentType::Cb>(i);
				constexpr size_t v_idx = UVLayout::template find_nth_pos<ComponentType::Cr>(i);

				auto u = SubsampleHelper<h_sub, v_sub>::subsample(
					[&group_view](size_t y, size_t x) { return group_view(y, x).u; },
					group_idx);

				auto v = SubsampleHelper<h_sub, v_sub>::subsample(
					[&group_view](size_t y, size_t x) { return group_view(y, x).v; },
					group_idx);

				uv_values[u_idx] =
					u >> (16 - UVLayout::template component_size<u_idx>);
				uv_values[v_idx] =
					v >> (16 - UVLayout::template component_size<v_idx>);
			}.template operator()<I>(),
			...);

		uv_view(y_dst, x_dst) = UVLayout::pack(uv_values);
	}
};

} // namespace kms
