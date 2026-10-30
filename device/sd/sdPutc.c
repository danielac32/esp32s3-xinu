/**
 * @file sdPutc.c
 *
 * SDMMC (SDHOST) microSD driver for the ESP32-S3: put character.
 * The card is a block device, so there is nothing to write one byte at
 * a time.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <stddef.h>

#include <sd.h>

#include "sdhost.h"

devcall sdPutc(device *devptr, char c)
{
    (void)devptr;
    (void)c;
    return SYSERR;
}
