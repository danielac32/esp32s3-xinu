/**
 * @file buttonGetc.c
 *
 * Push button (GPIO) driver for the ESP32-S3: read one button event.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>

#include <button.h>

/**
 * @ingroup button
 *
 * Wait for one press and return it as the value 1.
 *
 * @param devptr button device table entry
 * @return 1 on success, SYSERR on error, TIMEOUT if no press was seen
 */
devcall buttonGetc(device *devptr)
{
    uchar c;

    if (1 != buttonRead(devptr, &c, 1))
    {
        return SYSERR;
    }
    return c;
}
