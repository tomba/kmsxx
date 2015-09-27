#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>

#include "kms++.h"
#include "helpers.h"

using namespace std;

#define DEF_CONN(c) [DRM_MODE_CONNECTOR_##c] = #c

namespace kms
{

static const char * connector_names[] = {
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
};

static const char *connection_str[] = {
	[0] = "<unknown>",
	[DRM_MODE_CONNECTED] = "Connected",
	[DRM_MODE_DISCONNECTED] = "Disconnected",
	[DRM_MODE_UNKNOWNCONNECTION] = "Unknown",
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

	auto name = connector_names[m_priv->drm_connector->connector_type];
	m_fullname = std::string(string(name) + std::to_string(m_priv->drm_connector->connector_type_id));
}


Connector::~Connector()
{
	drmModeFreeConnector(m_priv->drm_connector);
	delete m_priv;
}

void Connector::setup()
{
	if (m_priv->drm_connector->encoder_id != 0) {
		auto enc = card().get_encoder(m_priv->drm_connector->encoder_id);
		if (enc)
			m_current_crtc = enc->get_crtc();
	}
}

void Connector::print_short() const
{
	auto c = m_priv->drm_connector;

	printf("Connector %d, %s, %dx%dmm, %s\n", id(), m_fullname.c_str(),
	       c->mmWidth, c->mmHeight, connection_str[c->connection]);
}

Videomode Connector::get_default_mode() const
{
	drmModeModeInfo drmmode = m_priv->drm_connector->modes[0];

	return drm_mode_to_video_mode(drmmode);
}

bool Connector::connected()
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
}
