#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace kms
{
constexpr uint32_t str_to_fourcc(const std::string& fourcc)
{
	if (fourcc.empty())
		return 0;

	return fourcc[0] | (fourcc[1] << 8) | (fourcc[2] << 16) | (fourcc[3] << 24);
}

constexpr std::string fourcc_to_str(uint32_t fourcc)
{
	char buf[4] = {
		(char)((fourcc >> 0) & 0xff),
		(char)((fourcc >> 8) & 0xff),
		(char)((fourcc >> 16) & 0xff),
		(char)((fourcc >> 24) & 0xff),
	};
	return std::string(buf, 4);
}

enum class PixelFormat : uint32_t {
	Undefined = 0,

	NV12 = str_to_fourcc("NV12"),
	NV21 = str_to_fourcc("NV21"),
	NV16 = str_to_fourcc("NV16"),
	NV61 = str_to_fourcc("NV61"),

	YUV420 = str_to_fourcc("YU12"),
	YVU420 = str_to_fourcc("YV12"),
	YUV422 = str_to_fourcc("YU16"),
	YVU422 = str_to_fourcc("YV16"),
	YUV444 = str_to_fourcc("YU24"),
	YVU444 = str_to_fourcc("YV24"),

	UYVY = str_to_fourcc("UYVY"),
	YUYV = str_to_fourcc("YUYV"),
	YVYU = str_to_fourcc("YVYU"),
	VYUY = str_to_fourcc("VYUY"),

	Y210 = str_to_fourcc("Y210"),
	Y212 = str_to_fourcc("Y212"),
	Y216 = str_to_fourcc("Y216"),

	XRGB8888 = str_to_fourcc("XR24"),
	XBGR8888 = str_to_fourcc("XB24"),
	RGBX8888 = str_to_fourcc("RX24"),
	BGRX8888 = str_to_fourcc("BX24"),

	ARGB8888 = str_to_fourcc("AR24"),
	ABGR8888 = str_to_fourcc("AB24"),
	RGBA8888 = str_to_fourcc("RA24"),
	BGRA8888 = str_to_fourcc("BA24"),

	RGB888 = str_to_fourcc("RG24"),
	BGR888 = str_to_fourcc("BG24"),

	RGB332 = str_to_fourcc("RGB8"),

	RGB565 = str_to_fourcc("RG16"),
	BGR565 = str_to_fourcc("BG16"),

	XRGB4444 = str_to_fourcc("XR12"),
	XRGB1555 = str_to_fourcc("XR15"),

	ARGB4444 = str_to_fourcc("AR12"),
	ARGB1555 = str_to_fourcc("AR15"),

	XRGB2101010 = str_to_fourcc("XR30"),
	XBGR2101010 = str_to_fourcc("XB30"),
	RGBX1010102 = str_to_fourcc("RX30"),
	BGRX1010102 = str_to_fourcc("BX30"),

	ARGB2101010 = str_to_fourcc("AR30"),
	ABGR2101010 = str_to_fourcc("AB30"),
	RGBA1010102 = str_to_fourcc("RA30"),
	BGRA1010102 = str_to_fourcc("BA30"),
};

inline PixelFormat fourcc_to_pixel_format(uint32_t fourcc)
{
	return (PixelFormat)fourcc;
}

inline uint32_t pixel_format_to_fourcc(PixelFormat f)
{
	return (uint32_t)f;
}

inline PixelFormat fourcc_str_to_pixel_format(const std::string& fourcc)
{
	return (PixelFormat)str_to_fourcc(fourcc.c_str());
}

inline std::string pixel_format_to_fourcc_str(PixelFormat f)
{
	return fourcc_to_str((uint32_t)f);
}

enum class PixelColorType {
	Undefined,
	RGB,
	YUV,
	RAW,
};

struct PixelFormatPlaneInfo {
	constexpr PixelFormatPlaneInfo() = default;

	constexpr PixelFormatPlaneInfo(uint8_t bytes_per_block)
		: bytes_per_block(bytes_per_block), pixels_per_block(1), hsub(1), vsub(1)
	{
	}

	constexpr PixelFormatPlaneInfo(uint8_t bytes_per_block, uint8_t pixels_per_block,
				       uint8_t hsub, uint8_t vsub)
		: bytes_per_block(bytes_per_block), pixels_per_block(pixels_per_block),
		  hsub(hsub), vsub(vsub)
	{
	}

	uint8_t bytes_per_block;
	uint8_t pixels_per_block;
	uint8_t hsub;
	uint8_t vsub;
};

struct PixelFormatInfo {
	constexpr PixelFormatInfo(const std::string& name, const std::string& drm_fourcc,
				  const std::string& v4l2_4cc, PixelColorType color,
				  std::tuple<uint8_t, uint8_t> pixel_align,
				  std::vector<PixelFormatPlaneInfo> planes)
		: name(name), drm_fourcc(kms::str_to_fourcc(drm_fourcc)),
		  v4l2_4cc(kms::str_to_fourcc(v4l2_4cc)), type(color),
		  pixel_align(pixel_align), num_planes(planes.size()), planes(planes)
	{
	}

	std::string name;
	uint32_t drm_fourcc;
	uint32_t v4l2_4cc;

	PixelColorType type;
	std::tuple<uint8_t, uint8_t> pixel_align;

	uint8_t num_planes; // this should be dropped, and use 'planes' vector size
	std::vector<PixelFormatPlaneInfo> planes;

	std::tuple<uint32_t, uint32_t> align_pixels(uint32_t width, uint32_t height) const;
	uint32_t stride(uint32_t width, uint32_t plane = 0, uint32_t align = 1) const;
	uint32_t planesize(uint32_t stride, uint32_t height, uint32_t plane = 0) const;
	uint32_t framesize(uint32_t width, uint32_t height, uint32_t align = 1) const;
	std::tuple<uint32_t, uint32_t, uint32_t> dumb_size(uint32_t width, uint32_t height, uint32_t plane = 0, uint32_t align = 1) const;
};

const struct PixelFormatInfo& get_pixel_format_info(PixelFormat format);

} // namespace kms
