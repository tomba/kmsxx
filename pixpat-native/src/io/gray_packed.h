#pragma once

// MIPI CSI-2 packed grayscale (Y10P / Y12P). Same byte packing as
// Bayer10P/Bayer12P (see io/csi2.h) but every sample is Y; the source
// emits neutral chroma to keep cross-color-kind ColorXfm consistent
// with GraySource.
//
// The Layout slot is a placeholder (matches the unpacked Y8 storage
// shape so dispatch plumbing is uniform); bytes_per_pixel from the
// Plane is unused.

#include <array>
#include <cstdint>

#include "../layout.h"
#include "csi2.h"

namespace pixpat
{

template <typename L, size_t BitDepth>
struct GrayPackedSource {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 1);
	static_assert(BitDepth == 10 || BitDepth == 12);

	using Traits = detail::csi2::packed_traits<BitDepth>;
	static constexpr size_t ppg = Traits::ppg;
	static constexpr size_t bpg = Traits::bpg;
	static constexpr unsigned shift = 16 - BitDepth;

	static YUV16 read(const Buffer<1>& buf, size_t x, size_t y,
	                  [[maybe_unused]] size_t W,
	                  [[maybe_unused]] size_t H) noexcept
	{
		const uint8_t* src = buf.data[0] + y * buf.stride[0]
		                     + (x / ppg) * bpg;
		const uint16_t val = detail::csi2::unpack_sample<BitDepth>(src, x % ppg);
		return YUV16{
		        uint16_t(val << shift),
		        0x8000, 0x8000, uint16_t(0),
		};
	}
};

template <typename L, size_t BitDepth>
struct GrayPackedSink {
	using Layout = L;
	using Pixel  = YUV16;

	static_assert(L::kind == ColorKind::YUV);
	static_assert(L::num_planes == 1);
	static_assert(BitDepth == 10 || BitDepth == 12);

	using Traits = detail::csi2::packed_traits<BitDepth>;
	static constexpr size_t ppg = Traits::ppg;
	static constexpr size_t bpg = Traits::bpg;

	static constexpr size_t block_h = 1;
	static constexpr size_t block_w = ppg;

	static void write_block(Buffer<1>& buf, size_t bx, size_t by,
	                        const YUV16 (&block)[1][ppg]) noexcept
	{
		std::array<uint16_t, ppg> vals{};
		for (size_t i = 0; i < ppg; ++i)
			vals[i] = uint16_t(block[0][i].y >> (16 - BitDepth));

		uint8_t* dst = buf.data[0] + by * buf.stride[0]
		               + (bx / ppg) * bpg;
		detail::csi2::pack_group<BitDepth>(dst, vals);
	}
};

} // namespace pixpat
