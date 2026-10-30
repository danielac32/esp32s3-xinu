/**
 * @file sdControl.c
 *
 * SDMMC (SDHOST) microSD driver for the ESP32-S3: control.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sd.h>

#include "sdhost.h"

devcall sdControl(device *devptr, int func, long arg1, long arg2)
{
    (void)devptr;
    (void)arg2;

    switch (func)
    {
    case SD_CTRL_PRESENT:
        return sd_present ? 1 : 0;

    case SD_CTRL_SEEK:
        if (arg1 < 0)
        {
            return SYSERR;
        }
        sd_lba = (uint32_t)arg1;
        return OK;

    case SD_CTRL_BLOCKS:
        return (devcall)sd_blocks;

    case SD_CTRL_HIGHCAP:
        return sd_hc ? 1 : 0;

    case SD_CTRL_INFO:
    {
        struct sdinfo *info = (struct sdinfo *)arg1;

        if (NULL == info)
        {
            return SYSERR;
        }
        bzero(info, sizeof(*info));
        info->blocks = sd_blocks;
        info->card_clk_khz = sd_clk_khz;
        info->rca = (uint16_t)sd_rca;
        info->present = sd_present ? 1 : 0;
        info->highcap = sd_hc ? 1 : 0;
        info->bus_width = sd_width;
        return OK;
    }

    case SD_CTRL_RESET:
        if (!sd_present)
        {
            return SYSERR;
        }
        lock(sd_lock);
        sd_present = FALSE;
        {
            bool ok = sdIdentify();
            unlock(sd_lock);
            return ok ? OK : SYSERR;
        }

    default:
        return SYSERR;
    }
}
