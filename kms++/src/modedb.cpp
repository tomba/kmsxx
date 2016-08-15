#include <xf86drm.h>
#include <stdexcept>
#include <cmath>

#include <kms++/modedb.h>

using namespace std;

namespace kms
{

static const Videomode& find_from_table(const Videomode* modes, uint32_t width, uint32_t height, float vrefresh, bool ilace)
{
	for (unsigned i = 0; modes[i].clock; ++i) {
		const Videomode& m = modes[i];

		if (m.hdisplay != width || m.vdisplay != height)
			continue;

		if (ilace != m.interlace())
			continue;

		if (vrefresh && std::abs(m.calculated_vrefresh() - vrefresh) >= 0.001)
			continue;

		return m;
	}

	throw invalid_argument("mode not found");
}

const Videomode& find_dmt(uint32_t width, uint32_t height, float vrefresh, bool ilace)
{
	return find_from_table(dmt_modes, width, height, vrefresh, ilace);
}

const Videomode& find_cea(uint32_t width, uint32_t height, float vrefresh, bool ilace)
{
	return find_from_table(cea_modes, width, height, vrefresh, ilace);
}

}
