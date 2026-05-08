#pragma once

#include <cmath>
#include <cstdint>

#include "color.h"
#include "layout.h"
#include "params.h"

namespace pixpat::patterns
{

// Patterns emit opaque pixels (a=kNormMax) unless they encode their
// own alpha (e.g. `plain`'s ARGB form). Alpha-bearing sinks
// (ARGB8888 etc) therefore see the pattern's chosen alpha; convert
// paths propagate the source's actual `a` instead (a=0 for X-only
// sources).
//
// A pattern is an instance with:
//   using Pixel = RGB16 | YUV16;
//   explicit Pat(const Params&) noexcept;
//   Pixel sample(size_t x, size_t y, size_t W, size_t H) const noexcept;
//   bool ready() const noexcept;   // optional, default true
// Patterns that don't read params ignore the constructor argument.

namespace detail
{
// 8-bit -> normalized 16 byte-replication. e.g. 255 -> 0xFFFF,
// 1 -> 0x0101.
constexpr RGB16 rgb8(uint8_t r, uint8_t g, uint8_t b) noexcept
{
	return RGB16{
	        uint16_t((uint16_t(r) << 8) | r),
	        uint16_t((uint16_t(g) << 8) | g),
	        uint16_t((uint16_t(b) << 8) | b),
	        kNormMax,
	};
}

// 12-bit -> normalized 16 bit-replication.
constexpr YUV16 yuv12(uint16_t y, uint16_t u, uint16_t v) noexcept
{
	return YUV16{
	        uint16_t((y << 4) | (y >> 8)),
	        uint16_t((u << 4) | (u >> 8)),
	        uint16_t((v << 4) | (v >> 8)),
	        kNormMax,
	};
}
} // namespace detail

// "kmstest" default pattern: white border + diagonals; blue rails on
// the top/left edges; red rails on the bottom/right; an 8-step color
// gradient block in the center.
struct Kmstest {
	using Pixel = RGB16;

	explicit Kmstest(const Params&) noexcept {
	}

	RGB16 sample(size_t x, size_t y, size_t W, size_t H) const noexcept
	{
		using detail::rgb8;
		const size_t mw = 20;
		const size_t xm1 = mw;
		const size_t xm2 = W - mw - 1;
		const size_t ym1 = mw;
		const size_t ym2 = H - mw - 1;

		if (x == xm1 || x == xm2 || y == ym1 || y == ym2)
			return rgb8(255, 255, 255);
		if (x < xm1 && y < ym1)
			return rgb8(255, 255, 255);
		if ((x == 0 || x == W - 1) && (y < ym1 || y > ym2))
			return rgb8(255, 255, 255);
		if ((y == 0 || y == H - 1) && (x < xm1 || x > xm2))
			return rgb8(255, 255, 255);
		if (x < xm1 && (y > ym1 && y < ym2))
			return rgb8(0, 0, 255);
		if (y < ym1 && (x > xm1 && x < xm2))
			return rgb8(0, 0, 255);
		if (x > xm2 && (y > ym1 && y < ym2))
			return rgb8(255, 0, 0);
		if (y > ym2 && (x > xm1 && x < xm2))
			return rgb8(255, 0, 0);
		if (x > xm1 && x < xm2 && y > ym1 && y < ym2) {
			if (x == y || W - x == H - y)
				return rgb8(255, 255, 255);
			if (W - x - 1 == y || x == H - y - 1)
				return rgb8(255, 255, 255);
			const int t = int((x - xm1 - 1) * 8 / (xm2 - xm1 - 1));
			const unsigned c = unsigned((y - ym1 - 1) % 256);
			unsigned r = 0, g = 0, b = 0;
			switch (t) {
			case 0: r = c; break;
			case 1: g = c; break;
			case 2: b = c; break;
			case 3: g = b = c; break;
			case 4: r = b = c; break;
			case 5: r = g = c; break;
			case 6: r = g = b = c; break;
			case 7: break;
			}
			return rgb8(uint8_t(r), uint8_t(g), uint8_t(b));
		}
		return rgb8(0, 0, 0);
	}
};

// SMPTE RP 219-1:2014 color bar pattern. Emits YUV directly with
// pixel values defined by the spec in BT.709 / Limited range. Pass
// `rec=BT709, range=Limited` for spec-correct output; other ColorSpec
// settings produce visibly-wrong colors when the sink crosses to RGB
// (the matrix the caller picked is applied to BT.709-encoded values).
// Callers are trusted — pixpat does not override the spec for them.
struct Smpte {
	using Pixel = YUV16;

	explicit Smpte(const Params&) noexcept {
	}

	YUV16 sample(size_t x, size_t y, size_t W, size_t H) const noexcept
	{
		using detail::yuv12;
		constexpr YUV16 gray40    = yuv12(1658, 2048, 2048);
		constexpr YUV16 white75   = yuv12(2884, 2048, 2048);
		constexpr YUV16 yellow75  = yuv12(2694,  704, 2171);
		constexpr YUV16 cyan75    = yuv12(2325, 2356,  704);
		constexpr YUV16 green75   = yuv12(2136, 1012,  827);
		constexpr YUV16 magenta75 = yuv12(1004, 3084, 3269);
		constexpr YUV16 red75     = yuv12( 815, 1740, 3392);
		constexpr YUV16 blue75    = yuv12( 446, 3392, 1925);
		constexpr YUV16 cyan100   = yuv12(3015, 2459,  256);
		constexpr YUV16 blue100   = yuv12( 509, 3840, 1884);
		constexpr YUV16 yellow100 = yuv12(3507,  256, 2212);
		constexpr YUV16 black     = yuv12( 256, 2048, 2048);
		constexpr YUV16 white100  = yuv12(3760, 2048, 2048);
		constexpr YUV16 red100    = yuv12(1001, 1637, 3840);
		constexpr YUV16 gray15    = yuv12( 782, 2048, 2048);

		constexpr YUV16 black_m2  = yuv12( 186, 2048, 2048);
		constexpr YUV16 black_p2  = yuv12( 326, 2048, 2048);
		constexpr YUV16 black_p4  = yuv12( 396, 2048, 2048);

		constexpr size_t M = 1024;
		const size_t xs = x * M;
		const size_t a  = W * M;
		const size_t c  = (a * 3 / 4) / 7;
		const size_t d  = a / 8;

		const size_t pattern1_height = (H * 7) / 12;
		const size_t pattern2_height = pattern1_height + (H / 12);
		const size_t pattern3_height = pattern2_height + (H / 12);

		if (y < pattern1_height) {
			if (xs < d || xs >= (a - d))
				return gray40;
			const size_t bar = (xs - d) / c;
			switch (bar) {
			case 0: return white75;
			case 1: return yellow75;
			case 2: return cyan75;
			case 3: return green75;
			case 4: return magenta75;
			case 5: return red75;
			default: return blue75;
			}
		}

		if (y < pattern2_height) {
			if (xs < d)         return cyan100;
			if (xs >= (a - d))  return blue100;
			return white75;
		}

		if (y < pattern3_height) {
			if (xs < d)         return yellow100;
			if (xs >= (a - d))  return red100;
			const size_t ramp_w = a - 2 * d;
			const size_t ramp_x = xs - d;
			const uint16_t y_val = uint16_t(256 + (3760 - 256) * ramp_x / ramp_w);
			return yuv12(y_val, 2048, 2048);
		}

		// pattern4 (PLUGE)
		const size_t c0 = d;
		const size_t c1 = c0 + c * 3 / 2;
		const size_t c2 = c1 + 2 * c;
		const size_t c3 = c2 + c * 5 / 6;

		if (xs < c0)            return gray15;
		if (xs < c1)            return black;
		if (xs < c2)            return white100;
		if (xs < c3)            return black;
		if (xs >= a - d)        return gray15;
		if (xs >= a - d - c)    return black;

		const size_t step = (xs - c3) / (c / 3);
		switch (step) {
		case 0: return black_m2;
		case 1: return black;
		case 2: return black_p2;
		case 3: return black;
		default: return black_p4;
		}
	}
};

// Solid fill from a hex color string. Reads `color=<hex>` from
// params; the value is parsed by Params::get_hex_color (8/16-bpc,
// alpha-first if present, optional `0x` prefix). Missing or
// malformed `color` leaves ready()=false and the dispatcher fails
// the call.
struct Plain {
	using Pixel = RGB16;

	explicit Plain(const Params& p) noexcept
	{
		if (auto c = p.get_hex_color("color")) {
			color_ = *c;
			ready_ = true;
		}
	}

	bool ready() const noexcept {
		return ready_;
	}

	RGB16 sample(size_t, size_t, size_t, size_t) const noexcept
	{
		return color_;
	}

private:
	RGB16 color_{};
	bool ready_{ false };
};

namespace detail
{
// Linear ramp 0..kNormMax across [0, span-1]. span<=1 returns kNormMax.
constexpr uint16_t ramp16(size_t pos, size_t span) noexcept
{
	if (span <= 1)
		return kNormMax;
	return uint16_t((uint64_t(pos) * kNormMax) / (span - 1));
}
} // namespace detail

// Black/white checkerboard. Reads optional `cell=<N>` (positive
// integer; default 8) for cell size in pixels.
struct Checker {
	using Pixel = RGB16;

	explicit Checker(const Params& p) noexcept
	{
		if (p.get("cell")) {
			auto n = p.get_int("cell");
			if (!n || *n <= 0) {
				ready_ = false;
				return;
			}
			cell_ = size_t(*n);
		}
	}

	bool ready() const noexcept {
		return ready_;
	}

	RGB16 sample(size_t x, size_t y, size_t, size_t) const noexcept
	{
		const bool dark = (((x / cell_) ^ (y / cell_)) & 1u) != 0;
		return dark ? RGB16{ 0, 0, 0, kNormMax }
		            : RGB16{ kNormMax, kNormMax, kNormMax, kNormMax };
	}

private:
	size_t cell_{ 8 };
	bool ready_{ true };
};

namespace detail
{
// Pick one of (R, G, B, gray) given a stripe index in [0, 4) and a
// scalar ramp value. Used by hramp/vramp.
constexpr RGB16 rgb_gray_stripe(size_t stripe, uint16_t v) noexcept
{
	switch (stripe) {
	case 0:  return RGB16{ v, 0, 0, kNormMax };
	case 1:  return RGB16{ 0, v, 0, kNormMax };
	case 2:  return RGB16{ 0, 0, v, kNormMax };
	default: return RGB16{ v, v, v, kNormMax };
	}
}
} // namespace detail

// Four horizontal stripes — R, G, B, gray — each a 0..max ramp
// along x. Per-channel and luma quantization in one pattern.
struct Hramp {
	using Pixel = RGB16;

	explicit Hramp(const Params&) noexcept {
	}

	RGB16 sample(size_t x, size_t y, size_t W, size_t H) const noexcept
	{
		const size_t stripe = (H == 0) ? 0 : (y * 4) / H;
		return detail::rgb_gray_stripe(stripe, detail::ramp16(x, W));
	}
};

// Four vertical columns — R, G, B, gray — each a 0..max ramp
// along y. Same coverage as hramp, rotated 90°.
struct Vramp {
	using Pixel = RGB16;

	explicit Vramp(const Params&) noexcept {
	}

	RGB16 sample(size_t x, size_t y, size_t W, size_t H) const noexcept
	{
		const size_t col = (W == 0) ? 0 : (x * 4) / W;
		return detail::rgb_gray_stripe(col, detail::ramp16(y, H));
	}
};

// Diagonal RGB ramp: R sweeps with x, G with y, B with x+y.
struct Dramp {
	using Pixel = RGB16;

	explicit Dramp(const Params&) noexcept {
	}

	RGB16 sample(size_t x, size_t y, size_t W, size_t H) const noexcept
	{
		const uint16_t r = detail::ramp16(x, W);
		const uint16_t g = detail::ramp16(y, H);
		const size_t span = (W + H >= 2) ? (W + H - 1) : 1;
		const uint16_t b = detail::ramp16(x + y, span);
		return RGB16{ r, g, b, kNormMax };
	}
};

namespace detail
{
// Seven-region color sequence used by hbar/vbar:
// white, red, white, green, white, blue, white. The white separators
// between R/G/B make per-channel offsets at the band boundaries
// visible.
constexpr RGB16 bar_color7(size_t band) noexcept
{
	switch (band) {
	case 1:  return rgb8(255,   0,   0);
	case 3:  return rgb8(  0, 255,   0);
	case 5:  return rgb8(  0,   0, 255);
	default: return rgb8(255, 255, 255);
	}
}
} // namespace detail

// Vertical bar (full image height, narrow along x) over a black
// background. `pos` is the left edge in pixels (signed; negative
// values clip at the left edge); `width` is the bar thickness in
// pixels (default 32). The bar is split into 7 equal-height regions
// colored white/red/white/green/white/blue/white.
struct VBarRGB {
	using Pixel = RGB16;

	explicit VBarRGB(const Params& p) noexcept
	{
		auto pp = p.get_int("pos");
		if (!pp) {
			ready_ = false;
			return;
		}
		pos_ = *pp;
		if (p.get("width")) {
			auto w = p.get_int("width");
			if (!w || *w <= 0) {
				ready_ = false;
				return;
			}
			width_ = size_t(*w);
		}
	}

	bool ready() const noexcept {
		return ready_;
	}

	RGB16 sample(size_t x, size_t y, size_t, size_t H) const noexcept
	{
		const long long sx = static_cast<long long>(x);
		const long long lo = pos_;
		const long long hi = lo + static_cast<long long>(width_);
		if (sx < lo || sx >= hi)
			return detail::rgb8(0, 0, 0);
		const size_t band = (H == 0) ? 0 : (y * 7) / H;
		return detail::bar_color7(band);
	}

private:
	int pos_{};
	size_t width_{ 32 };
	bool ready_{ true };
};

// Horizontal bar: vbar rotated 90°. `pos` is the top edge in pixels;
// `width` is the bar thickness in pixels (default 32). The bar spans
// the full image width and is split into 7 equal-width regions
// colored white/red/white/green/white/blue/white.
struct HBarRGB {
	using Pixel = RGB16;

	explicit HBarRGB(const Params& p) noexcept
	{
		auto pp = p.get_int("pos");
		if (!pp) {
			ready_ = false;
			return;
		}
		pos_ = *pp;
		if (p.get("width")) {
			auto w = p.get_int("width");
			if (!w || *w <= 0) {
				ready_ = false;
				return;
			}
			width_ = size_t(*w);
		}
	}

	bool ready() const noexcept {
		return ready_;
	}

	RGB16 sample(size_t x, size_t y, size_t W, size_t) const noexcept
	{
		const long long sy = static_cast<long long>(y);
		const long long lo = pos_;
		const long long hi = lo + static_cast<long long>(width_);
		if (sy < lo || sy >= hi)
			return detail::rgb8(0, 0, 0);
		const size_t band = (W == 0) ? 0 : (x * 7) / W;
		return detail::bar_color7(band);
	}

private:
	int pos_{};
	size_t width_{ 32 };
	bool ready_{ true };
};

// Same shape as VBarRGB but emits YUV16 directly. The five unique colors
// (black bg + white/red/green/blue bar regions) are precomputed from
// `spec` at construction so the cross-kind pass is a no-op when the
// sink is YUV. Use the RGB-native `VBarRGB` for RGB sinks instead — it
// avoids the YUV→RGB pass that this variant would incur there.
struct VBarYUV {
	using Pixel = YUV16;

	explicit VBarYUV(const Params& p, ColorSpec spec) noexcept
	{
		auto pp = p.get_int("pos");
		if (!pp) {
			ready_ = false;
			return;
		}
		pos_ = *pp;
		if (p.get("width")) {
			auto w = p.get_int("width");
			if (!w || *w <= 0) {
				ready_ = false;
				return;
			}
			width_ = size_t(*w);
		}
		const ColorCoeffs c = coeffs_for(spec);
		using X = ColorXfm<RGB16, YUV16>;
		bg_       = X::apply(detail::rgb8(  0,   0,   0), c);
		bands_[0] = X::apply(detail::rgb8(255, 255, 255), c);
		bands_[1] = X::apply(detail::rgb8(255,   0,   0), c);
		bands_[2] = bands_[0];
		bands_[3] = X::apply(detail::rgb8(  0, 255,   0), c);
		bands_[4] = bands_[0];
		bands_[5] = X::apply(detail::rgb8(  0,   0, 255), c);
		bands_[6] = bands_[0];
	}

	bool ready() const noexcept {
		return ready_;
	}

	YUV16 sample(size_t x, size_t y, size_t, size_t H) const noexcept
	{
		const long long sx = static_cast<long long>(x);
		const long long lo = pos_;
		const long long hi = lo + static_cast<long long>(width_);
		if (sx < lo || sx >= hi)
			return bg_;
		const size_t band = (H == 0) ? 0 : (y * 7) / H;
		return bands_[band];
	}

private:
	YUV16 bg_{};
	YUV16 bands_[7]{};
	int pos_{};
	size_t width_{ 32 };
	bool ready_{ true };
};

// YUV-native counterpart to HBarRGB. See VBarYUV.
struct HBarYUV {
	using Pixel = YUV16;

	explicit HBarYUV(const Params& p, ColorSpec spec) noexcept
	{
		auto pp = p.get_int("pos");
		if (!pp) {
			ready_ = false;
			return;
		}
		pos_ = *pp;
		if (p.get("width")) {
			auto w = p.get_int("width");
			if (!w || *w <= 0) {
				ready_ = false;
				return;
			}
			width_ = size_t(*w);
		}
		const ColorCoeffs c = coeffs_for(spec);
		using X = ColorXfm<RGB16, YUV16>;
		bg_       = X::apply(detail::rgb8(  0,   0,   0), c);
		bands_[0] = X::apply(detail::rgb8(255, 255, 255), c);
		bands_[1] = X::apply(detail::rgb8(255,   0,   0), c);
		bands_[2] = bands_[0];
		bands_[3] = X::apply(detail::rgb8(  0, 255,   0), c);
		bands_[4] = bands_[0];
		bands_[5] = X::apply(detail::rgb8(  0,   0, 255), c);
		bands_[6] = bands_[0];
	}

	bool ready() const noexcept {
		return ready_;
	}

	YUV16 sample(size_t x, size_t y, size_t W, size_t) const noexcept
	{
		const long long sy = static_cast<long long>(y);
		const long long lo = pos_;
		const long long hi = lo + static_cast<long long>(width_);
		if (sy < lo || sy >= hi)
			return bg_;
		const size_t band = (W == 0) ? 0 : (x * 7) / W;
		return bands_[band];
	}

private:
	YUV16 bg_{};
	YUV16 bands_[7]{};
	int pos_{};
	size_t width_{ 32 };
	bool ready_{ true };
};

// Centered radial cosine zone plate: 0.5 + 0.5 * cos(k * (cx² + cy²))
// with cx, cy measured from the image center and k chosen so the
// local frequency hits Nyquist at the longer edge — i.e. the pattern
// uses every spatial frequency the grid can resolve.
struct Zoneplate {
	using Pixel = RGB16;

	explicit Zoneplate(const Params&) noexcept {
	}

	RGB16 sample(size_t x, size_t y, size_t W, size_t H) const noexcept
	{
		const double max_dim = double(W > H ? W : H);
		// Local frequency d(k r²)/dr = 2 k r. At r = max_dim/2 the
		// frequency reaches π/pixel (Nyquist), giving k = π / max_dim.
		const double k = 3.14159265358979323846 / (max_dim > 0 ? max_dim : 1.0);
		const double cx = double(x) - 0.5 * double(W);
		const double cy = double(y) - 0.5 * double(H);
		const double phase = k * (cx * cx + cy * cy);
		const double v = 0.5 + 0.5 * std::cos(phase);
		const double scaled = v * 65535.0;
		const uint16_t g = (scaled < 0.0)        ? uint16_t(0)
		                 : (scaled > 65535.0)    ? kNormMax
		                                         : uint16_t(scaled + 0.5);
		return RGB16{ g, g, g, kNormMax };
	}
};

} // namespace pixpat::patterns
