/**
 * @file xsh_cp.c
 *
 * Copy a file.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <string.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>

static int file_copy(const char *cmd, const char *from, const char *to)
{
    char srcbuf[FS_BUF_LEN];
    char dstbuf[FS_BUF_LEN];
    char buf[256];
    FL_FILE *in, *out;
    int n;
    long total = 0;

    strncpy(srcbuf, full_path(from), sizeof(srcbuf) - 1);
    srcbuf[sizeof(srcbuf) - 1] = '\0';
    strncpy(dstbuf, full_path(to), sizeof(dstbuf) - 1);
    dstbuf[sizeof(dstbuf) - 1] = '\0';

    if (fl_is_dir(srcbuf))
    {
        printf("%s: '%s' is a directory\n", cmd, srcbuf);
        return SHELL_ERROR;
    }

    in = fl_fopen(srcbuf, "r");
    if (!in)
    {
        printf("%s: cannot open '%s'\n", cmd, srcbuf);
        return SHELL_ERROR;
    }

    out = fl_fopen(dstbuf, "w");
    if (!out)
    {
        printf("%s: cannot create '%s'\n", cmd, dstbuf);
        fl_fclose(in);
        return SHELL_ERROR;
    }

    while ((n = fl_fread(buf, 1, sizeof(buf), in)) > 0)
    {
        fl_fwrite(buf, 1, n, out);
        total += n;
    }
    fl_fclose(out);
    fl_fclose(in);
    printf("%s: %ld bytes\n", cmd, total);
    return SHELL_OK;
}

int fs_copy_file(const char *cmd, const char *from, const char *to)
{
    return file_copy(cmd, from, to);
}

shellcmd xsh_cp(int nargs, char *args[])
{
    if (!fs_need())
        return SHELL_ERROR;
    if (nargs < 3)
    {
        printf("Usage: cp <src> <dst>\n");
        return SHELL_ERROR;
    }
    return file_copy("cp", args[1], args[2]);
}
