/**
 * @file     xsh_button.c
 *
 * Shell command (button) reports the state of the board's button and can
 * wait for it to be pressed, which is how the polling driver is checked
 * from the console.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <button.h>
#include <device.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <shell.h>

#if NBUTTON

/** How long 'button wait' blocks when no count is given. */
#define BUTTON_WAIT_DEFAULT 5000

/** Print the usage message. */
static void buttonUsage(const char *cmd)
{
    printf("Usage: %s [--help]\n", cmd);
    printf("       %s wait [<ticks>]\n", cmd);
    printf("\n");
    printf("Description:\n");
    printf("\tWith no arguments, says whether the button is down.\n");
    printf("\t'wait' blocks until it is pressed or the wait runs out\n");
    printf("\t(one tick is a millisecond, %d by default).\n",
           BUTTON_WAIT_DEFAULT);
}

/**
 * @ingroup shell
 *
 * Read the button, or wait for a press.
 *
 * @param nargs number of arguments in args array
 * @param args  array of arguments
 * @return non-zero value on error
 */
shellcmd xsh_button(int nargs, char *args[])
{
    if (nargs == 2 && 0 == strcmp(args[1], "--help"))
    {
        buttonUsage(args[0]);
        return SHELL_OK;
    }

    /* The button may already be open from an earlier command; only
     * complain when it cannot be reached at all. */
    if (SYSERR == open(BTN0)
        && SYSERR == control(BTN0, BUTTON_CTRL_PRESSED, 0, 0))
    {
        fprintf(stderr, "%s: button is not available\n", args[0]);
        return SHELL_ERROR;
    }

    if (nargs == 1)
    {
        printf("Button is %s\n",
               control(BTN0, BUTTON_CTRL_PRESSED, 0, 0) ? "down" : "up");
        return SHELL_OK;
    }

    if (0 == strcmp(args[1], "wait"))
    {
        long ticks = BUTTON_WAIT_DEFAULT;

        if (nargs > 2 && 1 != sscanf(args[2], "%ld", &ticks))
        {
            fprintf(stderr, "%s: bad tick count '%s'\n", args[0], args[2]);
            return SHELL_ERROR;
        }

        printf("Press the button...\n");
        if (control(BTN0, BUTTON_CTRL_WAIT, ticks, 0))
        {
            printf("Button pressed.\n");
            return SHELL_OK;
        }

        printf("No press within %ld ticks.\n", ticks);
        return SHELL_ERROR;
    }

    fprintf(stderr, "%s: unknown option '%s'\n", args[0], args[1]);
    buttonUsage(args[0]);
    return SHELL_ERROR;
}

#endif /* NBUTTON */
