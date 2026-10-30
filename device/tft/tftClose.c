/**
 * @file tftClose.c
 *
 * ST7735 TFT display driver for the ESP32-S3: close the display.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>

#include <tft.h>

/**
 * @ingroup tft
 *
 * Release the display.  The panel keeps showing what was last drawn to
 * it; there is nothing to power down.
 *
 * @param devptr tft device table entry
 * @return OK on success, SYSERR if the device was not open
 */
devcall tftClose(device *devptr)
{
    struct tft *tftptr;

    tftptr = &tfttab[devptr->minor];

    if (isbadmon(tftptr->lock))
    {
        return SYSERR;
    }

    lock(tftptr->lock);
    if (!tftptr->opened)
    {
        unlock(tftptr->lock);
        return SYSERR;
    }
    tftptr->opened = FALSE;
    unlock(tftptr->lock);

    return OK;
}
