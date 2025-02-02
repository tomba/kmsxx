#pragma once

#include <vector>

#include <kms++/framebuffer.h>
#include <kms++util/color16.h>

#include "conv-common.h"

namespace kms
{
/* YUV Planar */

template<ComponentType P1, ComponentType P2, size_t HSubUV = 1, size_t VSubUV = 1>
class YUV_Planar_Layout
	: public FormatLayout<
		PlaneLayout<uint8_t, ComponentLayout<ComponentType::Y, 8, 0>>,
		PlaneLayout<uint8_t, ComponentLayout<P1, 8, 0>>,
		PlaneLayout<uint8_t, ComponentLayout<P2, 8, 0>>
	>
{
public:
	static constexpr std::array<ComponentType, 2> uv_order = { P1, P2 };
	static constexpr size_t h_sub = HSubUV;
	static constexpr size_t v_sub = VSubUV;

	template<ComponentType P>
	static constexpr size_t find_plane()
	{
		return 1 + (std::ranges::find(uv_order, P) - uv_order.begin());
	}

	static constexpr size_t y_plane = 0; // Y is always plane 0
	static constexpr size_t cb_plane = find_plane<ComponentType::Cb>();
	static constexpr size_t cr_plane = find_plane<ComponentType::Cr>();
};

using YUV444_Layout = YUV_Planar_Layout<ComponentType::Cb, ComponentType::Cr>;
using YVU444_Layout = YUV_Planar_Layout<ComponentType::Cr, ComponentType::Cb>;
using YUV422_Layout = YUV_Planar_Layout<ComponentType::Cb, ComponentType::Cr, 2, 1>;
using YVU422_Layout = YUV_Planar_Layout<ComponentType::Cr, ComponentType::Cb, 2, 1>;
using YUV420_Layout = YUV_Planar_Layout<ComponentType::Cb, ComponentType::Cr, 2, 2>;
using YVU420_Layout = YUV_Planar_Layout<ComponentType::Cr, ComponentType::Cb, 2, 2>;

template<typename Format>
class YUVPlanarWriter
{
	using YLayout = Format::template plane<Format::y_plane>;
	using CbLayout = Format::template plane<Format::cb_plane>;
	using CrLayout = Format::template plane<Format::cr_plane>;

	using TY = typename YLayout::storage_type;
	using TCb = typename CbLayout::storage_type;
	using TCr = typename CrLayout::storage_type;
public:
	static void write_pattern(IFramebuffer& fb, size_t start_y, size_t end_y,
				  auto&& generate_line)
	{
		assert(start_y % Format::v_sub == 0);
		assert((end_y + 1) % Format::v_sub == 0);

		// Line buffers
		std::vector<YUV16> linebuf_storage(fb.width() * Format::v_sub);
		auto linebuf = md::mdspan(linebuf_storage.data(), Format::v_sub, fb.width());

		// Views to all planes
		auto y_buf = make_strided_fb_view<TY>(fb.map(Format::y_plane), fb.height(),
						      fb.width(), fb.stride(Format::y_plane));

		auto cb_buf = make_strided_fb_view<TCb>(fb.map(Format::cb_plane),
							fb.height() / Format::v_sub,
							fb.width() / Format::h_sub,
							fb.stride(Format::cb_plane));

		auto cr_buf = make_strided_fb_view<TCr>(fb.map(Format::cr_plane),
							fb.height() / Format::v_sub,
							fb.width() / Format::h_sub,
							fb.stride(Format::cr_plane));

		for (size_t y_src = start_y; y_src <= end_y; y_src++) {
			size_t y_offset = y_src % Format::v_sub;

			if (y_offset == 0) {
				// Fill line buffers
				for (size_t buf_y = 0; buf_y < Format::v_sub; buf_y++) {
					auto line = md::submdspan(linebuf, buf_y, md::full_extent);
					std::span<YUV16> span(line.data_handle(), line.size());
					generate_line(y_src + buf_y, span);
				}
			}

			// Write Y plane
			write_y_line(y_buf, y_src, linebuf, y_offset);

			// Write Cb/Cr planes if we're at a subsampling boundary
			if (y_offset == 0)
				write_uv_line(cb_buf, cr_buf, linebuf, y_src);
		}
	}

	static void write_lines(IFramebuffer& fb, size_t start_y, size_t end_y,
				Is2Dspan<YUV16> auto&& lines)
	{
		const size_t height = end_y - start_y + 1;

		if (lines.extent(0) < height || lines.extent(1) < fb.width())
			throw std::invalid_argument("Source line buffer too small");

		assert(start_y % Format::v_sub == 0);
		assert((end_y + 1) % Format::v_sub == 0);

		// Views to all planes
		auto y_buf = make_strided_fb_view<TY>(fb.map(Format::y_plane), fb.height(),
						      fb.width(), fb.stride(Format::y_plane));

		auto cb_buf = make_strided_fb_view<TCb>(fb.map(Format::cb_plane),
							fb.height() / Format::v_sub,
							fb.width() / Format::h_sub,
							fb.stride(Format::cb_plane));

		auto cr_buf = make_strided_fb_view<TCr>(fb.map(Format::cr_plane),
							fb.height() / Format::v_sub,
							fb.width() / Format::h_sub,
							fb.stride(Format::cr_plane));

		for (size_t y_src = start_y; y_src <= end_y; y_src++) {
			size_t y_offset = y_src % Format::v_sub;

			// Write Y plane
			write_y_line(y_buf, y_src, lines, y_offset);

			// Write Cb/Cr planes if we're at a subsampling boundary
			if (y_offset == 0)
				write_uv_line(cb_buf, cr_buf, lines, y_src);
		}
	}

private:
	template<typename YBuf>
	static void write_y_line(YBuf& y_buf, size_t y_src, auto& linebuf, size_t y_offset)
	{
		for (size_t x = 0; x < linebuf.extent(1); x++)
			y_buf(y_src, x) = YLayout::pack(linebuf(y_offset, x).y >> 8);
	}

	template<typename UVBuf>
	static void write_uv_line(UVBuf& cb_buf, UVBuf& cr_buf, auto& linebuf, size_t y_src)
	{
		const size_t uv_y = y_src / Format::v_sub;

		for (size_t x = 0; x < linebuf.extent(1); x += Format::h_sub) {
			// Average subsampled region
			uint32_t u_sum = 0, v_sum = 0;
			for (size_t y = 0; y < Format::v_sub; y++) {
				for (size_t x_off = 0; x_off < Format::h_sub; x_off++) {
					u_sum += linebuf(y, x + x_off).u;
					v_sum += linebuf(y, x + x_off).v;
				}
			}

			const size_t total_samples = Format::h_sub * Format::v_sub;
			const size_t uv_x = x / Format::h_sub;

			cb_buf(uv_y, uv_x) = CbLayout::pack(u_sum / total_samples >> 8);
			cr_buf(uv_y, uv_x) = CrLayout::pack(v_sum / total_samples >> 8);
		}
	}

public:
	static void read_lines(IFramebuffer& fb, size_t start_y, size_t end_y,
			       Is2Dspan<YUV16> auto&& dest)
	{
		const size_t height = end_y - start_y + 1;

		if (dest.extent(0) < height || dest.extent(1) < fb.width())
			throw std::invalid_argument("Destination line buffer too small");

		assert(start_y % Format::v_sub == 0);
		assert((end_y + 1) % Format::v_sub == 0);

		auto y_buf = make_strided_fb_view<const TY>(fb.map(Format::y_plane), fb.height(),
							    fb.width(), fb.stride(Format::y_plane));

		auto cb_buf = make_strided_fb_view<const TCb>(fb.map(Format::cb_plane),
							      fb.height() / Format::v_sub,
							      fb.width() / Format::h_sub,
							      fb.stride(Format::cb_plane));

		auto cr_buf = make_strided_fb_view<const TCr>(fb.map(Format::cr_plane),
							      fb.height() / Format::v_sub,
							      fb.width() / Format::h_sub,
							      fb.stride(Format::cr_plane));

		for (size_t y = start_y; y <= end_y; y++) {
			for (size_t x = 0; x < fb.width(); x++) {
				const auto y_val = YLayout::unpack(y_buf(y, x))[0];

				const size_t uv_y = y / Format::v_sub;
				const size_t uv_x = x / Format::h_sub;

				const auto u_val = CbLayout::unpack(cb_buf(uv_y, uv_x))[0];
				const auto v_val = CrLayout::unpack(cr_buf(uv_y, uv_x))[0];

				dest(y - start_y, x) = YUV16 {
					static_cast<uint16_t>(y_val << 8),
					static_cast<uint16_t>(u_val << 8),
					static_cast<uint16_t>(v_val << 8)
				};
			}
		}
	}
};

} // namespace kms
