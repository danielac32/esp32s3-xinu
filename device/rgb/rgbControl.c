/**
 * @file rgbControl.c
 *
 * control(RGB0, ...) commands for the discrete RGB LED.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <kernel.h>
#include <stddef.h>
#include <stdint.h>
#include <rgb.h>

devcall __attribute__((section(".slowtext"), noinline)) rgbControl(device *devptr, int func, long arg1, long arg2)
{
    struct rgb *p = &rgbtab[devptr->minor];
    uint32_t colour;

    (void)arg2;

    if (!p->ready)
    {
        return SYSERR;
    }

    switch (func)
    {
    case RGB_CTRL_SET:
        colour = (uint32_t)arg1;
        return rgb_set((uint8_t)((colour >> 16) & 0xFF),
                       (uint8_t)((colour >> 8) & 0xFF),
                       (uint8_t)(colour & 0xFF));

    case RGB_CTRL_OFF:
        return rgb_off();

    case RGB_CTRL_ON:
        return rgb_on();

    case RGB_CTRL_GET:
        return (devcall)rgb_get();

    case RGB_CTRL_POLARITY:
        p->active_high = (arg1 != 0);
        if (p->lit)
        {
            return rgb_set(p->r, p->g, p->b);
        }
        return rgb_off();

    default:
        return SYSERR;
    }
}
