/**
 * @file w5500Open.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: open a device.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>

#include <w5500.h>

/**
 * @ingroup w5500
 *
 * Prepare an Ethernet device for use.  The chip must have been found by
 * w5500Init(); a device may only be open once.
 *
 * @param devptr W5500 device table entry
 * @return OK on success, SYSERR if the chip is missing or already open
 */
devcall w5500Open(device *devptr)
{
    struct w5500 *ethptr;

    ethptr = &w5500tab[devptr->minor];

    if (!ethptr->ready || isbadmon(ethptr->lock))
    {
        return SYSERR;
    }

    lock(ethptr->lock);
    if (ethptr->opened)
    {
        unlock(ethptr->lock);
        return SYSERR;
    }
    ethptr->opened = TRUE;
    unlock(ethptr->lock);

    return OK;
}
