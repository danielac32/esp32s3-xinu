/**
 * @file tftInit.c
 *
 * ST7735 TFT display driver for the ESP32-S3: device initialization.
 *
 * The panel's reset line (GPIO15) is shared with the W5500, and this
 * driver owns it.  It must be initialized before the W5500 so that the
 * hardware reset pulse it produces also resets that chip; the W5500
 * driver deliberately never drives GPIO15 itself.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <conf.h>
#include <device.h>
#include <kernel.h>
#include <monitor.h>
#include <stdlib.h>

#include <tft.h>
#include "st7735.h"

struct tft tfttab[NTFT];

/**
 * @ingroup tft
 *
 * Initialize the display: allocate its control block, create the monitor
 * that serializes access to it, and bring the panel up.
 *
 * @param devptr tft device table entry
 * @return OK if the panel is usable, otherwise SYSERR
 */
devcall tftInit(device *devptr)
{
    struct tft *tftptr;

    tftptr = &tfttab[devptr->minor];
    bzero(tftptr, sizeof(struct tft));

    tftptr->fg = TFT_WHITE;
    tftptr->bg = TFT_BLACK;

    tftptr->lock = moncreate();
    if (isbadmon(tftptr->lock))
    {
        return SYSERR;
    }

    tftHwInit();

     
    tftptr->opened = TRUE;
    tftptr->cx = 0;
    tftptr->cy = 0;
    st7735_clear(tftptr->bg);
    st7735_flush();
    return OK;
}
