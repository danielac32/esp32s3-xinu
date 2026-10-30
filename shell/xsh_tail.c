/**
 * @file xsh_tail.c
 *
 * Print the last lines of a file.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>

shellcmd xsh_tail(int nargs, char *args[])
{
    FL_FILE *f;
    char line[256];
    static char ring[16][256];
    int n = 10;
    int i, count = 0;
    int arg = 1;

    if (nargs < 2)
    {
        printf("Usage: tail [-n lines] <file>\n");
        return SHELL_ERROR;
    }
    if (!fs_need())
        return SHELL_ERROR;

    if (strcmp(args[1], "-n") == 0 && nargs >= 4)
    {
        n = atoi(args[2]);
        arg = 3;
    }
    if (n < 0)
        n = 0;
    if (n > 16)
        n = 16;

    f = fs_open("tail", args[arg], "r");
    if (!f)
        return SHELL_ERROR;

    while (fl_fgets(line, sizeof(line), f))
    {
        strncpy(ring[count % 16], line, sizeof(ring[0]) - 1);
        ring[count % 16][sizeof(ring[0]) - 1] = '\0';
        count++;
    }
    fl_fclose(f);

    int start = count - n;
    if (start < 0)
        start = 0;
    for (i = start; i < count; i++)
        printf("%s", ring[i % 16]);
    return SHELL_OK;
}
