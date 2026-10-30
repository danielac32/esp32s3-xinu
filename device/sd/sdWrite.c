/**
 * @file sdWrite.c
 *
 * SDMMC (SDHOST) microSD driver for the ESP32-S3: block write.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sd.h>

#include "sdhost.h"

/** Write \p len bytes; \p len must be a multiple of the block size. */
devcall sdWrite(device *devptr, const void *buf, uint len)
{
    uint32_t blocks;
    uint32_t i;
    const uint8_t *src = (const uint8_t *)buf;

    (void)devptr;

    if (!sd_present || NULL == buf || 0 == len || (len % SD_BLOCK_SIZE) != 0)
    {
        return SYSERR;
    }

    blocks = len / SD_BLOCK_SIZE;
    lock(sd_lock);
    for (i = 0; i < blocks; i++)
    {
        memcpy(sd_bounce, src, SD_BLOCK_SIZE);
        if (!sdOneBlock(SD_CMD24_WRITE_SINGLE, sd_lba + i, TRUE))
        {
            unlock(sd_lock);
            return SYSERR;
        }
        src += SD_BLOCK_SIZE;
    }
    sd_lba += blocks;
    unlock(sd_lock);
    return (devcall)len;
}
