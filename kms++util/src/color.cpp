#include <kms++util/color.h>

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

uint32_t RGB::rgb888() const
{
	return (r << 16) | (g << 8) | (b << 0);
}

uint32_t RGB::bgr888() const
{
	return (b << 16) | (g << 8) | (r << 0);
}

uint32_t RGB::argb8888() const
{
	return (a << 24) | (r << 16) | (g << 8) | (b << 0);
}

uint32_t RGB::abgr8888() const
{
	return (a << 24) | (b << 16) | (g << 8) | (r << 0);
}

uint32_t RGB::rgba8888() const
{
	return (r << 24) | (g << 16) | (b << 8) | (a << 0);
}

uint32_t RGB::bgra8888() const
{
	return (b << 24) | (g << 16) | (r << 8) | (a << 0);
}

uint32_t RGB::argb2101010() const
{
	return ((a >> 6) << 30) | (r << 22) | (g << 12) | (b << 2);
}

uint32_t RGB::abgr2101010() const
{
	return ((a >> 6) << 30) | (b << 22) | (g << 12) | (r << 2);
}

uint32_t RGB::rgba1010102() const
{
	return (r << 24) | (g << 14) | (b << 4) | (a >> 6);
}

uint32_t RGB::bgra1010102() const
{
	return (b << 24) | (g << 14) | (r << 4) | (a >> 6);
}

uint16_t RGB::rgb565() const
{
	return ((r >> 3) << 11) | ((g >> 2) << 5) | ((b >> 3) << 0);
}

uint16_t RGB::bgr565() const
{
	return ((b >> 3) << 11) | ((g >> 2) << 5) | ((r >> 3) << 0);
}

uint16_t RGB::argb4444() const
{
	return ((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | ((b >> 4) << 0);
}

uint16_t RGB::argb1555() const
{
	return ((!!a) << 15) | ((r >> 3) << 10) | ((g >> 3) << 5) | ((b >> 3) << 0);
}

YUV RGB::yuv(YUVType type) const
{
	return YUV(*this, type);
}

#define CF_ONE (256)
#define CF(a, b, c) { ((int) ((a) * CF_ONE)), ((int) ((b) * CF_ONE)), ((int) ((c) * CF_ONE)) }
#define CLAMP(a) ((a) > (CF_ONE-1) ? (CF_ONE-1) : (a) < 0 ? 0 : (a))

const int YUVcoef[static_cast<unsigned>(YUVType::MAX)][3][3] = {
	[static_cast<unsigned>(YUVType::BT601_Lim)] = {
		CF( 0.257,  0.504,  0.098),
		CF(-0.148, -0.291,  0.439),
		CF( 0.439, -0.368, -0.071) },
	[static_cast<unsigned>(YUVType::BT601_Full)] = {
		CF( 0.299,  0.587,  0.114),
		CF(-0.169, -0.331,  0.500),
		CF( 0.500, -0.419, -0.081) },
	[static_cast<unsigned>(YUVType::BT709_Lim)] = {
		CF( 0.1826,  0.6142,  0.0620),
		CF(-0.1006, -0.3386,  0.4392),
		CF( 0.4392, -0.3989, -0.0403) },
	[static_cast<unsigned>(YUVType::BT709_Full)] = {
		CF( 0.2126,  0.7152,  0.0722),
		CF(-0.1146, -0.3854,  0.5000),
		CF( 0.5000, -0.4542, -0.0468) },
};

const int YUVoffset[static_cast<unsigned>(YUVType::MAX)][3] = {
	[static_cast<unsigned>(YUVType::BT601_Lim)]  = CF(0.0625,  0.5,  0.5),
	[static_cast<unsigned>(YUVType::BT601_Full)] = CF(     0,  0.5,  0.5),
	[static_cast<unsigned>(YUVType::BT709_Lim)]  = CF(0.0625,  0.5,  0.5),
	[static_cast<unsigned>(YUVType::BT709_Full)] = CF(     0,  0.5,  0.5),
};

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

static inline
uint8_t MAKE_YUV_Y(uint8_t r, uint8_t g, uint8_t b, YUVType type)
{
	unsigned tidx = static_cast<unsigned>(type);

	return CLAMP(((YUVcoef[tidx][0][0] * r + YUVcoef[tidx][0][1] * g +
		      YUVcoef[tidx][0][2] * b + CF_ONE/2) / CF_ONE) +
		     YUVoffset[tidx][0]);
}

static inline
uint8_t MAKE_YUV_U(uint8_t r, uint8_t g, uint8_t b, YUVType type)
{
	unsigned tidx = static_cast<unsigned>(type);

	return CLAMP(((YUVcoef[tidx][1][0] * r + YUVcoef[tidx][1][1] * g +
		       YUVcoef[tidx][1][2] * b + CF_ONE/2) / CF_ONE) +
		     YUVoffset[tidx][1]);
}

static inline
uint8_t MAKE_YUV_V(uint8_t r, uint8_t g, uint8_t b, YUVType type)
{
	unsigned tidx = static_cast<unsigned>(type);

	return CLAMP(((YUVcoef[tidx][2][0] * r + YUVcoef[tidx][2][1] * g +
		       YUVcoef[tidx][2][2] * b + CF_ONE/2) / CF_ONE) +
		     YUVoffset[tidx][2]);
}

YUV::YUV(const RGB& rgb, YUVType type)
{
	this->y = MAKE_YUV_Y(rgb.r, rgb.g, rgb.b, type);
	this->u = MAKE_YUV_U(rgb.r, rgb.g, rgb.b, type);
	this->v = MAKE_YUV_V(rgb.r, rgb.g, rgb.b, type);
	this->a = rgb.a;
}
}
