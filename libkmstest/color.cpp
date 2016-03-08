#include "color.h"

namespace kms
{
RGB::RGB()
{
	r = g = b = 0;
	a = 255;
}

RGB::RGB(uint8_t r, uint8_t g, uint8_t b)
	:RGB(255, r, g, b)
{
}

RGB::RGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
	this->r = r;
	this->g = g;
	this->b = b;
	this->a = a;
}

RGB::RGB(uint32_t argb)
{
	this->b = (argb >> 0) & 0xff;
	this->g = (argb >> 8) & 0xff;
	this->r = (argb >> 16) & 0xff;
	this->a = (argb >> 24) & 0xff;
}

uint32_t RGB::argb8888() const
{
	return (a << 24) | (r << 16) | (g << 8) | (b << 0);
}

uint32_t RGB::abgr8888() const
{
	return (a << 24) | (b << 16) | (g << 8) | (r << 0);
}

uint16_t RGB::rgb565() const
{
	return ((r >> 3) << 11) | ((g >> 2) << 5) | ((b >> 3) << 0);
}

YUV RGB::yuv() const
{
	return YUV(*this);
}


YUV::YUV()
{
	y = u = v = a = 0;
}

YUV::YUV(uint8_t y, uint8_t u, uint8_t v)
{
	this->y = y;
	this->u = u;
	this->v = v;
	this->a = 0;
}

static inline uint8_t MAKE_YUV_601_Y(uint8_t r, uint8_t g, uint8_t b)
{
	return (((66 * r + 129 * g +  25 * b + 128) >> 8) + 16);
}

static inline uint8_t MAKE_YUV_601_U(uint8_t r, uint8_t g, uint8_t b)
{
	return (((-38 * r -  74 * g + 112 * b + 128) >> 8) + 128);
}

static inline uint8_t MAKE_YUV_601_V(uint8_t r, uint8_t g, uint8_t b)
{
	return (((112 * r -  94 * g -  18 * b + 128) >> 8) + 128);
}

YUV::YUV(const RGB& rgb)
{
	this->y = MAKE_YUV_601_Y(rgb.r, rgb.g, rgb.b);
	this->u = MAKE_YUV_601_U(rgb.r, rgb.g, rgb.b);
	this->v = MAKE_YUV_601_V(rgb.r, rgb.g, rgb.b);
	this->a = rgb.a;
}
}
