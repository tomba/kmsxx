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

float Videomode::calculated_vrefresh() const
{
	return (clock * 1000.0) / (htotal * vtotal) * (interlace() ? 2 : 1);
}

bool Videomode::interlace() const
{
	return flags & DRM_MODE_FLAG_INTERLACE;
}

SyncPolarity Videomode::hsync() const
{
	if (flags & DRM_MODE_FLAG_PHSYNC)
		return SyncPolarity::Positive;
	if (flags & DRM_MODE_FLAG_NHSYNC)
		return SyncPolarity::Negative;
	return SyncPolarity::Undefined;
}

SyncPolarity Videomode::vsync() const
{
	if (flags & DRM_MODE_FLAG_PVSYNC)
		return SyncPolarity::Positive;
	if (flags & DRM_MODE_FLAG_NVSYNC)
		return SyncPolarity::Negative;
	return SyncPolarity::Undefined;
}

void Videomode::set_interlace(bool ilace)
{
	if (ilace)
		flags |= DRM_MODE_FLAG_INTERLACE;
	else
		flags &= ~DRM_MODE_FLAG_INTERLACE;
}

void Videomode::set_hsync(SyncPolarity pol)
{
	flags &= ~(DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NHSYNC);

	switch (pol) {
	case SyncPolarity::Positive:
		flags |= DRM_MODE_FLAG_PHSYNC;
		break;
	case SyncPolarity::Negative:
		flags |= DRM_MODE_FLAG_NHSYNC;
		break;
	default:
		break;
	}
}

void Videomode::set_vsync(SyncPolarity pol)
{
	flags &= ~(DRM_MODE_FLAG_PVSYNC | DRM_MODE_FLAG_NVSYNC);

	switch (pol) {
	case SyncPolarity::Positive:
		flags |= DRM_MODE_FLAG_PVSYNC;
		break;
	case SyncPolarity::Negative:
		flags |= DRM_MODE_FLAG_NVSYNC;
		break;
	default:
		break;
	}
}

Videomode videomode_from_timings(uint32_t clock_khz,
				 uint16_t hact, uint16_t hfp, uint16_t hsw, uint16_t hbp,
				 uint16_t vact, uint16_t vfp, uint16_t vsw, uint16_t vbp)
{
	Videomode m { };
	m.clock = clock_khz;

	m.hdisplay = hact;
	m.hsync_start = hact + hfp;
	m.hsync_end = hact + hfp + hsw;
	m.htotal = hact + hfp + hsw + hbp;

	m.vdisplay = vact;
	m.vsync_start = vact + vfp;
	m.vsync_end = vact + vfp + vsw;
	m.vtotal = vact + vfp + vsw + vbp;

	return m;
}

}
