/**
 * @file xsh_curl.c
 *
 * Alias for the wget command.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <shell.h>
#include <string.h>
#include <stdio.h>

shellcmd xsh_curl(int nargs, char *args[])
{
    return xsh_wget(nargs, args);
}
