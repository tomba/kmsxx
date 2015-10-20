#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>

#include "kms++.h"
#include "helpers.h"

using namespace std;

namespace kms
{


static const map<int, string> connector_names = {
#define DEF_CONN(c) { DRM_MODE_CONNECTOR_##c, #c }
	DEF_CONN(Unknown),
	DEF_CONN(VGA),
	DEF_CONN(DVII),
	DEF_CONN(DVID),
	DEF_CONN(DVIA),
	DEF_CONN(Composite),
	DEF_CONN(SVIDEO),
	DEF_CONN(LVDS),
	DEF_CONN(Component),
	DEF_CONN(9PinDIN),
	DEF_CONN(DisplayPort),
	DEF_CONN(HDMIA),
	DEF_CONN(HDMIB),
	DEF_CONN(TV),
	DEF_CONN(eDP),
	DEF_CONN(VIRTUAL),
	DEF_CONN(DSI),
#undef DEF_CONN
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
	:DrmObject(card, id, DRM_MODE_OBJECT_CONNECTOR, idx)
{
	m_priv = new ConnectorPriv();

	m_priv->drm_connector = drmModeGetConnector(this->card().fd(), this->id());
	assert(m_priv->drm_connector);

	const auto& name = connector_names.at(m_priv->drm_connector->connector_type);
	m_fullname = name + to_string(m_priv->drm_connector->connector_type_id);
}


Connector::~Connector()
{
	drmModeFreeConnector(m_priv->drm_connector);
	delete m_priv;
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

void Connector::print_short() const
{
	auto c = m_priv->drm_connector;

	printf("Connector %d, %s, %dx%dmm, %s\n", id(), m_fullname.c_str(),
	       c->mmWidth, c->mmHeight, connection_str.at(c->connection).c_str());
}

Videomode Connector::get_default_mode() const
{
	drmModeModeInfo drmmode = m_priv->drm_connector->modes[0];

	return drm_mode_to_video_mode(drmmode);
}

Videomode Connector::get_mode(const string& mode) const
{
	auto c = m_priv->drm_connector;

	for (int i = 0; i < c->count_modes; i++)
                if (mode == c->modes[i].name)
                        return drm_mode_to_video_mode(c->modes[i]);

        throw invalid_argument(mode + ": mode not found");
}

bool Connector::connected() const
{
	return m_priv->drm_connector->connection == DRM_MODE_CONNECTED;
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
