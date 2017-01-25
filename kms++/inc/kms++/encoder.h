#pragma once

#include <vector>
#include "drmpropobject.h"

namespace kms
{

struct EncoderPriv;

class Encoder : public DrmPropObject
{
	friend class Card;
public:
	void refresh();

	Crtc* get_crtc() const;
	std::vector<Crtc*> get_possible_crtcs() const;

	const std::string& get_encoder_type() const;
private:
	Encoder(Card& card, uint32_t id, uint32_t idx);
	~Encoder();

	EncoderPriv* m_priv;
};
}
