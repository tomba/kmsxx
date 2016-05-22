#pragma once

#include <cstdint>
#include <string>

struct _drmModeAtomicReq;

#include "decls.h"

namespace kms
{
class AtomicReq
{
public:
	AtomicReq(Card& card);
	~AtomicReq();

	AtomicReq(const AtomicReq& other) = delete;
	AtomicReq& operator=(const AtomicReq& other) = delete;

	void add(uint32_t ob_id, uint32_t prop_id, uint64_t value);
	void add(DrmObject *ob, Property *prop, uint64_t value);
	void add(DrmObject *ob, const std::string& prop, uint64_t value);

	int test();
	int commit(void* data);
	int commit_sync();

private:
	Card& m_card;
	_drmModeAtomicReq* m_req;
};

}
