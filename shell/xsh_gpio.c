/**
 * @file xsh_gpio.c
 *
 * gpio: inspect and drive ESP32-S3 GPIO pins.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shell.h>
#include <xtensa.h>

#define GPIO_MAX_PIN 48

static void usage(void)
{
    printf("Usage: gpio read <pin>\n");
    printf("       gpio write <pin> <0|1>\n");
    printf("       gpio mode <pin> <in|out>\n");
}

shellcmd xsh_gpio(int nargs, char *args[])
{
    int pin;

    if (nargs < 2)
    {
        usage();
        return SHELL_ERROR;
    }

    if (!strcmp(args[1], "read") && nargs >= 3)
    {
        pin = atoi(args[2]);
        if (pin < 0 || pin >= GPIO_MAX_PIN)
        {
            printf("gpio: invalid pin %d\n", pin);
            return SHELL_ERROR;
        }
        printf("gpio %d = %u\n", pin, xt_gpio_get_level((uint)pin));
        return SHELL_OK;
    }

    if (!strcmp(args[1], "write") && nargs >= 4)
    {
        pin = atoi(args[2]);
        if (pin < 0 || pin >= GPIO_MAX_PIN)
        {
            printf("gpio: invalid pin %d\n", pin);
            return SHELL_ERROR;
        }
        xt_gpio_output((uint)pin, 0);
        xt_gpio_set_level((uint)pin, (uint)atoi(args[3]) ? 1U : 0U);
        printf("gpio %d <- %s\n", pin, atoi(args[3]) ? "1" : "0");
        return SHELL_OK;
    }

    if (!strcmp(args[1], "mode") && nargs >= 4)
    {
        pin = atoi(args[2]);
        if (pin < 0 || pin >= GPIO_MAX_PIN)
        {
            printf("gpio: invalid pin %d\n", pin);
            return SHELL_ERROR;
        }
        if (!strcmp(args[3], "out"))
            xt_gpio_output((uint)pin, 0);
        else if (!strcmp(args[3], "in"))
            xt_gpio_input((uint)pin, 0);
        else
        {
            usage();
            return SHELL_ERROR;
        }
        printf("gpio %d mode %s\n", pin, args[3]);
        return SHELL_OK;
    }

    usage();
    return SHELL_ERROR;
}
