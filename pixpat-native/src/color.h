#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "layout.h"

namespace pixpat
{

// BT.601 / BT.709 / BT.2020 × Limited / Full range, dispatched at
// runtime via a small `ColorCoeffs` struct that the caller hoists out
// of the per-pixel loop. The convert and pattern entry points compute
// `coeffs_for(spec)` once before the stripe loop, then pass the
// resulting struct into every `ColorXfm::apply()` in the inner loop.
// This avoids per-pixel matrix branching and also the alternative of
// a 6×-instantiated template (would push the hot pivot from 121 to
// 726 `Converter` bodies). The coefficient values are loop-invariant
// broadcast scalars, so the compiler vectorizes the inner loop with
// vbroadcastss + vmulps in place of constant folds.
//
// Math runs in float.

enum class Rec   : uint8_t { BT601, BT709, BT2020 };
enum class Range : uint8_t { Limited, Full };

struct ColorSpec {
	Rec rec;
	Range range;
	constexpr bool operator==(const ColorSpec&) const = default;
};

inline constexpr ColorSpec kDefaultColorSpec{ Rec::BT601, Range::Limited };

struct ColorCoeffs {
	// RGB->YUV
	float kr, kg, kb;
	float y_scale, y_offset;
	float c_scale, c_offset;
	float u_factor, v_factor;
	// YUV->RGB
	float y_inv, c_inv;
	float gu, gv, ru, bv;
	// normalized 16-bit scale (kNormMax in float, plus its inverse)
	float norm_scale, norm_inv_scale;
};

namespace detail
{
constexpr ColorCoeffs make_coeffs(float kr, float kg, float kb, bool full) noexcept
{
	const float y_min = full ? 0.0f :  16.0f / 255.0f;
	const float y_max = full ? 1.0f : 235.0f / 255.0f;
	const float c_min = full ? 0.0f :  16.0f / 255.0f;
	const float c_max = full ? 1.0f : 240.0f / 255.0f;

	const float y_scale  = y_max - y_min;
	const float y_offset = y_min;
	const float c_scale  = c_max - c_min;
	const float c_offset = (c_max + c_min) * 0.5f;

	const float u_factor = 1.0f / (2.0f * (1.0f - kb));
	const float v_factor = 1.0f / (2.0f * (1.0f - kr));
	const float y_inv = 1.0f / y_scale;
	const float c_inv = 1.0f / c_scale;
	const float gu = -2.0f * (1.0f - kb) * kb / kg;
	const float gv = -2.0f * (1.0f - kr) * kr / kg;
	const float ru =  2.0f * (1.0f - kr);
	const float bv =  2.0f * (1.0f - kb);

	const float norm_scale     = float(kNormMax);
	const float norm_inv_scale = 1.0f / norm_scale;

	return ColorCoeffs{
	        kr, kg, kb,
	        y_scale, y_offset,
	        c_scale, c_offset,
	        u_factor, v_factor,
	        y_inv, c_inv,
	        gu, gv, ru, bv,
	        norm_scale, norm_inv_scale,
	};
}
} // namespace detail

constexpr ColorCoeffs coeffs_for(ColorSpec spec) noexcept
{
	const bool full = spec.range == Range::Full;
	switch (spec.rec) {
	case Rec::BT601:  return detail::make_coeffs(0.299f,  0.587f,  0.114f,  full);
	case Rec::BT2020: return detail::make_coeffs(0.2627f, 0.6780f, 0.0593f, full);
	default:          return detail::make_coeffs(0.2126f, 0.7152f, 0.0722f, full);
	}
}

template <typename SrcPix, typename DstPix>
struct ColorXfm;

template <>
struct ColorXfm<RGB16, RGB16> {
	static constexpr RGB16 apply(RGB16 p) noexcept {
		return p;
	}
	static constexpr RGB16 apply(RGB16 p, const ColorCoeffs&) noexcept {
		return p;
	}
};

template <>
struct ColorXfm<YUV16, YUV16> {
	static constexpr YUV16 apply(YUV16 p) noexcept {
		return p;
	}
	static constexpr YUV16 apply(YUV16 p, const ColorCoeffs&) noexcept {
		return p;
	}
};

// Cross-color-kind conversions reset `a` to kNormMax (sinks with X
// write 0; sinks with A see fully opaque pixels). Within the same
// color kind, identity ColorXfm propagates `a` unchanged.
template <>
struct ColorXfm<RGB16, YUV16> {
	static YUV16 apply(RGB16 rgb, const ColorCoeffs& c) noexcept
	{
		const float r = float(rgb.r) * c.norm_inv_scale;
		const float g = float(rgb.g) * c.norm_inv_scale;
		const float b = float(rgb.b) * c.norm_inv_scale;

		const float yp = c.kr * r + c.kg * g + c.kb * b;
		const float u  = (b - yp) * c.u_factor;
		const float v  = (r - yp) * c.v_factor;

		// No clamp on RGB→YUV: for any uint16_t (RGB) input the
		// output Y/U/V is structurally in [0, 1] (limited-range
		// chroma stays within [c_min, c_max] ⊂ [0, 1]). The +0.5
		// rounds half-up before the integer cast.
		return YUV16{
		        uint16_t((yp * c.y_scale + c.y_offset) * c.norm_scale + 0.5f),
		        uint16_t((u  * c.c_scale + c.c_offset) * c.norm_scale + 0.5f),
		        uint16_t((v  * c.c_scale + c.c_offset) * c.norm_scale + 0.5f),
		        kNormMax,
		};
	}
};

template <>
struct ColorXfm<YUV16, RGB16> {
	static RGB16 apply(YUV16 yuv, const ColorCoeffs& c) noexcept
	{
		const float yp = (float(yuv.y) * c.norm_inv_scale - c.y_offset) * c.y_inv;
		const float u  = (float(yuv.u) * c.norm_inv_scale - c.c_offset) * c.c_inv;
		const float v  = (float(yuv.v) * c.norm_inv_scale - c.c_offset) * c.c_inv;

		const float r = yp + c.ru * v;
		const float g = yp + c.gu * u + c.gv * v;
		const float b = yp + c.bv * u;

		// Clamp on YUV→RGB: the inverse matrix produces out-of-range
		// RGB for some valid YUV inputs. Written as min/max so it
		// vectorizes to vminps/vmaxps; std::clamp can defeat that.
		auto pack = [&](float x) -> uint16_t {
				    x = x * c.norm_scale + 0.5f;
				    x = std::min(std::max(x, 0.0f), c.norm_scale);
				    return uint16_t(x);
			    };

		return RGB16{
		        pack(r), pack(g), pack(b),
		        kNormMax,
		};
	}
};

// In-place cross-color-kind passes over a normalized line buffer.
// RGB16 and YUV16 are both 4 uint16_t with identical layout, so we
// can memcpy through the same buffer pixel-by-pixel without aliasing.
inline void norm_rgb_to_yuv(uint8_t* buf, size_t n, const ColorCoeffs& c) noexcept
{
	for (size_t i = 0; i < n; ++i) {
		RGB16 rgb;
		std::memcpy(&rgb, buf + i * sizeof(RGB16), sizeof(RGB16));
		YUV16 yuv = ColorXfm<RGB16, YUV16>::apply(rgb, c);
		std::memcpy(buf + i * sizeof(YUV16), &yuv, sizeof(YUV16));
	}
}

inline void norm_yuv_to_rgb(uint8_t* buf, size_t n, const ColorCoeffs& c) noexcept
{
	for (size_t i = 0; i < n; ++i) {
		YUV16 yuv;
		std::memcpy(&yuv, buf + i * sizeof(YUV16), sizeof(YUV16));
		RGB16 rgb = ColorXfm<YUV16, RGB16>::apply(yuv, c);
		std::memcpy(buf + i * sizeof(RGB16), &rgb, sizeof(RGB16));
	}
}

} // namespace pixpat
