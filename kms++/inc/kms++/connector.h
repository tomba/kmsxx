#pragma once

#include <vector>

#include "drmpropobject.h"
#include "videomode.h"

namespace kms
{

struct ConnectorPriv;

enum class ConnectorStatus
{
	Unknown,
	Connected,
	Disconnected,
};

class Connector : public DrmPropObject
{
	friend class Card;
public:
	void refresh();

	Videomode get_default_mode() const;

	Videomode get_mode(const std::string& mode) const;
	Videomode get_mode(unsigned xres, unsigned yres, float vrefresh, bool ilace) const;

	Crtc* get_current_crtc() const;
	std::vector<Crtc*> get_possible_crtcs() const;

	// true if connected or unknown
	bool connected() const;
	ConnectorStatus connector_status() const;

	const std::string& fullname() const { return m_fullname; }
	uint32_t connector_type() const;
	uint32_t connector_type_id() const;
	uint32_t mmWidth() const;
	uint32_t mmHeight() const;
	uint32_t subpixel() const;
	const std::string& subpixel_str() const;
	std::vector<Videomode> get_modes() const;
	std::vector<Encoder*> get_encoders() const;
private:
	Connector(Card& card, uint32_t id, uint32_t idx);
	~Connector();

	void setup();
	void restore_mode();

	ConnectorPriv* m_priv;

	std::string m_fullname;

	Encoder* m_current_encoder;

	Crtc* m_saved_crtc;
};
}
