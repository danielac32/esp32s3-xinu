/**
 * @file xsh_df.c
 *
 * Report filesystem usage.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <shell.h>
#include <fat_filelib.h>
#include <fat_access.h>
#include <fat_table.h>
#include <sd.h>
#include <xsh_fs.h>
#include <string.h>

extern struct fatfs *fl_get_fs(void);

shellcmd xsh_df(int nargs, char *args[])
{
    struct fatfs *fs;
    uint32 free_clusters;
    uint32 total_blocks;
    uint32 free_blocks;

    (void)nargs;
    (void)args;

    if (!fs_need())
        return SHELL_ERROR;

    fs = fl_get_fs();
    total_blocks = sd_capacity_blocks();
    free_clusters = fs ? fatfs_count_free_clusters(fs) : 0;
    free_blocks = (fs ? (uint32)free_clusters * fs->sectors_per_cluster : 0);
    if (free_blocks > total_blocks)
        free_blocks = total_blocks;

    printf("Filesystem     Blocks     Used     Free\n");
    printf("/sd          %8lu %8lu %8lu\n",
           (unsigned long)total_blocks,
           (unsigned long)(total_blocks - free_blocks),
           (unsigned long)free_blocks);
    printf("%lu KiB total, %lu KiB free\n",
           (unsigned long)(total_blocks / 2), (unsigned long)(free_blocks / 2));
    return SHELL_OK;
}
