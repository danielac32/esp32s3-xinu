/**
 * @file panic.c
 *
 * Panic handler for the ESP32-S3 port.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <kernel.h>
#include <platform.h>

void _panic(const char *msg)
{
    kprintf("\n*** PANIC: %s ***\n", msg);
    for (;;)
        ;
}