/**
 * @file xsh_dumpfile.c
 *
 * Hex dump of a file.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <stdlib.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>

shellcmd xsh_dumpfile(int nargs, char *args[])
{
    if (!fs_need())
        return SHELL_ERROR;

    if (nargs < 2)
    {
        printf("Usage: dumpfile <file> [max_bytes]\n");
        return SHELL_ERROR;
    }

    char *p = full_path(args[1]);
    FL_FILE *f = fl_fopen(p, "r");
    if (!f)
    {
        printf("dumpfile: cannot open '%s'\n", p);
        return SHELL_ERROR;
    }

    long max = -1;
    if (nargs >= 3)
    {
        max = atoi(args[2]);
        if (max < 0)
            max = 0;
    }

    printf("dumpfile: %s\n", p);
    unsigned char buf[16];
    long offset = 0;
    long total = 0;
    int n;
    while ((n = fl_fread(buf, 1, sizeof(buf), f)) > 0
           && (max < 0 || total < max))
    {
        if (max >= 0 && total + n > max)
            n = (int)(max - total);

        printf("%08lx  ", (unsigned long)offset);
        for (int i = 0; i < 16; i++)
        {
            if (i < n)
                printf("%02x ", buf[i]);
            else
                printf("   ");
            if (i == 7)
                printf(" ");
        }
        printf(" |");
        for (int i = 0; i < n; i++)
        {
            unsigned char c = buf[i];
            printf("%c", (c >= 0x20 && c <= 0x7e) ? c : '.');
        }
        printf("|\n");

        offset += n;
        total += n;
    }
    fl_fclose(f);

    printf("dumpfile: %ld bytes total\n", total);
    return SHELL_OK;
}
