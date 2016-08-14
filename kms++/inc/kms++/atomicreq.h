#pragma once

#include <cstdint>
#include <string>
#include <map>

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
	void add(DrmPropObject *ob, Property *prop, uint64_t value);
	void add(DrmPropObject *ob, const std::string& prop, uint64_t value);
	void add(DrmPropObject *ob, const std::map<std::string, uint64_t>& values);

	void add_display(Connector* conn, Crtc* crtc, Blob* videomode,
			 Plane* primary, Framebuffer* fb);

	int test(bool allow_modeset = false);
	int commit(void* data, bool allow_modeset = false);
	int commit_sync(bool allow_modeset = false);

private:
	Card& m_card;
	_drmModeAtomicReq* m_req;
};

}
