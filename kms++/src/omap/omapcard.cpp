
#include <kms++/omap/omapcard.h>

extern "C" {
#include <omap_drmif.h>
}

using namespace std;

namespace kms
{

OmapCard::OmapCard(const string& device)
	: Card(device)
{
	m_omap_dev = omap_device_new(fd());
}

OmapCard::~OmapCard()
{
	omap_device_del(m_omap_dev);
}

}
