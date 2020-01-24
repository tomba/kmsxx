#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdexcept>
#include <array>

#include <linux/ion.h>

#include <kms++gl/ion.h>

using namespace std;
using namespace kms;

Ion::Ion()
{
	m_fd = open("/dev/ion", O_RDWR);
	if (m_fd < 0)
		throw runtime_error("Failed to open /dev/ion");

	array<ion_heap_data, 10> heaps;

	//ion_heap_data heaps[10] {};

	ion_heap_query ihq {};
	ihq.cnt = heaps.size();
	ihq.heaps = (uint64_t)&heaps;

	int r = ioctl(m_fd, ION_IOC_HEAP_QUERY, &ihq);
	if (r < 0)
		throw runtime_error("ION heap query failed");

	for (uint32_t i = 0; i < ihq.cnt; ++i) {
		const auto& h = heaps[i];
		m_heaps.push_back({h.name, h.heap_id, (IonHeapType)h.type});
	}
}

int Ion::alloc(size_t size, Ion::IonHeapType type, bool cached)
{
	uint32_t id_mask = 0;
	for (const auto& h : m_heaps) {
		if (h.type == type)
			id_mask |= 1 << h.id;
	}

	if (id_mask == 0)
		throw runtime_error("ION alloc failed: no heaps found");

	ion_allocation_data iad {};
	iad.len = size;
	iad.heap_id_mask = id_mask;
	iad.flags = cached ? ION_FLAG_CACHED : 0;

	int r = ioctl(m_fd, ION_IOC_ALLOC, &iad);
	if (r < 0)
		throw runtime_error("ION alloc failed");

	return iad.fd;
}

Ion::~Ion()
{
	close(m_fd);
	m_fd = 0;
}
