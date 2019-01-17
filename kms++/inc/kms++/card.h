#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <memory>

#include "decls.h"
#include "pipeline.h"

namespace kms
{
class Card
{
	friend class Framebuffer;
public:
	Card(const std::string& dev_path = "");
	Card(const std::string& driver, uint32_t idx);
	virtual ~Card();

	Card(const Card& other) = delete;
	Card& operator=(const Card& other) = delete;

	int fd() const { return m_fd; }

	void drop_master();

	Connector* get_first_connected_connector() const;

	DrmObject* get_object(uint32_t id) const;
	Connector* get_connector(uint32_t id) const;
	Crtc* get_crtc(uint32_t id) const;
	Encoder* get_encoder(uint32_t id) const;
	Plane* get_plane(uint32_t id) const;
	Property* get_prop(uint32_t id) const;

	bool is_master() const { return m_is_master; }
	bool has_atomic() const { return m_has_atomic; }
	bool has_has_universal_planes() const { return m_has_universal_planes; }
	bool has_dumb_buffers() const { return m_has_dumb; }
	bool has_kms() const;

	const std::vector<Connector*> get_connectors() const { return m_connectors; }
	const std::vector<Encoder*> get_encoders() const { return m_encoders; }
	const std::vector<Crtc*> get_crtcs() const { return m_crtcs; }
	const std::vector<Plane*> get_planes() const { return m_planes; }
	const std::vector<Property*> get_properties() const { return m_properties; }

	const std::vector<DrmObject*> get_objects() const;

	std::vector<Pipeline> get_connected_pipelines();

	void call_page_flip_handlers();

	int disable_all();

	const std::string& version_name() const { return m_version_name; }

private:
	void setup();
	void restore_modes();

	std::map<uint32_t, DrmObject*> m_obmap;

	std::vector<Connector*> m_connectors;
	std::vector<Encoder*> m_encoders;
	std::vector<Crtc*> m_crtcs;
	std::vector<Plane*> m_planes;
	std::vector<Property*> m_properties;
	std::vector<Framebuffer*> m_framebuffers;

	int m_fd;
	bool m_is_master;

	bool m_has_atomic;
	bool m_has_universal_planes;
	bool m_has_dumb;

	int m_version_major;
	int m_version_minor;
	int m_version_patchlevel;
	std::string m_version_name;
	std::string m_version_date;
	std::string m_version_desc;
};
}
