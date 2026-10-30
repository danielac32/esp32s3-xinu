/**
 * @file xsh_pwd.c
 *
 * Print the current working directory.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <xsh_fs.h>
#include <string.h>

shellcmd xsh_pwd(int nargs, char *args[])
{
    (void)nargs;
    (void)args;

    printf("%s\n", fs_curdir);
    return SHELL_OK;
}
