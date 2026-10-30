/**
 * @file rgbInit.c
 *
 * Initialize the discrete RGB LED: create its monitor and configure the
 * three pads as plain GPIO outputs, initially off.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <kernel.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <xtensa.h>

#include <board.h>
#include <rgb.h>

devcall rgbInit(device *devptr)
{
    struct rgb *p = &rgbtab[devptr->minor];

    bzero(p, sizeof(struct rgb));

    p->lock = moncreate();
    if (SYSERR == p->lock)
    {
        return SYSERR;
    }

    xt_gpio_output(BOARD_RGB_R_PIN, GPIO_PULL_NONE);
    xt_gpio_output(BOARD_RGB_G_PIN, GPIO_PULL_NONE);
    xt_gpio_output(BOARD_RGB_B_PIN, GPIO_PULL_NONE);

    p->active_high = (BOARD_RGB_ACTIVE_HIGH != 0);
    xt_gpio_set_level(BOARD_RGB_R_PIN, p->active_high ? 0 : 1);
    xt_gpio_set_level(BOARD_RGB_G_PIN, p->active_high ? 0 : 1);
    xt_gpio_set_level(BOARD_RGB_B_PIN, p->active_high ? 0 : 1);

    p->ready = TRUE;
    p->opened = FALSE;
    p->lit = FALSE;
    p->r = 0;
    p->g = 0;
    p->b = 0;

    return OK;
}
