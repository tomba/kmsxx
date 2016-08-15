#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>
#include "helpers.h"

using namespace std;

namespace kms
{

unique_ptr<Blob> Videomode::to_blob(Card& card) const
{
	drmModeModeInfo drm_mode = video_mode_to_drm_mode(*this);

	return unique_ptr<Blob>(new Blob(card, &drm_mode, sizeof(drm_mode)));
}

bool Videomode::interlace() const
{
	return flags & DRM_MODE_FLAG_INTERLACE;
}

float Videomode::calculated_vrefresh() const
{
	return (clock * 1000.0) / (htotal * vtotal) * (interlace() ? 2 : 1);
}

}
