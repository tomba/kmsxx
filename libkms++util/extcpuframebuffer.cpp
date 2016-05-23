
#include "extcpuframebuffer.h"
#include "kms++util.h"

using namespace std;

namespace kms
{

ExtCPUFramebuffer::ExtCPUFramebuffer(uint32_t width, uint32_t height, PixelFormat format,
				     uint8_t* buffer, uint32_t pitch)
	: m_width(width), m_height(height), m_format(format)
{
	const PixelFormatInfo& format_info = get_pixel_format_info(m_format);

	m_num_planes = format_info.num_planes;

	ASSERT(m_num_planes == 1);

	const PixelFormatPlaneInfo& pi = format_info.planes[0];
	FramebufferPlane& plane = m_planes[0];

	plane.stride = pitch;
	plane.size = plane.stride * height / pi.ysub;
	plane.map = buffer;
}

ExtCPUFramebuffer::ExtCPUFramebuffer(uint32_t width, uint32_t height, PixelFormat format,
				     uint8_t* buffers[4], uint32_t pitches[4])
	: m_width(width), m_height(height), m_format(format)
{
	const PixelFormatInfo& format_info = get_pixel_format_info(m_format);

	m_num_planes = format_info.num_planes;

	for (unsigned i = 0; i < format_info.num_planes; ++i) {
		const PixelFormatPlaneInfo& pi = format_info.planes[i];
		FramebufferPlane& plane = m_planes[i];

		plane.stride = pitches[i];
		plane.size = plane.stride * height / pi.ysub;
		plane.map = buffers[i];
	}
}

ExtCPUFramebuffer::~ExtCPUFramebuffer()
{
}

}
