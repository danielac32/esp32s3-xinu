/**
 * @file sdOpen.c
 *
 * SDMMC (SDHOST) microSD driver for the ESP32-S3: open.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <stddef.h>

#include <sd.h>

#include "sdhost.h"

devcall sdOpen(device *devptr)
{
    (void)devptr;
    return sd_present ? OK : SYSERR;
}
