#pragma once

// Shared MIPI CSI-2 byte (un)packing for the 10P / 12P forms used by
// Bayer raw and Y-only grayscale.
//
//   10P: 4 samples in 5 bytes — bytes 0..3 hold the high 8 bits of
//        samples 0..3; byte 4 holds 4 x 2 LSBs (sample 0 in bits 6..7,
//        sample 1 in bits 4..5, ...).
//   12P: 2 samples in 3 bytes — bytes 0..1 hold the high 8 bits of
//        samples 0..1; byte 2 holds 2 x 4 LSBs (sample 0 in bits 4..7,
//        sample 1 in bits 0..3).
//
// Helpers deal in the stored integer (low BitDepth bits set);
// normalization to/from the 16-bit pivot stays in the caller.

#include <array>
#include <cstddef>
#include <cstdint>

namespace pixpat::detail::csi2
{

template <size_t BitDepth>
struct packed_traits;

template <>
struct packed_traits<10> {
	static constexpr size_t ppg = 4;
	static constexpr size_t bpg = 5;
};

template <>
struct packed_traits<12> {
	static constexpr size_t ppg = 2;
	static constexpr size_t bpg = 3;
};

// Extract one BitDepth-bit sample from a packed group, where `i` is the
// in-group index (0..ppg-1). The returned value occupies the low
// BitDepth bits.
template <size_t BitDepth>
inline uint16_t unpack_sample(const uint8_t* src, size_t i) noexcept
{
	if constexpr (BitDepth == 10) {
		const uint8_t hi  = src[i];
		const uint8_t lsb = (src[4] >> ((3 - i) * 2)) & 0x03;
		return uint16_t((hi << 2) | lsb);
	} else { // 12
		const uint8_t hi  = src[i];
		const uint8_t lsb = (i == 0) ? ((src[2] >> 4) & 0x0F)
		                             :  (src[2]       & 0x0F);
		return uint16_t((hi << 4) | lsb);
	}
}

// Write `ppg` BitDepth-bit samples (low BitDepth bits significant) into
// a packed group of `bpg` bytes.
template <size_t BitDepth>
inline void pack_group(
	uint8_t* dst,
	const std::array<uint16_t, packed_traits<BitDepth>::ppg>& vals) noexcept
{
	if constexpr (BitDepth == 10) {
		dst[0] = (vals[0] >> 2) & 0xFF;
		dst[1] = (vals[1] >> 2) & 0xFF;
		dst[2] = (vals[2] >> 2) & 0xFF;
		dst[3] = (vals[3] >> 2) & 0xFF;
		dst[4] = ((vals[0] & 0x03) << 6)
		         | ((vals[1] & 0x03) << 4)
		         | ((vals[2] & 0x03) << 2)
		         | ((vals[3] & 0x03) << 0);
	} else { // 12
		dst[0] = (vals[0] >> 4) & 0xFF;
		dst[1] = (vals[1] >> 4) & 0xFF;
		dst[2] = ((vals[0] & 0x0F) << 4)
		         | ((vals[1] & 0x0F) << 0);
	}
}

} // namespace pixpat::detail::csi2
