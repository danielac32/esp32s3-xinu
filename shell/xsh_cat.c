/**
 * @file xsh_cat.c
 *
 * Print a file to standard output.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>
#include <string.h>

shellcmd xsh_cat(int nargs, char *args[])
{
    if (!fs_need())
        return SHELL_ERROR;

    if (nargs < 2)
    {
        printf("Usage: cat <file>\n");
        return SHELL_ERROR;
    }

    FL_FILE *f = fs_open("cat", args[1], "r");
    if (!f)
        return SHELL_ERROR;

    char line[FS_BUF_LEN];
    while (fl_fgets(line, sizeof(line), f))
        printf("%s", line);
    fl_fclose(f);
    return SHELL_OK;
}
