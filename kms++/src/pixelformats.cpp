#include <map>

#include <kms++/pixelformats.h>

using namespace std;

namespace kms
{
static const map<PixelFormat, PixelFormatInfo> format_info_array = {
	/* YUV packed */
	{ PixelFormat::UYVY, { 1, { { 16, 2, 1 } }, } },
	{ PixelFormat::YUYV, { 1, { { 16, 2, 1 } }, } },
	{ PixelFormat::YVYU, { 1, { { 16, 2, 1 } }, } },
	{ PixelFormat::VYUY, { 1, { { 16, 2, 1 } }, } },
	/* YUV semi-planar */
	{ PixelFormat::NV12, { 2, { { 8, 1, 1, }, { 8, 2, 2 } }, } },
	{ PixelFormat::NV21, { 2, { { 8, 1, 1, }, { 8, 2, 2 } }, } },
	/* RGB16 */
	{ PixelFormat::RGB565, { 1, { { 16, 1, 1 } }, } },
	{ PixelFormat::BGR565, { 1, { { 16, 1, 1 } }, } },
	/* RGB24 */
	{ PixelFormat::RGB888, { 1, { { 24, 1, 1 } }, } },
	{ PixelFormat::BGR888, { 1, { { 24, 1, 1 } }, } },
	/* RGB32 */
	{ PixelFormat::XRGB8888, { 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::XBGR8888, { 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::ARGB8888, { 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::ABGR8888, { 1, { { 32, 1, 1 } }, } },
};

const struct PixelFormatInfo& get_pixel_format_info(PixelFormat format)
{
	return format_info_array.at(format);
}

}
