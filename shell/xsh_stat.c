/**
 * @file xsh_stat.c
 *
 * Report information about a file.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>
#include <string.h>

shellcmd xsh_stat(int nargs, char *args[])
{
    FL_FILE *f;
    long size;

    if (nargs < 2)
    {
        printf("Usage: stat <file>\n");
        return SHELL_ERROR;
    }
    if (!fs_need())
        return SHELL_ERROR;

    char *p = full_path(args[1]);
    if (fl_is_dir(p))
    {
        printf("  File: %s\n  Type: directory\n", p);
        return SHELL_OK;
    }

    f = fl_fopen(p, "r");
    if (!f)
    {
        printf("stat: cannot open '%s'\n", p);
        return SHELL_ERROR;
    }
    fl_fseek(f, 0, 2);
    size = fl_ftell(f);
    fl_fclose(f);

    printf("  File: %s\n  Size: %ld bytes\n  Type: regular file\n",
           p, size);
    return SHELL_OK;
}
