#pragma once

// Per-component encode/decode against the descriptor + memcpy-based
// load/store_word helpers. Shared by every Source / Sink template.

#include <cstdint>
#include <cstring>

#include "../layout.h"

namespace pixpat::detail
{

// Decode an N-bit stored value into the 16-bit normalized space and
// encode it back. Decode bit-replicates the stored value across the 16
// bits so that N-bit max maps to normalized max (e.g. 8-bit 0xFF →
// 0xFFFF, not 0xFF00). Encode is a plain truncating right-shift: the
// replicated bits land in the low (16-N) bits and get dropped, so
// stored→norm→stored is exact for any N in [1, 16].
//
// `bits` is taken at runtime; in every call site it traces back to a
// constexpr Plane::comps[I].bits read, which the optimizer constant-
// folds after inlining.

constexpr uint16_t decode_norm(unsigned bits, uint16_t stored) noexcept
{
	const int N = int(bits);
	// Loop, not a single OR: one replication only covers 2N bits, so
	// N < 8 (RGB565, RGBA4444, 1-bit alpha, ...) needs multiple tiles.
	uint32_t result = 0;
	for (int s = 16 - N; s > -N; s -= N) {
		if (s >= 0)
			result |= uint32_t(stored) << s;
		else
			result |= uint32_t(stored) >> -s;
	}
	return uint16_t(result);
}

constexpr uint16_t encode_norm(unsigned bits, uint16_t norm) noexcept
{
	return uint16_t(norm >> (16u - bits));
}

// Read one storage word from `p`. memcpy is uniform for tight and
// non-tight (e.g. BGR888 24-bit) layouts; the optimizer folds it to a
// single load when the size is constant.
template <typename Plane>
inline typename Plane::storage_t load_word(const uint8_t* p) noexcept
{
	typename Plane::storage_t word{};
	std::memcpy(&word, p, Plane::bytes_per_pixel);
	return word;
}

template <typename Plane>
inline void store_word(uint8_t* p, typename Plane::storage_t word) noexcept
{
	std::memcpy(p, &word, Plane::bytes_per_pixel);
}

} // namespace pixpat::detail
