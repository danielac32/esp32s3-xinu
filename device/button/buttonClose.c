/**
 * @file buttonClose.c
 *
 * Push button (GPIO) driver for the ESP32-S3: close a button device.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>

#include <button.h>

/**
 * @ingroup button
 *
 * Release a button device so that it may be opened again.
 *
 * @param devptr button device table entry
 * @return OK on success, SYSERR if the device is unusable
 */
devcall buttonClose(device *devptr)
{
    struct button *btnptr;

    btnptr = &buttontab[devptr->minor];

    if (isbadmon(btnptr->lock))
    {
        return SYSERR;
    }

    lock(btnptr->lock);
    btnptr->opened = FALSE;
    unlock(btnptr->lock);

    return OK;
}
