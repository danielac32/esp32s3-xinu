/**
 * @file xsh_head.c
 *
 * Print the first lines of a file.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>

shellcmd xsh_head(int nargs, char *args[])
{
    FL_FILE *f;
    char line[256];
    int n = 10;
    int i = 0;

    if (nargs < 2)
    {
        printf("Usage: head [-n lines] <file>\n");
        return SHELL_ERROR;
    }
    if (!fs_need())
        return SHELL_ERROR;

    int arg = 1;
    if (strcmp(args[1], "-n") == 0 && nargs >= 4)
    {
        n = atoi(args[2]);
        arg = 3;
    }
    if (n < 0)
        n = 0;

    f = fs_open("head", args[arg], "r");
    if (!f)
        return SHELL_ERROR;

    while (i < n && fl_fgets(line, sizeof(line), f))
    {
        printf("%s", line);
        i++;
    }
    fl_fclose(f);
    return SHELL_OK;
}
