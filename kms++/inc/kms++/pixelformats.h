#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace kms
{
constexpr uint32_t MakeFourCC(const std::string& fourcc)
{
	if (fourcc.empty())
		return 0;

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
	uint32_t v = (uint32_t)f;

	char buf[4] = { (char)((v >> 0) & 0xff),
			(char)((v >> 8) & 0xff),
			(char)((v >> 16) & 0xff),
			(char)((v >> 24) & 0xff),
			};
	return std::string(buf, 4);
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
		: name(name), drm_fourcc(kms::MakeFourCC(drm_fourcc)),
		  v4l2_4cc(kms::MakeFourCC(v4l2_4cc)), type(color),
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
