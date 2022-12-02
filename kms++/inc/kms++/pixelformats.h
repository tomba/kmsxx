#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>

namespace kms
{
constexpr uint32_t MakeFourCC(const char* fourcc)
{
	return fourcc[0] | (fourcc[1] << 8) | (fourcc[2] << 16) | (fourcc[3] << 24);
}

enum class PixelFormat : uint32_t {
	Undefined = 0,

	NV12 = MakeFourCC("NV12"),
	NV21 = MakeFourCC("NV21"),
	NV16 = MakeFourCC("NV16"),
	NV61 = MakeFourCC("NV61"),

	YUV420 = MakeFourCC("YU12"),
	YVU420 = MakeFourCC("YV12"),
	YUV422 = MakeFourCC("YU16"),
	YVU422 = MakeFourCC("YV16"),
	YUV444 = MakeFourCC("YU24"),
	YVU444 = MakeFourCC("YV24"),

	UYVY = MakeFourCC("UYVY"),
	YUYV = MakeFourCC("YUYV"),
	YVYU = MakeFourCC("YVYU"),
	VYUY = MakeFourCC("VYUY"),

	Y210 = MakeFourCC("Y210"),
	Y212 = MakeFourCC("Y212"),
	Y216 = MakeFourCC("Y216"),

	XRGB8888 = MakeFourCC("XR24"),
	XBGR8888 = MakeFourCC("XB24"),
	RGBX8888 = MakeFourCC("RX24"),
	BGRX8888 = MakeFourCC("BX24"),

	ARGB8888 = MakeFourCC("AR24"),
	ABGR8888 = MakeFourCC("AB24"),
	RGBA8888 = MakeFourCC("RA24"),
	BGRA8888 = MakeFourCC("BA24"),

	RGB888 = MakeFourCC("RG24"),
	BGR888 = MakeFourCC("BG24"),

	RGB332 = MakeFourCC("RGB8"),

	RGB565 = MakeFourCC("RG16"),
	BGR565 = MakeFourCC("BG16"),

	XRGB4444 = MakeFourCC("XR12"),
	XRGB1555 = MakeFourCC("XR15"),

	ARGB4444 = MakeFourCC("AR12"),
	ARGB1555 = MakeFourCC("AR15"),

	XRGB2101010 = MakeFourCC("XR30"),
	XBGR2101010 = MakeFourCC("XB30"),
	RGBX1010102 = MakeFourCC("RX30"),
	BGRX1010102 = MakeFourCC("BX30"),

	ARGB2101010 = MakeFourCC("AR30"),
	ABGR2101010 = MakeFourCC("AB30"),
	RGBA1010102 = MakeFourCC("RA30"),
	BGRA1010102 = MakeFourCC("BA30"),
};

inline PixelFormat FourCCToPixelFormat(const std::string& fourcc)
{
	return (PixelFormat)MakeFourCC(fourcc.c_str());
}

inline std::string PixelFormatToFourCC(PixelFormat f)
{
	char buf[5] = { (char)(((uint32_t)f >> 0) & 0xff),
			(char)(((uint32_t)f >> 8) & 0xff),
			(char)(((uint32_t)f >> 16) & 0xff),
			(char)(((uint32_t)f >> 24) & 0xff),
			0 };
	return std::string(buf);
}

enum class PixelColorType {
	RGB,
	YUV,
};

struct PixelFormatPlaneInfo {
	uint8_t bitspp;
	uint8_t xsub;
	uint8_t ysub;
};

struct PixelFormatInfo {
	PixelColorType type;
	uint8_t num_planes;
	struct PixelFormatPlaneInfo planes[4];
};

const struct PixelFormatInfo& get_pixel_format_info(PixelFormat format);

} // namespace kms
