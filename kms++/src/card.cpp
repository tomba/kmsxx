#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <utility>
#include <stdexcept>
#include <string.h>
#include <algorithm>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{

Card::Card()
	: Card("/dev/dri/card0")
{
}


Card::Card(const std::string& device)
{
	int fd = open(device.c_str(), O_RDWR | O_CLOEXEC);
	if (fd < 0)
		throw invalid_argument(string(strerror(errno)) + " opening " + device);
	m_fd = fd;

	int r;

	r = drmSetMaster(fd);
	m_master = r == 0;

	if (getenv("KMSXX_DISABLE_UNIVERSAL_PLANES") == 0) {
		r = drmSetClientCap(m_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
		m_has_universal_planes = r == 0;
	} else {
		m_has_universal_planes = false;
	}

#ifdef DRM_CLIENT_CAP_ATOMIC
	if (getenv("KMSXX_DISABLE_ATOMIC") == 0) {
		r = drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1);
		m_has_atomic = r == 0;
	} else {
		m_has_atomic = false;
	}
#else
	m_has_atomic = false;
#endif

	uint64_t has_dumb;
	r = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb);
	if (r || !has_dumb)
		throw invalid_argument("Dumb buffers not available");

	auto res = drmModeGetResources(m_fd);
	if (!res)
		throw invalid_argument("Can't get card resources");

	for (int i = 0; i < res->count_connectors; ++i) {
		uint32_t id = res->connectors[i];
		auto ob = new Connector(*this, id, i);
		m_obmap[id] = ob;
		m_connectors.push_back(ob);
	}

	for (int i = 0; i < res->count_crtcs; ++i) {
		uint32_t id = res->crtcs[i];
		auto ob = new Crtc(*this, id, i);
		m_obmap[id] = ob;
		m_crtcs.push_back(ob);
	}

	for (int i = 0; i < res->count_encoders; ++i) {
		uint32_t id = res->encoders[i];
		auto ob = new Encoder(*this, id, i);
		m_obmap[id] = ob;
		m_encoders.push_back(ob);
	}

	drmModeFreeResources(res);

	auto planeRes = drmModeGetPlaneResources(m_fd);

	for (uint i = 0; i < planeRes->count_planes; ++i) {
		uint32_t id = planeRes->planes[i];
		auto ob = new Plane(*this, id, i);
		m_obmap[id] = ob;
		m_planes.push_back(ob);
	}

	drmModeFreePlaneResources(planeRes);

	// collect all possible props
	for (auto ob : get_objects()) {
		auto props = drmModeObjectGetProperties(m_fd, ob->id(), ob->object_type());

		if (props == nullptr)
			continue;

		for (unsigned i = 0; i < props->count_props; ++i) {
			uint32_t prop_id = props->props[i];

			if (m_obmap.find(prop_id) == m_obmap.end()) {
				auto ob = new Property(*this, prop_id);
				m_obmap[prop_id] = ob;
				m_properties.push_back(ob);
			}
		}

		drmModeFreeObjectProperties(props);
	}

	for (auto pair : m_obmap)
		pair.second->setup();
}

Card::~Card()
{
	restore_modes();

	while (m_framebuffers.size() > 0)
		delete m_framebuffers.back();

	for (auto pair : m_obmap)
		delete pair.second;

	close(m_fd);
}

void Card::drop_master()
{
	drmDropMaster(fd());
}

void Card::restore_modes()
{
	for (auto conn : get_connectors())
		conn->restore_mode();
}

Connector* Card::get_first_connected_connector() const
{
	for(auto c : m_connectors) {
		if (c->connected())
			return c;
	}

	throw invalid_argument("no connected connectors");
}

DrmObject* Card::get_object(uint32_t id) const
{
	auto iter = m_obmap.find(id);
	if (iter != m_obmap.end())
		return iter->second;
	return nullptr;
}

const vector<DrmObject*> Card::get_objects() const
{
	vector<DrmObject*> v;
	for(auto pair : m_obmap)
		v.push_back(pair.second);
	return v;
}

Connector* Card::get_connector(uint32_t id) const { return dynamic_cast<Connector*>(get_object(id)); }
Crtc* Card::get_crtc(uint32_t id) const { return dynamic_cast<Crtc*>(get_object(id)); }
Encoder* Card::get_encoder(uint32_t id) const { return dynamic_cast<Encoder*>(get_object(id)); }
Property* Card::get_prop(uint32_t id) const { return dynamic_cast<Property*>(get_object(id)); }
Plane* Card::get_plane(uint32_t id) const { return dynamic_cast<Plane*>(get_object(id)); }

std::vector<kms::Pipeline> Card::get_connected_pipelines()
{
	vector<Pipeline> outputs;

	for (auto conn : get_connectors())
	{
		if (conn->connected() == false)
			continue;

		Crtc* crtc = conn->get_current_crtc();

		if (!crtc) {
			for (auto possible : conn->get_possible_crtcs()) {
				if (find_if(outputs.begin(), outputs.end(), [possible](Pipeline out) { return out.crtc == possible; }) == outputs.end()) {
					crtc = possible;
					break;
				}
			}
		}

		if (!crtc)
			throw invalid_argument(string("Connector #") +
					       to_string(conn->idx()) +
					       " has no possible crtcs");

		outputs.push_back(Pipeline { crtc, conn });
	}

	return outputs;
}

static void page_flip_handler(int fd, unsigned int frame,
			      unsigned int sec, unsigned int usec,
			      void *data)
{
	auto handler = (PageFlipHandlerBase*)data;
	double time = sec + usec / 1000000.0;
	handler->handle_page_flip(frame, time);
}

void Card::call_page_flip_handlers()
{
	drmEventContext ev { };
	ev.version = DRM_EVENT_CONTEXT_VERSION;
	ev.page_flip_handler = page_flip_handler;

	drmHandleEvent(fd(), &ev);
}

int Card::disable_all()
{
	AtomicReq req(*this);

	for (Crtc* c : m_crtcs) {
		req.add(c, {
				{ "ACTIVE", 0 },
			});
	}

	for (Plane* p : m_planes) {
		req.add(p, {
				{ "FB_ID", 0 },
				{ "CRTC_ID", 0 },
			});
	}

	return req.commit_sync(true);
}

}
