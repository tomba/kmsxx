#pragma once

namespace kms
{

class MappedFramebuffer : public Framebuffer, public IMappedFramebuffer
{
public:
	virtual ~MappedFramebuffer() { }

protected:
	MappedFramebuffer(Card& card, uint32_t id);
	MappedFramebuffer(Card& card, uint32_t width, uint32_t height);

public:
	virtual uint32_t width() const = 0;
	virtual uint32_t height() const = 0;

};

}
