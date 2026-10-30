/**
 * @file xsh_fsutil.c
 *
 * Shared helpers for the FAT filesystem shell commands: path resolution
 * against the current directory and the mount check.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shell.h>
#include <fat_filelib.h>
#include <fat_table.h>
#include <fat_port.h>

#include <xsh_fs.h>

char fs_curdir[FS_BUF_LEN] = "/";
static char fs_path[FS_BUF_LEN];

static void safe_copy(char *dst, const char *src, size_t n)
{
    if (n == 0)
        return;
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

static void append_str(char *dst, const char *src, size_t dstsz)
{
    size_t l = strlen(dst);
    if (l >= dstsz - 1)
        return;
    strncat(dst, src, dstsz - l - 1);
}

char *full_path(const char *name)
{
    char tmp[FS_BUF_LEN];
    char *p;

    if (!name || !name[0])
        name = ".";

    if (name[0] == '/')
    {
        safe_copy(tmp, name, sizeof(tmp));
    }
    else
    {
        if (strcmp(fs_curdir, "/") == 0)
            tmp[0] = '\0';
        else
            safe_copy(tmp, fs_curdir, sizeof(tmp));
        size_t t = strlen(tmp);
        if (t + 1 < sizeof(tmp))
        {
            tmp[t] = '/';
            tmp[t + 1] = '\0';
            strncat(tmp, name, sizeof(tmp) - t - 2);
        }
    }

    fs_path[0] = '\0';
    /* Tokenize manually on '/' */
    char *s = tmp;
    while (*s)
    {
        while (*s == '/')
            s++;
        if (*s == '\0')
            break;
        char *tok = s;
        while (*s && *s != '/')
            s++;
        int len = (int)(s - tok);
        char word[FS_BUF_LEN];
        if (len >= (int)sizeof(word))
            len = sizeof(word) - 1;
        memcpy(word, tok, len);
        word[len] = '\0';

        if (strcmp(word, ".") == 0)
        {
            /* nothing */
        }
        else if (strcmp(word, "..") == 0)
        {
            p = strrchr(fs_path, '/');
            if (p)
            {
                if (p == fs_path)
                    fs_path[1] = '\0';
                else
                    *p = '\0';
            }
        }
        else
        {
            if (strlen(fs_path) + strlen(word) + 1 < sizeof(fs_path))
            {
                append_str(fs_path, "/", sizeof(fs_path));
                append_str(fs_path, word, sizeof(fs_path));
            }
        }
    }

    if (fs_path[0] == '\0')
        strcpy(fs_path, "/");

    return fs_path;
}

int fs_need(void)
{
    if (!fat_mounted())
    {
        if (!fat_mount())
        {
            printf("Filesystem FAT not mounted (run 'fat mount' first)\n");
            return 0;
        }
    }
    return 1;
}

FL_FILE *fs_open(const char *cmd, const char *name, const char *mode)
{
    char *p = full_path(name);
    FL_FILE *f = fl_fopen(p, mode);

    if (!f)
        printf("%s: cannot open '%s'\n", cmd, p);
    return f;
}

int fs_remove_file(const char *path)
{
    struct fatfs *fs;
    FL_FILE *f;

    f = fl_fopen(path, "r");
    if (!f)
        return SHELL_ERROR;

    fs = fl_get_fs();
    if (!fs || !fatfs_mark_file_deleted(fs, f->parentcluster,
                                        (char *)f->shortfilename))
    {
        fl_fclose(f);
        return SHELL_ERROR;
    }

    if (f->startcluster != 0)
        fatfs_free_cluster_chain(fs, f->startcluster);

    fl_fclose(f);
    return SHELL_OK;
}
