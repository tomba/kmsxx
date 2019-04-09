#include <map>

#include <kms++/pixelformats.h>

using namespace std;

namespace kms
{
static const map<PixelFormat, PixelFormatInfo> format_info_array = {
	/* YUV packed */
	{ PixelFormat::UYVY, { PixelColorType::YUV, 1, { { 16, 2, 1 } }, } },
	{ PixelFormat::YUYV, { PixelColorType::YUV, 1, { { 16, 2, 1 } }, } },
	{ PixelFormat::YVYU, { PixelColorType::YUV, 1, { { 16, 2, 1 } }, } },
	{ PixelFormat::VYUY, { PixelColorType::YUV, 1, { { 16, 2, 1 } }, } },
	/* YUV semi-planar */
	{ PixelFormat::NV12, { PixelColorType::YUV, 2, { { 8, 1, 1, }, { 8, 2, 2 } }, } },
	{ PixelFormat::NV21, { PixelColorType::YUV, 2, { { 8, 1, 1, }, { 8, 2, 2 } }, } },
	/* RGB16 */
	{ PixelFormat::RGB565, { PixelColorType::RGB, 1, { { 16, 1, 1 } }, } },
	{ PixelFormat::BGR565, { PixelColorType::RGB, 1, { { 16, 1, 1 } }, } },
	/* RGB24 */
	{ PixelFormat::RGB888, { PixelColorType::RGB, 1, { { 24, 1, 1 } }, } },
	{ PixelFormat::BGR888, { PixelColorType::RGB, 1, { { 24, 1, 1 } }, } },
	/* RGB32 */
	{ PixelFormat::XRGB8888, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::XBGR8888, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::RGBX8888, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::BGRX8888, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },

	{ PixelFormat::ARGB8888, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::ABGR8888, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::RGBA8888, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::BGRA8888, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },

	{ PixelFormat::XRGB2101010, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::XBGR2101010, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::RGBX1010102, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::BGRX1010102, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },

	{ PixelFormat::ARGB2101010, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::ABGR2101010, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::RGBA1010102, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::BGRA1010102, { PixelColorType::RGB, 1, { { 32, 1, 1 } }, } },

	{ PixelFormat::ARGB4444, { PixelColorType::RGB, 1, { { 16, 1, 1 } }, } },
	{ PixelFormat::ARGB1555, { PixelColorType::RGB, 1, { { 16, 1, 1 } }, } },
};

const struct PixelFormatInfo& get_pixel_format_info(PixelFormat format)
{
	if (!format_info_array.count(format))
		throw invalid_argument("get_pixel_format_info: Unsupported pixelformat");

	return format_info_array.at(format);
}

}
