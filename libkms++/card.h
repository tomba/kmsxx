#pragma once

#include <cstdint>
#include <vector>
#include <map>

#include "decls.h"
#include "pipeline.h"

namespace kms
{
class Card
{
public:
	Card();
	~Card();

	Card(const Card& other) = delete;
	Card& operator=(const Card& other) = delete;

	int fd() const { return m_fd; }

	Connector* get_first_connected_connector() const;

	DrmObject* get_object(uint32_t id) const;
	Connector* get_connector(uint32_t id) const;
	Crtc* get_crtc(uint32_t id) const;
	Crtc* get_crtc_by_index(uint32_t idx) const;
	Encoder* get_encoder(uint32_t id) const;
	Property* get_prop(uint32_t id) const;
	Property* get_prop(const std::string& name) const;

	bool master() const { return m_master; }
	bool has_atomic() const { return m_has_atomic; }
	bool has_has_universal_planes() const { return m_has_universal_planes; }

	void print_short() const;

	std::vector<Connector*> get_connectors() const;
	std::vector<Crtc*> get_crtcs() const;
	std::vector<DrmObject*> get_objects() const;
	std::vector<Plane*> get_planes() const;

	std::vector<Pipeline> get_connected_pipelines();

	void call_page_flip_handlers();

private:
	void restore_modes();

	std::map<uint32_t, DrmObject*> m_obmap;

	int m_fd;
	bool m_master;

	bool m_has_atomic;
	bool m_has_universal_planes;
};
}
