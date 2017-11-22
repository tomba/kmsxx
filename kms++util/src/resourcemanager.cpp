#include <kms++util/resourcemanager.h>
#include <algorithm>
#include <kms++util/strhelpers.h>

using namespace kms;
using namespace std;

ResourceManager::ResourceManager(Card& card)
	: m_card(card)
{
}

void ResourceManager::reset()
{
	m_reserved_connectors.clear();
	m_reserved_crtcs.clear();
	m_reserved_planes.clear();
}

static Connector* find_connector(Card& card, const set<Connector*> reserved)
{
	for (Connector* conn : card.get_connectors()) {
		if (!conn->connected())
			continue;

		if (reserved.count(conn))
			continue;

		return conn;
	}

	return nullptr;
}

static Connector* resolve_connector(Card& card, const string& name, const set<Connector*> reserved)
{
	auto connectors = card.get_connectors();

	if (name[0] == '@') {
		char* endptr;
		unsigned id = strtoul(name.c_str() + 1, &endptr, 10);
		if (*endptr == 0) {
			Connector* c = card.get_connector(id);

			if (!c || reserved.count(c))
				return nullptr;

			return c;
		}
	} else {
		char* endptr;
		unsigned idx = strtoul(name.c_str(), &endptr, 10);
		if (*endptr == 0) {
			if (idx >= connectors.size())
				return nullptr;

			Connector* c = connectors[idx];

			if (reserved.count(c))
				return nullptr;

			return c;
		}
	}

	for (Connector* conn : connectors) {
		if (to_lower(conn->fullname()).find(to_lower(name)) == string::npos)
			continue;

		if (reserved.count(conn))
			continue;

		return conn;
	}

	return nullptr;
}

Connector* ResourceManager::reserve_connector(const string& name)
{
	Connector* conn;

	if (name.empty())
		conn = find_connector(m_card, m_reserved_connectors);
	else
		conn = resolve_connector(m_card, name, m_reserved_connectors);

	if (!conn)
		return nullptr;

	m_reserved_connectors.insert(conn);
	return conn;
}

Connector* ResourceManager::reserve_connector(Connector* conn)
{
	if (!conn)
		return nullptr;

	if (m_reserved_connectors.count(conn))
		return nullptr;

	m_reserved_connectors.insert(conn);
	return conn;
}

Crtc* ResourceManager::reserve_crtc(Connector* conn)
{
	if (!conn)
		return nullptr;

	if (Crtc* crtc = conn->get_current_crtc()) {
		m_reserved_crtcs.insert(crtc);
		return crtc;
	}

	for (Crtc* crtc : conn->get_possible_crtcs()) {
		if (m_reserved_crtcs.count(crtc))
			continue;

		m_reserved_crtcs.insert(crtc);
		return crtc;
	}

	return nullptr;
}

Crtc* ResourceManager::reserve_crtc(Crtc* crtc)
{
	if (!crtc)
		return nullptr;

	if (m_reserved_crtcs.count(crtc))
		return nullptr;

	m_reserved_crtcs.insert(crtc);

	return crtc;
}

Plane* ResourceManager::reserve_plane(Crtc* crtc, PlaneType type, PixelFormat format)
{
	if (!crtc)
		return nullptr;

	for (Plane* plane : crtc->get_possible_planes()) {
		if (plane->plane_type() != type)
			continue;

		if (format != PixelFormat::Undefined && !plane->supports_format(format))
			continue;

		if (m_reserved_planes.count(plane))
			continue;

		m_reserved_planes.insert(plane);
		return plane;
	}

	return nullptr;
}

Plane* ResourceManager::reserve_plane(Plane* plane)
{
	if (!plane)
		return nullptr;

	if (m_reserved_planes.count(plane))
		return nullptr;

	m_reserved_planes.insert(plane);

	return plane;
}

Plane* ResourceManager::reserve_generic_plane(Crtc* crtc, PixelFormat format)
{
	if (!crtc)
		return nullptr;

	for (Plane* plane : crtc->get_possible_planes()) {
		if (plane->plane_type() == PlaneType::Cursor)
			continue;

		if (format != PixelFormat::Undefined && !plane->supports_format(format))
			continue;

		if (m_reserved_planes.count(plane))
			continue;

		m_reserved_planes.insert(plane);
		return plane;
	}

	return nullptr;
}

Plane* ResourceManager::reserve_primary_plane(Crtc* crtc, PixelFormat format)
{
	return reserve_plane(crtc, PlaneType::Primary, format);
}

Plane* ResourceManager::reserve_overlay_plane(Crtc* crtc, PixelFormat format)
{
	return reserve_plane(crtc, PlaneType::Overlay, format);
}
