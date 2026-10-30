/**
 * @file xsh_du.c
 *
 * Summarize disk usage, recursively.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <string.h>
#include <shell.h>
#include <fat_filelib.h>
#include <xsh_fs.h>

static void du_join(char *dst, size_t n, const char *base, const char *name)
{
    strncpy(dst, base, n - 1);
    dst[n - 1] = '\0';
    if (strcmp(base, "/") != 0)
        strncat(dst, "/", n - strlen(dst) - 1);
    strncat(dst, name, n - strlen(dst) - 1);
}

static ulong du_walk(const char *path, int depth)
{
    FL_DIR dir;
    fl_dirent ent;
    ulong total = 0;

    if (depth > 16)
        return 0;
    if (!fl_opendir(path, &dir))
        return 0;

    while (fl_readdir(&dir, &ent) == 0)
    {
        char child[FS_BUF_LEN];
        ulong sz;

        if (!strcmp(ent.filename, ".") || !strcmp(ent.filename, ".."))
            continue;

        du_join(child, sizeof(child), path, ent.filename);

        if (ent.is_dir)
            sz = du_walk(child, depth + 1);
        else
            sz = (ulong)ent.size;

        printf("%8lu  %s\n", sz, child);
        total += sz;
    }
    fl_closedir(&dir);
    return total;
}

shellcmd xsh_du(int nargs, char *args[])
{
    const char *path;
    ulong total;

    if (nargs > 1 && !strcmp(args[1], "--help"))
    {
        printf("Usage: du [<path>]\n");
        printf("Prints the size of every file below <path> and the total.\n");
        return SHELL_OK;
    }

    if (!fs_need())
        return SHELL_ERROR;

    path = (nargs > 1) ? full_path(args[1]) : fs_curdir;

    if (!fl_is_dir(path))
    {
        FL_FILE *f = fl_fopen(path, "r");
        if (!f)
        {
            printf("du: cannot open '%s'\n", path);
            return SHELL_ERROR;
        }
        fl_fseek(f, 0, 2);
        total = (ulong)fl_ftell(f);
        fl_fclose(f);
        printf("%8lu  %s\n", total, path);
        return SHELL_OK;
    }

    total = du_walk(path, 0);
    printf("%8lu  %s (total)\n", total, path);
    return SHELL_OK;
}
