#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <utility>
#include <stdexcept>
#include <string.h>
#include <algorithm>
#include <cerrno>
#include <algorithm>
#include <glob.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{

static vector<string> glob(const string& pattern)
{
	glob_t glob_result;
	memset(&glob_result, 0, sizeof(glob_result));

	int r = glob(pattern.c_str(), 0, NULL, &glob_result);
	if(r != 0) {
		globfree(&glob_result);
		throw runtime_error("failed to find DRM cards");
	}

	vector<string> filenames;
	for(size_t i = 0; i < glob_result.gl_pathc; ++i)
		filenames.push_back(string(glob_result.gl_pathv[i]));

	globfree(&glob_result);

	return filenames;
}

static int open_first_kms_device()
{
	vector<string> paths = glob("/dev/dri/card*");

	for (const string& path : paths) {
		int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
		if (fd < 0)
			throw invalid_argument(string(strerror(errno)) + " opening device " + path);

		auto res = drmModeGetResources(fd);
		if (!res) {
			close(fd);
			continue;
		}

		bool has_kms = res->count_crtcs > 0 && res->count_connectors > 0 && res->count_encoders > 0;

		drmModeFreeResources(res);

		if (has_kms)
			return fd;

		close(fd);
	}

	throw runtime_error("No modesetting DRM card found");
}

static int open_device_by_path(string path)
{
	int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
	if (fd < 0)
		throw invalid_argument(string(strerror(errno)) + " opening device " + path);
	return fd;
}

// open Nth DRM card with the given driver name
static int open_device_by_driver(string name, uint32_t idx)
{
	transform(name.begin(), name.end(), name.begin(), ::tolower);

	uint32_t num_matches = 0;
	vector<string> paths = glob("/dev/dri/card*");

	for (const string& path : paths) {
		int fd = open_device_by_path(path);

		drmVersionPtr ver = drmGetVersion(fd);
		string drv_name = string(ver->name, ver->name_len);
		drmFreeVersion(ver);

		transform(drv_name.begin(), drv_name.end(), drv_name.begin(), ::tolower);

		if (name == drv_name) {
			if (idx == num_matches)
				return fd;
			num_matches++;
		}

		close(fd);
	}

	throw invalid_argument("Failed to find a DRM device " + name + ":" + to_string(idx));
}

Card::Card(const std::string& dev_path)
{
	const char* drv_p = getenv("KMSXX_DRIVER");
	const char* dev_p = getenv("KMSXX_DEVICE");

	if (!dev_path.empty()) {
		m_fd = open_device_by_path(dev_path);
	} else if (dev_p) {
		string dev(dev_p);
		m_fd = open_device_by_path(dev);
	} else if (drv_p) {
		string drv(drv_p);

		auto isplit = find(drv.begin(), drv.end(), ':');

		if (isplit == drv.begin())
			throw runtime_error("Invalid KMSXX_DRIVER");

		string name;
		uint32_t num = 0;

		if (isplit == drv.end()) {
			name = drv;
		} else {
			name = string(drv.begin(), isplit);
			string numstr = string(isplit + 1, drv.end());
			num = stoul(numstr);
		}

		m_fd = open_device_by_driver(name, num);
	} else {
		m_fd = open_first_kms_device();
	}

	setup();
}

Card::Card(const std::string& driver, uint32_t idx)
{
	m_fd = open_device_by_driver(driver, idx);

	setup();
}

void Card::setup()
{
	drmVersionPtr ver = drmGetVersion(m_fd);
	m_version_major = ver->version_major;
	m_version_minor = ver->version_minor;
	m_version_patchlevel = ver->version_patchlevel;
	m_version_name = string(ver->name, ver->name_len);
	m_version_date = string(ver->date, ver->date_len);
	m_version_desc = string(ver->desc, ver->desc_len);
	drmFreeVersion(ver);

	int r;

	r = drmSetMaster(m_fd);
	m_is_master = r == 0;

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
	r = drmGetCap(m_fd, DRM_CAP_DUMB_BUFFER, &has_dumb);
	m_has_dumb = r == 0 && has_dumb;

	auto res = drmModeGetResources(m_fd);
	if (res) {
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
		if (planeRes) {
			for (uint i = 0; i < planeRes->count_planes; ++i) {
				uint32_t id = planeRes->planes[i];
				auto ob = new Plane(*this, id, i);
				m_obmap[id] = ob;
				m_planes.push_back(ob);
			}

			drmModeFreePlaneResources(planeRes);
		}
	}

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
	m_is_master = false;
}

bool Card::has_kms() const
{
	return m_connectors.size() > 0 && m_encoders.size() > 0 && m_crtcs.size() > 0;
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
