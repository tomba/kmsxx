#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <linux/dma-buf.h>

#include <linux/ti-pat.h>

#include <kms++gl/ti-pat.h>

using namespace std;
using namespace kms;


Pat::Pat()
{
	m_fd = open("/dev/31010000.pat", O_RDWR);
	if (m_fd < 0)
		throw runtime_error("failed to open PAT");
}

int Pat::export_buf(int backing_fd)
{
	ti_pat_export_data ped {};
	ped.fd = backing_fd;
	ped.flags = 0;

	int r = ioctl(m_fd, TI_PAT_IOC_EXPORT, &ped);
	if (r)
		throw runtime_error("pat map failed");

	return ped.fd;
}

PatFramebuffer::PatFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format, int fd, int backing_fd, uint32_t pitch)
	: DmabufFramebuffer(card, width, height, format, vector<int> { fd }, { pitch }, { 0 }),
	  m_backing_fd(backing_fd)
{

}

uint8_t* PatFramebuffer::map(unsigned plane)
{
	assert(plane == 0);

	if (!m_backing_fd)
		throw invalid_argument("cannot mmap non-dmabuf fb");

	if (m_map)
		return m_map;

	m_map = (uint8_t *)mmap(0, size(plane), PROT_READ | PROT_WRITE, MAP_SHARED,
				m_backing_fd, 0);
	if (m_map == MAP_FAILED)
		throw invalid_argument(string("mmap failed: ") + strerror(errno));

	return m_map;
}

int PatFramebuffer::prime_fd(unsigned plane)
{
	assert(plane == 0);

	if (!m_backing_fd)
		throw invalid_argument("no primefb for non-dmabuf fb");

	return m_backing_fd;
}

void PatFramebuffer::begin_cpu_access(CpuAccess access)
{
	printf("PAT SYNC\n");

	if (m_sync_flags != 0)
		throw runtime_error("begin_cpu sync already started");

	switch (access) {
	case CpuAccess::Read:
		m_sync_flags = DMA_BUF_SYNC_READ;
		break;
	case CpuAccess::Write:
		m_sync_flags = DMA_BUF_SYNC_WRITE;
		break;
	case CpuAccess::ReadWrite:
		m_sync_flags = DMA_BUF_SYNC_RW;
		break;
	}

	dma_buf_sync dbs {
		.flags = DMA_BUF_SYNC_START | m_sync_flags
	};

	int r = ioctl(m_backing_fd, DMA_BUF_IOCTL_SYNC, &dbs);
	if (r)
		throw runtime_error("DMA_BUF_IOCTL_SYNC failed");
}

void PatFramebuffer::end_cpu_access()
{
	if (m_sync_flags == 0)
		throw runtime_error("begin_cpu sync not started");

	dma_buf_sync dbs {
		.flags = DMA_BUF_SYNC_END | m_sync_flags
	};

	int r = ioctl(m_backing_fd, DMA_BUF_IOCTL_SYNC, &dbs);
	if (r)
		throw runtime_error("DMA_BUF_IOCTL_SYNC failed");

	m_sync_flags = 0;
}
