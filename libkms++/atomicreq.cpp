#include <cassert>
#include <stdexcept>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "atomicreq.h"
#include "property.h"
#include "card.h"

#ifndef DRM_CLIENT_CAP_ATOMIC

#define DRM_MODE_ATOMIC_TEST_ONLY 0
#define DRM_MODE_ATOMIC_NONBLOCK 0

struct _drmModeAtomicReq;
typedef struct _drmModeAtomicReq* drmModeAtomicReqPtr;

static inline drmModeAtomicReqPtr drmModeAtomicAlloc() { return 0; }
static inline void drmModeAtomicFree(drmModeAtomicReqPtr) { }
static inline int drmModeAtomicAddProperty(drmModeAtomicReqPtr, uint32_t, uint32_t, uint64_t) { return 0; }
static inline int drmModeAtomicCommit(int, drmModeAtomicReqPtr, int, void*) { return 0; }

#endif // DRM_CLIENT_CAP_ATOMIC

using namespace std;

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

void AtomicReq::add(DrmObject* ob, const string& prop, uint64_t value)
{
	add(ob, m_card.get_prop(prop), value);
}

void AtomicReq::add(DrmObject* ob, const map<string, uint64_t>& values)
{
	for(const auto& kvp : values)
		add(ob, kvp.first, kvp.second);
}

int AtomicReq::test()
{
	uint32_t flags = DRM_MODE_ATOMIC_TEST_ONLY;

	return drmModeAtomicCommit(m_card.fd(), m_req, flags, 0);
}

int AtomicReq::commit(void* data)
{
	uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;

	return drmModeAtomicCommit(m_card.fd(), m_req, flags, data);
}

int AtomicReq::commit_sync()
{
	uint32_t flags = 0;

	return drmModeAtomicCommit(m_card.fd(), m_req, flags, 0);
}
}
