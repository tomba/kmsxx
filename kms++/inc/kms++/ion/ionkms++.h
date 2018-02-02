#pragma once

#include <list>
#include <kms++/ion/ion.h>

namespace kms
{
class Ion
{
public:
	Ion();
	Ion(const std::string& device);
	virtual ~Ion();

	Ion(const Ion& other) = delete;
	Ion& operator=(const Ion& other) = delete;

	int fd() const { return m_fd; }

	int alloc(ion_heap_type type, size_t len);
	void free(int fd);

private:
	const int MAX_HEAP_COUNT = ION_HEAP_TYPE_CUSTOM;

	int m_fd;

	std::map<ion_heap_type, ion_heap_data> m_heaps;
	std::list<int> m_buf_fd;

	void free_internal(int fd);
};
}
