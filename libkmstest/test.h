#pragma once

#include "color.h"
#include "kmstest.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define unlikely(x) __builtin_expect(!!(x), 0)

#define ASSERT(x) \
	if (unlikely(!(x))) { \
		fprintf(stderr, "%s:%d: %s: ASSERT(%s) failed\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, __STRING(x)); \
		abort(); \
	}

#define FAIL_IF(x, fmt, ...) \
	if (unlikely(x)) { \
		fprintf(stderr, "%s:%d: %s:\n" fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
		abort(); \
	}
