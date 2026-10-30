/**
 * @file button.h
 *
 * Push button (GPIO) driver for the ESP32-S3.
 *
 * The board's BOOT button sits on GPIO0, pulled up by the module, so a
 * press pulls the pin low.  The driver polls instead of using the GPIO
 * interrupt: the pin is readable at any time and polling keeps the button
 * off the shared GPIO interrupt source that the other peripherals use.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _BUTTON_H_
#define _BUTTON_H_

#include <device.h>
#include <monitor.h>
#include <stddef.h>
#include <stdint.h>
#include <conf.h>

/* The board's only user button is BOOT, wired to GPIO0 and pulled up by
 * the module, so it reads 0 while it is being held down. */
#define BUTTON_GPIO         0

/* How many 10 ms ticks a read is allowed to wait before giving up. */
#define BUTTON_WAIT_TICKS   500

/* Settling time, in microseconds, before a level change is believed. */
#define BUTTON_DEBOUNCE_US  10000

/* buttonControl() functions */
#define BUTTON_CTRL_PRESSED 0x20    /**< 1 while the button is held down  */
#define BUTTON_CTRL_WAIT    0x21    /**< arg1 = ticks to wait for a press */

/** Button control block, one per device. */
struct button
{
    monitor lock;               /**< serializes access to the device      */
    bool opened;                /**< TRUE once buttonOpen() has run       */
    bool ready;                 /**< TRUE once the pin has been configured */
    uint presses;               /**< number of presses seen               */
};

extern struct button buttontab[];

/** TRUE while the button is held down. */
bool button_pressed(void);

/* Driver functions */
devcall buttonInit(device *);
devcall buttonOpen(device *);
devcall buttonClose(device *);
devcall buttonRead(device *, void *, uint);
devcall buttonWrite(device *, const void *, uint);
devcall buttonGetc(device *);
devcall buttonPutc(device *, char);
devcall buttonControl(device *, int, long, long);
interrupt buttonInterrupt(void);

#endif                          /* _BUTTON_H_ */
