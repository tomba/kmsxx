#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <utility>
#include <stdexcept>
#include <string.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "kms++.h"

#ifndef DRM_CLIENT_CAP_ATOMIC
#define DRM_CLIENT_CAP_ATOMIC 3
#endif

namespace kms
{

Card::Card()
{
	const char *card = "/dev/dri/card0";

	int fd = open(card, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		throw std::invalid_argument("foo");
	m_fd = fd;

	int r;

	r = drmSetMaster(fd);
	m_master = r == 0;

	r = drmSetClientCap(m_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	if (r)
		throw std::invalid_argument("foo");

	r = drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1);
	m_has_atomic = r == 0;

	uint64_t has_dumb;
	r = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb);
	if (r || !has_dumb)
		throw std::invalid_argument("foo");

	auto res = drmModeGetResources(m_fd);
	if (!res)
		throw std::invalid_argument("foo");

	for (int i = 0; i < res->count_connectors; ++i) {
		uint32_t id = res->connectors[i];
		m_obmap[id] = new Connector(*this, id, i);
	}

	for (int i = 0; i < res->count_crtcs; ++i) {
		uint32_t id = res->crtcs[i];
		m_obmap[id] = new Crtc(*this, id, i);
	}

	for (int i = 0; i < res->count_encoders; ++i) {
		uint32_t id = res->encoders[i];
		m_obmap[id] = new Encoder(*this, id);
	}

	drmModeFreeResources(res);

	auto planeRes = drmModeGetPlaneResources(m_fd);

	for (uint i = 0; i < planeRes->count_planes; ++i) {
		uint32_t id = planeRes->planes[i];
		m_obmap[id] = new Plane(*this, id);
	}

	drmModeFreePlaneResources(planeRes);

	// collect all possible props
	for (auto ob : get_objects()) {
		auto props = drmModeObjectGetProperties(m_fd, ob->id(), ob->object_type());

		if (props == nullptr)
			continue;

		for (unsigned i = 0; i < props->count_props; ++i) {
			uint32_t prop_id = props->props[i];

			if (m_obmap.find(prop_id) == m_obmap.end())
				m_obmap[prop_id] = new Property(*this, prop_id);
		}

		drmModeFreeObjectProperties(props);
	}

	for (auto pair : m_obmap)
		pair.second->setup();
}

Card::~Card()
{
	for (auto pair : m_obmap)
		delete pair.second;

	close(m_fd);
}

template <class T> static void print_obs(const std::map<uint32_t, DrmObject*>& obmap)
{
	for (auto pair : obmap) {
		auto ob = pair.second;
		if (dynamic_cast<T*>(ob)) {
			ob->print_short();
			//ob->print_props();
		}
	}
}

void Card::print_short() const
{
	print_obs<Connector>(m_obmap);
	print_obs<Encoder>(m_obmap);
	print_obs<Crtc>(m_obmap);
	print_obs<Plane>(m_obmap);
}

Property* Card::get_prop(const char *name) const
{
	for (auto pair : m_obmap) {
		auto prop = dynamic_cast<Property*>(pair.second);
		if (!prop)
			continue;

		if (strcmp(name, prop->name()) == 0)
			return prop;
	}

	throw std::invalid_argument("foo");
}

Connector* Card::get_first_connected_connector() const
{
	for(auto pair : m_obmap) {
		auto c = dynamic_cast<Connector*>(pair.second);

		if (c && c->connected())
			return c;
	}

	throw std::invalid_argument("no connected connectors");
}

DrmObject* Card::get_object(uint32_t id) const
{
	return m_obmap.at(id);
}

std::vector<Connector*> Card::get_connectors() const
{
	std::vector<Connector*> v;
	for(auto pair : m_obmap) {
		auto p = dynamic_cast<Connector*>(pair.second);
		if (p)
			v.push_back(p);
	}
	return v;
}

std::vector<Plane*> Card::get_planes() const
{
	std::vector<Plane*> v;
	for(auto pair : m_obmap) {
		auto p = dynamic_cast<Plane*>(pair.second);
		if (p)
			v.push_back(p);
	}
	return v;
}

std::vector<DrmObject*> Card::get_objects() const
{
	std::vector<DrmObject*> v;
	for(auto pair : m_obmap)
		v.push_back(pair.second);
	return v;
}

Crtc* Card::get_crtc_by_index(uint32_t idx) const
{
	for(auto pair : m_obmap) {
		auto crtc = dynamic_cast<Crtc*>(pair.second);
		if (crtc && crtc->idx() == idx)
			return crtc;
	}
	throw std::invalid_argument("fob");
}

Crtc* Card::get_crtc(uint32_t id) const { return dynamic_cast<Crtc*>(get_object(id)); }
Encoder* Card::get_encoder(uint32_t id) const { return dynamic_cast<Encoder*>(get_object(id)); }
Property* Card::get_prop(uint32_t id) const { return dynamic_cast<Property*>(get_object(id)); }
}
