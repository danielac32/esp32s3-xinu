/**
 * @file interrupt.h
 *
 * Constants and declarations associated with interrupt and exception
 * handling on the ESP32-S3 (Xtensa LX7, CALL0 ABI).
 */
/* Embedded Xinu, ported to ESP32-S3.  All rights reserved. */

#ifndef _XTENSA_INTERRUPT_H_
#define _XTENSA_INTERRUPT_H_

#include <stddef.h>
#include <stdint.h>

typedef interrupt (*interrupt_handler_t)(void);

/**
 * Handler table indexed by Xtensa CPU interrupt line (0..31).  A line with
 * no handler is masked off automatically so that a stray source cannot wedge
 * the CPU in an interrupt loop.
 */
extern interrupt_handler_t interruptVector[];

typedef unsigned long irqmask;  /**< machine status for disable/restore  */

void enable(void);
irqmask disable(void);
irqmask restore(irqmask);
void enable_irq(irqmask);
void disable_irq(irqmask);

/** The vector table (placed in I-SRAM by the linker). */
extern void _xt_vector_table(void);

/**
 * C entry point shared by the level-3 interrupt vector.
 * @param pending value read from the Xtensa INTERRUPT special register
 */
void xt_int_dispatch(uint32_t pending);

/** Route peripheral interrupt source @p src to CPU interrupt line @p line. */
void intc_route(uint32_t src, uint32_t line);

/* ---------------- exceptions and software traps ---------------- */

typedef void (*exception_handler_t)(uint32_t exccause, uint32_t *frame);
typedef syscall (*syscall_handler_t)(long, long, long);

void exception_register(uint32_t cause, exception_handler_t fn);
void syscall_register(uint32_t num, syscall_handler_t fn);
syscall syscall_invoke(uint32_t num, long a, long b, long c);

/**
 * C entry point for the level-1 exception vector.
 * @param exccause value read from EXCCAUSE
 * @param frame    saved register frame on the faulting thread's stack
 */
void xt_exception(uint32_t exccause, uint32_t *frame);

/** Halt the machine with a message. */
void _panic(const char *msg);

/**
 * Write one byte straight to the UART0 TX FIFO, bypassing the console
 * device.  Used by the fault reporters, which may run before sysinit() has
 * brought the console up (or after it has gone wrong).
 */
void xt_dbg_putc(char c);

#endif /* _XTENSA_INTERRUPT_H_ */
