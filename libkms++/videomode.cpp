#include <xf86drm.h>
#include <xf86drmMode.h>

#include "videomode.h"
#include "helpers.h"

using namespace std;

namespace kms
{

unique_ptr<Blob> Videomode::to_blob(Card& card) const
{
	drmModeModeInfo drm_mode = video_mode_to_drm_mode(*this);

	return unique_ptr<Blob>(new Blob(card, &drm_mode, sizeof(drm_mode)));
}

}
