/**
 * @file rgbClose.c
 *
 * Close the RGB LED device.  The LED keeps its current colour; closing is
 * only bookkeeping so a later demo does not unexpectedly turn the board
 * dark.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <kernel.h>
#include <stddef.h>
#include <rgb.h>

devcall rgbClose(device *devptr)
{
    struct rgb *p = &rgbtab[devptr->minor];

    if (!p->ready)
    {
        return SYSERR;
    }

    lock(p->lock);
    p->opened = FALSE;
    unlock(p->lock);

    return OK;
}
