/**
 * @file     xsh_reset.c
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <xtensa.h>

/**
 * @ingroup shell
 *
 * Shell command (reset) performs a soft restart of the board.
 * @param nargs number of arguments in args array
 * @param args  array of arguments
 * @return non-zero value on error (the call does not return on success)
 */
shellcmd xsh_reset(int nargs, char *args[])
{
    /* Output help, if '--help' argument was supplied */
    if (nargs == 2 && strcmp(args[1], "--help") == 0)
    {
        printf("Usage: %s\n\n", args[0]);
        printf("Description:\n");
        printf("\tPerforms a soft reset of the system.\n");
        printf("Options:\n");
        printf("\t--help\t display this help and exit\n");
        return 1;
    }

    /* Check for correct number of arguments */
    if (nargs > 1)
    {
        fprintf(stderr, "%s: too many arguments\n", args[0]);
        fprintf(stderr, "Try '%s --help' for more information\n",
                args[0]);
        return 1;
    }

    printf("Restarting...\n");

    /* RTC_CNTL_OPTIONS0.SW_SYS_RST resets the digital core and the CPU,
     * after which the ROM boot loader runs again and loads the image. */
    REG32(RTC_CNTL_OPTIONS0) |= XT_RTC_CNTL_SW_SYS_RST;

    /* The reset takes a few cycles to take hold; do not run on. */
    for (;;)
    {
    }

    return 1;
}
