/**
 * @file xsh_echo.c
 *
 * Write its arguments to standard output.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <string.h>

shellcmd xsh_echo(int nargs, char *args[])
{
    if (nargs < 2)
        return SHELL_OK;

    for (int i = 1; i < nargs; i++)
    {
        if (i > 1)
            printf(" ");
        printf("%s", args[i]);
    }
    printf("\n");
    return SHELL_OK;
}
