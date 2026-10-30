/**
 * @file sdGetc.c
 *
 * SDMMC (SDHOST) microSD driver for the ESP32-S3: get character.
 * The card is a block device, so there is nothing to read one byte at
 * a time.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <stddef.h>

#include <sd.h>

#include "sdhost.h"

devcall sdGetc(device *devptr)
{
    /* Block device: no character interface. */
    (void)devptr;
    return SYSERR;
}
