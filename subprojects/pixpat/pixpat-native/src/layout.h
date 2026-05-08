#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

namespace pixpat
{

enum class ColorKind { RGB, YUV };

// Normalized inter-stage pixel types. 16-bit-per-component integer.
// N-bit storage values bit-replicate up to normalized 16-bit (so
// N-bit max maps to 0xFFFF); encoding back is a truncating
// `normalized >> (16 - N)`. See io/detail.h for the round-trip
// argument. Sources without an A component emit a=0; cross-color-kind
// ColorXfm resets a=0xFFFF; sinks with X write 0, sinks with A
// encode `a`.
struct RGB16 {
	static constexpr ColorKind kind = ColorKind::RGB;
	uint16_t r, g, b, a;
};

struct YUV16 {
	static constexpr ColorKind kind = ColorKind::YUV;
	uint16_t y, u, v, a;
};

inline constexpr uint16_t kNormMax = 0xFFFF;

enum class C : uint8_t { X, A, R, G, B, Y, U, V };

struct Comp {
	C c;
	uint8_t bits;
	uint8_t shift;
};

template <typename Storage, Comp... Cs>
struct Plane {
	using storage_t = Storage;

	static constexpr size_t num_comps = sizeof...(Cs);
	static constexpr std::array<Comp, num_comps> comps{ Cs ... };
	static constexpr size_t total_bits = (size_t(Cs.bits) + ... + 0);
	static constexpr size_t storage_bits = sizeof(Storage) * 8;
	static constexpr size_t bytes_per_pixel = (total_bits + 7) / 8;

	static_assert(total_bits <= storage_bits, "components overflow storage word");

	// Index of the n-th component matching Tag, or num_comps if absent.
	template <C Tag>
	static constexpr size_t find_pos(size_t n = 0)
	{
		for (size_t i = 0; i < num_comps; ++i) {
			if (comps[i].c == Tag) {
				if (n == 0)
					return i;
				--n;
			}
		}
		return num_comps;
	}

	// Count of components matching Tag. Used to derive
	// pixels_per_word for multi-pixel-per-storage formats (XYYY2101010,
	// P030, ...).
	template <C Tag>
	static constexpr size_t component_count()
	{
		size_t cnt = 0;
		for (size_t i = 0; i < num_comps; ++i)
			if (comps[i].c == Tag)
				++cnt;
		return cnt;
	}

	// Mask each input value to its bit-width and OR-shift it into the
	// storage word. The loop trip count and the comps[i] reads are
	// compile-time constant, so the optimizer unrolls and folds.
	static constexpr Storage pack(const std::array<uint16_t, num_comps>& v) noexcept
	{
		Storage out{};
		for (size_t i = 0; i < num_comps; ++i) {
			const Storage mask = (Storage{ 1 } << comps[i].bits) - 1;
			out |= Storage(v[i] & mask) << comps[i].shift;
		}
		return out;
	}

	// Mirror of `pack`.
	static constexpr std::array<uint16_t, num_comps> unpack(Storage word) noexcept
	{
		std::array<uint16_t, num_comps> out{};
		for (size_t i = 0; i < num_comps; ++i) {
			const Storage mask = (Storage{ 1 } << comps[i].bits) - 1;
			out[i] = uint16_t((word >> comps[i].shift) & mask);
		}
		return out;
	}
};

template <ColorKind Kind, size_t Hsub, size_t Vsub, typename ... Planes>
struct Layout {
	static constexpr ColorKind kind = Kind;
	static constexpr size_t h_sub = Hsub;
	static constexpr size_t v_sub = Vsub;
	static constexpr size_t num_planes = sizeof...(Planes);

	template <size_t N>
	using plane = std::tuple_element_t<N, std::tuple<Planes...> >;

	// Index of the first plane containing component Tag, or num_planes
	// if no plane has it. Lets PlanarSource/Sink map C::U / C::V to a
	// plane regardless of YUV vs YVU ordering.
	// Comma-fold over plane indices: for each plane I check if it has
	// Tag, and on the first hit assign `found = I`. Subsequent hits are
	// suppressed by the `found == num_planes` guard. The whole fold
	// evaluates to a discarded list of int 0s; the `found` capture
	// carries the result out.
	template <C Tag>
	static constexpr size_t find_plane()
	{
		return [&]<size_t... I>(std::index_sequence<I...>) {
			       size_t found = num_planes;
			       ((plane<I>::template find_pos<Tag>() < plane<I>::num_comps
			         ? (found == num_planes ? (found = I, 0) : 0)
			         : 0), ...);
			       return found;
		} (std::make_index_sequence<num_planes>{});
	}
};

template <size_t N>
struct Buffer {
	std::array<uint8_t*, N> data;
	std::array<size_t,   N> stride;
};

} // namespace pixpat
