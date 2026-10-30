/**
 * @file vectors.S
 *
 * Xtensa exception / interrupt vector table for the ESP32-S3.
 *
 * VECBASE is programmed (in start.S) to point at _xt_vector_table.  The
 * ESP32-S3's XEA2 core uses the following offsets from VECBASE:
 *
 *   0x000  WindowOverflow4        (unused: we build with -mabi=call0)
 *   0x040  WindowUnderflow4       (unused)
 *   0x080  WindowOverflow8        (unused)
 *   0x0C0  WindowUnderflow8       (unused)
 *   0x100  WindowOverflow12       (unused)
 *   0x140  WindowUnderflow12      (unused)
 *   0x180  Level-2 interrupt
 *   0x1C0  Level-3 interrupt      <-- all of our interrupts land here
 *   0x200  Level-4 interrupt
 *   0x240  Level-5 interrupt
 *   0x280  Level-6 interrupt (debug)
 *   0x2C0  Level-7 interrupt (NMI)
 *   0x300  Level-1 kernel vector  <-- CPU exceptions (and SYSCALL traps)
 *   0x340  Level-1 user vector
 *   0x3C0  Double exception
 *
 * Why level 3 for the interrupts?
 *
 *   Every interrupt source is routed to a CPU interrupt line through the
 *   interrupt matrix, and we are free to choose lines that the core
 *   decodes as level 3 (INT11, INT15, INT22, INT23, INT27, INT29).  Level 3
 *   is also the level of CCOMPARE1, which is the core timer we use for the
 *   Xinu tick.  Using a single level for every interrupt means a single
 *   entry point and a single return instruction (rfi 3).
 *
 *   This also dodges a toolchain quirk: this binutils release knows the
 *   names EPC3/EPS3 but not EPS1, so a level-1 interrupt handler could not
 *   save and restore its return state by name.
 *
 * The level-1 vector is used for CPU exceptions (illegal instruction,
 * syscall, load/store faults...).  Taking an exception sets PS.EXCM, and
 * EXCM_LEVEL on this core is 3, so interrupts at levels 1-3 are masked while
 * an exception is being handled.  Nothing can therefore disturb EPS1/EPC1
 * before the handler returns with "rfe".
 */
/* Embedded Xinu, ported to ESP32-S3. */

    .section .vector, "ax"

/* TEMP bring-up: every unexpected vector prints its tag and halts, so a
 * stray exception is visible instead of an invisible "j ." spin. */
    .macro VSTUB ch
    movi    a2, 6
    slli    a2, a2, 28
    movi    a3, \ch
    s32i    a3, a2, 0
9999:
    j       9999b
    .endm

    .globl  _xt_vector_table
    .align  1024
_xt_vector_table:

    /* ---- 0x000-0x140 : window vectors (unused with the CALL0 ABI) ---- */
    .org    _xt_vector_table + 0x000
    VSTUB   'w'                     /* WindowOverflow4   */

    .org    _xt_vector_table + 0x040
    VSTUB   'u'                     /* WindowUnderflow4  */

    .org    _xt_vector_table + 0x080
    VSTUB   'W'                     /* WindowOverflow8   */

    .org    _xt_vector_table + 0x0C0
    VSTUB   'U'                     /* WindowUnderflow8  */

    .org    _xt_vector_table + 0x100
    VSTUB   'v'                     /* WindowOverflow12  */

    .org    _xt_vector_table + 0x140
    VSTUB   'V'                     /* WindowUnderflow12 */

    /* ---- 0x180 : level-2 (nothing is routed here) ---- */
    .org    _xt_vector_table + 0x180
    VSTUB   '2'                     /* Level2Interrupt   */

    /* ---- 0x1C0 : level-3 (all interrupts) ---- */
    .org    _xt_vector_table + 0x1C0
    j       _xt_level3

    /* ---- 0x200-0x240 : level-4 / level-5 (unused) ---- */
    .org    _xt_vector_table + 0x200
    VSTUB   '4'                     /* Level4Interrupt   */

    .org    _xt_vector_table + 0x240
    VSTUB   '5'                     /* Level5Interrupt   */

    /* ---- 0x280 : level-6 (debug) ---- */
    .org    _xt_vector_table + 0x280
    VSTUB   '6'                     /* DebugException    */

    /* ---- 0x2C0 : level-7 (NMI) ---- */
    .org    _xt_vector_table + 0x2C0
    VSTUB   'N'                     /* NMIException      */

    /* ---- 0x300 / 0x340 : level-1 (exceptions, SYSCALL) ---- */
    .org    _xt_vector_table + 0x300
    j       _xt_level1              /* kernel mode       */

    .org    _xt_vector_table + 0x340
    j       _xt_level1              /* user mode         */

    /* ---- 0x3C0 : double exception ---- */
    .org    _xt_vector_table + 0x3C0
    j       _xt_double              /* DoubleException   */

    .org    _xt_vector_table + 0x400   /* pad the table to 1 KiB */


/* ================================================================== */
/*  Double-exception entry point (bring-up diagnostic)                 */
/* ================================================================== */
/*
 * Reached only when a second fault occurs while PS.EXCM is already set.
 * The running stack cannot be trusted here, so this handler uses no stack
 * at all: it drops a byte into the UART0 TX FIFO and spins.  A run that
 * prints 'X' therefore died inside an exception handler or inside the
 * printing path itself.
 */
    .section .text, "ax"
    .align  4
    .type   _xt_double, @function
_xt_double:
    movi    a2, 6
    slli    a2, a2, 28              /* a2 = UART0 FIFO (0x60000000) */
    movi    a3, 'X'
    s32i    a3, a2, 0

    /* exccause, excvaddr, depc, epc1, ps, sp */
    rsr     a3, exccause
    call0   _xt_dump_hex
    rsr     a3, excvaddr
    call0   _xt_dump_hex
    rsr     a3, depc
    call0   _xt_dump_hex
    rsr     a3, epc1
    call0   _xt_dump_hex
    rsr     a3, ps
    call0   _xt_dump_hex
    mov     a3, a1
    call0   _xt_dump_hex
    movi    a3, 10
    s32i    a3, a2, 0
.Ldouble_spin:
    j       .Ldouble_spin
    .size   _xt_double, . - _xt_double

/* Print a3 as eight hex digits followed by a space.  a2 holds the FIFO
 * address; a4/a5 are scratch.  Uses no stack, so it is safe to call from
 * the double-exception handler, where sp cannot be trusted. */
    .align  4
    .type   _xt_dump_hex, @function
_xt_dump_hex:
    movi    a5, 8
.Lhex_loop:
    extui   a4, a3, 28, 4
    slli    a3, a3, 4
    bgeui   a4, 10, .Lhex_alpha
    addi    a4, a4, '0'
    j       .Lhex_store
.Lhex_alpha:
    addi    a4, a4, 'a' - 10
.Lhex_store:
    s32i    a4, a2, 0
    addi    a5, a5, -1
    bnez    a5, .Lhex_loop
    movi    a4, ' '
    s32i    a4, a2, 0
    ret
    .size   _xt_dump_hex, . - _xt_dump_hex


/* ================================================================== */
/*  Level-3 interrupt entry point                                      */
/* ================================================================== */
/*
 * On entry:
 *   EPC3     = interrupted PC
 *   EPS3     = interrupted PS
 *   PS.EXCM  = 0, PS.INTLEVEL = 3   (so levels 1-3 are masked)
 *
 * The saved frame is 80 bytes so that sp stays 16-byte aligned and so that
 * the C dispatcher can switch stacks and return into a different thread:
 * EPS3/EPC3 travel with the thread inside its frame.
 */
    .section .text, "ax"
    .align  4
    .type   _xt_level3, @function
_xt_level3:
    addi    sp, sp, -80

    s32i    a0,  sp, 0
    s32i    a2,  sp, 4
    s32i    a3,  sp, 8
    s32i    a4,  sp, 12
    s32i    a5,  sp, 16
    s32i    a6,  sp, 20
    s32i    a7,  sp, 24
    s32i    a8,  sp, 28
    s32i    a9,  sp, 32
    s32i    a10, sp, 36
    s32i    a11, sp, 40
    s32i    a12, sp, 44
    s32i    a13, sp, 48
    s32i    a14, sp, 52
    s32i    a15, sp, 56

    rsr     a0, eps3
    s32i    a0, sp, 60
    rsr     a0, epc3
    s32i    a0, sp, 64

    /* xt_int_dispatch(pending) */
    rsr     a2, interrupt
    call0   xt_int_dispatch

    /* Put the return state back, then reload the register file. */
    l32i    a0, sp, 60
    wsr     a0, eps3
    rsync
    l32i    a0, sp, 64
    wsr     a0, epc3
    rsync

    l32i    a0,  sp, 0
    l32i    a2,  sp, 4
    l32i    a3,  sp, 8
    l32i    a4,  sp, 12
    l32i    a5,  sp, 16
    l32i    a6,  sp, 20
    l32i    a7,  sp, 24
    l32i    a8,  sp, 28
    l32i    a9,  sp, 32
    l32i    a10, sp, 36
    l32i    a11, sp, 40
    l32i    a12, sp, 44
    l32i    a13, sp, 48
    l32i    a14, sp, 52
    l32i    a15, sp, 56

    addi    sp, sp, 80
    rfi     3
    .size   _xt_level3, . - _xt_level3


/* ================================================================== */
/*  Level-1 exception entry point                                      */
/* ================================================================== */
/*
 * Reached for CPU exceptions (EXCCAUSE != 0), including the SYSCALL
 * instruction.  PS.EXCM is set and EXCM_LEVEL == 3, so interrupts cannot
 * interleave and EPS1/EPC1 are safe until we execute "rfe".
 *
 * xt_exception() either halts the machine or, for a syscall, adjusts EPC1
 * so that "rfe" resumes at the instruction after the trap.
 */
    .align  4
    .type   _xt_level1, @function
_xt_level1:
    addi    sp, sp, -80

    s32i    a0,  sp, 0
    s32i    a2,  sp, 4
    s32i    a3,  sp, 8
    s32i    a4,  sp, 12
    s32i    a5,  sp, 16
    s32i    a6,  sp, 20
    s32i    a7,  sp, 24
    s32i    a8,  sp, 28
    s32i    a9,  sp, 32
    s32i    a10, sp, 36
    s32i    a11, sp, 40
    s32i    a12, sp, 44
    s32i    a13, sp, 48
    s32i    a14, sp, 52
    s32i    a15, sp, 56

    movi    a0, 6
    slli    a0, a0, 28              /* TEMP bring-up marker: level-1 */
    movi    a3, 'L'
    s32i    a3, a0, 0

    /* TEMP: dump the faulting context: cause, epc1, excvaddr, ps,
     * windowbase, windowstart, sp, saved a0/a3/a4/a5, and the words
     * around epc1. */
    movi    a2, 6
    slli    a2, a2, 28              /* a2 = UART0 FIFO */
    rsr     a3, exccause
    call0   _xt_dump_hex
    rsr     a3, epc1
    call0   _xt_dump_hex
    rsr     a3, excvaddr
    call0   _xt_dump_hex
    rsr     a3, ps
    call0   _xt_dump_hex
    rsr     a3, windowbase
    call0   _xt_dump_hex
    rsr     a3, windowstart
    call0   _xt_dump_hex
    mov     a3, sp
    call0   _xt_dump_hex
    l32i    a3, sp, 0
    call0   _xt_dump_hex
    l32i    a3, sp, 8
    call0   _xt_dump_hex
    l32i    a3, sp, 12
    call0   _xt_dump_hex
    l32i    a3, sp, 16
    call0   _xt_dump_hex
    movi    a3, 10
    s32i    a3, a2, 0

    rsr     a2, exccause
    mov     a3, sp
    call0   xt_exception

    l32i    a0,  sp, 0
    l32i    a2,  sp, 4
    l32i    a3,  sp, 8
    l32i    a4,  sp, 12
    l32i    a5,  sp, 16
    l32i    a6,  sp, 20
    l32i    a7,  sp, 24
    l32i    a8,  sp, 28
    l32i    a9,  sp, 32
    l32i    a10, sp, 36
    l32i    a11, sp, 40
    l32i    a12, sp, 44
    l32i    a13, sp, 48
    l32i    a14, sp, 52
    l32i    a15, sp, 56

    addi    sp, sp, 80
    rfe
    .size   _xt_level1, . - _xt_level1
