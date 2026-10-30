/**
 * @file buttonOpen.c
 *
 * Push button (GPIO) driver for the ESP32-S3: open a button device.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>

#include <button.h>

/**
 * @ingroup button
 *
 * Prepare a button device for use.  A button may only be open once.
 *
 * @param devptr button device table entry
 * @return OK on success, SYSERR if the device is unusable or already open
 */
devcall buttonOpen(device *devptr)
{
    struct button *btnptr;

    btnptr = &buttontab[devptr->minor];

    if (!btnptr->ready || isbadmon(btnptr->lock))
    {
        return SYSERR;
    }

    lock(btnptr->lock);
    if (btnptr->opened)
    {
        unlock(btnptr->lock);
        return SYSERR;
    }
    btnptr->opened = TRUE;
    unlock(btnptr->lock);

    return OK;
}
