#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cmath>
#include <sstream>
#include <fmt/format.h>

#include <kms++/kms++.h>
#include "helpers.h"

using namespace std;

namespace kms
{
bool Videomode::valid() const
{
	return !!clock;
}

unique_ptr<Blob> Videomode::to_blob(Card& card) const
{
	drmModeModeInfo drm_mode = video_mode_to_drm_mode(*this);

	return unique_ptr<Blob>(new Blob(card, &drm_mode, sizeof(drm_mode)));
}

float Videomode::calculated_vrefresh() const
{
	// XXX interlace should only halve visible vertical lines, not blanking
	float refresh = (clock * 1000.0) / (htotal * vtotal) * (interlace() ? 2 : 1);
	return roundf(refresh * 100.0) / 100.0;
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

string Videomode::to_string_short() const
{
	return fmt::format("{}x{}{}@{:.2f}", hdisplay, vdisplay, interlace() ? "i" : "", calculated_vrefresh());
}

static char sync_to_char(SyncPolarity pol)
{
	switch (pol) {
	case SyncPolarity::Positive:
		return '+';
	case SyncPolarity::Negative:
		return '-';
	default:
		return '?';
	}
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

template<typename T>
std::string join(const T& values, const std::string& delim)
{
	std::ostringstream ss;
	for (const auto& v : values) {
		if (&v != &values[0])
			ss << delim;
		ss << v;
	}
	return ss.str();
}

static const map<int, string> mode_type_map = {
	// the deprecated ones don't care about a short name
	{ DRM_MODE_TYPE_BUILTIN, "builtin" }, // deprecated
	{ DRM_MODE_TYPE_CLOCK_C, "clock_c" }, // deprecated
	{ DRM_MODE_TYPE_CRTC_C, "crtc_c" }, // deprecated
	{ DRM_MODE_TYPE_PREFERRED, "P" },
	{ DRM_MODE_TYPE_DEFAULT, "default" }, // deprecated
	{ DRM_MODE_TYPE_USERDEF, "U" },
	{ DRM_MODE_TYPE_DRIVER, "D" },
};

static const map<int, string> mode_flag_map = {
	// the first 5 flags are displayed elsewhere
	{ DRM_MODE_FLAG_PHSYNC, "" },
	{ DRM_MODE_FLAG_NHSYNC, "" },
	{ DRM_MODE_FLAG_PVSYNC, "" },
	{ DRM_MODE_FLAG_NVSYNC, "" },
	{ DRM_MODE_FLAG_INTERLACE, "" },
	{ DRM_MODE_FLAG_DBLSCAN, "dblscan" },
	{ DRM_MODE_FLAG_CSYNC, "csync" },
	{ DRM_MODE_FLAG_PCSYNC, "pcsync" },
	{ DRM_MODE_FLAG_NCSYNC, "ncsync" },
	{ DRM_MODE_FLAG_HSKEW, "hskew" },
	{ DRM_MODE_FLAG_BCAST, "bcast" }, // deprecated
	{ DRM_MODE_FLAG_PIXMUX, "pixmux" }, // deprecated
	{ DRM_MODE_FLAG_DBLCLK, "2x" },
	{ DRM_MODE_FLAG_CLKDIV2, "clkdiv2" },
};

static const map<int, string> mode_3d_map = {
	{ DRM_MODE_FLAG_3D_NONE, "" },
	{ DRM_MODE_FLAG_3D_FRAME_PACKING, "3dfp" },
	{ DRM_MODE_FLAG_3D_FIELD_ALTERNATIVE, "3dfa" },
	{ DRM_MODE_FLAG_3D_LINE_ALTERNATIVE, "3dla" },
	{ DRM_MODE_FLAG_3D_SIDE_BY_SIDE_FULL, "3dsbs" },
	{ DRM_MODE_FLAG_3D_L_DEPTH, "3dldepth" },
	{ DRM_MODE_FLAG_3D_L_DEPTH_GFX_GFX_DEPTH, "3dgfx" },
	{ DRM_MODE_FLAG_3D_TOP_AND_BOTTOM, "3dtab" },
	{ DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF, "3dsbs" },
};

static const map<int, string> mode_aspect_map = {
	{ DRM_MODE_FLAG_PIC_AR_NONE, "" },
	{ DRM_MODE_FLAG_PIC_AR_4_3, "4:3" },
	{ DRM_MODE_FLAG_PIC_AR_16_9, "16:9" },
	{ DRM_MODE_FLAG_PIC_AR_64_27, "64:27" },
	{ DRM_MODE_FLAG_PIC_AR_256_135, "256:135" },
};

static string mode_type_str(uint32_t val)
{
	vector<string> s;
	for (const auto& [k, v] : mode_type_map) {
		if (val & k) {
			if (!v.empty())
				s.push_back(v);
			val &= ~k;
		}
	}
	// any unknown bits
	if (val != 0)
		s.push_back(fmt::format("{:#x}", val));
	return join(s, "|");
}

static string mode_flag_str(uint32_t val)
{
	vector<string> s;
	for (const auto& [k, v] : mode_flag_map) {
		if (val & k) {
			if (!v.empty())
				s.push_back(v);
			val &= ~k;
		}
	}
	auto it = mode_3d_map.find(val & DRM_MODE_FLAG_3D_MASK);
	if (it != mode_3d_map.end()) {
		if (!it->second.empty())
			s.push_back(it->second);
		val &= ~DRM_MODE_FLAG_3D_MASK;
	}
	it = mode_aspect_map.find(val & DRM_MODE_FLAG_PIC_AR_MASK);
	if (it != mode_aspect_map.end()) {
		if (!it->second.empty())
			s.push_back(it->second);
		val &= ~DRM_MODE_FLAG_PIC_AR_MASK;
	}
	// any unknown bits
	if (val != 0)
		s.push_back(fmt::format("{:#x}", val));
	return join(s, "|");
}

string Videomode::to_string_long() const
{
	string h = fmt::format("{}/{}/{}/{}/{}", hdisplay, hfp(), hsw(), hbp(), sync_to_char(hsync()));
	string v = fmt::format("{}/{}/{}/{}/{}", vdisplay, vfp(), vsw(), vbp(), sync_to_char(vsync()));

	string str = fmt::format("{} {:.3f} {} {} {} ({:.2f}) {} {}",
				 to_string_short(),
				 clock / 1000.0,
				 h, v,
				 vrefresh, calculated_vrefresh(),
				 mode_type_str(type),
				 mode_flag_str(flags));

	return str;
}

string Videomode::to_string_long_padded() const
{
	string h = fmt::format("{}/{}/{}/{}/{}", hdisplay, hfp(), hsw(), hbp(), sync_to_char(hsync()));
	string v = fmt::format("{}/{}/{}/{}/{}", vdisplay, vfp(), vsw(), vbp(), sync_to_char(vsync()));

	string str = fmt::format("{:<16} {:7.3f} {:<18} {:<18} {:2} ({:.2f}) {:<5} {}",
				 to_string_short(),
				 clock / 1000.0,
				 h, v,
				 vrefresh, calculated_vrefresh(),
				 mode_type_str(type),
				 mode_flag_str(flags));

	return str;
}

Videomode videomode_from_timings(uint32_t clock_khz,
				 uint16_t hact, uint16_t hfp, uint16_t hsw, uint16_t hbp,
				 uint16_t vact, uint16_t vfp, uint16_t vsw, uint16_t vbp)
{
	Videomode m{};
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

} // namespace kms
