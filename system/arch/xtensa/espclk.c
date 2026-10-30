/**
 * @file espclk.c
 *
 * Xinu system clock for the ESP32-S3, built on the Xtensa core timer.
 *
 * CCOUNT is a free-running 32-bit counter clocked at the CPU frequency
 * (20 MHz here; see ESP32S3_CPU_FREQ_HZ).  CCOMPARE1 is compared
 * against CCOUNT and raises a CPU interrupt when they are equal.  On this
 * core CCOMPARE1 is XCHAL interrupt 15, i.e. CPU interrupt line 15, which
 * the ESP32-S3 decodes as a level-3 interrupt.  Level-3 interrupts vector
 * through the level-3 vector (see vectors.S) and are dispatched to
 * clkhandler() by xt_int_dispatch().
 *
 * CCOMPARE0 (line 6, level 1) is deliberately left alone: its level-1
 * vector would need EPS1/EPC1, which this toolchain cannot name.
 *
 * clkcount() and clkupdate() are documented in clock.h.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <clock.h>
#include <platform.h>
#include <xtensa.h>

/**
 * @ingroup timer
 *
 * Return the current value of the free-running cycle counter.
 */
ulong clkcount(void)
{
    return (ulong)XT_CCOUNT();
}

/**
 * @ingroup timer
 *
 * Program the next timer interrupt to fire @p cycles cycles from now.
 *
 * The timer interrupt is level-triggered on the equality CCOUNT ==
 * CCOMPARE1, so writing a new (future) value both clears the pending
 * interrupt and schedules the next one.
 */
void clkupdate(ulong cycles)
{
    WSR(ccompare1, (uint32_t)(XT_CCOUNT() + (uint32_t)cycles));
}
