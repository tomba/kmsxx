#pragma once

#include <string>
#include <cstdint>
#include <memory>

#include "blob.h"

namespace kms
{

struct Videomode
{
	std::string name;

	uint32_t clock;
	uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
	uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

	uint32_t vrefresh;

	uint32_t flags;		// DRM_MODE_FLAG_*
	uint32_t type;		// DRM_MODE_TYPE_*

	std::unique_ptr<Blob> to_blob(Card& card) const;

	uint16_t hfp() const { return hsync_start - hdisplay; }
	uint16_t hsw() const { return hsync_end - hsync_start; }
	uint16_t hbp() const { return htotal - hsync_end; }

	uint16_t vfp() const { return vsync_start - vdisplay; }
	uint16_t vsw() const { return vsync_end - vsync_start; }
	uint16_t vbp() const { return vtotal - vsync_end; }
};

}
