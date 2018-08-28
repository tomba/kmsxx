#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cmath>

#include <kms++/kms++.h>
#include "helpers.h"

using namespace std;

namespace kms
{

#ifndef DRM_MODE_CONNECTOR_DPI
#define DRM_MODE_CONNECTOR_DPI 17
#endif

static const map<int, string> connector_names = {
	{ DRM_MODE_CONNECTOR_Unknown, "Unknown" },
	{ DRM_MODE_CONNECTOR_VGA, "VGA" },
	{ DRM_MODE_CONNECTOR_DVII, "DVI-I" },
	{ DRM_MODE_CONNECTOR_DVID, "DVI-D" },
	{ DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
	{ DRM_MODE_CONNECTOR_Composite, "Composite" },
	{ DRM_MODE_CONNECTOR_SVIDEO, "S-Video" },
	{ DRM_MODE_CONNECTOR_LVDS, "LVDS" },
	{ DRM_MODE_CONNECTOR_Component, "Component" },
	{ DRM_MODE_CONNECTOR_9PinDIN, "9-Pin-DIN" },
	{ DRM_MODE_CONNECTOR_DisplayPort, "DP" },
	{ DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
	{ DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
	{ DRM_MODE_CONNECTOR_TV, "TV" },
	{ DRM_MODE_CONNECTOR_eDP, "eDP" },
	{ DRM_MODE_CONNECTOR_VIRTUAL, "Virtual" },
	{ DRM_MODE_CONNECTOR_DSI, "DSI" },
	{ DRM_MODE_CONNECTOR_DPI, "DPI" },
};

static const map<int, string> connection_str = {
	{ 0, "<unknown>" },
	{ DRM_MODE_CONNECTED, "Connected" },
	{ DRM_MODE_DISCONNECTED, "Disconnected" },
	{ DRM_MODE_UNKNOWNCONNECTION, "Unknown" },
};

static const map<int, string> subpix_str = {
#define DEF_SUBPIX(c) { DRM_MODE_SUBPIXEL_##c, #c }
	DEF_SUBPIX(UNKNOWN),
	DEF_SUBPIX(HORIZONTAL_RGB),
	DEF_SUBPIX(HORIZONTAL_BGR),
	DEF_SUBPIX(VERTICAL_RGB),
	DEF_SUBPIX(VERTICAL_BGR),
	DEF_SUBPIX(NONE),
#undef DEF_SUBPIX
};

struct ConnectorPriv
{
	drmModeConnectorPtr drm_connector;
};

Connector::Connector(Card &card, uint32_t id, uint32_t idx)
	:DrmPropObject(card, id, DRM_MODE_OBJECT_CONNECTOR, idx)
{
	m_priv = new ConnectorPriv();

	m_priv->drm_connector = drmModeGetConnector(this->card().fd(), this->id());
	assert(m_priv->drm_connector);

	// XXX drmModeGetConnector() does forced probe, which seems to change (at least) EDID blob id.
	// XXX So refresh the props again here.
	refresh_props();

	const auto& name = connector_names.at(m_priv->drm_connector->connector_type);
	m_fullname = name + "-" + to_string(m_priv->drm_connector->connector_type_id);
}

Connector::~Connector()
{
	drmModeFreeConnector(m_priv->drm_connector);
	delete m_priv;
}

void Connector::refresh()
{
	drmModeFreeConnector(m_priv->drm_connector);

	m_priv->drm_connector = drmModeGetConnector(this->card().fd(), this->id());
	assert(m_priv->drm_connector);

	// XXX drmModeGetConnector() does forced probe, which seems to change (at least) EDID blob id.
	// XXX So refresh the props again here.
	refresh_props();

	const auto& name = connector_names.at(m_priv->drm_connector->connector_type);
	m_fullname = name + "-" + to_string(m_priv->drm_connector->connector_type_id);
}

void Connector::setup()
{
	if (m_priv->drm_connector->encoder_id != 0)
		m_current_encoder = card().get_encoder(m_priv->drm_connector->encoder_id);
	else
		m_current_encoder = 0;

	if (m_current_encoder)
		m_saved_crtc = m_current_encoder->get_crtc();
	else
		m_saved_crtc = 0;
}

void Connector::restore_mode()
{
	if (m_saved_crtc)
		m_saved_crtc->restore_mode(this);
}

Videomode Connector::get_default_mode() const
{
	if (m_priv->drm_connector->count_modes == 0)
		throw invalid_argument("no modes available\n");
	drmModeModeInfo drmmode = m_priv->drm_connector->modes[0];

	return drm_mode_to_video_mode(drmmode);
}

Videomode Connector::get_mode(const string& mode) const
{
	auto c = m_priv->drm_connector;

	size_t idx = mode.find('@');

	string name = idx == string::npos ? mode : mode.substr(0, idx);
	float vrefresh = idx == string::npos ? 0.0 : stod(mode.substr(idx + 1));

	for (int i = 0; i < c->count_modes; i++) {
		Videomode m = drm_mode_to_video_mode(c->modes[i]);

		if (m.name != name)
			continue;

		if (vrefresh && vrefresh != m.calculated_vrefresh())
			continue;

		return m;
	}

	throw invalid_argument(mode + ": mode not found");
}

Videomode Connector::get_mode(unsigned xres, unsigned yres, float vrefresh, bool ilace) const
{
	auto c = m_priv->drm_connector;

	for (int i = 0; i < c->count_modes; i++) {
		Videomode m = drm_mode_to_video_mode(c->modes[i]);

		if (m.hdisplay != xres || m.vdisplay != yres)
			continue;

		if (ilace != m.interlace())
			continue;

		if (vrefresh && vrefresh != m.calculated_vrefresh())
			continue;

		return m;
	}

	// If not found, do another round using rounded vrefresh

	for (int i = 0; i < c->count_modes; i++) {
		Videomode m = drm_mode_to_video_mode(c->modes[i]);

		if (m.hdisplay != xres || m.vdisplay != yres)
			continue;

		if (ilace != m.interlace())
			continue;

		if (vrefresh && vrefresh != roundf(m.calculated_vrefresh()))
			continue;

		return m;
	}

	throw invalid_argument("mode not found");
}

bool Connector::connected() const
{
	return m_priv->drm_connector->connection == DRM_MODE_CONNECTED ||
			m_priv->drm_connector->connection == DRM_MODE_UNKNOWNCONNECTION;
}

ConnectorStatus Connector::connector_status() const
{
	switch (m_priv->drm_connector->connection) {
	case DRM_MODE_CONNECTED:
		return ConnectorStatus::Connected;
	case DRM_MODE_DISCONNECTED:
		return ConnectorStatus::Disconnected;
	default:
		return ConnectorStatus::Unknown;
	}
}

vector<Crtc*> Connector::get_possible_crtcs() const
{
	vector<Crtc*> crtcs;

	for (int i = 0; i < m_priv->drm_connector->count_encoders; ++i) {
		auto enc = card().get_encoder(m_priv->drm_connector->encoders[i]);

		auto l = enc->get_possible_crtcs();

		crtcs.insert(crtcs.end(), l.begin(), l.end());
	}

	return crtcs;
}

Crtc* Connector::get_current_crtc() const
{
	if (m_current_encoder)
		return m_current_encoder->get_crtc();
	else
		return 0;
}

uint32_t Connector::connector_type() const
{
	return m_priv->drm_connector->connector_type;
}

uint32_t Connector::connector_type_id() const
{
	return m_priv->drm_connector->connector_type_id;
}

uint32_t Connector::mmWidth() const
{
	return m_priv->drm_connector->mmWidth;
}

uint32_t Connector::mmHeight() const
{
	return m_priv->drm_connector->mmHeight;
}

uint32_t Connector::subpixel() const
{
	return m_priv->drm_connector->subpixel;
}

const string& Connector::subpixel_str() const
{
	return subpix_str.at(subpixel());
}

std::vector<Videomode> Connector::get_modes() const
{
	vector<Videomode> modes;

	for (int i = 0; i < m_priv->drm_connector->count_modes; i++)
		modes.push_back(drm_mode_to_video_mode(
					m_priv->drm_connector->modes[i]));

	return modes;
}

std::vector<Encoder*> Connector::get_encoders() const
{
	vector<Encoder*> encoders;

	for (int i = 0; i < m_priv->drm_connector->count_encoders; i++) {
		auto enc = card().get_encoder(m_priv->drm_connector->encoders[i]);
		encoders.push_back(enc);
	}
	return encoders;
}

}
