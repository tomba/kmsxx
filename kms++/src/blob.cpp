#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{

Blob::Blob(Card& card, uint32_t blob_id)
	: DrmObject(card, blob_id, DRM_MODE_OBJECT_BLOB), m_created(false)
{
	// XXX should we verify that the blob_id is a blob object?
}

Blob::Blob(Card& card, void* data, size_t len)
	: DrmObject(card, DRM_MODE_OBJECT_BLOB), m_created(true)
{
	uint32_t id;

	int r = drmModeCreatePropertyBlob(card.fd(), data, len, &id);
	if (r)
		throw invalid_argument("FAILED TO CREATE PROP\n");

	set_id(id);
}

Blob::~Blob()
{
	if (m_created)
		drmModeDestroyPropertyBlob(card().fd(), id());
}

vector<uint8_t> Blob::data()
{
	drmModePropertyBlobPtr blob = drmModeGetPropertyBlob(card().fd(), id());

	if (!blob)
		throw invalid_argument("Blob data not available");

	uint8_t* data = (uint8_t*)blob->data;

	auto v = vector<uint8_t>(data, data + blob->length);

	drmModeFreePropertyBlob(blob);

	return v;
}

}
