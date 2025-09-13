#include <map>

#include <kms++util/cpuframebuffer.h>

using namespace std;

namespace kms
{
CPUFramebuffer::CPUFramebuffer(uint32_t width, uint32_t height, PixelFormat format)
	: m_width(width), m_height(height), m_format(format)
{
	const PixelFormatInfo& format_info = get_pixel_format_info(m_format);

	m_num_planes = format_info.num_planes;

	for (unsigned i = 0; i < format_info.num_planes; ++i) {
		FramebufferPlane& plane = m_planes[i];

		plane.stride = format_info.stride(width, i);
		plane.size = format_info.planesize(plane.stride, height, i);
		plane.offset = 0;
		plane.map = new uint8_t[plane.size];
	}
}

CPUFramebuffer::~CPUFramebuffer()
{
	for (unsigned i = 0; i < m_num_planes; ++i) {
		const FramebufferPlane& plane = m_planes[i];

		delete[] plane.map;
	}
}

} // namespace kms
