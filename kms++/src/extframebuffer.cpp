
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <kms++/kms++.h>

using namespace std;

namespace kms
{

ExtFramebuffer::ExtFramebuffer(Card& card, uint32_t width, uint32_t height, uint8_t depth, uint8_t bpp, uint32_t stride, uint32_t handle)
	:Framebuffer(card, width, height)
{
	uint32_t id;
	int r = drmModeAddFB(card.fd(), width, height, depth, bpp, stride, handle, &id);
	if (r)
		throw invalid_argument("fob");

	set_id(id);
}

ExtFramebuffer::ExtFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format,
			       uint32_t handles[], uint32_t pitches[], uint32_t offsets[])
	: Framebuffer(card, width, height)
{
	uint32_t id;
	int r = drmModeAddFB2(card.fd(), width, height, (uint32_t)format, handles, pitches, offsets, &id, 0);
	if (r)
		throw std::invalid_argument(string("Failed to create ExtFramebuffer: ") + strerror(r));

	set_id(id);
}

ExtFramebuffer::ExtFramebuffer(Card& card, uint32_t width, uint32_t height, PixelFormat format,
			       int fds[4], uint32_t pitches[4], uint32_t offsets[4])
	: Framebuffer(card, width, height)
{
	int r;

	const PixelFormatInfo& format_info = get_pixel_format_info(format);

	uint32_t handles[4] = { 0 };

	for (int i = 0; i < format_info.num_planes; ++i) {
		r = drmPrimeFDToHandle(card.fd(), fds[i], &handles[i]);
		if (r)
			throw invalid_argument(string("drmPrimeFDToHandle: ") + strerror(errno));
	}

	uint32_t id;
	r = drmModeAddFB2(card.fd(), width, height, (uint32_t)format,
			  handles, pitches, offsets, &id, 0);
	if (r)
		throw invalid_argument(string("drmModeAddFB2 failed: ") + strerror(errno));

	set_id(id);
}

ExtFramebuffer::~ExtFramebuffer()
{
	drmModeRmFB(card().fd(), id());
}

}
