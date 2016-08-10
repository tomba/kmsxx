#pragma once

#include <cstdint>
#include <string>

namespace kms
{
constexpr uint32_t MakeFourCC(const char *fourcc)
{
	return fourcc[0] | (fourcc[1] << 8) | (fourcc[2] << 16) | (fourcc[3] << 24);
}

enum class PixelFormat : uint32_t
{
	Undefined = 0,

	NV12 = MakeFourCC("NV12"),
	NV21 = MakeFourCC("NV21"),

	UYVY = MakeFourCC("UYVY"),
	YUYV = MakeFourCC("YUYV"),
	YVYU = MakeFourCC("YVYU"),
	VYUY = MakeFourCC("VYUY"),

	XRGB8888 = MakeFourCC("XR24"),
	XBGR8888 = MakeFourCC("XB24"),
	ARGB8888 = MakeFourCC("AR24"),
	ABGR8888 = MakeFourCC("AB24"),

	RGB888 = MakeFourCC("RG24"),
	BGR888 = MakeFourCC("BG24"),

	RGB565 = MakeFourCC("RG16"),
	BGR565 = MakeFourCC("BG16"),
};

static inline PixelFormat FourCCToPixelFormat(const std::string& fourcc)
{
	return (PixelFormat)MakeFourCC(fourcc.c_str());
}

static inline std::string PixelFormatToFourCC(PixelFormat f)
{
	char buf[5] = { (char)(((int)f >> 0) & 0xff),
			(char)(((int)f >> 8) & 0xff),
			(char)(((int)f >> 16) & 0xff),
			(char)(((int)f >> 24) & 0xff),
			0 };
	return std::string(buf);
}

struct PixelFormatPlaneInfo
{
	uint8_t bitspp;
	uint8_t xsub;
	uint8_t ysub;
};

struct PixelFormatInfo
{
	uint8_t num_planes;
	struct PixelFormatPlaneInfo planes[4];
};

const struct PixelFormatInfo& get_pixel_format_info(PixelFormat format);

}
