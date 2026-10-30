/**
 * @file     xsh_sd.c
 *
 * Shell command (sd) reports the microSD card and dumps single blocks of
 * it, which is enough to check the SDMMC driver and to read a boot
 * sector by hand.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <debug.h>
#include <device.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sd.h>
#include <shell.h>

#if NSD

/** Order dependent checksum of a whole block. */
static uint32_t sdChecksum(const uchar *buf)
{
    uint32_t sum = 0x12345678UL ^ SD_BLOCK_SIZE;
    uint i;

    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        sum = (sum << 5) + sum + buf[i];
    }
    return sum;
}

/** Print the usage message. */
static void sdUsage(const char *cmd)
{
    printf("Usage: %s [--help]\n", cmd);
    printf("       %s read <lba>\n", cmd);
    printf("       %s test [<lba>]\n", cmd);
    printf("\n");
    printf("Description:\n");
    printf("\tReports the card found on the SDMMC bus.\n");
    printf("\t'read' dumps the first 64 bytes of one 512 byte block.\n");
    printf("\t'test' writes a block back with its own contents and\n");
    printf("\tchecks the result, so nothing on the card changes.\n");
}

/**
 * @ingroup shell
 *
 * Report the microSD card, or dump one of its blocks.
 *
 * @param nargs number of arguments in args array
 * @param args  array of arguments
 * @return non-zero value on error
 */
shellcmd xsh_sd(int nargs, char *args[])
{
    static uchar blk[SD_BLOCK_SIZE];
    struct sdinfo info;

    if (nargs == 2 && 0 == strcmp(args[1], "--help"))
    {
        sdUsage(args[0]);
        return SHELL_OK;
    }

    /* Make sure the card is usable.  It may already be open. */
    if (SYSERR == open(SD0))
    {
        fprintf(stderr, "%s: no card in the slot\n", args[0]);
        return SHELL_ERROR;
    }

    if (nargs == 1)
    {
        if (SYSERR == control(SD0, SD_CTRL_INFO, (long)&info, 0))
        {
            fprintf(stderr, "%s: card is not available\n", args[0]);
            return SHELL_ERROR;
        }

        printf("Card:     %s\n", info.present ? "present" : "missing");
        printf("Capacity: %lu blocks (%lu MB)\n", (unsigned long)info.blocks,
               (unsigned long)info.blocks / 2048UL);
        printf("Type:     %s\n", info.highcap ? "SDHC/SDXC" : "SDSC");
        printf("Bus:      %u-bit at %lu kHz\n", info.bus_width,
               (unsigned long)info.card_clk_khz);
        printf("RCA:      0x%04x\n", info.rca);
        return SHELL_OK;
    }

    if (0 == strcmp(args[1], "read"))
    {
        ulong lba;

        if (nargs < 3 || 1 != sscanf(args[2], "%lu", &lba))
        {
            fprintf(stderr, "%s: read needs a block number\n", args[0]);
            sdUsage(args[0]);
            return SHELL_ERROR;
        }

        if (SYSERR == control(SD0, SD_CTRL_SEEK, (long)lba, 0)
            || SYSERR == read(SD0, blk, SD_BLOCK_SIZE))
        {
            fprintf(stderr, "%s: read of block %lu failed\n", args[0], lba);
            return SHELL_ERROR;
        }

        hexdump(blk, 64, TRUE);
        return SHELL_OK;
    }

    if (0 == strcmp(args[1], "test"))
    {
        ulong lba;
        uint32_t before;

        if (nargs > 2)
        {
            if (1 != sscanf(args[2], "%lu", &lba))
            {
                fprintf(stderr, "%s: bad block number '%s'\n", args[0],
                        args[2]);
                return SHELL_ERROR;
            }
        }
        else
        {
            /* Nothing given: use the last block of the card, which lies
             * past the data area of a freshly formatted one.  Only its
             * own contents are written, so the card is left as it was
             * even when the block does belong to a file. */
            if (SYSERR == control(SD0, SD_CTRL_INFO, (long)&info, 0)
                || 0 == info.blocks)
            {
                fprintf(stderr, "%s: card is not available\n", args[0]);
                return SHELL_ERROR;
            }
            lba = info.blocks - 1;
        }

        if (SYSERR == control(SD0, SD_CTRL_SEEK, (long)lba, 0)
            || SYSERR == read(SD0, blk, SD_BLOCK_SIZE))
        {
            fprintf(stderr, "%s: read of block %lu failed\n", args[0], lba);
            return SHELL_ERROR;
        }
        before = sdChecksum(blk);

        if (SYSERR == control(SD0, SD_CTRL_SEEK, (long)lba, 0)
            || SYSERR == write(SD0, blk, SD_BLOCK_SIZE))
        {
            fprintf(stderr, "%s: write of block %lu failed\n", args[0], lba);
            return SHELL_ERROR;
        }

        if (SYSERR == control(SD0, SD_CTRL_SEEK, (long)lba, 0)
            || SYSERR == read(SD0, blk, SD_BLOCK_SIZE))
        {
            fprintf(stderr, "%s: re-read of block %lu failed\n", args[0],
                    lba);
            return SHELL_ERROR;
        }

        if (sdChecksum(blk) != before)
        {
            fprintf(stderr, "%s: block %lu did not come back unchanged\n",
                    args[0], lba);
            return SHELL_ERROR;
        }

        printf("Block %lu written and read back unchanged.\n", lba);
        return SHELL_OK;
    }

    fprintf(stderr, "%s: unknown option '%s'\n", args[0], args[1]);
    sdUsage(args[0]);
    return SHELL_ERROR;
}

#endif /* NSD */
