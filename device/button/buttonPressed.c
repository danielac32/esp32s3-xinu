/**
 * @file buttonPressed.c
 *
 * Push button (GPIO) driver for the ESP32-S3: current pin state.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <button.h>
#include <xtensa.h>

/**
 * @ingroup button
 *
 * Report whether the button is held down right now.
 *
 * The pin is pulled up, so a pressed button reads as 0.  No debouncing is
 * done here; callers that must reject contact bounce sample it twice.
 *
 * @return TRUE while the button is held down, otherwise FALSE
 */
bool button_pressed(void)
{
    return (0 == xt_gpio_get_level(BUTTON_GPIO)) ? TRUE : FALSE;
}
