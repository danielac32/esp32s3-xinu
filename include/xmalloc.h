/**
 * @file xmalloc.h
 *
 * Allocator para buffers de trabajo que no deben vivir en la pila de una
 * tarea.  Intenta PSRAM primero y cae al heap interno del kernel si la PSRAM
 * no está disponible.
 */
#ifndef _XMALLOC_H_
#define _XMALLOC_H_

#include <stddef.h>

/** Allocate @p n bytes from PSRAM, or from the kernel heap as a fallback. */
void *xmalloc(size_t n);

/**
 * Free a block from xmalloc().
 * @param p   pointer returned by xmalloc()
 * @param n   same size passed to xmalloc() (needed by the kernel heap)
 */
void xfree(void *p, size_t n);

#endif /* _XMALLOC_H_ */
