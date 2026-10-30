/**
 * @file buttonWrite.c
 *
 * Push button (GPIO) driver for the ESP32-S3: writes are not supported.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>

#include <button.h>

/**
 * @ingroup button
 *
 * A button is an input-only device, so writing to it is an error.
 *
 * @param devptr button device table entry
 * @param buf unused
 * @param len unused
 * @return SYSERR
 */
devcall buttonWrite(device *devptr, const void *buf, uint len)
{
    (void)devptr;
    (void)buf;
    (void)len;
    return SYSERR;
}
