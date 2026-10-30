/**
 * @file buttonControl.c
 *
 * Push button (GPIO) driver for the ESP32-S3: device control.
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
 * Query and wait on the button without consuming a press.
 *
 * @param devptr button device table entry
 * @param func control function to execute
 * @param arg1 argument to the control function
 * @param arg2 unused
 * @return depends on @p func, SYSERR for an unknown function
 */
devcall buttonControl(device *devptr, int func, long arg1, long arg2)
{
    struct button *btnptr;
    long ticks;
    long i;

    (void)arg2;

    btnptr = &buttontab[devptr->minor];

    if (isbadmon(btnptr->lock))
    {
        return SYSERR;
    }

    switch (func)
    {
    case BUTTON_CTRL_PRESSED:
        lock(btnptr->lock);
        i = button_pressed() ? 1 : 0;
        unlock(btnptr->lock);
        return i;

    case BUTTON_CTRL_WAIT:
        ticks = (arg1 > 0) ? arg1 : 1;

        /* Poll with the monitor released: the wait is a sleep, and a
         * thread that died holding the button would lock it forever. */
        for (i = 0; i < ticks; i++)
        {
            bool down;

            lock(btnptr->lock);
            down = button_pressed();
            unlock(btnptr->lock);

            if (down)
            {
                return 1;
            }
            sleep(1);
        }
        return 0;

    default:
        return SYSERR;
    }
}
