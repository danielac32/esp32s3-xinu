/**
 * @file sdInit.c
 *
 * SDMMC (SDHOST) microSD driver for the ESP32-S3: device creation.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <kernel.h>
#include <monitor.h>
#include <stddef.h>
#include <stdint.h>

#include <sd.h>

#include "sdhost.h"

devcall sdInit(device *devptr)
{
    (void)devptr;

    /* Create the lock first, so that a failed probe still leaves every
     * entry point able to reject work safely. */
    sd_lock = moncreate();
    if (isbadmon(sd_lock))
    {
        return SYSERR;
    }

    sdPadsInit();

    if (!sdControllerInit())
    {
        kprintf("sd: controller reset failed\r\n");
        return SYSERR;
    }

    if (!sdIdentify())
    {
        kprintf("sd: no card found on SDMMC slot 0 (stopped at %s)\r\n",
                sd_fail);
        return SYSERR;
    }

    kprintf("sd: %lu blocks (%lu MB), %s, %u-bit bus, %lu kHz\r\n",
            sd_blocks, (sd_blocks * SD_BLOCK_SIZE) / (1024UL * 1024UL),
            sd_hc ? "SDHC/SDXC" : "SDSC", sd_width, sd_clk_khz);
    return OK;
}
