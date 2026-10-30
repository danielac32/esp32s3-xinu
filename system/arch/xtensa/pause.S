/**
 * @file pause.S
 *
 * Pause (yield) the CPU briefly.
 */
/* Embedded Xinu, ported to ESP32-S3. */

    .text
    .globl pause
    .align 4
pause:
    nop
    ret