/**
 * @file assert.h
 *
 * Minimal assert() for the freestanding Xinu build.
 */
#ifndef _ASSERT_H_
#define _ASSERT_H_

#include <stddef.h>

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) ((void)0)
#endif

#endif /* _ASSERT_H_ */
