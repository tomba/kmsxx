#pragma once

#include <kms++/ion/ionkms++.h>
#include <kms++/pixelformats.h>

namespace kms
{

class IonBuffer
{
public:
	IonBuffer(Ion& ion, int heap_type, int plane, uint32_t width, uint32_t height, const std::string& fourcc);
	IonBuffer(Ion& ion, int heap_type, int plane, uint32_t width, uint32_t height, PixelFormat format);
	virtual ~IonBuffer();

	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }
	uint32_t stride() const { return m_stride; }
	uint32_t offset() const { return m_offset; }
	PixelFormat format() const { return m_format; }
	int prime_fd() const { return m_prime_fd; };

private:
	void Create();
	void Destroy();

	Ion& m_ion;

	ion_heap_type m_heap_type;
	int m_plane;
	uint32_t m_width;
	uint32_t m_height;
	PixelFormat m_format;
	uint32_t m_stride;
	uint32_t m_offset;

	int m_prime_fd;

};
}
