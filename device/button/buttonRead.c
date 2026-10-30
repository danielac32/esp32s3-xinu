/**
 * @file buttonRead.c
 *
 * Push button (GPIO) driver for the ESP32-S3: wait for a press.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <clock.h>
#include <monitor.h>
#include <thread.h>

#include <button.h>

/**
 * @ingroup button
 *
 * Wait for the button to be pressed and report one byte, 1, for each
 * press.  The contact must stay closed through the debounce interval and
 * the call only returns once the button is released again, so a single
 * press yields exactly one byte.
 *
 * @param devptr button device table entry
 * @param buf buffer to receive the byte (at least one byte long)
 * @param len size of @p buf
 * @return 1 on success, SYSERR on error, TIMEOUT if no press was seen
 */
devcall buttonRead(device *devptr, void *buf, uint len)
{
    struct button *btnptr;
    uchar *dst = (uchar *)buf;
    uint i;

    btnptr = &buttontab[devptr->minor];

    if (NULL == buf || 0 == len || !btnptr->ready ||
        isbadmon(btnptr->lock))
    {
        return SYSERR;
    }

    lock(btnptr->lock);

    for (i = 0; i < BUTTON_WAIT_TICKS; i++)
    {
        if (!button_pressed())
        {
            sleep(1);
            continue;
        }

        /* Level changed: make sure it survives the bounce. */
        udelay(BUTTON_DEBOUNCE_US);
        if (!button_pressed())
        {
            continue;
        }

        /* Report the press once the button comes back up. */
        while (button_pressed())
        {
            sleep(1);
        }

        btnptr->presses++;
        dst[0] = 1;
        unlock(btnptr->lock);
        return 1;
    }

    unlock(btnptr->lock);
    return TIMEOUT;
}
