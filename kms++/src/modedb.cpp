#include <xf86drm.h>
#include <stdexcept>

#include <kms++/modedb.h>

using namespace std;

namespace kms
{

static const Videomode& find_from_table(const Videomode* modes, uint32_t width, uint32_t height, uint32_t refresh, bool ilace)
{
	for (unsigned i = 0; modes[i].clock; ++i) {
		const Videomode& m = modes[i];

		if (m.hdisplay != width || m.vdisplay != height)
			continue;

		if (refresh && m.vrefresh != refresh)
			continue;

		if (ilace != !!(m.flags & DRM_MODE_FLAG_INTERLACE))
			continue;

		return m;
	}

	throw invalid_argument("mode not found");
}

const Videomode& find_dmt(uint32_t width, uint32_t height, uint32_t refresh, bool ilace)
{
	return find_from_table(dmt_modes, width, height, refresh, ilace);
}

const Videomode& find_cea(uint32_t width, uint32_t height, uint32_t refresh, bool ilace)
{
	return find_from_table(cea_modes, width, height, refresh, ilace);
}

}
