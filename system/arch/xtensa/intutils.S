/**
 * @file intutils.S
 *
 * Functions to enable, disable, or restore global interrupts on the
 * ESP32-S3 (Xtensa CALL0 ABI).
 *
 * Xtensa keeps the interrupt mask in the PS (Processor Status) register:
 * bits 0..3 hold the interrupt level.  Level 0 means "all enabled",
 * level 15 means "all level-1..15 interrupts disabled".  We only use
 * level-1 interrupts, so 0 and 15 are the only two states that matter.
 *
 *   enable()              - drop to interrupt level 0
 *   disable()             - raise to level 15, return previous PS
 *   restore(ps)           - write a PS value back
 *   enable_irq(irqnum)    - set bit 'irqnum' of the INTENABLE register
 *   disable_irq(irqnum)   - clear bit 'irqnum' of the INTENABLE register
 *
 * IMPORTANT: under the CALL0 ABI a0 holds the return address, so a `ret`
 * (an alias for `jx a0`) returns to whatever a0 points at.  rsil writes
 * the *previous* PS into its destination register, so `rsil a0, 15`
 * followed by `ret` jumps straight to the previous PS value.  These
 * routines therefore use a2 (the CALL0 return-value and scratch
 * register) and never touch a0.
 */
/* Embedded Xinu, ported to ESP32-S3. */

    .text

/* ------------------------------------------------------------------ */
/*  void enable(void)                                                  */
/* ------------------------------------------------------------------ */
    .globl  enable
    .align  4
    .type   enable, @function
enable:
    rsil    a2, 0
    ret
    .size   enable, . - enable

/* ------------------------------------------------------------------ */
/*  irqmask disable(void)                                              */
/* ------------------------------------------------------------------ */
    .globl  disable
    .align  4
    .type   disable, @function
disable:
    rsil    a2, 15          /* a2 <- previous PS, interrupts now off */
    ret
    .size   disable, . - disable

/* ------------------------------------------------------------------ */
/*  irqmask restore(irqmask ps)                                        */
/* ------------------------------------------------------------------ */
    .globl  restore
    .align  4
    .type   restore, @function
restore:
    wsr.ps  a2
    rsync
    ret                     /* a2 still holds the restored value */
    .size   restore, . - restore

/* ------------------------------------------------------------------ */
/*  void enable_irq(irqmask irq)                                       */
/*                                                                     */
/*  'irq' is a CPU interrupt-line number (0..31), matching the XINU     */
/*  convention used by clkinit() and the device drivers.                */
/* ------------------------------------------------------------------ */
    .globl  enable_irq
    .align  4
    .type   enable_irq, @function
enable_irq:
    movi    a3, 1
    ssl     a2              /* SAR <- a2[4:0] */
    sll     a3, a3          /* a3 <- 1 << irq */
    rsr     a4, intenable
    or      a4, a4, a3
    wsr     a4, intenable
    rsync
    ret
    .size   enable_irq, . - enable_irq

/* ------------------------------------------------------------------ */
/*  void disable_irq(irqmask irq)                                      */
/* ------------------------------------------------------------------ */
    .globl  disable_irq
    .align  4
    .type   disable_irq, @function
disable_irq:
    movi    a3, 1
    ssl     a2
    sll     a3, a3          /* a3 <- 1 << irq */
    movi    a4, -1
    xor     a4, a4, a3      /* a4 <- ~(1 << irq) */
    rsr     a5, intenable
    and     a5, a5, a4
    wsr     a5, intenable
    rsync
    ret
    .size   disable_irq, . - disable_irq
