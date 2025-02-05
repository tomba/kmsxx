#include <map>
#include <stdexcept>
#include <cassert>

#include <kms++/pixelformats.h>

using namespace std;

namespace kms
{
static map<PixelFormat, PixelFormatInfo> format_info_array = {
	{
		PixelFormat::R8, {
			PixelFormatInfo {
				"R8",
				"R8  ",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 1, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::RGB332, {
			PixelFormatInfo {
				"RGB332",
				"RGB8",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 1, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::RGB565, {
			PixelFormatInfo {
				"RGB565",
				"RG16",
				"RGBP",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::BGR565, {
			PixelFormatInfo {
				"BGR565",
				"BG16",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::XRGB1555, {
			PixelFormatInfo {
				"XRGB1555",
				"XR15",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::RGBX4444, {
			PixelFormatInfo {
				"RGBX4444",
				"RX12",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::XRGB4444, {
			PixelFormatInfo {
				"XRGB4444",
				"XR12",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::ARGB1555, {
			PixelFormatInfo {
				"ARGB1555",
				"AR15",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::RGBA4444, {
			PixelFormatInfo {
				"RGBA4444",
				"RA12",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::ARGB4444, {
			PixelFormatInfo {
				"ARGB4444",
				"AR12",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::RGB888, {
			PixelFormatInfo {
				"RGB888",
				"RG24",
				"BGR3",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 3, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::BGR888, {
			PixelFormatInfo {
				"BGR888",
				"BG24",
				"RGB3",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 3, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::XRGB8888, {
			PixelFormatInfo {
				"XRGB8888",
				"XR24",
				"XR24",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::XBGR8888, {
			PixelFormatInfo {
				"XBGR8888",
				"XB24",
				"XB24",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::RGBX8888, {
			PixelFormatInfo {
				"RGBX8888",
				"RX24",
				"RX24",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::BGRX8888, {
			PixelFormatInfo {
				"BGRX8888",
				"BX24",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::XBGR2101010, {
			PixelFormatInfo {
				"XBGR2101010",
				"XB30",
				"RX30",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::XRGB2101010, {
			PixelFormatInfo {
				"XRGB2101010",
				"XR30",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::RGBX1010102, {
			PixelFormatInfo {
				"RGBX1010102",
				"RX30",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::BGRX1010102, {
			PixelFormatInfo {
				"BGRX1010102",
				"BX30",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::ARGB8888, {
			PixelFormatInfo {
				"ARGB8888",
				"AR24",
				"AR24",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::ABGR8888, {
			PixelFormatInfo {
				"ABGR8888",
				"AB24",
				"AB24",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::RGBA8888, {
			PixelFormatInfo {
				"RGBA8888",
				"RA24",
				"RA24",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::BGRA8888, {
			PixelFormatInfo {
				"BGRA8888",
				"BA24",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::ABGR2101010, {
			PixelFormatInfo {
				"ABGR2101010",
				"AB30",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::ARGB2101010, {
			PixelFormatInfo {
				"ARGB2101010",
				"AR30",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::RGBA1010102, {
			PixelFormatInfo {
				"RGBA1010102",
				"RA30",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::BGRA1010102, {
			PixelFormatInfo {
				"BGRA1010102",
				"BA30",
				"",
				PixelColorType::RGB,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::YUYV, {
			PixelFormatInfo {
				"YUYV",
				"YUYV",
				"YUYV",
				PixelColorType::YUV,
				{ 2, 1 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::UYVY, {
			PixelFormatInfo {
				"UYVY",
				"UYVY",
				"UYVY",
				PixelColorType::YUV,
				{ 2, 1 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::YVYU, {
			PixelFormatInfo {
				"YVYU",
				"YVYU",
				"YVYU",
				PixelColorType::YUV,
				{ 2, 1 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::VYUY, {
			PixelFormatInfo {
				"VYUY",
				"VYUY",
				"VYUY",
				PixelColorType::YUV,
				{ 2, 1 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::VUY888, {
			PixelFormatInfo {
				"VUY888",
				"VU24",
				"YUV3",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 3, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::XVUY8888, {
			PixelFormatInfo {
				"XVUY8888",
				"XVUY",
				"YUVX",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::Y210, {
			PixelFormatInfo {
				"Y210",
				"Y210",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 8, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::Y212, {
			PixelFormatInfo {
				"Y212",
				"Y212",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 8, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::Y216, {
			PixelFormatInfo {
				"Y216",
				"Y216",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 8, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::NV12, {
			PixelFormatInfo {
				"NV12",
				"NV12",
				"NM12",
				PixelColorType::YUV,
				{ 2, 2 },
				{ { 1, 1, 1, 1 }, { 2, 1, 2, 2 } },
			}
		}
	},
	{
		PixelFormat::NV21, {
			PixelFormatInfo {
				"NV21",
				"NV21",
				"NM21",
				PixelColorType::YUV,
				{ 2, 2 },
				{ { 1, 1, 1, 1 }, { 2, 1, 2, 2 } },
			}
		}
	},
	{
		PixelFormat::NV16, {
			PixelFormatInfo {
				"NV16",
				"NV16",
				"NM16",
				PixelColorType::YUV,
				{ 2, 1 },
				{ { 1, 1, 1, 1 }, { 2, 1, 2, 1 } },
			}
		}
	},
	{
		PixelFormat::NV61, {
			PixelFormatInfo {
				"NV61",
				"NV61",
				"NM61",
				PixelColorType::YUV,
				{ 2, 1 },
				{ { 1, 1, 1, 1 }, { 2, 1, 2, 1 } },
			}
		}
	},
	{
		PixelFormat::XV15, {
			PixelFormatInfo {
				"XV15",
				"XV15",
				"",
				PixelColorType::YUV,
				{ 6, 2 },
				{ { 4, 3, 1, 1 }, { 8, 3, 2, 2 } },
			}
		}
	},
	{
		PixelFormat::XV20, {
			PixelFormatInfo {
				"XV20",
				"XV20",
				"",
				PixelColorType::YUV,
				{ 6, 2 },
				{ { 4, 3, 1, 1 }, { 8, 3, 2, 1 } },
			}
		}
	},
	{
		PixelFormat::XVUY2101010, {
			PixelFormatInfo {
				"XVUY2101010",
				"XY30",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::YUV420, {
			PixelFormatInfo {
				"YUV420",
				"YU12",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 1, 1, 1, 1 }, { 1, 1, 2, 2 }, { 1, 1, 2, 2 } },
			}
		}
	},
	{
		PixelFormat::YVU420, {
			PixelFormatInfo {
				"YVU420",
				"YV12",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 1, 1, 1, 1 }, { 1, 1, 2, 2 }, { 1, 1, 2, 2 } },
			}
		}
	},
	{
		PixelFormat::YUV422, {
			PixelFormatInfo {
				"YUV422",
				"YU16",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 1, 1, 1, 1 }, { 1, 1, 2, 1 }, { 1, 1, 2, 1 } },
			}
		}
	},
	{
		PixelFormat::YVU422, {
			PixelFormatInfo {
				"YVU422",
				"YV16",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 1, 1, 1, 1 }, { 1, 1, 2, 1 }, { 1, 1, 2, 1 } },
			}
		}
	},
	{
		PixelFormat::YUV444, {
			PixelFormatInfo {
				"YUV444",
				"YU24",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::YVU444, {
			PixelFormatInfo {
				"YVU444",
				"YV24",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::X403, {
			PixelFormatInfo {
				"X403",
				"X403",
				"",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 4, 1, 1, 1 }, { 4, 1, 1, 1 }, { 4, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::Y8, {
			PixelFormatInfo {
				"Y8",
				"GREY",
				"GREY",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 1, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::Y10, {
			PixelFormatInfo {
				"Y10",
				"",
				"Y10 ",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::Y10P, {
			PixelFormatInfo {
				"Y10P",
				"",
				"Y10P",
				PixelColorType::YUV,
				{ 4, 1 },
				{ { 5, 4, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::Y12, {
			PixelFormatInfo {
				"Y12",
				"",
				"Y12 ",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 2, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::Y12P, {
			PixelFormatInfo {
				"Y12P",
				"",
				"Y12P",
				PixelColorType::YUV,
				{ 2, 1 },
				{ { 3, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::Y10_P32, {
			PixelFormatInfo {
				"Y10_P32",
				"YPA4",
				"",
				PixelColorType::YUV,
				{ 3, 1 },
				{ { 4, 3, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SBGGR8, {
			PixelFormatInfo {
				"SBGGR8",
				"",
				"BA81",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 1, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGBRG8, {
			PixelFormatInfo {
				"SGBRG8",
				"",
				"GBRG",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 1, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGRBG8, {
			PixelFormatInfo {
				"SGRBG8",
				"",
				"GRBG",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 1, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SRGGB8, {
			PixelFormatInfo {
				"SRGGB8",
				"",
				"RGGB",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 1, 1, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SBGGR10, {
			PixelFormatInfo {
				"SBGGR10",
				"",
				"BG10",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGBRG10, {
			PixelFormatInfo {
				"SGBRG10",
				"",
				"GB10",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGRBG10, {
			PixelFormatInfo {
				"SGRBG10",
				"",
				"BA10",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SRGGB10, {
			PixelFormatInfo {
				"SRGGB10",
				"",
				"RG10",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SBGGR10P, {
			PixelFormatInfo {
				"SBGGR10P",
				"",
				"pBAA",
				PixelColorType::RAW,
				{ 4, 2 },
				{ { 5, 4, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGBRG10P, {
			PixelFormatInfo {
				"SGBRG10P",
				"",
				"pGAA",
				PixelColorType::RAW,
				{ 4, 2 },
				{ { 5, 4, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGRBG10P, {
			PixelFormatInfo {
				"SGRBG10P",
				"",
				"pgAA",
				PixelColorType::RAW,
				{ 4, 2 },
				{ { 5, 4, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SRGGB10P, {
			PixelFormatInfo {
				"SRGGB10P",
				"",
				"pRAA",
				PixelColorType::RAW,
				{ 4, 2 },
				{ { 5, 4, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SBGGR12, {
			PixelFormatInfo {
				"SBGGR12",
				"",
				"BG12",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGBRG12, {
			PixelFormatInfo {
				"SGBRG12",
				"",
				"GB12",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGRBG12, {
			PixelFormatInfo {
				"SGRBG12",
				"",
				"BA12",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SRGGB12, {
			PixelFormatInfo {
				"SRGGB12",
				"",
				"RG12",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SBGGR12P, {
			PixelFormatInfo {
				"SBGGR12P",
				"",
				"pBCC",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 3, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGBRG12P, {
			PixelFormatInfo {
				"SGBRG12P",
				"",
				"pGCC",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 3, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SGRBG12P, {
			PixelFormatInfo {
				"SGRBG12P",
				"",
				"pgCC",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 3, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SRGGB12P, {
			PixelFormatInfo {
				"SRGGB12P",
				"",
				"pRCC",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 3, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::SRGGB16, {
			PixelFormatInfo {
				"SRGGB16",
				"",
				"RG16",
				PixelColorType::RAW,
				{ 2, 2 },
				{ { 4, 2, 1, 1 } },
			}
		}
	},
	{
		PixelFormat::MJPEG, {
			PixelFormatInfo {
				"MJPEG",
				"MJPG",
				"MJPG",
				PixelColorType::YUV,
				{ 1, 1 },
				{ { 1, 1, 1, 1 } },
			}
		}
	},
};

const struct PixelFormatInfo& get_pixel_format_info(PixelFormat format)
{
	if (!format_info_array.count(format))
		throw invalid_argument("get_pixel_format_info: Unsupported pixelformat");

	return format_info_array.at(format);
}

PixelFormat fourcc_to_pixel_format(uint32_t fourcc)
{
	for (const auto& [fmt, pfi] : format_info_array) {
		if (pfi.drm_fourcc == fourcc)
			return fmt;
	}

	throw invalid_argument("FourCC not supported");
}

uint32_t pixel_format_to_fourcc(PixelFormat f)
{
	return format_info_array.at(f).drm_fourcc;
}

PixelFormat fourcc_str_to_pixel_format(const std::string& fourcc)
{
	return fourcc_to_pixel_format(str_to_fourcc(fourcc));
}

std::string pixel_format_to_fourcc_str(PixelFormat f)
{
	return fourcc_to_str(format_info_array.at(f).drm_fourcc);
}

static constexpr uint32_t _div_round_up(uint32_t a, uint32_t b)
{
	return (a + b - 1) / b;
}

static constexpr uint32_t _align_up(uint32_t a, uint32_t b)
{
	return _div_round_up(a, b) * b;
}

std::tuple<uint32_t, uint32_t> PixelFormatInfo::align_pixels(uint32_t width,
							     uint32_t height) const
{
	return { _align_up(width, std::get<0>(pixel_align)),
		 _align_up(height, std::get<1>(pixel_align)) };
}

uint32_t PixelFormatInfo::stride(uint32_t width, uint32_t plane, uint32_t align) const
{
	if (plane >= num_planes)
		throw std::runtime_error("Invalid plane number");

	assert(width % std::get<0>(pixel_align) == 0);

	const auto& pi = planes[plane];

	assert(width % pi.pixels_per_block == 0);
	uint32_t stride = width / pi.pixels_per_block * pi.bytes_per_block;

	assert(stride % pi.hsub == 0);
	stride = stride / pi.hsub;

	stride = _align_up(stride, align);

	return stride;
}

uint32_t PixelFormatInfo::planesize(uint32_t stride, uint32_t height,
				    uint32_t plane) const
{
	assert(height % std::get<1>(pixel_align) == 0);

	const auto& pi = planes[plane];

	assert(height % pi.vsub == 0);

	return stride * (height / pi.vsub);
}

uint32_t PixelFormatInfo::framesize(uint32_t width, uint32_t height,
				    uint32_t align) const
{
	uint32_t size = 0;

	for (uint32_t i = 0; i < num_planes; ++i) {
		uint32_t s = stride(width, i, align);
		size += planesize(s, height, i);
	}

	return size;
}

std::tuple<uint32_t, uint32_t, uint32_t> PixelFormatInfo::dumb_size(uint32_t width,
								    uint32_t height,
								    uint32_t plane,
								    uint32_t align) const
{
	/*
        Helper function mainly for DRM dumb framebuffer
        Returns (width, height, bitspp) tuple which results in a suitable plane
        size.

        DRM_IOCTL_MODE_CREATE_DUMB takes a 'bpp' (bits-per-pixel) argument,
        which is then used with the width and height to allocate the buffer.
        This doesn't work for pixel formats where the average bits-per-pixel
        is not an integer (e.g. XV15)

        So, we instead use the bytes_per_block (in bits) as
        the 'bpp' argument, and adjust the width accordingly.
        */

	assert(height % std::get<1>(pixel_align) == 0);

	const auto& pi = planes[plane];

	assert(height % pi.vsub == 0);

	uint32_t stride = PixelFormatInfo::stride(width, plane, align);

	assert(stride % pi.bytes_per_block == 0);

	width = stride / pi.bytes_per_block;
	height = height / pi.vsub;
	uint32_t bitspp = pi.bytes_per_block * 8;

	return { width, height, bitspp };
}

} // namespace kms
