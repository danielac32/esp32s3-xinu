/**
 * @file halt.S
 *
 * Halt the CPU.  On the ESP32-S3 we spin in a WFI-equivalent loop.
 */
/* Embedded Xinu, ported to ESP32-S3. */

    .text
    .globl halt
    .align 4
halt:
    /* Disable interrupts and wait forever. */
    rsil    a0, 15
1:
    waiti   15
    j       1b