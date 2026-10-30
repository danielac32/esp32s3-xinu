/**
 * @file tftWrite.c
 *
 * ST7735 TFT display driver for the ESP32-S3: write a string to the
 * display.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <stddef.h>

#include <tft.h>

/**
 * @ingroup tft
 *
 * Draw the bytes of @p buf at the cursor, advancing it as tftPutc() does.
 * The cursor is shared with tftPutc(), so a write continues whatever the
 * display was last showing.
 *
 * @param devptr tft device table entry
 * @param buf    bytes to draw
 * @param len    number of bytes
 * @return the number of bytes drawn, or SYSERR if @p buf is NULL
 */
devcall tftWrite(device *devptr, const void *buf, uint len)
{
    const char *s = (const char *)buf;
    uint i;

    if (NULL == buf)
    {
        return SYSERR;
    }

    for (i = 0; i < len; i++)
    {
        if (SYSERR == tftPutc(devptr, s[i]))
        {
            return SYSERR;
        }
    }

    return (devcall)len;
}
