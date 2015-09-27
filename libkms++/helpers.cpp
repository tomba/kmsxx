
#include "connector.h"
#include "helpers.h"
#include <cstring>

namespace kms
{
Videomode drm_mode_to_video_mode(const drmModeModeInfo& drmmode)
{
	// XXX these are the same at the moment
	Videomode mode;
	memcpy(&mode, &drmmode, sizeof(mode));
	return mode;
}

drmModeModeInfo video_mode_to_drm_mode(const Videomode& mode)
{
	// XXX these are the same at the moment
	drmModeModeInfo drmmode;
	memcpy(&drmmode, &mode, sizeof(drmmode));
	return drmmode;
}
}
