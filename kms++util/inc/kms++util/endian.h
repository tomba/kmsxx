#pragma once

#include <type_traits>
#include <byteswap.h>
#include <stdint.h>

static_assert((__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) ||
		      (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__),
	      "Unable to detect endianness");

enum class endian {
	little = __ORDER_LITTLE_ENDIAN__,
	big = __ORDER_BIG_ENDIAN__,
	native = __BYTE_ORDER__
};

template<typename T>
constexpr T byteswap(T value) noexcept
{
	static_assert(std::is_integral<T>(), "Type is not integral");
	static_assert(sizeof(T) == 2 ||
			      sizeof(T) == 4 ||
			      sizeof(T) == 8,
		      "Illegal value size");

	switch (sizeof(T)) {
	case 2:
		return bswap_16(value);
	case 4:
		return bswap_32(value);
	case 8:
		return bswap_64(value);
	}
}

template<endian E, typename T>
static void write_endian(T* dst, T val)
{
	if constexpr (E != endian::native)
		val = byteswap(val);

	*dst = val;
}

[[maybe_unused]] static void write16le(uint16_t* dst, uint16_t val)
{
	write_endian<endian::little, uint16_t>(dst, val);
}
