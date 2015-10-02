#pragma once

#include "color.h"
#include "conv.h"
#include "testpat.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define unlikely(x) __builtin_expect(!!(x), 0)

static void ASSERT_FAIL(const char *cond, const char *file,
			   unsigned int line, const char *func) __attribute__ ((__noreturn__));

static void ASSERT_FAIL(const char *cond, const char *file,
			   unsigned int line, const char *func)
{
	fprintf(stderr, "%s:%d: %s: ASSERT(%s) failed\n", file, line, func, cond);
	abort();
}

#define ASSERT(x) if (unlikely(!(x))) { ASSERT_FAIL( __STRING(x), __FILE__, __LINE__, __PRETTY_FUNCTION__); }

static void FAIL_IF_FAIL(const char *txt, const char *file,
			   unsigned int line, const char *func) __attribute__ ((__noreturn__));

static void FAIL_IF_FAIL(const char *txt, const char *file,
			   unsigned int line, const char *func)
{
	fprintf(stderr, "%s:%d: %s: FAIL: %s\n", file, line, func, txt);
	abort();
}

#define FAIL_IF(x, y) if (unlikely(x)) { FAIL_IF_FAIL(y, __FILE__, __LINE__, __PRETTY_FUNCTION__); }
