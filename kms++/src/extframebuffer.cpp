
#include <cstring>
#include <cerrno>

#include <stdexcept>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{
ExtFramebuffer::ExtFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format,
			       vector<uint32_t> handles, vector<uint32_t> pitches, vector<uint32_t> offsets, vector<uint64_t> modifiers)
	: Framebuffer(card, width, height)
{
	m_format = format;

	const PixelFormatInfo& format_info = get_pixel_format_info(format);

	m_num_planes = format_info.num_planes;

	if (handles.size() != m_num_planes || pitches.size() != m_num_planes || offsets.size() != m_num_planes)
		throw std::invalid_argument("the size of handles, pitches and offsets has to match number of planes");

	for (int i = 0; i < format_info.num_planes; ++i) {
		FramebufferPlane& plane = m_planes.at(i);

		plane.handle = handles[i];
		plane.stride = pitches[i];
		plane.offset = offsets[i];
		plane.modifier = modifiers.empty() ? 0 : modifiers[i];
		plane.size = plane.stride * height;
		plane.map = 0;
	}

	uint32_t id;
	handles.resize(4);
	pitches.resize(4);
	offsets.resize(4);
	int r;

	if (modifiers.empty()) {
		r = drmModeAddFB2(card.fd(), width, height, (uint32_t)format, handles.data(), pitches.data(), offsets.data(), &id, 0);
	} else {
		modifiers.resize(4);
		r = drmModeAddFB2WithModifiers(card.fd(), width, height, (uint32_t)format, handles.data(), pitches.data(), offsets.data(), modifiers.data(), &id, DRM_MODE_FB_MODIFIERS);
	}

	if (r)
		throw std::invalid_argument(string("Failed to create ExtFramebuffer: ") + strerror(r));

	set_id(id);
}

ExtFramebuffer::~ExtFramebuffer()
{
	drmModeRmFB(card().fd(), id());
}

} // namespace kms
