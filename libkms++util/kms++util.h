#pragma once

#include "color.h"
#include <cstdio>
#include <cstdlib>

namespace kms
{
class IMappedFramebuffer;

void draw_color_bar(IMappedFramebuffer& buf, int old_xpos, int xpos, int width);

void draw_test_pattern(IMappedFramebuffer &fb);

void draw_rect(IMappedFramebuffer &fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, RGB color);
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define unlikely(x) __builtin_expect(!!(x), 0)

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
