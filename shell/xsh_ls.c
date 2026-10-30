/**
 * @file xsh_ls.c
 *
 * List a directory.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>
#include <string.h>

shellcmd xsh_ls(int nargs, char *args[])
{
    const char *path;

    if (!fs_need())
        return SHELL_ERROR;

    path = fs_curdir;
    if (nargs > 1)
        path = full_path(args[1]);

    FL_DIR dir;
    fl_dirent ent;
    if (!fl_opendir(path, &dir))
    {
        printf("ls: cannot open '%s'\n", path);
        return SHELL_ERROR;
    }

    printf("%s:\n", path);
    while (fl_readdir(&dir, &ent) == 0)
    {
        if (ent.is_dir)
            printf("  %-24s <DIR>\n", ent.filename);
        else
            printf("  %-24s [%lu bytes]\n", ent.filename,
                   (unsigned long)ent.size);
    }
    fl_closedir(&dir);
    return SHELL_OK;
}
