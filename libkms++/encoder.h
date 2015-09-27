#pragma once

#include <vector>
#include "drmobject.h"

namespace kms
{

struct EncoderPriv;

class Encoder : public DrmObject
{
public:
	Encoder(Card& card, uint32_t id);
	~Encoder();

	void print_short() const;

	Crtc* get_crtc() const;
	std::vector<Crtc*> get_possible_crtcs() const;

private:
	EncoderPriv* m_priv;
};
}
