/**
 * @file rgbWrite.c
 *
 * write(RGB0, buf, 3): the first three bytes are the red, green and blue
 * channel values.  This is a convenience for code that already has a
 * three-byte colour record; the normal interface is control().
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <kernel.h>
#include <stddef.h>
#include <stdint.h>
#include <rgb.h>

devcall rgbWrite(device *devptr, const void *buf, uint len)
{
    const uint8_t *rgb = (const uint8_t *)buf;

    (void)devptr;

    if (NULL == rgb || len < 3)
    {
        return SYSERR;
    }

    return rgb_set(rgb[0], rgb[1], rgb[2]);
}
