#pragma once

#include <string>
#include <cstdint>
#include <memory>

#include "blob.h"

namespace kms
{

enum class SyncPolarity
{
	Undefined,
	Positive,
	Negative,
};

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

	float calculated_vrefresh() const;

	bool interlace() const;
	SyncPolarity hsync() const;
	SyncPolarity vsync() const;

	void set_interlace(bool ilace);
	void set_hsync(SyncPolarity pol);
	void set_vsync(SyncPolarity pol);

	std::string to_string() const;
};

struct Videomode videomode_from_timings(uint32_t clock_khz,
					uint16_t hact, uint16_t hfp, uint16_t hsw, uint16_t hbp,
					uint16_t vact, uint16_t vfp, uint16_t vsw, uint16_t vbp);
}
