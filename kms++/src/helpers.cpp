
#include <kms++/kms++.h>
#include "helpers.h"
#include <cstring>

#define CPY(field) dst.field = src.field

namespace kms
{
Videomode drm_mode_to_video_mode(const drmModeModeInfo& drmmode)
{
	Videomode mode = { };

	auto& src = drmmode;
	auto& dst = mode;

	CPY(clock);

	CPY(hdisplay);
	CPY(hsync_start);
	CPY(hsync_end);
	CPY(htotal);
	CPY(hskew);

	CPY(vdisplay);
	CPY(vsync_start);
	CPY(vsync_end);
	CPY(vtotal);
	CPY(vscan);

	CPY(vrefresh);

	CPY(flags);
	CPY(type);

	mode.name = drmmode.name;

	return mode;
}

drmModeModeInfo video_mode_to_drm_mode(const Videomode& mode)
{
	drmModeModeInfo drmmode = { };

	auto& src = mode;
	auto& dst = drmmode;

	CPY(clock);

	CPY(hdisplay);
	CPY(hsync_start);
	CPY(hsync_end);
	CPY(htotal);
	CPY(hskew);

	CPY(vdisplay);
	CPY(vsync_start);
	CPY(vsync_end);
	CPY(vtotal);
	CPY(vscan);

	CPY(vrefresh);

	CPY(flags);
	CPY(type);

	strncpy(drmmode.name, mode.name.c_str(), sizeof(drmmode.name));
	drmmode.name[sizeof(drmmode.name) - 1] = 0;

	return drmmode;
}
}
