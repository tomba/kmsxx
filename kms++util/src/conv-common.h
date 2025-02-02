#pragma once

#include <algorithm>
#include <array>

#define MDSPAN_IMPL_STANDARD_NAMESPACE md
#define MDSPAN_IMPL_PROPOSED_NAMESPACE exp

#include <mdspan/mdspan.hpp>

namespace kms
{

/*
 * Helpers
 */

template<class T>
constexpr auto make_strided_view(T* data, size_t rows, size_t cols, size_t row_stride_bytes)
{
	assert(row_stride_bytes % sizeof(T) == 0 && "Row stride must be aligned to element size");

	size_t row_stride = row_stride_bytes / sizeof(T);
	std::array<size_t, 2> strides = { row_stride, 1 };

	auto layout = md::layout_stride::mapping(md::dextents<size_t, 2>(rows, cols), strides);

	return md::mdspan<T, md::dextents<size_t, 2>, md::layout_stride>(data, layout);
}

template<class T>
constexpr auto make_strided_fb_view(void* data, size_t rows, size_t cols, size_t row_stride_bytes)
{
	return make_strided_view(reinterpret_cast<T*>(data), rows, cols, row_stride_bytes);
}

// Helper for static loop unrolling
template<size_t Begin, size_t End, typename F>
constexpr void static_for(F&& f)
{
	[&]<size_t... I>(std::index_sequence<I...>) {
		(f(std::integral_constant<size_t, Begin + I>{}), ...);
	}(std::make_index_sequence<End - Begin>{});
}

template<typename T, typename TElement>
concept HasIndexOperatorReturning = requires(T t) {
	{ t[0] } -> std::convertible_to<const TElement&>;
};

template<typename T, typename TElement>
concept Is2Dspan = std::convertible_to<T, md::mdspan<TElement, md::dextents<size_t, 2>>>;

/*
 * Packing and Layouts
 */

/* This type must be big enough to hold any of the components' value */
using component_storage_type = uint16_t;

enum class ComponentType {
	X, A,
	R, G, B,
	Y, Cb, Cr,
	Y0, Y1, Y2,
	Cb0, Cb1, Cb2,
	Cr0, Cr1, Cr2,
};

// Describes a single component's bit layout
template<ComponentType Type, size_t BitSize, size_t BitOffset>
struct ComponentLayout {
	static constexpr size_t size = BitSize;
	static constexpr size_t offset = BitOffset;
	static constexpr ComponentType type = Type;

	static_assert(sizeof(component_storage_type) * 8 >= size);

	static constexpr auto get_mask() { return ((1ull << BitSize) - 1) << BitOffset; }

	template<typename TStorage>
	static constexpr TStorage pack_value(TStorage value)
	{
		return (value & ((1ull << BitSize) - 1)) << BitOffset;
	}

	template<typename TStorage>
	static constexpr component_storage_type unpack_value(TStorage value)
	{
		return (value >> BitOffset) & ((1ull << BitSize) - 1);
	}
};

// Describes N components packed into a storage type
template<typename TStorage, typename... Components>
struct PlaneLayout {
	using components_tuple = std::tuple<Components...>;

	static constexpr size_t num_components = sizeof...(Components);

	static constexpr size_t total_bits = (Components::size + ...);
	static constexpr size_t storage_bits = sizeof(TStorage) * 8;
	static_assert(total_bits <= storage_bits, "Components don't fit in storage type");

	template<size_t N>
	static constexpr size_t component_size = std::tuple_element_t<N, components_tuple>::size;

	using storage_type = TStorage;

	static constexpr std::array<ComponentType, sizeof...(Components)> order = {
		Components::type...
	};

	template<ComponentType C>
	static constexpr size_t component_count()
	{
		return std::ranges::count(order, C);
	}

	template<ComponentType C>
	static constexpr size_t find_pos()
	{
		return std::ranges::find(order, C) - order.begin();
	}

	template<ComponentType C>
	static constexpr size_t find_nth_pos(size_t n = 0)
	{
		auto it = std::ranges::find_if(
			order, [&n](ComponentType type) mutable {
				return type == C && n-- == 0;
			});
		return it - order.begin();
	}

	// Pack values into storage type (separate parameters version)
	template<typename... Values>
	static constexpr TStorage pack(Values... values)
	{
		static_assert(sizeof...(values) == num_components,
			      "Number of values must match number of components");
		return (Components::template pack_value<TStorage>(values) | ...);
	}

	// Pack values into storage type (std:array version)
	template<typename T, size_t N>
	static constexpr TStorage pack(const std::array<T, N>& values)
	{
		static_assert(N == num_components,
			      "Number of values must match number of components");
		return [&]<size_t... I>(std::index_sequence<I...>) {
			return (Components::template pack_value<TStorage>(values[I]) | ...);
		}(std::make_index_sequence<N>{});
	}

	static constexpr std::array<component_storage_type, num_components>
	unpack(TStorage value)
	{
		return [&]<size_t... I>(std::index_sequence<I...>) {
			return std::array{
				std::tuple_element_t<I, components_tuple>::template unpack_value<
					TStorage>(value)...
			};
		}(std::make_index_sequence<num_components>{});
	}
};

template<typename... Planes>
class FormatLayout
{
public:
	static constexpr size_t num_planes = sizeof...(Planes);

	template<size_t N> using plane = std::tuple_element_t<N, std::tuple<Planes...>>;
};

} // namespace kms
