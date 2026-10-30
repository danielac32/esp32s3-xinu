/**
 * @file rgb.h
 *
 * Discrete RGB LED driver for the ESP32-S3 board.
 *
 * The LED is not a smart LED: it is three ordinary GPIOs (red, green, blue)
 * driven by the kernel.  The default wiring is declared in <board.h>:
 *
 *     red   = GPIO48
 *     green = GPIO47
 *     blue  = GPIO41
 *
 * The device is intentionally small.  Applications can either use the
 * device interface:
 *
 *     control(RGB0, RGB_CTRL_SET, (r << 16) | (g << 8) | b, 0);
 *
 * or the helper functions below.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _RGB_H_
#define _RGB_H_

#include <device.h>
#include <monitor.h>
#include <stddef.h>
#include <stdint.h>
#include <conf.h>

/* tftControl-style commands.  The colour is packed as 0x00RRGGBB. */
#define RGB_CTRL_SET        0x30    /**< arg1 = 0x00RRGGBB                 */
#define RGB_CTRL_OFF        0x31    /**< turn all three channels off       */
#define RGB_CTRL_ON         0x32    /**< restore the last colour           */
#define RGB_CTRL_GET        0x33    /**< return the current packed colour  */
#define RGB_CTRL_POLARITY   0x34    /**< arg1 != 0: active high (cathode)  */

/** RGB control block, one per device. */
struct rgb
{
    monitor lock;               /**< serializes access to the LED        */
    bool opened;                /**< TRUE once rgbOpen() has run         */
    bool ready;                 /**< TRUE once the pins are configured   */
    bool lit;                   /**< TRUE when the last colour is on     */
    bool active_high;           /**< TRUE = common cathode (active high) */
    uint8_t r;                  /**< last red value, 0..255              */
    uint8_t g;                  /**< last green value, 0..255            */
    uint8_t b;                  /**< last blue value, 0..255             */
};

extern struct rgb rgbtab[];

/* Device-independent helpers.  They do not require open(). */
int rgb_set(uint8_t r, uint8_t g, uint8_t b);
int rgb_off(void);
int rgb_on(void);
uint32_t rgb_get(void);

/* Driver functions */
devcall rgbInit(device *);
devcall rgbOpen(device *);
devcall rgbClose(device *);
devcall rgbWrite(device *, const void *, uint);
devcall rgbControl(device *, int, long, long);

#endif /* _RGB_H_ */
