/**
 * @file sdhost.h
 *
 * Register map, board wiring and shared state of the ESP32-S3 SDMMC
 * (SDHOST) driver.  The card's public interface is in <sd.h>; this
 * header is for the driver's own source files.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _SDHOST_H_
#define _SDHOST_H_

#include <clock.h>
#include <monitor.h>
#include <platform.h>
#include <stddef.h>
#include <stdint.h>
#include <xtensa.h>

#include <sd.h>

/* ------------------------------------------------------------------ */
/* Board wiring                                                        */
/* ------------------------------------------------------------------ */
/* The socket is wired to the SDMMC host in 4-bit mode.  SDHOST has no
 * IO_MUX function of its own on this chip, so every line is routed
 * through the GPIO matrix.
 */
#define SD_PIN_CLK      36
#define SD_PIN_CMD      35
#define SD_PIN_D0       37
#define SD_PIN_D1       38
#define SD_PIN_D2       33
#define SD_PIN_D3       34

/* GPIO-matrix signal indices of the slot 0 group (set "1") */
#define SD_SIG_CCLK     172
#define SD_SIG_CCMD     178
#define SD_SIG_CDATA0   180
#define SD_SIG_CDATA1   181
#define SD_SIG_CDATA2   182
#define SD_SIG_CDATA3   183

/* The SDMMC block is clocked from the 40 MHz crystal, not from the
 * 160 MHz PLL: this port leaves the SoC clock tree alone, so the PLL is
 * not running and a divider off it would clock nothing at all.
 *
 *   f_card = 40 MHz / host_div / (2 * card_div), card_div 0 = bypass
 */
#define SD_HOST_DIV_PROBE   10      /**< 4 MHz source -> 400 kHz probe */
#define SD_CARD_DIV_PROBE   5
#define SD_HOST_DIV_WORK    10      /**< 4 MHz source -> 2 MHz card    */
#define SD_CARD_DIV_WORK    1

/* SDHOST register map                                                */
/* ------------------------------------------------------------------ */
#define SDHOST_CTRL        0x000
#define SDHOST_PWREN       0x004
#define SDHOST_CLKDIV      0x008
#define SDHOST_CLKSRC      0x00C
#define SDHOST_CLKENA      0x010
#define SDHOST_TMOUT       0x014
#define SDHOST_CTYPE       0x018
#define SDHOST_BLKSIZ      0x01C
#define SDHOST_BYTCNT      0x020
#define SDHOST_INTMASK     0x024
#define SDHOST_CMDARG      0x028
#define SDHOST_CMD         0x02C
#define SDHOST_RESP(n)     (0x030 + (4 * (n)))
#define SDHOST_MINTSTS     0x040
#define SDHOST_RINTSTS     0x044
#define SDHOST_STATUS      0x048
#define SDHOST_FIFOTH      0x04C
#define SDHOST_CDETECT     0x050
#define SDHOST_WRTPRT      0x054
#define SDHOST_RST_N       0x078
#define SDHOST_BMOD        0x080
#define SDHOST_IDSTS       0x08C
#define SDHOST_BUFFIFO     0x200
/* Phase delays, clock dividers and the clock source live far above the
 * rest of the block. */
#define SDHOST_CLKEDGE     0x800

#define SD_REG(off)     REG32(SDHOST_BASE + (off))

/* CTRL */
#define SD_CTRL_CONTROLLER_RESET   (1U << 0)
#define SD_CTRL_FIFO_RESET         (1U << 1)
#define SD_CTRL_DMA_RESET          (1U << 2)
#define SD_CTRL_INT_ENABLE         (1U << 4)
#define SD_CTRL_DMA_ENABLE         (1U << 5)
#define SD_CTRL_USE_INTERNAL_DMA   (1U << 25)

/* CLKENA */
#define SD_CLKENA_CARD0            (1U << 0)
#define SD_CLKENA_LOW_POWER0       (1U << 16)

/* CMD */
#define SD_CMD_INDEX_MASK          0x3FU
#define SD_CMD_RESP_EXPECT         (1U << 6)
#define SD_CMD_RESP_LONG           (1U << 7)
#define SD_CMD_RESP_CRC            (1U << 8)
#define SD_CMD_DATA_EXPECTED       (1U << 9)
#define SD_CMD_RW                  (1U << 10)   /* 1 = write to card  */
#define SD_CMD_STREAM              (1U << 11)
#define SD_CMD_SEND_AUTO_STOP      (1U << 12)
#define SD_CMD_WAIT_COMPLETE       (1U << 13)
#define SD_CMD_STOP_ABORT          (1U << 14)
#define SD_CMD_SEND_INIT           (1U << 15)
#define SD_CMD_UPDATE_CLK          (1U << 21)
#define SD_CMD_USE_HOLD            (1U << 29)
#define SD_CMD_START               (1U << 31)

/* STATUS */
#define SD_STATUS_FIFO_FULL        (1U << 3)
#define SD_STATUS_FIFO_COUNT(s)    (((s) >> 17) & 0x1FFFU)

/* RINTSTS bits, same order as INTMASK */
#define SD_IRQ_CD                  (1U << 0)
#define SD_IRQ_RESP_ERR            (1U << 1)
#define SD_IRQ_CMD_DONE            (1U << 2)
#define SD_IRQ_DATA_OVER           (1U << 3)
#define SD_IRQ_TXDR                (1U << 4)
#define SD_IRQ_RXDR                (1U << 5)
#define SD_IRQ_RCRC                (1U << 6)
#define SD_IRQ_DCRC                (1U << 7)
#define SD_IRQ_RTO                 (1U << 8)
#define SD_IRQ_DRTO                (1U << 9)
#define SD_IRQ_HTO                 (1U << 10)
#define SD_IRQ_FRUN                (1U << 11)
#define SD_IRQ_HLE                 (1U << 12)
#define SD_IRQ_SBE                 (1U << 13)
#define SD_IRQ_ACD                 (1U << 14)
#define SD_IRQ_EBE                 (1U << 15)

#define SD_IRQ_ALL                 0xFFFFFFFFUL

/* Errors that end a command phase. */
#define SD_CMD_ERR_MASK \
    (SD_IRQ_RTO | SD_IRQ_RCRC | SD_IRQ_HLE | SD_IRQ_RESP_ERR)

/* Errors that end a data phase. */
#define SD_DATA_ERR_MASK \
    (SD_IRQ_EBE | SD_IRQ_SBE | SD_IRQ_DCRC | SD_IRQ_DRTO | SD_IRQ_HTO | \
     SD_IRQ_FRUN | SD_IRQ_HLE)

/* CLK_EDGE_SEL */
#define SD_CLKEDGE_DRV_POS         0
#define SD_CLKEDGE_SAM_POS         3
#define SD_CLKEDGE_SLF_POS         6
#define SD_CLKEDGE_H_POS           9
#define SD_CLKEDGE_L_POS           13
#define SD_CLKEDGE_N_POS           17
#define SD_CLKEDGE_PLL160M         (1U << 23)

/* ------------------------------------------------------------------ */

/* SD protocol                                                        */
/* ------------------------------------------------------------------ */
#define SD_CMD0_GO_IDLE            0
#define SD_CMD2_ALL_SEND_CID       2
#define SD_CMD3_SEND_RCA           3
#define SD_CMD6_SWITCH             6
#define SD_CMD7_SELECT             7
#define SD_CMD8_SEND_IF_COND       8
#define SD_CMD9_SEND_CSD           9
#define SD_CMD13_SEND_STATUS       13
#define SD_CMD16_SET_BLOCKLEN      16
#define SD_CMD17_READ_SINGLE       17
#define SD_CMD24_WRITE_SINGLE      24
#define SD_CMD55_APP_CMD           55
#define SD_CMD58_READ_OCR          58
#define SD_ACMD6_SET_BUS_WIDTH     6
#define SD_ACMD41_SEND_OP_COND     41

/* R1 error bits and card states. */
#define SD_R1_ERR_MASK             0xFEF80000UL
#define SD_R1_READY_FOR_DATA       (1UL << 8)
#define SD_R1_STATE(r)             (((r) >> 9) & 0xFU)
#define SD_R1_STATE_TRAN           4U

/* Timeouts, in milliseconds. */
#define SD_TO_CMD_MS               250
#define SD_TO_DATA_MS              2000
#define SD_TO_BUSY_MS              2000
#define SD_TO_RESET_MS             200

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Driver state (defined in sdHost.c)                                  */
/* ------------------------------------------------------------------ */
extern monitor sd_lock;
extern bool sd_present;
extern bool sd_hc;                  /* SDHC/SDXC (block addressed)     */
extern uint32_t sd_rca;
extern uint32_t sd_blocks;
extern uint32_t sd_lba;             /* cursor for sdRead()/sdWrite()   */
extern uint32_t sd_clk_khz;
extern uint8_t sd_width;
extern const char *sd_fail;         /* stage a failed probe stopped at */

/** Staging buffer: the FIFO is word sized, callers are not aligned. */
extern uint32_t sd_bounce[SD_BLOCK_SIZE / sizeof(uint32_t)];

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static inline uint32_t sdNow(void)
{
    return (uint32_t)clkcount();
}

/** True once \p ms milliseconds have passed since \p start. */
static inline bool sdTimedOut(uint32_t start, uint ms)
{
    uint32_t cycles = ms * (platform.clkfreq / 1000UL);

    return (uint32_t)(sdNow() - start) > cycles;
}

/* ------------------------------------------------------------------ */
/* Hardware layer (sdHost.c)                                           */
/* ------------------------------------------------------------------ */
void sdPadsInit(void);
bool sdControllerInit(void);
bool sdSetClock(uint host_div, uint card_div);
bool sdSendCmd(uint idx, uint32_t arg, uint resp, bool crc, uint32_t *r0);
bool sdSendCmdR1(uint idx, uint32_t arg, bool ignore_errors, uint32_t *r0);
bool sdAppCmd(uint idx, uint32_t arg, uint resp, bool crc, uint32_t *r0);
bool sdOneBlock(uint cmd, uint32_t lba, bool write);

/** Card identification (sdCard.c). */
bool sdIdentify(void);

#endif                          /* _SDHOST_H_ */
