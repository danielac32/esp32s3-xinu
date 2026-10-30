/**
 * @file xsh_mkdir.c
 *
 * Create a directory.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>
#include <string.h>

shellcmd xsh_mkdir(int nargs, char *args[])
{
    if (!fs_need())
        return SHELL_ERROR;

    if (nargs < 2)
    {
        printf("Usage: mkdir <dir>\n");
        return SHELL_ERROR;
    }

    char *p = full_path(args[1]);
    if (fl_createdirectory(p) != 1)
    {
        printf("mkdir: cannot create '%s'\n", p);
        return SHELL_ERROR;
    }
    printf("mkdir: created %s\n", p);
    return SHELL_OK;
}
