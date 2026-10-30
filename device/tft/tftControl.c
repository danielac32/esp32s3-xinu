/**
 * @file tftControl.c
 *
 * ST7735 TFT display driver for the ESP32-S3: device control.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>

#include "st7735.h"
#include <tft.h>

/**
 * @ingroup tft
 *
 * Clear the display, change the text colours, or move the text cursor.
 *
 * @param devptr tft device table entry
 * @param func   control function to execute
 * @param arg1   depends on @p func
 * @param arg2   depends on @p func
 * @return OK on success, SYSERR for an unknown function
 */
devcall tftControl(device *devptr, int func, long arg1, long arg2)
{
    struct tft *tftptr;

    tftptr = &tfttab[devptr->minor];

    if (isbadmon(tftptr->lock))
    {
        return SYSERR;
    }

    switch (func)
    {
    case TFTCTL_CLEAR:
        lock(tftptr->lock);
        tftptr->cx = 0;
        tftptr->cy = 0;
        st7735_clear(tftptr->bg);
        st7735_flush();
        unlock(tftptr->lock);
        return OK;

    case TFTCTL_FLUSH:
        lock(tftptr->lock);
        st7735_flush();
        unlock(tftptr->lock);
        return OK;

    case TFTCTL_SETCOLOR:
        lock(tftptr->lock);
        tftptr->fg = (uint)arg1;
        tftptr->bg = (uint)arg2;
        unlock(tftptr->lock);
        return OK;
    case TFTCTL_CURSOR:
        lock(tftptr->lock);
        tftptr->cx = (uint)arg1;
        tftptr->cy = (uint)arg2;
        unlock(tftptr->lock);
        return OK;
    case TFTCTL_HOME:
        lock(tftptr->lock);
        tftptr->cx = 0;
        tftptr->cy = 0;
        unlock(tftptr->lock);
        return OK;

    default:
        return SYSERR;
    }
}
