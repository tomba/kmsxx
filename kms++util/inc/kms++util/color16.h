#pragma once

#include <cstdint>
#include <algorithm>

namespace kms
{

enum class ColorRange { Limited, Full };

enum class RecStandard { BT601, BT709, BT2020 };

class YUV16;
class RGB16;

// Conversion coefficients for different standards
struct ConversionCoefficients {
	double kr;
	double kg;
	double kb;

	static constexpr ConversionCoefficients get(RecStandard rec) noexcept
	{
		switch (rec) {
		case RecStandard::BT601:
			return { 0.299, 0.587, 0.114 };
		default:	 // Default to BT709
		case RecStandard::BT709:
			return { 0.2126, 0.7152, 0.0722 };
		case RecStandard::BT2020:
			return { 0.2627, 0.6780, 0.0593 };
		}
	}
};

// Range scaling factors
struct RangeScaling {
	double y_min;
	double y_max;
	double c_min;
	double c_max;

	static constexpr RangeScaling get(ColorRange range) noexcept
	{
		switch (range) {
		case ColorRange::Limited:
			return {
				16.0 / 255.0, // y_min
				235.0 / 255.0, // y_max
				16.0 / 255.0, // c_min
				240.0 / 255.0 // c_max
			};
		case ColorRange::Full:
		default:
			return { 0.0, 1.0, 0.0, 1.0 };
		}
	}
};

class RGB16
{
public:
	uint16_t r = 0;
	uint16_t g = 0;
	uint16_t b = 0;
	uint16_t a = 0;

	constexpr RGB16() noexcept = default;
	constexpr RGB16(uint16_t r, uint16_t g, uint16_t b) noexcept : r(r), g(g), b(b), a(max_value) {}
	constexpr RGB16(uint16_t r, uint16_t g, uint16_t b, uint16_t a) noexcept : r(r), g(g), b(b), a(a) {}

	static constexpr RGB16 from_8(uint8_t r, uint8_t g, uint8_t b)
	{
		return RGB16 {
			static_cast<uint16_t>((r << 8) | r),
			static_cast<uint16_t>((g << 8) | g),
			static_cast<uint16_t>((b << 8) | b),
		};
	}

	[[nodiscard]]
	constexpr YUV16 to_yuv(RecStandard rec = RecStandard::BT709,
			       ColorRange range = ColorRange::Limited) const noexcept;

	static constexpr uint16_t max_value = 0xffff;
};

class YUV16
{
public:
	uint16_t y = 0;
	uint16_t u = 0;
	uint16_t v = 0;
	uint16_t a = 0;

	constexpr YUV16() noexcept = default;
	constexpr YUV16(uint16_t y, uint16_t u, uint16_t v) noexcept : y(y), u(u), v(v), a(max_value) {}
	constexpr YUV16(uint16_t y, uint16_t u, uint16_t v, uint16_t a) noexcept : y(y), u(u), v(v), a(a) {}

	[[nodiscard]]
	constexpr RGB16 to_rgb(RecStandard rec = RecStandard::BT709,
			       ColorRange range = ColorRange::Limited) const noexcept;

	static constexpr uint16_t max_value = 0xffff;
};

constexpr YUV16 RGB16::to_yuv(RecStandard rec, ColorRange range) const noexcept
{
	const auto coeff = ConversionCoefficients::get(rec);
	const auto scaling = RangeScaling::get(range);

	// Normalize RGB values to [0,1]
	const double r_norm = static_cast<double>(r) / max_value;
	const double g_norm = static_cast<double>(g) / max_value;
	const double b_norm = static_cast<double>(b) / max_value;

	// Calculate Y
	double y = coeff.kr * r_norm + coeff.kg * g_norm + coeff.kb * b_norm;

	// Scale Y to target range
	y = y * (scaling.y_max - scaling.y_min) + scaling.y_min;

	// Calculate U and V
	double u = (b_norm - y) / (2.0 * (1.0 - coeff.kb));
	double v = (r_norm - y) / (2.0 * (1.0 - coeff.kr));

	// Scale U and V to target range
	u = u * (scaling.c_max - scaling.c_min) + (scaling.c_max + scaling.c_min) / 2.0;
	v = v * (scaling.c_max - scaling.c_min) + (scaling.c_max + scaling.c_min) / 2.0;

	// Convert back to 16-bit values
	return YUV16(static_cast<uint16_t>(y * max_value), static_cast<uint16_t>(u * max_value),
		     static_cast<uint16_t>(v * max_value));
}

constexpr RGB16 YUV16::to_rgb(RecStandard rec, ColorRange range) const noexcept
{
	const auto coeff = ConversionCoefficients::get(rec);
	const auto scaling = RangeScaling::get(range);

	// Normalize YUV values to [0,1]
	double y_norm = static_cast<double>(y) / max_value;
	double u_norm = static_cast<double>(u) / max_value;
	double v_norm = static_cast<double>(v) / max_value;

	// Rescale from target range to [0,1]
	y_norm = (y_norm - scaling.y_min) / (scaling.y_max - scaling.y_min);
	u_norm = (u_norm - (scaling.c_max + scaling.c_min) / 2.0) / (scaling.c_max - scaling.c_min);
	v_norm = (v_norm - (scaling.c_max + scaling.c_min) / 2.0) / (scaling.c_max - scaling.c_min);

	// Convert to RGB
	const double r = y_norm + 2.0 * v_norm * (1.0 - coeff.kr);
	const double g = y_norm - 2.0 * u_norm * (1.0 - coeff.kb) * coeff.kb / coeff.kg -
			 2.0 * v_norm * (1.0 - coeff.kr) * coeff.kr / coeff.kg;
	const double b = y_norm + 2.0 * u_norm * (1.0 - coeff.kb);

	// Clamp and convert back to 16-bit values
	return RGB16(static_cast<uint16_t>(std::clamp(r, 0.0, 1.0) * max_value),
		     static_cast<uint16_t>(std::clamp(g, 0.0, 1.0) * max_value),
		     static_cast<uint16_t>(std::clamp(b, 0.0, 1.0) * max_value));
}

} // namespace kms
