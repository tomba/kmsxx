#include <map>
#include <stdexcept>
#include <cassert>

#include <kms++/pixelformats.h>

using namespace std;

namespace kms
{
static map<PixelFormat, PixelFormatInfo> format_info_array = {
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
};

const struct PixelFormatInfo& get_pixel_format_info(PixelFormat format)
{
	if (!format_info_array.count(format))
		throw invalid_argument("get_pixel_format_info: Unsupported pixelformat");

	return format_info_array.at(format);
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
