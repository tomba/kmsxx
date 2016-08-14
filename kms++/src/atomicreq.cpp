#include <cassert>
#include <stdexcept>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

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

void AtomicReq::add(DrmPropObject* ob, Property *prop, uint64_t value)
{
	add(ob->id(), prop->id(), value);
}

void AtomicReq::add(kms::DrmPropObject* ob, const string& prop, uint64_t value)
{
	add(ob, ob->get_prop(prop), value);
}

void AtomicReq::add(kms::DrmPropObject* ob, const map<string, uint64_t>& values)
{
	for(const auto& kvp : values)
		add(ob, kvp.first, kvp.second);
}

void AtomicReq::add_display(Connector* conn, Crtc* crtc, Blob* videomode, Plane* primary, Framebuffer* fb)
{
	add(conn, {
		    { "CRTC_ID", crtc->id() },
	    });

	add(crtc, {
		    { "ACTIVE", 1 },
		    { "MODE_ID", videomode->id() },
	    });

	add(primary, {
		    { "FB_ID", fb->id() },
		    { "CRTC_ID", crtc->id() },
		    { "SRC_X", 0 << 16 },
		    { "SRC_Y", 0 << 16 },
		    { "SRC_W", fb->width() << 16 },
		    { "SRC_H", fb->height() << 16 },
		    { "CRTC_X", 0 },
		    { "CRTC_Y", 0 },
		    { "CRTC_W", fb->width() },
		    { "CRTC_H", fb->height() },
	    });
}

int AtomicReq::test(bool allow_modeset)
{
	uint32_t flags = DRM_MODE_ATOMIC_TEST_ONLY;

	if (allow_modeset)
		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;

	return drmModeAtomicCommit(m_card.fd(), m_req, flags, 0);
}

int AtomicReq::commit(void* data, bool allow_modeset)
{
	uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;

	if (allow_modeset)
		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;

	return drmModeAtomicCommit(m_card.fd(), m_req, flags, data);
}

int AtomicReq::commit_sync(bool allow_modeset)
{
	uint32_t flags = 0;

	if (allow_modeset)
		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;

	return drmModeAtomicCommit(m_card.fd(), m_req, flags, 0);
}
}
