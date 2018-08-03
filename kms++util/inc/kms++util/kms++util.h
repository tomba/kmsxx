#pragma once

#include <kms++/kms++.h>

#include <kms++util/color.h>
#include <kms++util/strhelpers.h>
#include <kms++util/cpuframebuffer.h>
#include <kms++util/extcpuframebuffer.h>
#include <kms++util/stopwatch.h>
#include <kms++util/opts.h>
#include <kms++util/resourcemanager.h>

#include <cstdio>
#include <cstdlib>

namespace kms
{
class IFramebuffer;

void draw_rgb_pixel(IFramebuffer& buf, unsigned x, unsigned y, RGB color);
void draw_yuv422_macropixel(IFramebuffer& buf, unsigned x, unsigned y, YUV yuv1, YUV yuv2);
void draw_yuv420_macropixel(IFramebuffer& buf, unsigned x, unsigned y,
				   YUV yuv1, YUV yuv2, YUV yuv3, YUV yuv4);
void draw_rect(IFramebuffer &fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color);
void draw_circle(IFramebuffer& fb, int32_t xCenter, int32_t yCenter, int32_t radius, RGB color);
void draw_text(IFramebuffer& buf, uint32_t x, uint32_t y, const std::string& str, RGB color);

void draw_color_bar(IFramebuffer& buf, int old_xpos, int xpos, int width);

void draw_test_pattern(IFramebuffer &fb, YUVType yuvt = YUVType::BT601_Lim);
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define unlikely(x) __builtin_expect(!!(x), 0)

/* __STRING(x) is a glibcism (i.e. not standard), which happens to also
 * be available in uClibc. However, musl does not define it. Do it here.
 */
#ifndef __STRING
#define __STRING(x) #x
#endif

#define ASSERT(x) \
	if (unlikely(!(x))) { \
		fprintf(stderr, "%s:%d: %s: ASSERT(%s) failed\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, __STRING(x)); \
		abort(); \
	}

#define FAIL(fmt, ...) \
	do { \
		fprintf(stderr, "%s:%d: %s:\n" fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
		abort(); \
	} while(0)

#define FAIL_IF(x, fmt, ...) \
	if (unlikely(x)) { \
		fprintf(stderr, "%s:%d: %s:\n" fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
		abort(); \
	}

#define EXIT(fmt, ...) \
	do { \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
		exit(-1); \
	} while(0)

#define EXIT_IF(x, fmt, ...) \
	if (unlikely(x)) { \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
		exit(-1); \
	}
