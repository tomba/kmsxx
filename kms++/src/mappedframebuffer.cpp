#include <kms++/kms++.h>

using namespace std;

namespace kms
{

MappedFramebuffer::MappedFramebuffer(Card& card, uint32_t id)
	: Framebuffer(card, id)
{

}

MappedFramebuffer::MappedFramebuffer(Card& card, uint32_t width, uint32_t height)
	: Framebuffer(card, width, height)
{

}

}
