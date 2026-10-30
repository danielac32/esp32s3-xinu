/**
 * @file psrammem.c
 *
 * A second kernel memory allocator that hands out the PSRAM region mapped by
 * psramInit().  It mirrors the internal allocator in memget.c/memfree.c and
 * stkget.c, but keeps its own free list (psramlist) so the internal SRAM
 * heap stays untouched.
 *
 * Stacks are allocated here in preference to internal SRAM (see stkget.c) and
 * the network buffer pools are placed here too (see bfpalloc.c); free() in
 * memfree.c routes any address that lands in this region back to
 * psram_memfree(), so the existing stkfree()/bfpfree() callers need no
 * changes.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <interrupt.h>
#include <memory.h>
#include <psram.h>
#include <stdio.h>

/** Free list for the mapped PSRAM region. */
static struct memblock psramlist;

/** Total number of bytes handed to this allocator. */
static uint32_t psram_total;

/** Diagnostic counters: total allocated and largest single allocation. */
static uint32_t psram_alloc_count = 0;
static uint32_t psram_alloc_bytes = 0;
static uint32_t psram_largest_alloc = 0;

void psramMemInit(void)
{
    struct memblock *pm;

    if (!psram_present || psram_size == 0)
    {
        return;
    }

    /* Round the region down to a whole number of memblocks. */
    psram_total = psram_size & ~((uint32_t)MEMORY_DMBDEC);

    psramlist.next = pm = (struct memblock *)PSRAM_BASE;
    psramlist.length = psram_total;
    pm->next = NULL;
    pm->length = psram_total;
}

void *psram_memget(uintptr_t nbytes)
{
    register struct memblock *prev, *curr, *leftover;
    irqmask im;

    if (!psram_present || 0 == nbytes)
    {
        return (void *)SYSERR;
    }

    nbytes = (uintptr_t)roundmb(nbytes);

    im = disable();

    prev = &psramlist;
    curr = psramlist.next;
    while (curr != NULL)
    {
        if (curr->length == nbytes)
        {
            prev->next = curr->next;
            psramlist.length -= nbytes;
            psram_alloc_count++;
            psram_alloc_bytes += nbytes;
            if (nbytes > psram_largest_alloc) psram_largest_alloc = nbytes;
            restore(im);
            return (void *)curr;
        }
        else if (curr->length > nbytes)
        {
            /* split block into two */
            leftover = (struct memblock *)((ulong)curr + nbytes);
            leftover->next = curr->next;
            leftover->length = curr->length - nbytes;
            prev->next = leftover;
            psramlist.length -= nbytes;
            psram_alloc_count++;
            psram_alloc_bytes += nbytes;
            if (nbytes > psram_largest_alloc) psram_largest_alloc = nbytes;
            restore(im);
            return (void *)curr;
        }
        prev = curr;
        curr = curr->next;
    }

    restore(im);
    return (void *)SYSERR;
}

syscall psram_memfree(void *memptr, uintptr_t nbytes)
{
    register struct memblock *block, *next, *prev;
    irqmask im;
    ulong top;
    ulong base = (ulong)PSRAM_BASE;
    ulong end = base + psram_total;

    if ((0 == nbytes) || ((ulong)memptr < base) || ((ulong)memptr >= end))
    {
        return SYSERR;
    }

    block = (struct memblock *)memptr;
    nbytes = (ulong)roundmb(nbytes);

    im = disable();

    prev = &psramlist;
    next = psramlist.next;
    while ((next != NULL) && (next < block))
    {
        prev = next;
        next = next->next;
    }

    if (prev == &psramlist)
    {
        top = NULL;
    }
    else
    {
        top = (ulong)prev + prev->length;
    }

    if ((top > (ulong)block)
        || ((next != NULL) && ((ulong)block + nbytes) > (ulong)next))
    {
        restore(im);
        return SYSERR;
    }

    psramlist.length += nbytes;

    if (top == (ulong)block)
    {
        prev->length += nbytes;
        block = prev;
    }
    else
    {
        block->next = next;
        block->length = nbytes;
        prev->next = block;
    }

    if (((ulong)block + block->length) == (ulong)next)
    {
        block->length += next->length;
        block->next = next->next;
    }

    restore(im);
    return OK;
}

void *psram_stkget(uintptr_t nbytes)
{
    irqmask im;
    struct memblock *prev, *next, *fits, *fitsprev;

    if (!psram_present || 0 == nbytes)
    {
        return (void *)SYSERR;
    }

    nbytes = (uintptr_t)roundmb(nbytes);

    im = disable();

    prev = &psramlist;
    next = psramlist.next;
    fits = NULL;
    fitsprev = NULL;

    /* scan list for highest block that fits */
    while (next != NULL)
    {
        if (next->length >= nbytes)
        {
            fits = next;
            fitsprev = prev;
        }
        prev = next;
        next = next->next;
    }

    if (NULL == fits)
    {
        restore(im);
        return (void *)SYSERR;
    }

    if (nbytes == fits->length)
    {
        fitsprev->next = fits->next;
    }
    else
    {
        /* take top portion */
        fits->length -= nbytes;
        fits = (struct memblock *)((ulong)fits + fits->length);
    }

    psramlist.length -= nbytes;
    restore(im);
    return (void *)((ulong)fits + nbytes - sizeof(void *));
}

uint32_t psramFreeBytes(void)
{
    return (uint32_t)psramlist.length;
}

uint32_t psramUsedBytes(void)
{
    return psram_total - (uint32_t)psramlist.length;
}

uint32_t psramLargestFree(void)
{
    struct memblock *curr = psramlist.next;
    uint32_t best = 0;

    while (curr != NULL)
    {
        if (curr->length > best)
        {
            best = (uint32_t)curr->length;
        }
        curr = curr->next;
    }
    return best;
}

uint32_t psramAllocCount(void) { return psram_alloc_count; }
uint32_t psramAllocBytes(void) { return psram_alloc_bytes; }
uint32_t psramLargestAlloc(void) { return psram_largest_alloc; }

/* ---- malloc-style interface ----------------------------------------
 * Same accounting trick as libxc/malloc.c: stash the block size in a
 * struct memblock just before the pointer handed to the caller, so free()
 * needs no size argument.  The magic value (next == self) marks a live
 * block and lets psram_free() reject pointers it never handed out (a
 * string literal, a stack address) instead of corrupting the free list. */

void *psram_malloc(size_t size)
{
    struct memblock *pmem;

    if (0 == size)
    {
        return NULL;
    }

    size += sizeof(struct memblock);    /* room for the accounting header */

    pmem = (struct memblock *)psram_memget(size);
    if (SYSERR == (intptr_t)pmem)
    {
        return NULL;
    }

    pmem->next = pmem;
    pmem->length = size;

    return (void *)(pmem + 1);          /* +1 skips the header */
}

void *psram_calloc(size_t nmemb, size_t size)
{
    void *ptr;
    size_t total;

    if (0 == nmemb || 0 == size)
    {
        return NULL;
    }

    total = nmemb * size;
    if (total / nmemb != size)          /* multiplication overflow */
    {
        return NULL;
    }

    ptr = psram_malloc(total);
    if (ptr != NULL)
    {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *psram_realloc(void *ptr, size_t size)
{
    struct memblock *block;
    void *fresh;
    size_t old;

    if (NULL == ptr)
    {
        return psram_malloc(size);
    }
    if (0 == size)
    {
        psram_free(ptr);
        return NULL;
    }

    block = ((struct memblock *)ptr) - 1;
    if (block->next != block)           /* not a block we handed out */
    {
        return NULL;
    }
    old = block->length - sizeof(struct memblock);

    fresh = psram_malloc(size);
    if (NULL == fresh)
    {
        return NULL;
    }
    memcpy(fresh, ptr, (old < size) ? old : size);
    psram_free(ptr);
    return fresh;
}

void psram_free(void *ptr)
{
    struct memblock *block;

    if (NULL == ptr)
    {
        return;
    }

    block = ((struct memblock *)ptr) - 1;
    if (block->next != block)           /* not a block we handed out */
    {
        return;
    }

    psram_memfree(block, block->length);
}
