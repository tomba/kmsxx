#pragma once

#include <vector>

#include "drmobject.h"

namespace kms
{

struct ConnectorPriv;

struct Videomode
{
	uint32_t clock;
	uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
	uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

	uint32_t vrefresh;

	uint32_t flags;
	uint32_t type;
	char name[32]; // XXX
};

class Connector : public DrmObject
{
public:
	Connector(Card& card, uint32_t id, uint32_t idx);
	~Connector();

	void setup();

	void print_short() const;

	Videomode get_default_mode() const;

	Videomode get_mode(const std::string& mode) const;

	Crtc* get_current_crtc() const { return m_current_crtc; }
	std::vector<Crtc*> get_possible_crtcs() const;

	bool connected() const;

private:
	ConnectorPriv* m_priv;

	std::string m_fullname;

	Crtc* m_current_crtc;
};
}
