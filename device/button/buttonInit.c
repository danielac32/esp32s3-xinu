/**
 * @file buttonInit.c
 *
 * Push button (GPIO) driver for the ESP32-S3: device initialization.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>
#include <stdlib.h>
#include <xtensa.h>

#include <button.h>

struct button buttontab[NBUTTON];

/**
 * @ingroup button
 *
 * Initialize a button device: allocate its control block, create the
 * monitor that serializes access to it and configure the pin as an input
 * with the module's pull-up.
 *
 * @param devptr button device table entry
 * @return OK if the device is usable, otherwise SYSERR
 */
devcall buttonInit(device *devptr)
{
    struct button *btnptr;

    btnptr = &buttontab[devptr->minor];
    bzero(btnptr, sizeof(struct button));

    btnptr->lock = moncreate();
    if (isbadmon(btnptr->lock))
    {
        return SYSERR;
    }

    /* The module already has an external pull-up on this pin; ask for the
     * internal one as well so the level is defined even if a bare chip is
     * used. */
    xt_gpio_input(BUTTON_GPIO, GPIO_PULL_UP);

    /* Clear any edge the boot ROM may have latched on this pin. */
    REG32(GPIO_STATUS_W1TC) = (1UL << BUTTON_GPIO);

    btnptr->ready = TRUE;
    btnptr->opened = FALSE;
    btnptr->presses = 0;

    return OK;
}
