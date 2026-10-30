/**
 * @file xmalloc.c
 *
 * Buffers de trabajo fuera de la pila: PSRAM si está presente, heap interno
 * del kernel como respaldo.  Evita meter buffers de cientos o miles de bytes
 * en tareas con pila pequeña (SHELL_CMDSTK = 4096).
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <kernel.h>
#include <memory.h>
#include <psram.h>
#include <stddef.h>
#include <stdint.h>

#include <xmalloc.h>

void *xmalloc(size_t n)
{
    void *p;

    if (0 == n)
    {
        return NULL;
    }

    p = psram_malloc(n);
    if (p != NULL)
    {
        return p;
    }

    p = memget((uintptr_t)n);
    if ((void *)SYSERR == p)
    {
        return NULL;
    }

    return p;
}

void xfree(void *p, size_t n)
{
    uintptr_t addr;

    if (NULL == p || 0 == n)
    {
        return;
    }

    addr = (uintptr_t)p;

    /* psram_malloc() only hands out addresses inside the mapped die. */
    if (psram_present && addr >= (uintptr_t)PSRAM_BASE &&
        addr < (uintptr_t)PSRAM_BASE + (uintptr_t)psram_size)
    {
        psram_free(p);
    }
    else
    {
        memfree(p, (uintptr_t)n);
    }
}
