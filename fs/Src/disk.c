/**
 * @file disk.c
 *
 * Low-level disk interface for the FAT file library.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <stdint.h>
#include <sd.h>

#include "disk.h"

int sd_init(void)
{
    return sd_is_up() ? (int)sd_capacity_blocks() : 0;
}

int sd_readsector(uint32 sector, uint8 *buffer, uint32 sector_count)
{
    if (!sd_is_up() || NULL == buffer || 0 == sector_count)
    {
        return 0;
    }
    return (sd_read_blocks(sector, buffer, sector_count) == OK) ? 1 : 0;
}

int sd_writesector(uint32 sector, uint8 *buffer, uint32 sector_count)
{
    if (!sd_is_up() || NULL == buffer || 0 == sector_count)
    {
        return 0;
    }
    return (sd_write_blocks(sector, buffer, sector_count) == OK) ? 1 : 0;
}
