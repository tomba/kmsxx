#pragma once

#include <vector>

#include <kms++/framebuffer.h>
#include <kms++util/color16.h>

#include "conv-common.h"

namespace kms
{

/* YUV Packed */

template<ComponentType C0, ComponentType C1, ComponentType C2, ComponentType C3>
struct YUV_Packed_Format
	: public FormatLayout<
		PlaneLayout<uint32_t,
			ComponentLayout<C0, 8, 0>,
			ComponentLayout<C1, 8, 8>,
			ComponentLayout<C2, 8, 16>,
			ComponentLayout<C3, 8, 24>
		>
	>
{
};

// Define common packed YUV formats
using YUYV_Layout = YUV_Packed_Format<ComponentType::Y0, ComponentType::Cb,
				      ComponentType::Y1, ComponentType::Cr>;

using YVYU_Layout = YUV_Packed_Format<ComponentType::Y0, ComponentType::Cr,
				      ComponentType::Y1, ComponentType::Cb>;

using UYVY_Layout = YUV_Packed_Format<ComponentType::Cb, ComponentType::Y0,
				      ComponentType::Cr, ComponentType::Y1>;

using VYUY_Layout = YUV_Packed_Format<ComponentType::Cr, ComponentType::Y0,
				      ComponentType::Cb, ComponentType::Y1>;

template<typename Layout>
class YUVPackedWriter
{
	using Plane = typename Layout::template plane<0>;
	using TStorage = typename Plane::storage_type;

	static constexpr size_t y0_pos = Plane::template find_pos<ComponentType::Y0>();
	static constexpr size_t y1_pos = Plane::template find_pos<ComponentType::Y1>();
	static constexpr size_t cb_pos = Plane::template find_pos<ComponentType::Cb>();
	static constexpr size_t cr_pos = Plane::template find_pos<ComponentType::Cr>();

public:
	static void write_pattern(IFramebuffer& fb, size_t start_y, size_t end_y,
				  auto&& generate_line)
	{
		std::vector<YUV16> linebuf(fb.width());

		auto view = make_strided_fb_view<TStorage>(fb.map(0), fb.height(),
							   fb.width() / 2, // Two pixels per storage unit
							   fb.stride(0));

		for (size_t y = start_y; y <= end_y; y++) {
			generate_line(y, linebuf);

			for (size_t x = 0; x < fb.width(); x += 2) {
				// Get two pixels
				const YUV16& pix0 = linebuf[x];
				const YUV16& pix1 = linebuf[x + 1];

				std::array<uint8_t, 4> components;

				components[y0_pos] = pix0.y >> 8;
				components[y1_pos] = pix1.y >> 8;
				components[cb_pos] = ((pix0.u + pix1.u) / 2) >> 8;
				components[cr_pos] = ((pix0.v + pix1.v) / 2) >> 8;

				view(y, x / 2) = Plane::pack(components);
			}
		}
	}
};

} // namespace kms
