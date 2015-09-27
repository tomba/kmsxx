#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "kms++.h"

using namespace std;

namespace kms
{

struct EncoderPriv
{
	drmModeEncoderPtr drm_encoder;
};

Encoder::Encoder(Card &card, uint32_t id)
	:DrmObject(card, id, DRM_MODE_OBJECT_ENCODER)
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

void Encoder::print_short() const
{
	auto e = m_priv->drm_encoder;

	printf("Encoder %d, %d\n", id(),
	       e->encoder_type);
}

Crtc* Encoder::get_crtc() const
{
	return card().get_crtc(m_priv->drm_encoder->crtc_id);
}

vector<Crtc*> Encoder::get_possible_crtcs() const
{
	unsigned bits = m_priv->drm_encoder->possible_crtcs;
	vector<Crtc*> crtcs;

	for (int idx = 0; bits; idx++, bits >>= 1) {
		if ((bits & 1) == 0)
			continue;

		auto crtc = card().get_crtc_by_index(idx);
		crtcs.push_back(crtc);
	}

	return crtcs;
}
}
