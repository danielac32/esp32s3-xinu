/**
 * @file     xsh_memstat.c
 *
 */
/* Embedded Xinu, Copyright (C) 2009.  All rights reserved. */

#include <stddef.h>
#include <platform.h>
#include <xtensa.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <thread.h>
#include <stdlib.h>

#define PRINT_DEFAULT 0x01
#define PRINT_KERNEL  0x02
#define PRINT_THREAD  0x08

#if  __LP64__
#define HEXWIDTH 16
#else
#define HEXWIDTH 8
#endif

extern char *maxaddr;
extern void _start(void);

static void printMemUsage(void);
static void printFreeList(struct memblock *, char *);

static void usage(char *command)
{
    printf("Usage: %s [-k] [-q] [-t <TID>]\n\n", command);
    printf("Description:\n");
    printf("\tDisplays the current memory usage and prints the\n");
    printf("\tfree list.\n");
    printf("Options:\n");
    printf("\t-k\t\tprint kernel free list\n");
    printf("\t-q\t\tsuppress current system memory usage screen\n");
    printf("\t-t <TID>\tprint user free list of thread id tid\n");
    printf("\t--help\t\tdisplay this help and exit\n");
}



/**
 * @ingroup shell
 *
 * Shell command (memstat) provides memory use and free list information.
 * @param nargs number of arguments in args array
 * @param args   array of arguments
 * @return non-zero value on error
 */
shellcmd xsh_memstat(int nargs, char *args[])
{
    int i;
    tid_typ tid;                /* thread to dump memlist of      */
    char print;                 /* print region free/alloc lists  */

    print = PRINT_DEFAULT;

    /* Output help, if '--help' argument was supplied */
    if (nargs == 2 && strcmp(args[1], "--help") == 0)
    {
        usage(args[0]);
        return 0;
    }

    tid = BADTID;
    for (i = 1; i < nargs; i++)
    {
        if (0 == strcmp(args[i], "-t"))
        {
            if (i + 1 < nargs)
            {
                /* Get thread id to display freelist of */
                print |= PRINT_THREAD;
                tid = atoi(args[i + 1]);
            }
            else
            {
                usage(args[0]);
                return 1;
            }
        }
        else if (0 == strcmp(args[i], "-k"))
        {
            print |= PRINT_KERNEL;
        }
        else if (0 == strcmp(args[i], "-q"))
        {
            print &= ~(PRINT_DEFAULT);
        }
    }

    if (print & PRINT_DEFAULT)
    {
        printMemUsage();
    }

    if (print & PRINT_KERNEL)
    {
        printFreeList(&memlist, "kernel");
    }

    if (print & PRINT_THREAD)
    {
        if (isbadtid(tid))
        {
            fprintf(stderr, "Invalid thread id: %d\n", tid);
        }
        else
        {
            printFreeList(&(thrtab[tid].memlist), thrtab[tid].name);
        }
    }

    return 0;
}

static void printMemUsage(void)
{
    int i;
    uintptr_t phys = 0;         /* total physical memory          */
    uintptr_t resrv = 0;        /* total reserved system memory   */
    uintptr_t code = 0;         /* total Xinu code memory         */
    uintptr_t stack = 0;        /* total stack memory             */
    uintptr_t kheap = 0;        /* total kernel heap memory       */
    uintptr_t kused = 0;        /* total used kernel heap memory  */
    uintptr_t kfree = 0;        /* total free memory              */
    struct memblock *block;     /* memory block pointer           */


    /* The ESP32-S3 exposes a single bank of physical SRAM through two
     * windows: instructions are fetched through the instruction window and
     * data is accessed through the data window, 0x6F0000 apart.  The two
     * must never be mixed, so platform.minaddr/maxaddr describe the data
     * window and "physical memory" below is that window's size.  The
     * kernel's text is measured apart from it. */
    phys = (ulong)platform.maxaddr - (ulong)platform.minaddr;

    /* Amount of the data window claimed before the heap is carved out. */
    resrv = (ulong)&_end - (ulong)platform.minaddr;

    /* Calculate amount of text memory */
    code = (ulong)&_etext - (ulong)_start;

    /* Calculate amount of stack memory.  Only stacks carved out of the
     * internal data window belong in this budget; the allocator prefers
     * PSRAM, and those must not be subtracted from the internal heap. */
    for (i = 0; i < NTHREAD; i++)
    {
        if (thrtab[i].state != THRFREE &&
            (ulong)thrtab[i].stkbase >= (ulong)platform.minaddr &&
            (ulong)thrtab[i].stkbase < (ulong)platform.maxaddr)
        {
            stack += (ulong)thrtab[i].stklen;
        }
    }

    /* Calculate amount of free kernel memory */
    for (block = memlist.next; block != NULL; block = block->next)
    {
        kfree += block->length;
    }

    /* Caculate amount of kernel heap memory */
    kheap = phys - resrv - stack;
    kused = kheap - kfree;

    /* Ouput current memory usage */
    printf("Current System Memory Usage:\n");
    printf("-----------------------------------------------------\n");
    printf("%20ld bytes system area\n", (ulong)resrv);
    printf("%20ld bytes Xinu code\n",  (ulong)code);
    printf("%20ld bytes stack space\n",  (ulong)stack);
    printf("%20ld bytes kernel heap space (%ld used)\n",  (ulong)kheap,  (ulong)kused);
    printf("-----------------------------------------------------\n");
    printf("%20ld bytes physical memory\n\n", (ulong)phys);
}

/**
 * Dump the current free list of a specific thread.
 * @param tid Id of thread to dump free list.
 */
static void printFreeList(struct memblock *base, char *ident)
{
    struct memblock *block;

    /* Output free list */
    printf("Free List (%s):\n", ident);
    printf("BLOCK START         LENGTH  \n");
    printf("------------------  --------------------\n");
    for (block = base->next; block != NULL; block = block->next)
    {
        printf("0x%0*lX  %20lu\n", HEXWIDTH, (ulong)block, (ulong)block->length);
    }
    printf("\n");
}
