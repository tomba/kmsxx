#pragma once

namespace kms
{
class PageFlipHandlerBase
{
public:
	PageFlipHandlerBase() {}
	virtual ~PageFlipHandlerBase() {}
	virtual void handle_vblank(uint32_t frame) { };
	virtual void handle_page_flip(uint32_t frame, double time) { };
	virtual void handle_page_flip2(uint32_t frame, double time, uint32_t crtc_id) { };
};
} // namespace kms
