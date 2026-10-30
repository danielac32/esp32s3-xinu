/**
 * @file xsh_ip.c
 *
 * Alias for ifconfig.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <shell.h>
#include <string.h>
#include <stdio.h>

shellcmd xsh_ip(int nargs, char *args[])
{
    return xsh_ifconfig(nargs, args);
}
