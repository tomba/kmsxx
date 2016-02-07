#include <map>

#include "mappedbuffer.h"

using namespace std;

namespace kms {

struct FormatPlaneInfo
{
	uint8_t bitspp;	/* bits per (macro) pixel */
	uint8_t xsub;
	uint8_t ysub;
};

struct FormatInfo
{
	uint8_t num_planes;
	struct FormatPlaneInfo planes[4];
};

static const map<PixelFormat, FormatInfo> format_info_array = {
	/* YUV packed */
	{ PixelFormat::UYVY, { 1, { { 32, 2, 1 } }, } },
	{ PixelFormat::YUYV, { 1, { { 32, 2, 1 } }, } },
	{ PixelFormat::YVYU, { 1, { { 32, 2, 1 } }, } },
	{ PixelFormat::VYUY, { 1, { { 32, 2, 1 } }, } },
	/* YUV semi-planar */
	{ PixelFormat::NV12, { 2, { { 8, 1, 1, }, { 16, 2, 2 } }, } },
	{ PixelFormat::NV21, { 2, { { 8, 1, 1, }, { 16, 2, 2 } }, } },
	/* RGB16 */
	{ PixelFormat::RGB565, { 1, { { 16, 1, 1 } }, } },
	/* RGB32 */
	{ PixelFormat::XRGB8888, { 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::XBGR8888, { 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::ARGB8888, { 1, { { 32, 1, 1 } }, } },
	{ PixelFormat::ABGR8888, { 1, { { 32, 1, 1 } }, } },
};

MappedCPUBuffer::MappedCPUBuffer(uint32_t width, uint32_t height, PixelFormat format)
	: m_width(width), m_height(height), m_format(format)
{
	const FormatInfo& format_info = format_info_array.at(m_format);

	m_num_planes = format_info.num_planes;

	for (unsigned i = 0; i < format_info.num_planes; ++i) {
		const FormatPlaneInfo& pi = format_info.planes[i];
		FramebufferPlane& plane = m_planes[i];

		plane.stride = width * pi.bitspp / 8 / pi.xsub;
		plane.size = plane.stride * height/ pi.ysub;
		plane.offset = 0;
		plane.map = new uint8_t[plane.size];
	}
}

MappedCPUBuffer::~MappedCPUBuffer()
{
	for (unsigned i = 0; i < m_num_planes; ++i) {
		FramebufferPlane& plane = m_planes[i];

		delete plane.map;
	}
}

}
