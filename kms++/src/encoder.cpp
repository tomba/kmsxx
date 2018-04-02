#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{

struct EncoderPriv
{
	drmModeEncoderPtr drm_encoder;
};

static const map<int, string> encoder_types = {
#define DEF_ENC(c) { DRM_MODE_ENCODER_##c, #c }
	DEF_ENC(NONE),
	DEF_ENC(DAC),
	DEF_ENC(TMDS),
	DEF_ENC(LVDS),
	DEF_ENC(TVDAC),
	DEF_ENC(VIRTUAL),
	DEF_ENC(DSI),
	{ 7, "DPMST" },
	{ 8, "DPI" },
#undef DEF_ENC
};

Encoder::Encoder(Card &card, uint32_t id, uint32_t idx)
	:DrmPropObject(card, id, DRM_MODE_OBJECT_ENCODER, idx)
{
	m_priv = new EncoderPriv();
	m_priv->drm_encoder = drmModeGetEncoder(this->card().fd(), this->id());
	assert(m_priv->drm_encoder);
}

Encoder::~Encoder()
{
	drmModeFreeEncoder(m_priv->drm_encoder);
	delete m_priv;
}

void Encoder::refresh()
{
	drmModeFreeEncoder(m_priv->drm_encoder);

	m_priv->drm_encoder = drmModeGetEncoder(this->card().fd(), this->id());
	assert(m_priv->drm_encoder);
}

Crtc* Encoder::get_crtc() const
{
	if (m_priv->drm_encoder->crtc_id)
		return card().get_crtc(m_priv->drm_encoder->crtc_id);
	else
		return 0;
}

vector<Crtc*> Encoder::get_possible_crtcs() const
{
	unsigned bits = m_priv->drm_encoder->possible_crtcs;
	vector<Crtc*> crtcs;

	for (int idx = 0; bits; idx++, bits >>= 1) {
		if ((bits & 1) == 0)
			continue;

		auto crtc = card().get_crtcs()[idx];
		crtcs.push_back(crtc);
	}

	return crtcs;
}

const string& Encoder::get_encoder_type() const
{
	return encoder_types.at(m_priv->drm_encoder->encoder_type);
}

}
