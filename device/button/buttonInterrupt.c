/**
 * @file buttonInterrupt.c
 *
 * Push button (GPIO) driver for the ESP32-S3: GPIO interrupt handler.
 *
 * The driver polls the pin, so there is nothing to service here.  The
 * routine exists so that the device can carry an interrupt vector entry,
 * and it clears the pin's status bits to keep that harmless.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <xtensa.h>

#include <button.h>

/**
 * @ingroup button
 *
 * Clear the button pin's GPIO status bits.
 */
interrupt buttonInterrupt(void)
{
    REG32(GPIO_STATUS_W1TC) = (1UL << BUTTON_GPIO);
}
