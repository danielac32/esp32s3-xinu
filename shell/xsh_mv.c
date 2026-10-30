/**
 * @file xsh_mv.c
 *
 * Move (rename) a file.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <string.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>

extern int fs_copy_file(const char *cmd, const char *from, const char *to);

shellcmd xsh_mv(int nargs, char *args[])
{
    char srcbuf[FS_BUF_LEN];
    int ret;

    if (!fs_need())
        return SHELL_ERROR;
    if (nargs < 3)
    {
        printf("Usage: mv <src> <dst>\n");
        return SHELL_ERROR;
    }

    strncpy(srcbuf, full_path(args[1]), sizeof(srcbuf) - 1);
    srcbuf[sizeof(srcbuf) - 1] = '\0';

    ret = fs_copy_file("mv", srcbuf, args[2]);
    if (ret != SHELL_OK)
        return ret;
    if (fl_remove(srcbuf) != 0)
    {
        printf("mv: could not remove source '%s'\n", srcbuf);
        return SHELL_ERROR;
    }
    return SHELL_OK;
}
