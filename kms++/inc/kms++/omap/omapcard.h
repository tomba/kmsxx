#pragma once

#include <kms++/card.h>

struct omap_device;

namespace kms
{
class OmapCard : public Card
{
public:
	OmapCard(const std::string& device = "");
	virtual ~OmapCard();

	struct omap_device* dev() const { return m_omap_dev; }

private:
	struct omap_device* m_omap_dev;
};
}
