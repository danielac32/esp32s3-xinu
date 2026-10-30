/**
 * @file xsh_hexdump.c
 *
 * Hex dump of a file.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <stdlib.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>
#include <string.h>

shellcmd xsh_hexdump(int nargs, char *args[])
{
    FL_FILE *f;
    unsigned char buf[16];
    long offset = 0;
    long max = -1;
    long total = 0;
    int n;

    if (nargs < 2)
    {
        printf("Usage: hexdump <file> [max_bytes]\n");
        return SHELL_ERROR;
    }
    if (!fs_need())
        return SHELL_ERROR;

    if (nargs >= 3)
    {
        max = atoi(args[2]);
        if (max < 0)
            max = 0;
    }

    f = fs_open("hexdump", args[1], "r");
    if (!f)
        return SHELL_ERROR;

    while ((max < 0 || total < max)
           && (n = fl_fread(buf, 1, sizeof(buf), f)) > 0)
    {
        int i;

        if (max >= 0 && total + n > max)
            n = (int)(max - total);

        printf("%08lx  ", (unsigned long)offset);
        for (i = 0; i < 16; i++)
        {
            if (i < n)
                printf("%02x ", buf[i]);
            else
                printf("   ");
            if (i == 7)
                printf(" ");
        }
        printf(" |");
        for (i = 0; i < n; i++)
        {
            unsigned char c = buf[i];
            printf("%c", (c >= 0x20 && c <= 0x7e) ? c : '.');
        }
        printf("|\n");

        offset += n;
        total += n;
    }
    fl_fclose(f);
    return SHELL_OK;
}
