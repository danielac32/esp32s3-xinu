/**
 * @file xsh_rm.c
 *
 * Remove files.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>
#include <string.h>

shellcmd xsh_rm(int nargs, char *args[])
{
    int ret = SHELL_OK;

    if (!fs_need())
        return SHELL_ERROR;

    if (nargs < 2)
    {
        printf("Usage: rm <file> [file ...]\n");
        return SHELL_ERROR;
    }

    for (int i = 1; i < nargs; i++)
    {
        char *p = full_path(args[i]);
        if (fl_is_dir(p))
        {
            printf("rm: '%s' is a directory; use rmdir\n", p);
            ret = SHELL_ERROR;
            continue;
        }
        if (fs_remove_file(p) != SHELL_OK)
        {
            printf("rm: cannot delete '%s'\n", p);
            ret = SHELL_ERROR;
            continue;
        }
        printf("rm: removed %s\n", p);
    }
    return ret;
}
