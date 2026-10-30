/**
 * @file setupStack.c
 *
 * Build the initial saved-context frame for a newly created thread.
 *
 * The frame layout must match ctxsw.S exactly:
 *
 *      [0]  a0    return address for the outgoing context
 *      [1]  a2    \
 *      [2]  a3     |
 *      [3]  a4     |
 *      [4]  a5     |  CALL0 argument registers
 *      [5]  a6     |
 *      [6]  a7    /
 *      [7]  a8
 *      [8]  a9
 *      [9]  a10
 *      [10] a11
 *      [11] a12   -> thread body
 *      [12] a13   -> value a0 must hold when the body is entered
 *      [13] a14
 *      [14] a15
 *      [15] PS
 *
 * ctxsw() restores this frame and then executes "ret", which jumps to
 * [0] (the _xt_thread_start stub).  The stub moves [12] into a0 and
 * jumps to [11] (the thread body).  The frame sits exactly 64 bytes
 * below the spilled-argument area, which is where the CALL0 ABI wants
 * arguments 7 and beyond.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <platform.h>
#include <stddef.h>
#include <stdarg.h>

#define CONTEXT_WORDS 16       /* a0, a2-a15, PS */
#define MAX_REG_ARGS   6       /* a2-a7 in the CALL0 ABI */

extern void _xt_thread_start(void);

void *setupStack(void *stackaddr, void *procaddr,
                 void *retaddr, uint nargs, va_list ap)
{
    uint nspilled = 0;
    ulong *saddr = (ulong *)stackaddr;
    ulong args[64];
    uint i, j;

    if (nargs > MAX_REG_ARGS)
    {
        nspilled = nargs - MAX_REG_ARGS;
    }

    /* Snapshot the variadic arguments first (va_arg ordering).  */
    if (nargs > 64)
    {
        nargs = 64;
    }
    for (i = 0; i < nargs; i++)
    {
        args[i] = va_arg(ap, ulong);
    }

    /* Reserve room for the spilled arguments, then the context record.  */
    saddr -= nspilled;
    saddr -= CONTEXT_WORDS;

    /* Keep the stack 16-byte aligned (Xtensa ABI requirement).  */
    while (((ulong)saddr) & 0xfUL)
    {
        saddr = (ulong *)(((ulong)saddr) - 1UL);
    }

    for (i = 0; i < CONTEXT_WORDS; i++)
    {
        saddr[i] = 0;
    }

    saddr[0] = (ulong)_xt_thread_start;   /* a0  */
    for (j = 0; j < MAX_REG_ARGS && j < nargs; j++)
    {
        saddr[1 + j] = args[j];           /* a2-a7 */
    }
    saddr[11] = (ulong)procaddr;          /* a12 */
    saddr[12] = (ulong)retaddr;           /* a13 */
    saddr[15] = 0;                        /* PS: level 0, kernel mode */

    /* Arguments 7.. land immediately above the context record.  */
    for (j = MAX_REG_ARGS; j < nargs; j++)
    {
        saddr[CONTEXT_WORDS + (j - MAX_REG_ARGS)] = args[j];
    }

    return saddr;
}
