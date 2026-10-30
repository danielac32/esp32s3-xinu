/**
 * @file sdHost.c
 *
 * SDMMC (SDHOST) hardware layer for the ESP32-S3: pads, clock, command
 * engine and the programmed-I/O block transfer.  Data moves through the
 * controller's FIFO by hand, so the internal DMA engine stays off.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <clock.h>
#include <monitor.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xtensa.h>

#include <sd.h>

#include "sdhost.h"

/* ------------------------------------------------------------------ */
/* Driver state                                                       */
/* ------------------------------------------------------------------ */
monitor sd_lock;
bool sd_present = FALSE;
bool sd_hc = FALSE;                 /* SDHC/SDXC (block addressed)     */
uint32_t sd_rca;
uint32_t sd_blocks;
uint32_t sd_lba;                    /* cursor for sdRead()/sdWrite()   */
uint32_t sd_clk_khz;
uint8_t sd_width = 1;
const char *sd_fail = "start";      /* stage a failed probe stopped at */

/** Staging buffer: the FIFO is word sized, callers are not aligned. */
uint32_t sd_bounce[SD_BLOCK_SIZE / sizeof(uint32_t)];

/** Wait for the controller to finish taking the command register. */
static bool sdWaitCmdTaken(void)
{
    uint32_t t0 = sdNow();

    while (SD_REG(SDHOST_CMD) & SD_CMD_START)
    {
        if (sdTimedOut(t0, SD_TO_CMD_MS))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * Ask the controller to push the clock registers out to the card clock
 * generation unit.  The update command has no card index and no
 * response; the hardware sets HLE on success, which is expected and
 * needs a retry.
 */
static bool sdClockUpdate(void)
{
    uint tries;

    for (tries = 0; tries < 16; tries++)
    {
        uint32_t t0;

        if (!sdWaitCmdTaken())
        {
            return FALSE;
        }

        SD_REG(SDHOST_RINTSTS) = SD_IRQ_ALL;
        SD_REG(SDHOST_CMDARG) = 0;
        SD_REG(SDHOST_CMD) = SD_CMD_UPDATE_CLK | SD_CMD_WAIT_COMPLETE
                             | SD_CMD_USE_HOLD | SD_CMD_START;

        t0 = sdNow();
        for (;;)
        {
            uint32_t raw = SD_REG(SDHOST_RINTSTS);

            if (raw & SD_IRQ_HLE)
            {
                /* Expected for a clock update: retry. */
                SD_REG(SDHOST_RINTSTS) = SD_IRQ_HLE;
                break;
            }
            if (raw & SD_CMD_ERR_MASK)
            {
                SD_REG(SDHOST_RINTSTS) = raw;
                return FALSE;
            }
            if (0 == (SD_REG(SDHOST_CMD) & SD_CMD_START))
            {
                SD_REG(SDHOST_RINTSTS) = SD_IRQ_ALL;
                return TRUE;
            }
            if (sdTimedOut(t0, SD_TO_CMD_MS))
            {
                return FALSE;
            }
        }
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */

/* Pads and controller                                                */
/* ------------------------------------------------------------------ */

/**
 * Bring the six SDMMC lines up on the GPIO matrix.
 *
 * SDHOST has no IO_MUX function of its own on the ESP32-S3, so each pad
 * is switched to the GPIO function and the slot 0 signal group ("1") is
 * routed to it by hand.  CMD and DAT are bidirectional and get a pull-up
 * so that an idle bus reads high; CLK is output only.
 *
 * DAT3 is left out of the bus for now: a card that sees DAT3 low while
 * it is powering up switches to SPI mode and never answers a native SD
 * command, so it is held high as a plain output until the bus is really
 * widened (see sdIdentify()).
 */
void sdPadsInit(void)
{
    xt_gpio_matrix_out(SD_PIN_CLK,   SD_SIG_CCLK,   GPIO_PULL_NONE);
    xt_gpio_matrix_out(SD_PIN_CMD,   SD_SIG_CCMD,   GPIO_PULL_UP);
    xt_gpio_matrix_out(SD_PIN_D0,    SD_SIG_CDATA0, GPIO_PULL_UP);
    xt_gpio_matrix_out(SD_PIN_D1,    SD_SIG_CDATA1, GPIO_PULL_UP);
    xt_gpio_matrix_out(SD_PIN_D2,    SD_SIG_CDATA2, GPIO_PULL_UP);

    xt_gpio_output(SD_PIN_D3, GPIO_PULL_UP);
    xt_gpio_set_level(SD_PIN_D3, 1);
}

/** Program the host (core) and card clock dividers and the source. */
bool sdSetClock(uint host_div, uint card_div)
{
    uint32_t edge;

    /* Stop the card clock before touching the dividers. */
    SD_REG(SDHOST_CLKENA) &= ~SD_CLKENA_CARD0;
    if (!sdClockUpdate())
    {
        return FALSE;
    }

    /* Host divider off the 40 MHz crystal: the period is (L + 1) cycles
     * of the source and the high phase is (H + 1) of them, so an even
     * host_div gives a 50 % duty cycle. */
    edge  = SD_REG(SDHOST_CLKEDGE);
    edge &= ~((0xFU << SD_CLKEDGE_H_POS) | (0xFU << SD_CLKEDGE_L_POS)
              | (0xFU << SD_CLKEDGE_N_POS) | SD_CLKEDGE_PLL160M
              | (7U << SD_CLKEDGE_DRV_POS) | (7U << SD_CLKEDGE_SAM_POS)
              | (7U << SD_CLKEDGE_SLF_POS));
    edge |= ((host_div / 2 - 1) & 0xFU) << SD_CLKEDGE_H_POS;
    edge |= ((host_div - 1) & 0xFU) << SD_CLKEDGE_L_POS;
    edge |= ((host_div - 1) & 0xFU) << SD_CLKEDGE_N_POS;
    edge |= 1U << SD_CLKEDGE_DRV_POS;   /* 90 degree data output phase */
    /* clk_sel stays 0: the crystal, not the (unused) 160 MHz PLL. */
    SD_REG(SDHOST_CLKEDGE) = edge;
    udelay(10);

    /* Card divider: card 0 uses divider 0. */
    SD_REG(SDHOST_CLKSRC) = 0;
    SD_REG(SDHOST_CLKDIV) = card_div & 0xFFU;

    if (!sdClockUpdate())
    {
        return FALSE;
    }

    SD_REG(SDHOST_CLKENA) |= SD_CLKENA_CARD0;
    if (!sdClockUpdate())
    {
        return FALSE;
    }

    SD_REG(SDHOST_CLKENA) |= SD_CLKENA_LOW_POWER0;

    sd_clk_khz = 40000UL / host_div / ((card_div == 0) ? 1UL : card_div * 2UL);
    return TRUE;
}

/** Full controller reset; the reset bits clear themselves. */
static bool sdResetController(void)
{
    uint32_t t0;

    SD_REG(SDHOST_CTRL) = SD_CTRL_CONTROLLER_RESET | SD_CTRL_FIFO_RESET
                          | SD_CTRL_DMA_RESET;

    t0 = sdNow();
    while (SD_REG(SDHOST_CTRL) & (SD_CTRL_CONTROLLER_RESET
                                  | SD_CTRL_FIFO_RESET | SD_CTRL_DMA_RESET))
    {
        if (sdTimedOut(t0, SD_TO_RESET_MS))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * Bring the SDHOST block out of reset and configure it: bus clock,
 * clock source, no interrupts (everything is polled), no DMA, PIO only.
 */
bool sdControllerInit(void)
{
    uint32_t t0;

    /* Bus clock, then a clean reset through the system controller. */
    REG32(SYSTEM_PERIP_CLK_EN1) |= SYS_CLK_EN_SDIO_HOST;
    REG32(SYSTEM_PERIP_RST_EN1) |= SYS_RST_EN_SDIO_HOST;
    udelay(100);
    REG32(SYSTEM_PERIP_RST_EN1) &= ~SYS_RST_EN_SDIO_HOST;
    udelay(100);

    if (!sdResetController())
    {
        return FALSE;
    }

    /* Clear everything and stay quiet: this driver polls RINTSTS. */
    SD_REG(SDHOST_RINTSTS) = SD_IRQ_ALL;
    SD_REG(SDHOST_INTMASK) = 0;
    SD_REG(SDHOST_CTRL) &= ~(SD_CTRL_INT_ENABLE | SD_CTRL_DMA_ENABLE
                             | SD_CTRL_USE_INTERNAL_DMA);
    SD_REG(SDHOST_BMOD) = 0;
    SD_REG(SDHOST_IDSTS) = SD_IRQ_ALL;

    /* Timeouts wide open; the driver enforces its own limits. */
    SD_REG(SDHOST_TMOUT) = 0xFFU | (0x00FFFFFFUL << 8);

    /* Power the card and take it out of reset (RST_N is active low). */
    SD_REG(SDHOST_PWREN) = 1;
    SD_REG(SDHOST_RST_N) = 0x3;
    udelay(5000);

    /* Power-on delay for the card, then 1-bit bus for identification. */
    t0 = sdNow();
    while (!sdTimedOut(t0, 20))
    {
        /* spin */
    }
    SD_REG(SDHOST_CTYPE) = 0;

    return TRUE;
}

/* ------------------------------------------------------------------ */

/* Command engine                                                     */
/* ------------------------------------------------------------------ */

/**
 * Send one command and, if it expects a response, wait for it.
 *
 * \p resp is 0 (none), 1 (short, 48 bit) or 2 (long, 136 bit).
 * \p crc asks the controller to verify the response CRC, which only
 * makes sense for the commands whose responses actually carry one.
 */
bool sdSendCmd(uint idx, uint32_t arg, uint resp, bool crc,
                      uint32_t *r0)
{
    uint32_t cmd;
    uint32_t t0;

    if (!sdWaitCmdTaken())
    {
        return FALSE;
    }

    SD_REG(SDHOST_RINTSTS) = SD_IRQ_ALL;
    SD_REG(SDHOST_CMDARG) = arg;

    cmd = (uint32_t)(idx & SD_CMD_INDEX_MASK)
          | SD_CMD_WAIT_COMPLETE | SD_CMD_USE_HOLD;
    if (resp != 0)
    {
        cmd |= SD_CMD_RESP_EXPECT;
    }
    if (resp == 2)
    {
        cmd |= SD_CMD_RESP_LONG;
    }
    if (crc && resp != 0)
    {
        cmd |= SD_CMD_RESP_CRC;
    }
    SD_REG(SDHOST_CMD) = cmd | SD_CMD_START;

    if (!sdWaitCmdTaken())
    {
        return FALSE;
    }

    t0 = sdNow();
    for (;;)
    {
        uint32_t raw = SD_REG(SDHOST_RINTSTS);

        if (raw & SD_CMD_ERR_MASK)
        {
            SD_REG(SDHOST_RINTSTS) = raw;
            return FALSE;
        }
        if (raw & SD_IRQ_CMD_DONE)
        {
            SD_REG(SDHOST_RINTSTS) = raw;
            if (r0 != NULL && resp != 0)
            {
                *r0 = SD_REG(SDHOST_RESP(0));
            }
            return TRUE;
        }
        if (sdTimedOut(t0, SD_TO_CMD_MS))
        {
            SD_REG(SDHOST_RINTSTS) = SD_IRQ_ALL;
            return FALSE;
        }
    }
}

/** Command with a short response, checking the R1 error bits. */
bool sdSendCmdR1(uint idx, uint32_t arg, bool ignore_errors,
                        uint32_t *r0)
{
    uint32_t r;

    if (!sdSendCmd(idx, arg, 1, TRUE, &r))
    {
        return FALSE;
    }
    if ((r & SD_R1_ERR_MASK) != 0 && !ignore_errors)
    {
        return FALSE;
    }
    if (r0 != NULL)
    {
        *r0 = r;
    }
    return TRUE;
}

/** CMD55 (<rca>) followed by an application command. */
bool sdAppCmd(uint idx, uint32_t arg, uint resp, bool crc,
                     uint32_t *r0)
{
    uint32_t r;

    if (!sdSendCmd(SD_CMD55_APP_CMD, sd_rca << 16, 1, TRUE, &r))
    {
        return FALSE;
    }
    return sdSendCmd(idx, arg, resp, crc, r0);
}

/* ------------------------------------------------------------------ */

/* Data phase                                                         */
/* ------------------------------------------------------------------ */

/** Reset the data FIFO and wait for the reset to be taken. */
static bool sdFlushFifo(void)
{
    uint32_t t0;

    SD_REG(SDHOST_CTRL) |= SD_CTRL_FIFO_RESET;
    t0 = sdNow();
    while (SD_REG(SDHOST_CTRL) & SD_CTRL_FIFO_RESET)
    {
        if (sdTimedOut(t0, SD_TO_RESET_MS))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/** Pull one block out of the FIFO.  \p len must be a multiple of 4. */
static bool sdFifoRead(void *buf, uint len)
{
    uint32_t *dst = (uint32_t *)buf;
    uint32_t words = len / 4;
    uint32_t t0 = sdNow();

    while (words > 0)
    {
        uint32_t raw = SD_REG(SDHOST_RINTSTS);

        if (raw & SD_DATA_ERR_MASK)
        {
            SD_REG(SDHOST_RINTSTS) = raw;
            return FALSE;
        }

        while (words > 0 && SD_STATUS_FIFO_COUNT(SD_REG(SDHOST_STATUS)) > 0)
        {
            *dst++ = SD_REG(SDHOST_BUFFIFO);
            words--;
        }

        if (raw & SD_IRQ_DATA_OVER)
        {
            SD_REG(SDHOST_RINTSTS) = raw;
            while (words > 0
                   && SD_STATUS_FIFO_COUNT(SD_REG(SDHOST_STATUS)) > 0)
            {
                *dst++ = SD_REG(SDHOST_BUFFIFO);
                words--;
            }
            return (0 == words) ? TRUE : FALSE;
        }

        if (sdTimedOut(t0, SD_TO_DATA_MS))
        {
            SD_REG(SDHOST_RINTSTS) = SD_IRQ_ALL;
            return FALSE;
        }
    }
    return TRUE;
}

/** Push one block into the FIFO.  \p len must be a multiple of 4. */
static bool sdFifoWrite(const void *buf, uint len)
{
    const uint32_t *src = (const uint32_t *)buf;
    uint32_t words = len / 4;
    uint32_t t0 = sdNow();

    while (words > 0)
    {
        uint32_t raw = SD_REG(SDHOST_RINTSTS);

        if (raw & SD_DATA_ERR_MASK)
        {
            SD_REG(SDHOST_RINTSTS) = raw;
            return FALSE;
        }

        while (words > 0 && !(SD_REG(SDHOST_STATUS) & SD_STATUS_FIFO_FULL))
        {
            SD_REG(SDHOST_BUFFIFO) = *src++;
            words--;
        }

        if ((raw & SD_IRQ_DATA_OVER) && 0 == words)
        {
            SD_REG(SDHOST_RINTSTS) = raw;
            return TRUE;
        }

        if (sdTimedOut(t0, SD_TO_DATA_MS))
        {
            SD_REG(SDHOST_RINTSTS) = SD_IRQ_ALL;
            return FALSE;
        }
    }
    return TRUE;
}

/** Poll CMD13 until the card reports itself ready in the TRAN state. */
static bool sdWaitReady(uint ms)
{
    uint32_t t0 = sdNow();

    while (!sdTimedOut(t0, ms))
    {
        uint32_t r;

        if (!sdSendCmd(13, sd_rca << 16, 1, TRUE, &r))
        {
            return FALSE;
        }
        if ((r & SD_R1_ERR_MASK) == 0 && (r & SD_R1_READY_FOR_DATA)
            && SD_R1_STATE(r) == SD_R1_STATE_TRAN)
        {
            return TRUE;
        }
        udelay(1000);
    }
    return FALSE;
}

/**
 * One CMD17/CMD24 block transfer through sd_bounce.  The caller must
 * hold sd_lock, and reads land in sd_bounce for the caller to copy out.
 *
 * The card is addressed by block number on high capacity cards and by
 * byte offset on the older byte addressed ones.
 */
bool sdOneBlock(uint cmd, uint32_t lba, bool write)
{
    uint32_t cmdval;
    uint32_t t0;
    bool ok;

    /* Programmed I/O: the DMA engine must stay off, or the controller
     * runs off into DMA and the FIFO never fills. */
    SD_REG(SDHOST_CTRL) &= ~(SD_CTRL_DMA_ENABLE | SD_CTRL_USE_INTERNAL_DMA);
    SD_REG(SDHOST_BMOD) = 0;
    SD_REG(SDHOST_INTMASK) = 0;

    SD_REG(SDHOST_BLKSIZ) = SD_BLOCK_SIZE;
    SD_REG(SDHOST_BYTCNT) = SD_BLOCK_SIZE;
    if (!sdFlushFifo())
    {
        return FALSE;
    }

    if (!sdWaitCmdTaken())
    {
        return FALSE;
    }
    SD_REG(SDHOST_RINTSTS) = SD_IRQ_ALL;
    SD_REG(SDHOST_CMDARG) = sd_hc ? lba : (lba * SD_BLOCK_SIZE);

    cmdval = (uint32_t)(cmd & SD_CMD_INDEX_MASK)
             | SD_CMD_RESP_EXPECT | SD_CMD_RESP_CRC | SD_CMD_DATA_EXPECTED
             | SD_CMD_WAIT_COMPLETE | SD_CMD_USE_HOLD
             | (write ? SD_CMD_RW : 0U);
    SD_REG(SDHOST_CMD) = cmdval | SD_CMD_START;

    if (!sdWaitCmdTaken())
    {
        return FALSE;
    }

    /* The command completes first; the data then moves by itself. */
    t0 = sdNow();
    for (;;)
    {
        uint32_t raw = SD_REG(SDHOST_RINTSTS);

        if (raw & SD_CMD_ERR_MASK)
        {
            SD_REG(SDHOST_RINTSTS) = raw;
            return FALSE;
        }
        if (raw & SD_IRQ_CMD_DONE)
        {
            /* Keep DATA_OVER if it already arrived. */
            SD_REG(SDHOST_RINTSTS) = raw & ~SD_IRQ_DATA_OVER;
            break;
        }
        if (sdTimedOut(t0, SD_TO_CMD_MS))
        {
            SD_REG(SDHOST_RINTSTS) = SD_IRQ_ALL;
            return FALSE;
        }
    }

    ok = write ? sdFifoWrite(sd_bounce, SD_BLOCK_SIZE)
               : sdFifoRead(sd_bounce, SD_BLOCK_SIZE);
    if (!ok)
    {
        return FALSE;
    }

    if (write)
    {
        /* The card holds DAT0 low while it programs the block. */
        ok = sdWaitReady(SD_TO_BUSY_MS);
        if (!ok)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/* ------------------------------------------------------------------ */

/* Public block interface                                             */
/* ------------------------------------------------------------------ */

int sd_read_blocks(uint32_t lba, void *buf, uint32_t count)
{
    uint8_t *dst = (uint8_t *)buf;

    if (!sd_present || NULL == buf || count > SD_MAX_BLOCKS)
    {
        return SYSERR;
    }

    lock(sd_lock);
    for (; count > 0; count--)
    {
        if (!sdOneBlock(SD_CMD17_READ_SINGLE, lba, FALSE))
        {
            unlock(sd_lock);
            return SYSERR;
        }
        memcpy(dst, sd_bounce, SD_BLOCK_SIZE);
        dst += SD_BLOCK_SIZE;
        lba++;
    }
    unlock(sd_lock);
    return OK;
}

int sd_write_blocks(uint32_t lba, const void *buf, uint32_t count)
{
    const uint8_t *src = (const uint8_t *)buf;

    if (!sd_present || NULL == buf || count > SD_MAX_BLOCKS)
    {
        return SYSERR;
    }

    lock(sd_lock);
    for (; count > 0; count--)
    {
        memcpy(sd_bounce, src, SD_BLOCK_SIZE);
        if (!sdOneBlock(SD_CMD24_WRITE_SINGLE, lba, TRUE))
        {
            unlock(sd_lock);
            return SYSERR;
        }
        src += SD_BLOCK_SIZE;
        lba++;
    }
    unlock(sd_lock);
    return OK;
}

uint32_t sd_capacity_blocks(void)
{
    return sd_present ? sd_blocks : 0;
}

int sd_is_up(void)
{
    return sd_present ? 1 : 0;
}
