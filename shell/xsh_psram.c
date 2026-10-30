/**
 * @file     xsh_psram.c
 *
 * Shell command (psram) reports the state of the external PSRAM mapped by
 * psramInit(): how much is mapped, how much of it the kernel allocator has
 * handed out (thread stacks, network buffers), and how much is still free.
 * With -t it also runs a destructive read/write test over a sample of the
 * region.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <platform.h>
#include <psram.h>
#include <thread.h>
#include <memory.h>
#include <conf.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define KB(x)   ((ulong)(x) / 1024UL)

/** Whole-percent of @p part with respect to @p whole (rounded). */
static ulong pct(ulong part, ulong whole)
{
    if (0 == whole)
    {
        return 0;
    }
    return (part * 100UL + whole / 2) / whole;
}

/**
 * Walk a sample of the region writing a position-dependent pattern and
 * reading it back.  Catches a die that maps but cannot hold data.
 *
 * @return number of mismatches found (0 means the region is good).
 */
static ulong psram_test(void)
{
    volatile ulong *base = (volatile ulong *)PSRAM_BASE;
    ulong nwords = (ulong)psram_size / sizeof(ulong);
    ulong bad = 0;
    ulong stride = nwords / 256UL;
    ulong i;

    if (0 == stride)
    {
        stride = 1;
    }

    for (i = 0; i < nwords; i += stride)
    {
        base[i] = ~i;
    }
    for (i = 0; i < nwords; i += stride)
    {
        if (base[i] != ~i)
        {
            bad++;
        }
    }

    /* A short contiguous run over the very first words. */
    for (i = 0; i < 256UL && i < nwords; i++)
    {
        base[i] = 0xA5A50000UL + i;
    }
    for (i = 0; i < 256UL && i < nwords; i++)
    {
        if (base[i] != 0xA5A50000UL + i)
        {
            bad++;
        }
    }

    return bad;
}

static void usage(char *command)
{
    printf("Usage: %s [-t]\n\n", command);
    printf("Description:\n");
    printf("\tReports the external PSRAM (external RAM) mapped by the\n");
    printf("\tkernel: size, usage and free space.\n");
    printf("Options:\n");
    printf("\t-t\trun a destructive read/write test over the region\n");
    printf("\t--help\tdisplay this help and exit\n");
}

/**
 * @ingroup shell
 *
 * Shell command (psram) reports PSRAM usage.
 *
 * @param nargs number of arguments in args array
 * @param args  array of arguments
 * @return non-zero value on error
 */
shellcmd xsh_psram(int nargs, char *args[])
{
    ulong total;
    ulong free;
    ulong used;
    bool test = FALSE;
    int i;

    for (i = 1; i < nargs; i++)
    {
        if (0 == strcmp(args[i], "--help"))
        {
            usage(args[0]);
            return 0;
        }
        else if (0 == strcmp(args[i], "-t"))
        {
            test = TRUE;
        }
        else
        {
            fprintf(stderr, "%s: invalid argument '%s'\n", args[0], args[i]);
            fprintf(stderr, "Try '%s --help' for more information\n", args[0]);
            return 1;
        }
    }

    printf("External PSRAM\n");
    printf("===========================================================\n");

    if (!psram_present || 0 == psram_size)
    {
        printf("  Status            : not available\n");
        printf("  (the die did not come up; the kernel is running from\n");
        printf("   internal SRAM only)\n");
        printf("===========================================================\n");
        return 0;
    }

    total = (ulong)psram_size;
    free = (ulong)psramFreeBytes();
    used = (ulong)psramUsedBytes();

    printf("  Base address      : 0x%08lX\n", (ulong)PSRAM_BASE);
    printf("  Size              : %8lu B  (%lu KB)\n", total, KB(total));
    printf("-----------------------------------------------------------\n");
    printf("  In use            : %8lu B  %3lu%%\n", used, pct(used, total));
    printf("  Free              : %8lu B  %3lu%%\n", free, pct(free, total));
    printf("  Largest free block: %8lu B\n", (ulong)psramLargestFree());

    /* How much of the PSRAM is currently covered by thread stacks. */
    {
        ulong stack = 0;

        for (i = 0; i < NTHREAD; i++)
        {
            ulong b;

            if (thrtab[i].state == THRFREE)
            {
                continue;
            }
            b = (ulong)thrtab[i].stkbase;
            if (b >= (ulong)PSRAM_BASE && b < (ulong)PSRAM_BASE + total)
            {
                stack += thrtab[i].stklen;
            }
        }
        printf("  Thread stacks     : %8lu B\n", stack);
        printf("  Alloc count       : %8lu\n", (ulong)psramAllocCount());
        printf("  Alloc bytes       : %8lu B\n", (ulong)psramAllocBytes());
        printf("  Largest alloc     : %8lu B\n", (ulong)psramLargestAlloc());
    }

    if (test)
    {
        ulong bad = psram_test();

        printf("-----------------------------------------------------------\n");
        if (0 == bad)
        {
            printf("  Self-test         : PASS\n");
        }
        else
        {
            printf("  Self-test         : FAIL (%lu mismatches)\n", bad);
        }
    }

    printf("===========================================================\n");
    return 0;
}
