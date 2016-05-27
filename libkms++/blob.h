#pragma once

#include "drmobject.h"
#include <vector>

namespace kms
{

class Blob : public DrmObject
{
public:
	Blob(Card& card, uint32_t blob_id);
	Blob(Card& card, void* data, size_t len);
	virtual ~Blob();

	std::vector<uint8_t> data();

private:
	bool m_created;
};

}
