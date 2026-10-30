/**
 * @file xsh_cd.c
 *
 * Change the current working directory.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <string.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>

shellcmd xsh_cd(int nargs, char *args[])
{
    if (!fs_need())
        return SHELL_ERROR;

    if (nargs < 2)
    {
        strcpy(fs_curdir, "/");
        return SHELL_OK;
    }

    char *p = full_path(args[1]);
    if (!fl_is_dir(p))
    {
        printf("cd: '%s' is not a directory\n", p);
        return SHELL_ERROR;
    }

    if (strcmp(p, "/") == 0)
        strcpy(fs_curdir, "/");
    else
    {
        size_t l = strlen(p);
        while (l > 1 && p[l - 1] == '/')
            p[--l] = '\0';
        strcpy(fs_curdir, p);
    }
    return SHELL_OK;
}
