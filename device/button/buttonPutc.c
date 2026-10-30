/**
 * @file buttonPutc.c
 *
 * Push button (GPIO) driver for the ESP32-S3: writes are not supported.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>

#include <button.h>

/**
 * @ingroup button
 *
 * A button is an input-only device, so output is an error.
 *
 * @param devptr button device table entry
 * @param c unused
 * @return SYSERR
 */
devcall buttonPutc(device *devptr, char c)
{
    (void)devptr;
    (void)c;
    return SYSERR;
}
