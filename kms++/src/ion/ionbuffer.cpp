#include <kms++/kms++.h>
#include <kms++/ion/ionbuffer.h>

using namespace std;

namespace kms
{

IonBuffer::IonBuffer(Ion &ion, int heap_type, int plane, uint32_t width, uint32_t height, const string& fourcc)
	:IonBuffer(ion, heap_type, plane, width, height, FourCCToPixelFormat(fourcc))
{
}

IonBuffer::IonBuffer(Ion& ion, int heap_type, int plane, uint32_t width, uint32_t height, PixelFormat format)
	:m_ion(ion), m_heap_type(static_cast<ion_heap_type>(heap_type)), m_plane(plane), m_width(width), m_height(height), m_format(format)
{
	Create();
}

IonBuffer::~IonBuffer()
{
	Destroy();
}

void IonBuffer::Create()
{
	const PixelFormatInfo& format_info = get_pixel_format_info(m_format);
	size_t len;

	if (m_plane >= format_info.num_planes)
		throw invalid_argument("Invalid plane index");

	const PixelFormatPlaneInfo& pi = format_info.planes[m_plane];

	len = width() * height() / pi.ysub * pi.bitspp / 8;
	m_prime_fd = m_ion.alloc(m_heap_type, len);
	m_stride = width() * pi.bitspp / 8;
	m_offset = 0;
}

void IonBuffer::Destroy()
{
	m_ion.free(m_prime_fd);
}

}
