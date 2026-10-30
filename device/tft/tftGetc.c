/**
 * @file tftGetc.c
 *
 * ST7735 TFT display driver for the ESP32-S3: gets are not supported.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>

#include <tft.h>

/**
 * @ingroup tft
 *
 * The panel cannot be read.
 *
 * @param devptr tft device table entry
 * @return SYSERR, always
 */
devcall tftGetc(device *devptr)
{
    (void)devptr;
    return SYSERR;
}
