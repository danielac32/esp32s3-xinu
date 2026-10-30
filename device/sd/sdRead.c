/**
 * @file sdRead.c
 *
 * SDMMC (SDHOST) microSD driver for the ESP32-S3: block read.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sd.h>

#include "sdhost.h"

/**
 * Read \p len bytes from the device at its current LBA cursor, then
 * advance the cursor.  Transfers are whole 512 byte blocks, so \p len
 * is rounded up; the caller's buffer must hold that much.
 */
devcall sdRead(device *devptr, void *buf, uint len)
{
    uint32_t blocks;
    uint32_t i;
    uint8_t *dst = (uint8_t *)buf;

    (void)devptr;

    if (!sd_present || NULL == buf || 0 == len)
    {
        return SYSERR;
    }

    blocks = (len + SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE;
    lock(sd_lock);
    for (i = 0; i < blocks; i++)
    {
        if (!sdOneBlock(SD_CMD17_READ_SINGLE, sd_lba + i, FALSE))
        {
            unlock(sd_lock);
            return SYSERR;
        }
        if ((i + 1) == blocks)
        {
            memcpy(dst, sd_bounce, len - (i * SD_BLOCK_SIZE));
        }
        else
        {
            memcpy(dst, sd_bounce, SD_BLOCK_SIZE);
        }
        dst += SD_BLOCK_SIZE;
    }
    sd_lba += blocks;
    unlock(sd_lock);
    return (devcall)len;
}
