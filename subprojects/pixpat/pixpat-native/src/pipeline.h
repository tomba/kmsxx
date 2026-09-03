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

// A sink whose block layout depends on the row parity (Bayer: the
// colour a pixel carries flips between even and odd rows) opts in with
// `static constexpr bool split_row_parity = true` and provides
// `write_block_parity<y_even>` instead of `write_block`. The row loops
// (here and pack_from_norm) then branch on `by & 1` once per row, so
// the parity is a constant for the whole row loop. A branch per block
// inside the sink is not enough: clang-19 folds the two arms back into
// a select on the field offset and the loop never gets unswitched.
template <typename Sink>
concept SplitsRowParity =
	requires { Sink::split_row_parity; } && Sink::split_row_parity;

template <typename Source, typename Sink>
struct Converter {
	using Xfm = ColorXfm<typename Source::Pixel, typename Sink::Pixel>;
	static constexpr size_t bh = Sink::block_h;
	static constexpr size_t bw = Sink::block_w;

	// One row for a SplitsRowParity sink, parity as a constant. The
	// descriptors and coefficients are taken by value: this is not
	// always inlined, and through a reference the pixel stores could
	// alias them and pin every load in the loop.
	template <bool y_even>
	static void run_row(Buffer<Source::Layout::num_planes> src,
	                    Buffer<Sink::Layout::num_planes> dst,
	                    size_t W, size_t H, size_t by,
	                    ColorCoeffs c) noexcept
	{
		for (size_t bx = 0; bx < W; bx += bw) {
			typename Sink::Pixel block[bh][bw];
			for (size_t dy = 0; dy < bh; ++dy)
				for (size_t dx = 0; dx < bw; ++dx)
					block[dy][dx] = Xfm::apply(
						Source::read(src, bx + dx, by + dy,
						             W, H), c);
			Sink::template write_block_parity<y_even>(
				dst, bx, by, block);
		}
	}

	static void run(const Buffer<Source::Layout::num_planes>& src,
	                Buffer<Sink::Layout::num_planes>& dst,
	                size_t W, size_t H,
	                size_t by_start, size_t by_end,
	                ColorSpec spec) noexcept
	{
		const ColorCoeffs c = coeffs_for(spec);
		if constexpr (SplitsRowParity<Sink>) {
			for (size_t by = by_start; by < by_end; by += bh) {
				if ((by & 1) == 0)
					run_row<true>(src, dst, W, H, by, c);
				else
					run_row<false>(src, dst, W, H, by, c);
			}
		} else {
			// Every other sink keeps this loop exactly as it is:
			// routing it through run_row too changed clang's
			// inlining and vectorization decisions across the whole
			// catalog (+19-31% .text, YUV -> BGR888 halved).
			for (size_t by = by_start; by < by_end; by += bh) {
				for (size_t bx = 0; bx < W; bx += bw) {
					typename Sink::Pixel block[bh][bw];
					for (size_t dy = 0; dy < bh; ++dy)
						for (size_t dx = 0; dx < bw; ++dx)
							block[dy][dx] = Xfm::apply(
								Source::read(src, bx + dx,
								             by + dy, W, H), c);
					Sink::write_block(dst, bx, by, block);
				}
			}
		}
	}
};

} // namespace pixpat
