#pragma once

#include <kms++/dmabufframebuffer.h>

namespace kms {

class Pat
{
public:
	Pat();

	int export_buf(int backing_fd);

private:
	int m_fd;
};

class PatFramebuffer : public DmabufFramebuffer
{
public:
	PatFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format, int fd, int backing_fd, uint32_t pitch);

	uint8_t* map(unsigned plane);

	int prime_fd(unsigned plane);

	void begin_cpu_access(CpuAccess access);

	void end_cpu_access();
private:
	int m_backing_fd = 0;
	uint8_t *m_map;
	uint32_t m_sync_flags = 0;
};

}
