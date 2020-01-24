#pragma once

#include <string>
#include <vector>

namespace kms {

class Ion
{
public:
	enum class IonHeapType
	{
		SYSTEM,
		SYSTEM_CONTIG,
		CARVEOUT,
		CHUNK,
		DMA,
	};

	Ion();

	~Ion();

	int alloc(size_t size, IonHeapType type, bool cached = false);

private:
	int m_fd;

	struct IonHeap
	{
		std::string name;
		uint32_t id;
		IonHeapType type;
	};

	std::vector<IonHeap> m_heaps;
};

}
