/**
 * @file rgbOpen.c
 *
 * Open the RGB LED device.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <kernel.h>
#include <stddef.h>
#include <rgb.h>

devcall rgbOpen(device *devptr)
{
    struct rgb *p = &rgbtab[devptr->minor];

    if (!p->ready)
    {
        return SYSERR;
    }

    lock(p->lock);
    p->opened = TRUE;
    unlock(p->lock);

    return OK;
}
