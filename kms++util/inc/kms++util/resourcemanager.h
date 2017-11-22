#include <kms++/kms++.h>
#include <vector>
#include <string>

namespace kms {

class ResourceManager
{
public:
	ResourceManager(Card& card);

	void reset();

	Card& card() const { return m_card; }
	Connector* reserve_connector(const std::string& name = "");
	Connector* reserve_connector(Connector* conn);
	Crtc* reserve_crtc(Connector* conn);
	Crtc* reserve_crtc(Crtc* crtc);
	Plane* reserve_plane(Crtc* crtc, PlaneType type, PixelFormat format = PixelFormat::Undefined);
	Plane* reserve_plane(Plane* plane);
	Plane* reserve_generic_plane(Crtc* crtc, PixelFormat format = PixelFormat::Undefined);
	Plane* reserve_primary_plane(Crtc* crtc, PixelFormat format = PixelFormat::Undefined);
	Plane* reserve_overlay_plane(Crtc* crtc, PixelFormat format = PixelFormat::Undefined);

private:
	Card& m_card;
	std::vector<Connector*> m_reserved_connectors;
	std::vector<Crtc*> m_reserved_crtcs;
	std::vector<Plane*> m_reserved_planes;
};

}
