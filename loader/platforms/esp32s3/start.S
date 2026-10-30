/**
 * @file start.S
 *
 * Startup code for Embedded Xinu on the ESP32-S3.
 *
 * This closely follows the boot sequence used by the proven bare-metal
 * ESP32-S3 startup (Chalandi): mask/clear interrupts, stop the core
 * timers, point VECBASE at our own vector table, then disable every
 * watchdog *immediately* -- the boot ROM hands control over with the
 * Timer Group 0 watchdog armed, and it will reset the chip if the rest of
 * the bring-up takes too long.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#define PANICSTK 8192
#define NULLSTK  8192

    .section .init, "ax"
    .globl  _start
    .align  4
    .type   _start, @function
_start:
    /* Mask every interrupt, clear the pending ones and stop the core
     * timers (CCOMPARE0..2).  This mirrors the ROM's expectation. */
    movi    a2, 0
    wsr     a2, interrupt
    wsr     a2, intenable
    movi    a2, -1
    wsr     a2, intclear
    wsr     a2, ccompare0
    wsr     a2, ccompare1
    wsr     a2, ccompare2

    /* Point VECBASE at our vector table. */
    movi    a2, _xt_vector_table
    wsr     a2, vecbase
    rsync

    /* Establish a temporary stack (grows down from _bss_end) so that the
     * C watchdog helper below can run.  Real thread stacks are set up
     * further down, once .bss has been cleared. */
    movi    sp, _bss_end
    movi    a2, PANICSTK
    add     sp, sp, a2

    /* Kill the watchdogs before doing anything slow. */
    call0   xt_disable_watchdogs

    /* Clear the .bss section. */
    movi    a2, _bss_start
    movi    a3, _bss_end
    movi    a4, 0
.Lbss_loop:
    bgeu    a2, a3, .Lbss_done
    s32i    a4, a2, 0
    addi    a2, a2, 4
    j       .Lbss_loop
.Lbss_done:
    /* Lay out the kernel stacks. */
    movi    a0, _end
    movi    a2, PANICSTK
    add     a3, a0, a2
    movi    a4, panicstk
    s32i    a3, a4, 0

    movi    a2, NULLSTK
    add     a3, a3, a2
    movi    a4, memheap
    s32i    a3, a4, 0

    mov     sp, a3

    movi    a0, nulluser
    jx      a0

.Lspin:
    j       .Lspin
    .size   _start, . - _start
