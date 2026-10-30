/**
 * @file sdCard.c
 *
 * Card side of the ESP32-S3 SDMMC driver: reading the CSD register and
 * the CMD0 ... ACMD6 identification sequence.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <clock.h>
#include <stddef.h>
#include <stdint.h>
#include <xtensa.h>

#include <sd.h>

#include "sdhost.h"

/* Record the stage a failed probe stopped at, so the boot message says
 * which part of the sequence the card stopped answering. */
#define SD_ID_FAIL(stage)   do { sd_fail = (stage); return FALSE; } while (0)

/* ------------------------------------------------------------------ */
/* Card identification                                                */
/* ------------------------------------------------------------------ */

/** Decode CSD to work out how many 512 byte blocks the card holds. */
static bool sdDecodeCsd(void)
{
    uint32_t c1 = SD_REG(SDHOST_RESP(1));
    uint32_t c2 = SD_REG(SDHOST_RESP(2));
    uint32_t c3 = SD_REG(SDHOST_RESP(3));

    if (((c3 >> 30) & 0x3U) == 1U)
    {
        /* CSD version 2.0: (C_SIZE + 1) * 512 KiB, in 512 byte blocks. */
        uint32_t csize = ((c2 & 0x3FU) << 16) | ((c1 >> 16) & 0xFFFFU);

        sd_blocks = (csize + 1U) * 1024U;
    }
    else
    {
        /* CSD version 1.0: (C_SIZE+1) * 2^(C_SIZE_MULT+2) * 2^READ_BL_LEN
         * bytes, converted to 512 byte blocks:
         *   (C_SIZE+1) << (C_SIZE_MULT + READ_BL_LEN - 7) */
        uint32_t csize = ((c2 & 0x3FFU) << 2) | ((c1 >> 30) & 0x3U);
        uint32_t mult = (c1 >> 15) & 0x7U;
        uint32_t blen = (c2 >> 16) & 0xFU;
        uint32_t shift = mult + blen;

        if (shift < 7)
        {
            return FALSE;
        }
        sd_blocks = (csize + 1U) << (shift - 7U);
    }
    return TRUE;
}

/**
 * Identify the card: pads, power, clock, then the CMD0 ... ACMD6 dance.
 *
 * Each step that gives up records \p sd_fail first, so the boot message
 * says which part of the sequence the card stopped answering.
 */
#define SD_ID_FAIL(stage)   do { sd_fail = (stage); return FALSE; } while (0)

/** The full identification dance, from power up to a selected card. */
bool sdIdentify(void)
{
    uint32_t r;
    uint32_t t0;
    bool v2 = FALSE;        /**< card understands CMD8 (version 2.0) */

    sd_present = FALSE;
    sd_hc = FALSE;
    sd_rca = 0;
    sd_blocks = 0;
    sd_width = 1;

    if (!sdSetClock(SD_HOST_DIV_PROBE, SD_CARD_DIV_PROBE))
    {
        SD_ID_FAIL("probe clock");
    }
    /* Let the card see at least 74 clocks before the first command. */
    udelay(5000);

    /* CMD0: reset every card on the bus to the idle state. */
    if (!sdSendCmd(SD_CMD0_GO_IDLE, 0, 0, FALSE, NULL))
    {
        SD_ID_FAIL("CMD0");
    }
    udelay(2000);

    /* CMD8: voltage and pattern check.  It only exists on card version
     * 2, so its failure just means "version 1 card".  Version 2 cards
     * are the ones that understand ACMD6 (four data lines). */
    if (sdSendCmd(SD_CMD8_SEND_IF_COND, 0x1AA, 1, FALSE, &r)
        && ((r & 0xFFFU) == 0x1AAU))
    {
        v2 = TRUE;
    }

    /* ACMD41 until the card leaves its power-up busy state. */
    t0 = sdNow();
    for (;;)
    {
        uint32_t arg = (v2 ? 0x40FF8000UL : 0x00FF8000UL);

        if (!sdAppCmd(SD_ACMD41_SEND_OP_COND, arg, 1, FALSE, &r))
        {
            SD_ID_FAIL("ACMD41");
        }
        if (r & (1UL << 31))
        {
            break;
        }
        if (sdTimedOut(t0, 2000))
        {
            SD_ID_FAIL("ACMD41 busy");
        }
        udelay(10000);
    }
    sd_hc = (r & (1UL << 30)) != 0;

    /* CMD2 asks for the CID, CMD3 hands back the relative address. */
    if (!sdSendCmd(SD_CMD2_ALL_SEND_CID, 0, 2, FALSE, NULL))
    {
        SD_ID_FAIL("CMD2");
    }
    if (!sdSendCmdR1(SD_CMD3_SEND_RCA, 0, FALSE, &r))
    {
        SD_ID_FAIL("CMD3");
    }
    sd_rca = (r >> 16) & 0xFFFFU;
    if (0 == sd_rca)
    {
        SD_ID_FAIL("CMD3 rca");
    }

    /* While still in the identification state, read the CSD for the
     * capacity.  This has to happen before CMD7 selects the card. */
    if (!sdSendCmd(SD_CMD9_SEND_CSD, sd_rca << 16, 2, FALSE, NULL))
    {
        SD_ID_FAIL("CMD9");
    }
    if (!sdDecodeCsd())
    {
        SD_ID_FAIL("CSD");
    }

    /* CMD7 selects the card, moving it to the transfer state. */
    if (!sdSendCmdR1(SD_CMD7_SELECT, sd_rca << 16, FALSE, NULL))
    {
        SD_ID_FAIL("CMD7");
    }

    /* Byte addressed cards take a block length of their own.  That can
     * only be done from the stand-by or transfer state, so it happens
     * here rather than during power up. */
    if (!sd_hc)
    {
        if (!sdSendCmdR1(SD_CMD16_SET_BLOCKLEN, SD_BLOCK_SIZE, FALSE, NULL))
        {
            SD_ID_FAIL("CMD16");
        }
    }

    /* ACMD6 only means something on version 2 cards. */
    if (v2)
    {
        if (!sdAppCmd(SD_ACMD6_SET_BUS_WIDTH, 2, 1, TRUE, NULL))
        {
            SD_ID_FAIL("ACMD6");
        }
        /* The card is on four data lines now, so DAT3 finally joins the
         * bus (it was held high as a GPIO until this point). */
        xt_gpio_matrix_out(SD_PIN_D3, SD_SIG_CDATA3, GPIO_PULL_UP);
        SD_REG(SDHOST_CTYPE) = 1;       /* 4 bit bus for card 0 */
        sd_width = 4;
    }

    /* Up to the working clock now that the card is out of its slow
     * identification mode. */
    if (!sdSetClock(SD_HOST_DIV_WORK, SD_CARD_DIV_WORK))
    {
        SD_ID_FAIL("work clock");
    }

    sd_lba = 0;
    sd_present = TRUE;
    return TRUE;
}
