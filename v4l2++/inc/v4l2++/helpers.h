#pragma once

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define unlikely(x) __builtin_expect(!!(x), 0)

/* __STRING(x) is a glibcism (i.e. not standard), which happens to also
 * be available in uClibc. However, musl does not define it. Do it here.
 */
#ifndef __STRING
#define __STRING(x) #x
#endif

#define ASSERT(x)                                                                                                        \
	if (unlikely(!(x))) {                                                                                            \
		fprintf(stderr, "%s:%d: %s: ASSERT(%s) failed\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, __STRING(x)); \
		abort();                                                                                                 \
	}

#define FAIL(fmt, ...)                                                                                            \
	do {                                                                                                      \
		fprintf(stderr, "%s:%d: %s:\n" fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
		abort();                                                                                          \
	} while (0)

#define FAIL_IF(x, fmt, ...)                                                                                      \
	if (unlikely(x)) {                                                                                        \
		fprintf(stderr, "%s:%d: %s:\n" fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
		abort();                                                                                          \
	}

#define EXIT(fmt, ...)                                    \
	do {                                              \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
		exit(-1);                                 \
	} while (0)

#define EXIT_IF(x, fmt, ...)                              \
	if (unlikely(x)) {                                \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
		exit(-1);                                 \
	}

