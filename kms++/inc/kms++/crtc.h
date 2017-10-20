#pragma once

#include <vector>

#include "drmpropobject.h"

namespace kms
{

struct CrtcPriv;

class Crtc : public DrmPropObject
{
	friend class Card;
	friend class Connector;
public:
	void refresh();

	const std::vector<Plane*>& get_possible_planes() const { return m_possible_planes; }

	int set_mode(Connector* conn, const Videomode& mode);
	int set_mode(Connector* conn, Framebuffer& fb, const Videomode& mode);

	int set_plane(Plane *plane, Framebuffer &fb,
		      int32_t dst_x, int32_t dst_y, uint32_t dst_w, uint32_t dst_h,
		      float src_x, float src_y, float src_w, float src_h);
	int disable_mode();

	int disable_plane(Plane* plane);

	Plane* get_primary_plane();

	int page_flip(Framebuffer& fb, void *data);

	uint32_t buffer_id() const;
	uint32_t x() const;
	uint32_t y() const;
	uint32_t width() const;
	uint32_t height() const;
	int mode_valid() const;
	Videomode mode() const;
	int gamma_size() const;
private:
	Crtc(Card& card, uint32_t id, uint32_t idx);
	~Crtc();

	void setup();
	void restore_mode(Connector *conn);

	CrtcPriv* m_priv;

	std::vector<Plane*> m_possible_planes;
};
}
