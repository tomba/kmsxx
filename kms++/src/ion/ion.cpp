#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#include <kms++/kms++.h>
#include "../../inc/kms++/ion/ionkms++.h"

using namespace std;

/*
 * Based on: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/tools/testing/selftests/android/ion/
 */

namespace kms
{

Ion::Ion()
	: Ion("/dev/ion")
{
}

Ion::Ion(const std::string& device)
{
	struct ion_heap_query query {};
	struct ion_heap_data heap_data[MAX_HEAP_COUNT];
	int fd, ret;
	uint32_t i;

	fd = open(device.c_str(), O_RDWR);
	if (fd < 0)
		throw invalid_argument(string(strerror(errno)) + " opening " + device);

	m_fd = fd;

	query.cnt = MAX_HEAP_COUNT;
	query.heaps = (unsigned long int)&heap_data[0];
	/* Query ION heap_id_mask from ION heap */
	ret = ioctl(fd, ION_IOC_HEAP_QUERY, &query);
	if (ret < 0)
		throw invalid_argument("Failed: ION_IOC_HEAP_QUERY: " + string(strerror(errno)));

	for (i = 0; i < query.cnt; i++)
		m_heaps[static_cast<ion_heap_type>(heap_data[i].type)] = heap_data[i];
}

Ion::~Ion()
{
	for (std::list<int>::iterator i = m_buf_fd.begin(); i != m_buf_fd.end(); i++) {
		free_internal(*i);
		i = m_buf_fd.erase(i);
	}

	close(m_fd);
}

int Ion::alloc(ion_heap_type type, size_t len)
{
	struct ion_allocation_data alloc_data {};
	std::map<ion_heap_type, ion_heap_data>::iterator it;
	int ret;

	it = m_heaps.find(type);
	if (it == m_heaps.end())
		throw invalid_argument("Wrong ION heap type: " + type);

	alloc_data.len = len;
	alloc_data.heap_id_mask = 1 << it->second.heap_id;
	alloc_data.flags = 0;

	/* Allocate memory for this ION client as per heap_type */
	ret = ioctl(m_fd, ION_IOC_ALLOC, &alloc_data);
	if (ret < 0)
		throw invalid_argument("Failed: ION_IOC_ALLOC: " + string(strerror(errno)));

	if (static_cast<signed>(alloc_data.fd) < 0 || alloc_data.len <= 0)
		throw invalid_argument(std::string("Invalid map data, fd: " + to_string(alloc_data.fd) +
				", len: " + to_string(alloc_data.len)));

	m_buf_fd.push_back(alloc_data.fd);
	return alloc_data.fd;
}

void Ion::free(int fd)
{
	m_buf_fd.remove(fd);
	free_internal(fd);
}

void Ion::free_internal(int fd)
{
	close(fd);
}

}
