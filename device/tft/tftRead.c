/**
 * @file tftRead.c
 *
 * ST7735 TFT display driver for the ESP32-S3: reads are not supported.
 *
 * The panel is write only: it has no data output line on this board.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>

#include <tft.h>

/**
 * @ingroup tft
 *
 * @param devptr tft device table entry
 * @param buf    unused
 * @param len    unused
 * @return SYSERR, always
 */
devcall tftRead(device *devptr, void *buf, uint len)
{
    (void)devptr;
    (void)buf;
    (void)len;
    return SYSERR;
}
