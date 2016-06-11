#pragma once

#include <xf86drm.h>
#include <xf86drmMode.h>

namespace kms
{
struct Videomode;

Videomode drm_mode_to_video_mode(const drmModeModeInfo& drmmode);
drmModeModeInfo video_mode_to_drm_mode(const Videomode& mode);
}
