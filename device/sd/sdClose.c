/**
 * @file sdClose.c
 *
 * SDMMC (SDHOST) microSD driver for the ESP32-S3: close.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <stddef.h>

#include <sd.h>

#include "sdhost.h"

devcall sdClose(device *devptr)
{
    (void)devptr;
    return OK;
}
