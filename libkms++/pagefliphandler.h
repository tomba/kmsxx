#pragma once

namespace kms {
class PageFlipHandlerBase
{
public:
	PageFlipHandlerBase() { }
	virtual ~PageFlipHandlerBase() { }
	virtual void handle_page_flip(uint32_t frame, double time) = 0;
};
}
