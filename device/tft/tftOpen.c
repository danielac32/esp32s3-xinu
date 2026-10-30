/**
 * @file tftOpen.c
 *
 * ST7735 TFT display driver for the ESP32-S3: open the display.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>

#include <tft.h>

/**
 * @ingroup tft
 *
 * Prepare the display for use.  A panel may only be open once.
 *
 * @param devptr tft device table entry
 * @return OK on success, SYSERR if the panel is unusable or already open
 */
devcall tftOpen(device *devptr)
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
    tftptr->opened = TRUE;
    unlock(tftptr->lock);

    return OK;
}
