#pragma once

#include <cstddef>

#include "color.h"
#include "layout.h"

// Inlined source → color → sink composition. The intermediate Pixel
// values stay in registers across stages; there is no normalized RGB16
// or YUV16 buffer between source and sink. Block size is dictated by
// the sink: 1x1 for non-subsampled formats, h_sub × v_sub for chroma-
// subsampled ones.

namespace pixpat
{

template <typename Source, typename Sink>
struct Converter {
	using Xfm = ColorXfm<typename Source::Pixel, typename Sink::Pixel>;
	static constexpr size_t bh = Sink::block_h;
	static constexpr size_t bw = Sink::block_w;

	static void run(const Buffer<Source::Layout::num_planes>& src,
	                Buffer<Sink::Layout::num_planes>& dst,
	                size_t W, size_t H,
	                size_t by_start, size_t by_end,
	                ColorSpec spec) noexcept
	{
		const ColorCoeffs c = coeffs_for(spec);
		for (size_t by = by_start; by < by_end; by += bh) {
			for (size_t bx = 0; bx < W; bx += bw) {
				typename Sink::Pixel block[bh][bw];
				for (size_t dy = 0; dy < bh; ++dy)
					for (size_t dx = 0; dx < bw; ++dx)
						block[dy][dx] = Xfm::apply(
							Source::read(src, bx + dx, by + dy,
							             W, H), c);
				Sink::write_block(dst, bx, by, block);
			}
		}
	}
};

} // namespace pixpat
