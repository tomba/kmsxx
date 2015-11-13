#pragma once

namespace kms
{

struct Videomode
{
	uint32_t clock;
	uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
	uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

	uint32_t vrefresh;

	uint32_t flags;		// DRM_MODE_FLAG_*
	uint32_t type;		// DRM_MODE_TYPE_*

	std::string name;
};

}
