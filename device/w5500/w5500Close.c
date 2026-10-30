/**
 * @file w5500Close.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: close a device.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>

#include <w5500.h>

/**
 * @ingroup w5500
 *
 * Release an Ethernet device so that it may be opened again.  The chip
 * stays in MACRAW mode, so a later open finds it running.
 *
 * @param devptr W5500 device table entry
 * @return OK on success, SYSERR if the device is unusable
 */
devcall w5500Close(device *devptr)
{
    struct w5500 *ethptr;

    ethptr = &w5500tab[devptr->minor];

    if (isbadmon(ethptr->lock))
    {
        return SYSERR;
    }

    lock(ethptr->lock);
    ethptr->opened = FALSE;
    unlock(ethptr->lock);

    return OK;
}
