/**
 * @file rgbSet.c
 *
 * Helper functions for the discrete RGB LED.
 *
 * A plain RGB LED has no brightness control: each GPIO is either on or off.
 * The 0..255 arguments are kept in the API so callers can pass a colour in
 * the same range as the TFT and LVGL code, but a channel is simply on when
 * its value is non-zero.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>
#include <xtensa.h>

#include <board.h>
#include <rgb.h>

struct rgb rgbtab[NRGB];

static void __attribute__((section(".slowtext"), noinline)) rgb_apply(struct rgb *p, uint8_t r, uint8_t g, uint8_t b)
{
    uint on = p->active_high ? 1U : 0U;
    uint off = p->active_high ? 0U : 1U;

    xt_gpio_set_level(BOARD_RGB_R_PIN, (r != 0) ? on : off);
    xt_gpio_set_level(BOARD_RGB_G_PIN, (g != 0) ? on : off);
    xt_gpio_set_level(BOARD_RGB_B_PIN, (b != 0) ? on : off);

    p->r = r;
    p->g = g;
    p->b = b;
    p->lit = (r != 0 || g != 0 || b != 0);
}

/**
 * Set the three channels.  A zero value turns that channel off; any other
 * value turns it on.
 */
int __attribute__((section(".slowtext"), noinline)) rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    struct rgb *p = &rgbtab[0];

    if (!p->ready)
    {
        return SYSERR;
    }

    lock(p->lock);
    rgb_apply(p, r, g, b);
    unlock(p->lock);

    return OK;
}

/** Turn all channels off, remembering the previous colour. */
int __attribute__((section(".slowtext"), noinline)) rgb_off(void)
{
    struct rgb *p = &rgbtab[0];

    if (!p->ready)
    {
        return SYSERR;
    }

    lock(p->lock);
    {
        uint off = p->active_high ? 0U : 1U;
        xt_gpio_set_level(BOARD_RGB_R_PIN, off);
        xt_gpio_set_level(BOARD_RGB_G_PIN, off);
        xt_gpio_set_level(BOARD_RGB_B_PIN, off);
    }
    p->lit = FALSE;
    unlock(p->lock);

    return OK;
}

/** Restore the last colour set with rgb_set(). */
int __attribute__((section(".slowtext"), noinline)) rgb_on(void)
{
    struct rgb *p = &rgbtab[0];

    if (!p->ready)
    {
        return SYSERR;
    }

    lock(p->lock);
    rgb_apply(p, p->r, p->g, p->b);
    p->lit = (p->r != 0 || p->g != 0 || p->b != 0);
    unlock(p->lock);

    return OK;
}

/** Return the last colour as 0x00RRGGBB. */
uint32_t __attribute__((section(".slowtext"), noinline)) rgb_get(void)
{
    struct rgb *p = &rgbtab[0];
    uint32_t value;

    if (!p->ready)
    {
        return 0;
    }

    lock(p->lock);
    value = p->lit
        ? (((uint32_t)p->r << 16) | ((uint32_t)p->g << 8) | p->b)
        : 0;
    unlock(p->lock);

    return value;
}
