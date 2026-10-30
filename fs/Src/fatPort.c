/**
 * @file fatPort.c
 *
 * Integration layer between fat_filelib and Xinu's SD driver.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <monitor.h>
#include <stdio.h>

#include "fat_port.h"
#include "fat_filelib.h"
#include "disk.h"

static bool s_mounted = FALSE;
static monitor s_fat_mon = SYSERR;

static void fat_lock(void)
{
    if (SYSERR != s_fat_mon)
        lock(s_fat_mon);
}

static void fat_unlock(void)
{
    if (SYSERR != s_fat_mon)
        unlock(s_fat_mon);
}

bool fat_mount(void)
{
    uint32 size = 0;
    FL_DIR dir;

    if (s_mounted)
        return TRUE;

    if (SYSERR == s_fat_mon)
    {
        s_fat_mon = moncreate();
        if (SYSERR == s_fat_mon)
        {
            printf("FAT: cannot create monitor\n");
            return FALSE;
        }
    }

    size = (uint32)sd_init();
    if (0 == size)
    {
        printf("FAT: SD card not available\n");
        return FALSE;
    }

    fl_init();

    if (fl_attach_media(sd_readsector, sd_writesector) != FAT_INIT_OK ||
        fl_opendir("/", &dir) == NULL)
    {
        printf("FAT: formatting SD (%lu sectors)...\n", (unsigned long)size);
        if (fl_format(size, "XINUSD") == 0)
        {
            printf("FAT: format failed\n");
            return FALSE;
        }
        printf("FAT: format OK\n");
        if (fl_attach_media(sd_readsector, sd_writesector) != FAT_INIT_OK)
        {
            printf("FAT: mount after format failed\n");
            return FALSE;
        }
    }

    fl_attach_locks(fat_lock, fat_unlock);

    s_mounted = TRUE;
    return TRUE;
}

bool fat_mounted(void)
{
    return s_mounted;
}

bool fat_format(void)
{
    uint32 size;

    if (s_mounted)
    {
        fl_shutdown();
        s_mounted = FALSE;
    }

    fl_init();

    size = (uint32)sd_init();
    if (0 == size)
    {
        printf("FAT: sd_init failed\n");
        return FALSE;
    }

    printf("FAT: formatting SD (%lu sectors)...\n", (unsigned long)size);
    if (fl_format(size, "XINUSD") == 0)
    {
        printf("FAT: format failed\n");
        return FALSE;
    }

    if (fl_attach_media(sd_readsector, sd_writesector) != FAT_INIT_OK)
    {
        printf("FAT: remount after format failed\n");
        return FALSE;
    }

    s_mounted = TRUE;
    printf("FAT: format and remount OK\n");
    return TRUE;
}
