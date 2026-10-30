/**
 * @file intc.c
 *
 * ESP32-S3 interrupt controller support and the Xinu interrupt
 * dispatcher.
 *
 * The ESP32-S3 has a two-stage interrupt controller:
 *
 *   1) Every peripheral interrupt source (UART0 = 27, GPIO = 16, ...)
 *      has a 5-bit "map" register in INTERRUPT_CORE0.  Writing the CPU
 *      interrupt-line number (0..31) into it routes that source to one
 *      of the CPU's 32 interrupt inputs.
 *
 *   2) The Xtensa core assigns each of those 32 lines to an interrupt
 *      level.  Xinu routes every peripheral it uses to a level-3 line so
 *      that they share one vector: the timer is CCOMPARE1 (line 15),
 *      UART0 is line 22 and the GPIO block is line 23.  All three raise
 *      the level-3 interrupt, which vectors through the level-3 vector to
 *      xt_int_dispatch() below.
 *
 * Level-1 lines are left to the CPU exceptions (illegal instruction,
 * load/store error, syscall), which vector through the kernel exception
 * vector to xt_exception().
 *
 * interruptVector[] is indexed by the CPU interrupt-line number.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <platform.h>
#include <interrupt.h>
#include <stdint.h>
#include <xtensa.h>

interrupt_handler_t interruptVector[32];



/**
 * Route peripheral interrupt source @p src to CPU interrupt line @p line.
 */
void intc_route(uint32_t src, uint32_t line)
{
    REG32(INTR_MAP(src)) = line & 0x1FU;
}

/**
 * C entry point for the level-3 interrupt vector.
 *
 * @param pending value read from the INTERRUPT special register
 */
void xt_int_dispatch(uint32_t pending)
{
    uint32_t enabled, mask;
    int line;

    enabled = XT_INTENABLE();
    mask = pending & enabled;


    for (line = 0; line < 32; line++)
    {
        if (mask & (1u << line))
        {
            if (interruptVector[line] != NULL)
            {
                interruptVector[line]();
            }
            else
            {
                /* No handler: mask the line so we do not spin. */
                disable_irq((irqmask)line);
            }
        }
    }
}
