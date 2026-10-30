/**
 * @file xsh_rmdir.c
 *
 * Remove an empty directory.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>
#include <string.h>

shellcmd xsh_rmdir(int nargs, char *args[])
{
    if (!fs_need())
        return SHELL_ERROR;

    if (nargs < 2)
    {
        printf("Usage: rmdir <dir>\n");
        return SHELL_ERROR;
    }

    char *p = full_path(args[1]);
    /* fl_removedirectory() returns 0 on success and -1 on failure. */
    if (fl_removedirectory(p) != 0)
    {
        printf("rmdir: cannot remove '%s'\n", p);
        return SHELL_ERROR;
    }
    printf("rmdir: removed %s\n", p);
    return SHELL_OK;
}
