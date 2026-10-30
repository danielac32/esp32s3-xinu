/**
 * @file xsh_fat.c
 *
 * Shell commands for the FAT filesystem on the SD card.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shell.h>
#include <fat_filelib.h>
#include <fat_port.h>

shellcmd xsh_fat(int nargs, char *args[])
{
    kprintf("xsh_fat: entered nargs=%d\n", nargs);
    if (nargs < 2)
    {
        printf("Usage: fat <mount|ls|cat|write|format> [args...]\n");
        return SHELL_ERROR;
    }

    if (0 == strcmp(args[1], "mount"))
    {
        if (fat_mount())
        {
            printf("FAT mounted.\n");
            return SHELL_OK;
        }
        printf("FAT mount failed.\n");
        return SHELL_ERROR;
    }

    if (0 == strcmp(args[1], "format"))
    {
        if (fat_format())
        {
            printf("FAT formatted and mounted.\n");
            return SHELL_OK;
        }
        printf("FAT format failed.\n");
        return SHELL_ERROR;
    }

    if (!fat_mounted())
    {
        if (!fat_mount())
        {
            printf("FAT not mounted.\n");
            return SHELL_ERROR;
        }
    }

    if (0 == strcmp(args[1], "ls"))
    {
        const char *path = (nargs > 2) ? args[2] : "/";
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
                printf("  %-24s [%lu bytes]\n", ent.filename, (unsigned long)ent.size);
        }
        fl_closedir(&dir);
        return SHELL_OK;
    }

    if (0 == strcmp(args[1], "cat"))
    {
        if (nargs < 3)
        {
            printf("Usage: fat cat <file>\n");
            return SHELL_ERROR;
        }
        FL_FILE *f = fl_fopen(args[2], "r");
        if (!f)
        {
            printf("cat: cannot open '%s'\n", args[2]);
            return SHELL_ERROR;
        }
        char buf[128];
        int n;
        while ((n = fl_fread(buf, 1, sizeof(buf) - 1, f)) > 0)
        {
            buf[n] = '\0';
            printf("%s", buf);
        }
        fl_fclose(f);
        printf("\n");
        return SHELL_OK;
    }

    if (0 == strcmp(args[1], "write"))
    {
        if (nargs < 4)
        {
            printf("Usage: fat write <file> <text...>\n");
            return SHELL_ERROR;
        }
        FL_FILE *f = fl_fopen(args[2], "w");
        if (!f)
        {
            printf("write: cannot create '%s'\n", args[2]);
            return SHELL_ERROR;
        }
        for (int i = 3; i < nargs; i++)
        {
            if (i > 3)
                fl_fwrite(" ", 1, 1, f);
            fl_fwrite(args[i], 1, strlen(args[i]), f);
        }
        fl_fwrite("\n", 1, 1, f);
        fl_fclose(f);
        printf("written %s\n", args[2]);
        return SHELL_OK;
    }

    printf("Unknown subcommand: %s\n", args[1]);
    return SHELL_ERROR;
}
