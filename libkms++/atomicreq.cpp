#include <cassert>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "atomicreq.h"
#include "property.h"
#include "card.h"

#ifndef DRM_CLIENT_CAP_ATOMIC

#define DRM_MODE_ATOMIC_TEST_ONLY 0

struct _drmModeAtomicReq;
typedef struct _drmModeAtomicReq* drmModeAtomicReqPtr;

static inline drmModeAtomicReqPtr drmModeAtomicAlloc() { return 0; }
static inline void drmModeAtomicFree(drmModeAtomicReqPtr) { }
static inline int drmModeAtomicAddProperty(drmModeAtomicReqPtr, uint32_t, uint32_t, uint64_t) { return 0; }
static inline int drmModeAtomicCommit(int, drmModeAtomicReqPtr, int, void*) { return 0; }

#endif // DRM_CLIENT_CAP_ATOMIC

namespace kms
{
AtomicReq::AtomicReq(Card& card)
	: m_card(card)
{
	assert(card.has_atomic());
	m_req = drmModeAtomicAlloc();
}

AtomicReq::~AtomicReq()
{
	drmModeAtomicFree(m_req);
}

void AtomicReq::add(uint32_t ob_id, uint32_t prop_id, uint64_t value)
{
	int r = drmModeAtomicAddProperty(m_req, ob_id, prop_id, value);
	if (r <= 0)
		throw std::invalid_argument("foo");
}

void AtomicReq::add(DrmObject *ob, Property *prop, uint64_t value)
{
	add(ob->id(), prop->id(), value);
}

int AtomicReq::test()
{
	uint32_t flags = DRM_MODE_ATOMIC_TEST_ONLY;

	return drmModeAtomicCommit(m_card.fd(), m_req, flags, 0);
}

int AtomicReq::commit()
{
	uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT;
	void* data = 0;

	return drmModeAtomicCommit(m_card.fd(), m_req, flags, data);
}
}
