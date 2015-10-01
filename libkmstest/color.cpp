#include "color.h"

namespace kms
{
RGB::RGB()
{
	r = g = b = a = 0;
}

RGB::RGB(uint8_t r, uint8_t g, uint8_t b)
{
	this->r = r;
	this->g = g;
	this->b = b;
	this->a = 0;
}

uint16_t RGB::rgb565() const
{
	uint16_t r = (this->r >> 3) << 11;
	uint16_t g = (this->g >> 2) << 5;
	uint16_t b = (this->b >> 3) << 0;
	return r | g | b;
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
