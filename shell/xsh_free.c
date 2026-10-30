/**
 * @file     xsh_free.c
 *
 * Shell command (free) reports, in detail, how much RAM is in use and how
 * much is still free: the reserved system area, the kernel text, every
 * thread's stack, the kernel heap (with its free list) and the network
 * buffer pools.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <platform.h>
#include <memory.h>
#include <bufpool.h>
#include <semaphore.h>
#include <thread.h>
#include <psram.h>
#include <conf.h>
#include <stdio.h>
#include <string.h>

extern void _start(void);

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

static void printRegions(ulong *heap_total, ulong *heap_used, ulong *heap_free)
{
    ulong phys;                 /* data-window size                */
    ulong resrv;                /* reserved system area            */
    ulong code;                 /* kernel text                     */
    ulong stack = 0;            /* sum of every live thread stack   */
    ulong kfree = 0;            /* sum of the heap free list       */
    ulong kheap;
    ulong kused;
    struct memblock *block;
    int i;

    phys = (ulong)platform.maxaddr - (ulong)platform.minaddr;
    resrv = (ulong)&_end - (ulong)platform.minaddr;
    code = (ulong)&_etext - (ulong)_start;

    /* Only stacks carved out of the internal data window belong in this
     * budget; the allocator prefers PSRAM, and those live in the external
     * section reported further down. */
    for (i = 0; i < NTHREAD; i++)
    {
        if (thrtab[i].state != THRFREE &&
            (ulong)thrtab[i].stkbase >= (ulong)platform.minaddr &&
            (ulong)thrtab[i].stkbase < (ulong)platform.maxaddr)
        {
            stack += thrtab[i].stklen;
        }
    }

    for (block = memlist.next; block != NULL; block = block->next)
    {
        kfree += block->length;
    }

    kheap = phys - resrv - stack;
    kused = kheap - kfree;

    printf("RAM usage and free memory\n");
    printf("===========================================================\n");
    printf("  Physical RAM      : %8lu B  (%lu KB)\n", phys, KB(phys));
    printf("-----------------------------------------------------------\n");
    printf("  System reserved   : %8lu B  %3lu%%\n",
           resrv, pct(resrv, phys));
    printf("  Kernel code (.text): %7lu B\n", code);
    printf("  Thread stacks     : %8lu B  %3lu%%\n",
           stack, pct(stack, phys));
    printf("  Kernel heap       : %8lu B  %3lu%%\n",
           kheap, pct(kheap, phys));
    printf("      heap in use   : %8lu B  %3lu%% of heap\n",
           kused, pct(kused, kheap));
    printf("      heap free     : %8lu B  %3lu%% of heap\n",
           kfree, pct(kfree, kheap));
    printf("===========================================================\n");

    *heap_total = kheap;
    *heap_used = kused;
    *heap_free = kfree;
}

static void printHeap(void)
{
    struct memblock *block;
    ulong nblock = 0;
    ulong largest = 0;
    ulong smallest = (ulong)-1;
    ulong total = 0;

    for (block = memlist.next; block != NULL; block = block->next)
    {
        nblock++;
        total += block->length;
        if (block->length > largest)
        {
            largest = block->length;
        }
        if (block->length < smallest)
        {
            smallest = block->length;
        }
    }

    if (0 == nblock)
    {
        smallest = 0;
    }

    printf("Heap free list: %lu block(s)\n", nblock);
    printf("  largest block  : %8lu B\n", largest);
    printf("  smallest block : %8lu B\n", smallest);
    printf("  total free     : %8lu B\n", total);
    printf("\n");
}

static void printStacks(void)
{
    ulong b;
    ulong sp;
    ulong top;
    ulong used;
    ulong total = 0;
    ulong usedtot = 0;
    int i;

    printf("Thread stacks\n");
    printf("%3s %-12s %8s %8s %8s %5s\n",
           "TID", "NAME", "USED", "FREE", "TOTAL", "USE%");
    printf("%3s %-12s %8s %8s %8s %5s\n",
           "---", "------------", "--------", "--------",
           "--------", "-----");

    for (i = 0; i < NTHREAD; i++)
    {
        if (thrtab[i].state == THRFREE)
        {
            continue;
        }

        b = (ulong)thrtab[i].stkbase;
        sp = (ulong)thrtab[i].stkptr;

        /* Threads created through create() have stkbase at the top of the
         * region and the stack grows down from it.  The idle thread
         * (prnull) instead has stkbase at the bottom and grows up. */
        if (sp > b)
        {
            top = b + (ulong)thrtab[i].stklen;
        }
        else
        {
            top = b;
        }

        used = (top > sp) ? (top - sp) : 0;
        if (used > thrtab[i].stklen)
        {
            used = thrtab[i].stklen;
        }

        printf("%3d %-12s %8lu %8lu %8lu %4lu%%\n",
               i, thrtab[i].name,
               used, (ulong)thrtab[i].stklen - used,
               (ulong)thrtab[i].stklen,
               pct(used, (ulong)thrtab[i].stklen));

        total += thrtab[i].stklen;
        usedtot += used;
    }

    printf("%3s %-12s %8lu %8lu %8lu %4lu%%\n",
           "", "TOTAL", usedtot, total - usedtot, total,
           pct(usedtot, total));
    printf("\n");
}

static void printPools(void)
{
    int id;
    int any = 0;

    printf("Network buffer pools\n");
    printf("%3s %8s %6s %6s %6s %10s\n",
           "ID", "BUFSIZE", "TOTAL", "USED", "FREE", "BYTES");

    for (id = 0; id < NPOOL; id++)
    {
        if (BFPFREE == bfptab[id].state)
        {
            continue;
        }

        any = 1;
        {
            int nbuf = (int)bfptab[id].nbuf;
            int free = 0;
            int used;

            if (!isbadsem(bfptab[id].freebuf))
            {
                free = semtab[bfptab[id].freebuf].count;
            }
            used = nbuf - free;

            printf("%3d %8lu %6d %6d %6d %10lu\n",
                   id, (ulong)bfptab[id].bufsize, nbuf, used, free,
                   (ulong)bfptab[id].bufsize * (ulong)nbuf);
        }
    }

    if (!any)
    {
        printf("  (no pools allocated)\n");
    }
    printf("\n");
}

static void printPsram(void)
{
    ulong total;
    ulong free;
    ulong used;

    if (!psram_present || 0 == psram_size)
    {
        return;
    }

    total = (ulong)psram_size;
    free = (ulong)psramFreeBytes();
    used = (ulong)psramUsedBytes();

    printf("External PSRAM (0x%08lX)\n", (ulong)PSRAM_BASE);
    printf("  size           : %8lu B  (%lu KB)\n", total, KB(total));
    printf("  in use         : %8lu B  %3lu%%\n", used, pct(used, total));
    printf("  free           : %8lu B  %3lu%%\n", free, pct(free, total));
    printf("  largest block  : %8lu B\n", (ulong)psramLargestFree());
    printf("\n");
}

static void usage(char *command)
{
    printf("Usage: %s [-v]\n\n", command);
    printf("Description:\n");
    printf("\tReports RAM in use and free: reserved system area,\n");
    printf("\tkernel code, thread stacks, kernel heap and network\n");
    printf("\tbuffer pools.\n");
    printf("Options:\n");
    printf("\t-v\tdump the raw heap free list\n");
    printf("\t--help\tdisplay this help and exit\n");
}

/**
 * @ingroup shell
 *
 * Shell command (free) reports detailed RAM usage.
 *
 * @param nargs number of arguments in args array
 * @param args  array of arguments
 * @return non-zero value on error
 */
shellcmd xsh_free(int nargs, char *args[])
{
    ulong heap_total;
    ulong heap_used;
    ulong heap_free;
    bool verbose = FALSE;
    int i;

    for (i = 1; i < nargs; i++)
    {
        if (0 == strcmp(args[i], "--help"))
        {
            usage(args[0]);
            return 0;
        }
        else if (0 == strcmp(args[i], "-v"))
        {
            verbose = TRUE;
        }
        else
        {
            fprintf(stderr, "%s: invalid argument '%s'\n", args[0], args[i]);
            fprintf(stderr, "Try '%s --help' for more information\n", args[0]);
            return 1;
        }
    }

    printRegions(&heap_total, &heap_used, &heap_free);
    printHeap();
    printStacks();
    printPools();
    printPsram();

    printf("Summary: heap %lu B total, %lu B used, %lu B free\n",
           heap_total, heap_used, heap_free);

    if (verbose)
    {
        struct memblock *block;

        printf("\nRaw heap free list\n");
        printf("%-10s %10s\n", "ADDR", "LENGTH");
        for (block = memlist.next; block != NULL; block = block->next)
        {
            printf("0x%08lX %10lu\n", (ulong)block, (ulong)block->length);
        }
    }

    return 0;
}
