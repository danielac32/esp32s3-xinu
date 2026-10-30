/**
 * @file psramInit.c
 *
 * ESP32-S3 quad PSRAM bring-up.
 *
 * The ROM bootloader leaves the in-package PSRAM powered down and in its
 * default SPI (single-line) mode; nothing in this image has ever touched the
 * external-memory windows.  This file talks to the die over the SPI1
 * controller (the same controller the ROM used to read the flash), puts it
 * into quad (QPI) mode, teaches the SPI0 cache controller which read/write
 * commands to use for external RAM, maps the whole die into the data bus at
 * PSRAM_BASE and switches the data cache on.
 *
 * Everything here writes the SPI_MEM / EXTMEM registers directly.  The ROM's
 * C entry points (esp_rom_spi_cmd_config(), Cache_Dbus_MMU_Set(), ...) were
 * built for the Xtensa windowed ABI and cannot be called from this CALL0
 * kernel, so the handful of register sequences they perform are reproduced
 * here with plain register accesses instead.
 *
 * The function fails soft: it returns SYSERR and leaves psram_present 0 if
 * anything goes wrong, and the rest of the kernel keeps running out of
 * internal SRAM exactly as before.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <xtensa.h>
#include <psram.h>

/* ------------------------------------------------------------------ */
/*  SPI_MEM register map                                              */
/* ------------------------------------------------------------------ */
/*
 * The S3 has two SPI memory controllers 4 KiB apart:
 *
 *      index 0   0x60003000   cache controller (reads/writes PSRAM)
 *      index 1   0x60002000   user controller (used here to talk to the die)
 *
 * REG_SPI_MEM_BASE(i) = DR_REG_SPI0_BASE - i*0x1000, with DR_REG_SPI0_BASE
 * = 0x60003000.
 */
#define REG_SPI_MEM_BASE(i)      (0x60003000UL - ((uint32_t)(i) * 0x1000UL))
#define SPI_MEM(i, off)          REG32(REG_SPI_MEM_BASE(i) + (off))

#define SPI_MEM_CMD(i)           SPI_MEM(i, 0x00)
#define SPI_MEM_ADDR(i)          SPI_MEM(i, 0x04)
#define SPI_MEM_CTRL(i)          SPI_MEM(i, 0x08)
#define SPI_MEM_CLOCK(i)         SPI_MEM(i, 0x14)
#define SPI_MEM_USER(i)          SPI_MEM(i, 0x18)
#define SPI_MEM_USER1(i)         SPI_MEM(i, 0x1C)
#define SPI_MEM_USER2(i)         SPI_MEM(i, 0x20)
#define SPI_MEM_MOSI_DLEN(i)     SPI_MEM(i, 0x24)
#define SPI_MEM_MISO_DLEN(i)     SPI_MEM(i, 0x28)
#define SPI_MEM_MISC(i)          SPI_MEM(i, 0x34)
#define SPI_MEM_CACHE_SCTRL(i)   SPI_MEM(i, 0x40)
#define SPI_MEM_SRAM_DRD_CMD(i)  SPI_MEM(i, 0x48)
#define SPI_MEM_SRAM_DWR_CMD(i)  SPI_MEM(i, 0x4C)
#define SPI_MEM_SRAM_CLK(i)      SPI_MEM(i, 0x50)
#define SPI_MEM_W(i, n)          SPI_MEM(i, 0x58 + ((uint32_t)(n) * 4))
#define SPI_MEM_SMEM_AC(i)       SPI_MEM(i, 0xDC)

/* SPI_MEM_USER bits. */
#define SPI_USR                  (1U << 18)     /* start transaction      */
#define SPI_USR_MOSI             (1U << 27)
#define SPI_USR_MISO             (1U << 28)
#define SPI_USR_DUMMY            (1U << 29)
#define SPI_USR_ADDR             (1U << 30)
#define SPI_USR_COMMAND          (1U << 31)

/* SPI_MEM_CTRL bits. */
#define SPI_CTRL_FCMD_OCT        (1U << 9)
#define SPI_CTRL_FCMD_QUAD       (1U << 8)
#define SPI_CTRL_FCMD_DUAL       (1U << 7)
/* Every selector that can widen a phase onto more than one signal lives in
 * this one register on the S3: the FASTRD-mode aliases (FADDR/FDOUT/FDIN,
 * bits 3..9), the hardware fast-read shapes (FREAD_DUAL/DIO/QUAD/QIO,
 * bits 14/23/20/24) and FADDR's enable, FADDR_FASTRD_MODE (bit 13).  The
 * ROM leaves FREAD_DIO set from the boot image's DIO mode; left that way it
 * also widens the DIN phase of a *user* command, which is what turned every
 * id read into garbage. */
#define SPI_CTRL_LINEWIDTH_M     (0x3F8U)
#define SPI_CTRL_FASTRD_M        (1U << 13)
#define SPI_CTRL_FREAD_M         ((1U << 14) | (1U << 20) | (1U << 23) | (1U << 24))
#define SPI_CTRL_ALL_SINGLE_M    (SPI_CTRL_LINEWIDTH_M | SPI_CTRL_FASTRD_M \
                                  | SPI_CTRL_FREAD_M)

/* SPI_MEM_USER1 fields. */
#define SPI_USER1_ADDR_BITLEN_S  26
#define SPI_USER1_DUMMY_CYC_M    0x3FU

/* SPI_MEM_USER2 fields. */
#define SPI_USER2_CMD_BITLEN_S   28
#define SPI_USER2_CMD_VALUE_M    0xFFFFU

/* SPI_MEM_MOSI_DLEN / MISO_DLEN use a 10-bit length field. */
#define SPI_DLEN_MASK            0x3FFU

/* SPI_MEM_MISC bits. */
#define SPI_MISC_CS0_DIS         (1U << 0)
#define SPI_MISC_CS1_DIS         (1U << 1)

/* SPI_MEM_CACHE_SCTRL fields. */
#define SPI_CACHE_USR_SRAM_DIO   (1U << 1)
#define SPI_CACHE_USR_SRAM_QIO   (1U << 2)
#define SPI_CACHE_USR_RD_DUMMY   (1U << 4)
#define SPI_CACHE_SRAM_USR_RCMD  (1U << 5)
#define SPI_CACHE_SRAM_ADDR_BITLEN_S     14
#define SPI_CACHE_SRAM_ADDR_BITLEN_M     (0x3FU << 14)
#define SPI_CACHE_SRAM_RDUMMY_M          (0x3FU << 6)
#define SPI_CACHE_SRAM_USR_WCMD  (1U << 20)

/* SPI_MEM_SRAM_DRD_CMD / SRAM_DWR_CMD fields. */
#define SPI_SRAM_CMD_BITLEN_S    28
#define SPI_SRAM_CMD_BITLEN_M    (0xFU << 28)
#define SPI_SRAM_CMD_VALUE_M     0xFFFFU

/* SPI_MEM_SMEM_AC fields (external-RAM CS timing). */
#define SPI_SMEM_CS_SETUP        (1U << 0)
#define SPI_SMEM_CS_SETUP_TIME_S 2
#define SPI_SMEM_CS_HOLD         (1U << 1)
#define SPI_SMEM_CS_HOLD_TIME_S  7

/* SPI_MEM_CLOCK / SRAM_CLK fields. */
#define SPI_CLK_EQU_SYSCLK       (1U << 31)
#define SPI_CLKCNT_N_S           16
#define SPI_CLKCNT_H_S           8
#define SPI_CLKCNT_L_S           0
#define SPI_SCLKCNT_N_S          16
#define SPI_SCLKCNT_H_S          8
#define SPI_SCLKCNT_L_S          0

/* ------------------------------------------------------------------ */
/*  EXTMEM (cache / MMU) register map                                 */
/* ------------------------------------------------------------------ */
#define EXTMEM_BASE              0x600C4000UL
#define EXTMEM_DCACHE_CTRL       REG32(EXTMEM_BASE + 0x00)
#define EXTMEM_DCACHE_CTRL1      REG32(EXTMEM_BASE + 0x04)
#define EXTMEM_DCACHE_SYNC_CTRL  REG32(EXTMEM_BASE + 0x28)
#define EXTMEM_DCACHE_SYNC_ADDR  REG32(EXTMEM_BASE + 0x2C)
#define EXTMEM_DCACHE_SYNC_SIZE  REG32(EXTMEM_BASE + 0x30)

#define EXTMEM_DCACHE_ENABLE         (1U << 0)  /* DCACHE_CTRL              */
#define EXTMEM_DCACHE_SHUT_CORE0     (1U << 0)  /* DCACHE_CTRL1             */
#define EXTMEM_DCACHE_INVALIDATE_ENA (1U << 0)  /* DCACHE_SYNC_CTRL         */
#define EXTMEM_DCACHE_SYNC_DONE      (1U << 3)  /* DCACHE_SYNC_CTRL (RO)    */

/* The 512-entry MMU table is shared by the I- and D-buses; the entry for a
 * virtual address is (vaddr & 0x1FFFFFF) >> 16.  A PSRAM entry carries the
 * physical page number in bits 13:0 plus MMU_ACCESS_SPIRAM in bit 15. */
#define MMU_TABLE_BASE           0x600C5000UL
#define MMU_ENTRY_ID(vaddr)      (((vaddr) & 0x1FFFFFFUL) >> 16)
#define MMU_ACCESS_SPIRAM        0x8000UL

/* ------------------------------------------------------------------ */
/*  PSRAM command set (APS6404 / ESP-PSRAM)                           */
/* ------------------------------------------------------------------ */
#define PSRAM_CMD_DEVICE_ID      0x9F
#define PSRAM_CMD_RESET_EN       0x66
#define PSRAM_CMD_RESET          0x99
#define PSRAM_CMD_ENTER_QPI      0x35
#define PSRAM_CMD_EXIT_QPI       0xF5
#define PSRAM_CMD_READ_QUAD      0xEB
#define PSRAM_CMD_WRITE_QUAD     0x38

#define PSRAM_ID_KGD             0x5D      /* known-good-die value in id[15:8] */

/* Quad fast-read needs six dummy cycles; the S3 caches them in PSRAM. */
#define PSRAM_QUAD_READ_DUMMY    6

/* PSRAM CS is on chip select 1 (chip select 0 is the flash). */
#define PSRAM_CS_GPIO            26
#define FLASH_CS_GPIO            29
#define PSRAM_SD2_GPIO           27
#define PSRAM_SD3_GPIO           28

/* Nonzero once the die has been mapped and passes its self-test. */
int psram_present = 0;

/* Usable size of the mapped die, in bytes. */
uint32_t psram_size = 0;

/* Why the last psramInit() attempt ended the way it did. */
int psram_init_status = PSRAM_INIT_NOT_TRIED;

/* Raw id bytes from the last probe, kept for the psram/boot report. */
uint8_t psram_dbg_id[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t psram_dbg_flash[4] = { 0, 0, 0, 0 };
uint8_t psram_dbg_flash2[4] = { 0, 0, 0, 0 };
uint32_t psram_dbg_ctrl = 0;
uint32_t psram_dbg_clk = 0;
uint32_t psram_dbg_spin = 0;
uint32_t psram_dbg_misc = 0;

/* ------------------------------------------------------------------ */
/*  Small helpers                                                     */
/* ------------------------------------------------------------------ */

/** Busy-wait roughly @p us microseconds on the 20 MHz cycle counter. */
static void psram_delay(uint32_t us)
{
    uint32_t start = XT_CCOUNT();
    uint32_t ticks = us * 20U;              /* CCOUNT ticks at 20 MHz        */

    while ((uint32_t)(XT_CCOUNT() - start) < ticks)
    {
        /* spin */
    }
}

/**
 * Put the SPI1 user controller into single-line (0) or quad-command (1)
 * mode.  Only the command phase changes width here: the PSRAM setup
 * commands never carry an address or data phase while in QPI mode.
 */
static void psram_spi_mode(int qpi)
{
    uint32_t ctrl = SPI_MEM_CTRL(1);

    /* Command, address and data all on a single signal (slow read); only
     * the command phase goes quad when we are talking to a die that is
     * already latched into QPI mode. */
    ctrl &= ~SPI_CTRL_ALL_SINGLE_M;
    if (qpi)
    {
        ctrl |= SPI_CTRL_FCMD_QUAD;
    }
    SPI_MEM_CTRL(1) = ctrl;
}

/**
 * One user-mode transaction on SPI1 with PSRAM chip select asserted.
 *
 * @param cmd        Command byte (0xF5 when in quad mode).
 * @param cmd_bits   Command length, in bits (0 to omit the phase).
 * @param addr       Value driven during the address phase.
 * @param addr_bits  Address length, in bits (0 to omit the phase).
 * @param dummy_bits Dummy cycles between address and data (0 for none).
 * @param mosi       Bytes to transmit, or NULL.
 * @param mosi_bits  Transmit length, in bits.
 * @param miso       Receive buffer, or NULL.
 * @param miso_bits  Receive length, in bits.
 */
static int psram_exec(int cs, uint32_t cmd, int cmd_bits,
                      uint32_t addr, int addr_bits,
                      int dummy_bits,
                      const uint8_t *mosi, int mosi_bits,
                      uint8_t *miso, int miso_bits)
{
    uint32_t user = 0, user1 = 0, user2 = 0, misc, misc_orig;
    int words, b;
    uint32_t spin;

    if (cmd_bits > 0)
    {
        user  |= SPI_USR_COMMAND;
        user2 |= ((uint32_t)(cmd_bits - 1) << SPI_USER2_CMD_BITLEN_S)
               | ((cmd) & SPI_USER2_CMD_VALUE_M);
    }

    if (addr_bits > 0)
    {
        user  |= SPI_USR_ADDR;
        user1 |= ((uint32_t)(addr_bits - 1) << SPI_USER1_ADDR_BITLEN_S);
        SPI_MEM_ADDR(1) = addr;
    }

    if (dummy_bits > 0)
    {
        user  |= SPI_USR_DUMMY;
        user1 |= (uint32_t)(dummy_bits - 1) & SPI_USER1_DUMMY_CYC_M;
    }

    if (mosi_bits > 0)
    {
        user |= SPI_USR_MOSI;
        SPI_MEM_MOSI_DLEN(1) = ((uint32_t)mosi_bits - 1) & SPI_DLEN_MASK;
        words = (mosi_bits + 31) / 32;
        for (b = 0; b < words; b++)
        {
            uint32_t w = 0;
            int k;

            for (k = 0; k < 4; k++)
            {
                int idx = b * 4 + k;

                if (idx < (mosi_bits + 7) / 8)
                {
                    w |= (uint32_t)mosi[idx] << (8 * k);
                }
            }
            SPI_MEM_W(1, b) = w;
        }
    }

    if (miso_bits > 0)
    {
        user |= SPI_USR_MISO;
        SPI_MEM_MISO_DLEN(1) = ((uint32_t)miso_bits - 1) & SPI_DLEN_MASK;
    }

    SPI_MEM_USER1(1) = user1;
    SPI_MEM_USER2(1) = user2;
    SPI_MEM_USER(1)  = user;

    /* Select the requested chip select (1 = PSRAM, 0 = flash). */
    misc_orig = SPI_MEM_MISC(1);
    misc = misc_orig;
    if (cs == 0)
    {
        misc &= ~SPI_MISC_CS0_DIS;
        misc |= SPI_MISC_CS1_DIS;
    }
    else
    {
        misc |= SPI_MISC_CS0_DIS;
        misc &= ~SPI_MISC_CS1_DIS;
    }
    SPI_MEM_MISC(1) = misc;

    SPI_MEM_CMD(1) = SPI_USR;
    spin = 1000000U;
    while ((SPI_MEM_CMD(1) & SPI_USR) && spin > 0)
    {
        spin--;                     /* bounded: never wedge the boot */
    }
    psram_dbg_spin = 1000000U - spin;
    psram_dbg_misc = misc;

    /* Restore the chip-select enables we found. */
    SPI_MEM_MISC(1) = misc_orig;

    if (0 == spin)
    {
        return SYSERR;
    }

    if (miso_bits > 0 && miso != NULL)
    {
        words = (miso_bits + 31) / 32;
        for (b = 0; b < words; b++)
        {
            uint32_t w = SPI_MEM_W(1, b);
            int k;

            for (k = 0; k < 4; k++)
            {
                int idx = b * 4 + k;

                if (idx < (miso_bits + 7) / 8)
                {
                    miso[idx] = (uint8_t)(w >> (8 * k));
                }
            }
        }
    }
    return OK;
}

/** Read the 24- or 48-bit device/RAM id (sent in single-line mode). */
static int psram_read_id(int cs, uint8_t *out, int id_bits)
{
    return psram_exec(cs, PSRAM_CMD_DEVICE_ID, 8, 0, 24, 0, NULL, 0, out, id_bits);
}

/** Drive a pad as its IO_MUX peripheral function (function 1 = SPI). */
static void psram_pad_init(uint32_t pin, int is_output)
{
    uint32_t mux = REG32(IO_MUX_GPIO(pin));

    mux &= ~(IO_MUX_MCU_SEL_MASK | IO_MUX_FUN_DRV_MASK | IO_MUX_FUN_WPD |
             IO_MUX_MCU_WPD);
    mux |= IO_MUX_MCU_SEL_FUNC1 | (2U << IO_MUX_FUN_DRV_POS) | IO_MUX_FUN_WPU;

    if (is_output)
    {
        mux |= IO_MUX_MCU_OE;
    }
    else
    {
        mux |= IO_MUX_FUN_IE | IO_MUX_MCU_IE;
    }

    REG32(IO_MUX_GPIO(pin)) = mux;
}

/** Set the MSPI flash/PSRAM clocks from a divider of the MSPI core clock. */
static void psram_slow_clock(uint32_t freqdiv)
{
    uint32_t bits;

    if (freqdiv <= 1)
    {
        bits = SPI_CLK_EQU_SYSCLK;
    }
    else
    {
        bits = (((freqdiv - 1) << SPI_CLKCNT_N_S))
             | (((freqdiv / 2) - 1) << SPI_CLKCNT_H_S)
             | ((freqdiv - 1) << SPI_CLKCNT_L_S);
    }
    SPI_MEM_CLOCK(1) = bits;

    if (freqdiv <= 1)
    {
        bits = SPI_CLK_EQU_SYSCLK;
    }
    else
    {
        bits = (((freqdiv - 1) << SPI_SCLKCNT_N_S))
             | (((freqdiv / 2) - 1) << SPI_SCLKCNT_H_S)
             | ((freqdiv - 1) << SPI_SCLKCNT_L_S);
    }
    SPI_MEM_SRAM_CLK(0) = bits;
}

/* ------------------------------------------------------------------ */
/*  Cache / MMU setup                                                 */
/* ------------------------------------------------------------------ */

/** Point the SPI0 cache controller at the quad PSRAM commands. */
static void psram_cache_phases(void)
{
    uint32_t sctrl, drd, dwr, ac;

    /* Command phase: quad, using the explicit read/write commands. */
    sctrl  = SPI_MEM_CACHE_SCTRL(0);
    sctrl &= ~SPI_CACHE_USR_SRAM_DIO;
    sctrl |= SPI_CACHE_USR_SRAM_QIO | SPI_CACHE_SRAM_USR_RCMD |
             SPI_CACHE_SRAM_USR_WCMD | SPI_CACHE_USR_RD_DUMMY;

    /* Address phase: 24 bits. */
    sctrl &= ~SPI_CACHE_SRAM_ADDR_BITLEN_M;
    sctrl |= (23U << SPI_CACHE_SRAM_ADDR_BITLEN_S);

    /* Read dummy cycles: PSRAM_QUAD_READ_DUMMY - 1. */
    sctrl &= ~SPI_CACHE_SRAM_RDUMMY_M;
    sctrl |= ((uint32_t)(PSRAM_QUAD_READ_DUMMY - 1) << 6);
    SPI_MEM_CACHE_SCTRL(0) = sctrl;

    /* Read command: 0xEB, 8 bits. */
    drd  = SPI_MEM_SRAM_DRD_CMD(0);
    drd &= ~(SPI_SRAM_CMD_BITLEN_M | SPI_SRAM_CMD_VALUE_M);
    drd |= (7U << SPI_SRAM_CMD_BITLEN_S) | (PSRAM_CMD_READ_QUAD & SPI_SRAM_CMD_VALUE_M);
    SPI_MEM_SRAM_DRD_CMD(0) = drd;

    /* Write command: 0x38, 8 bits. */
    dwr  = SPI_MEM_SRAM_DWR_CMD(0);
    dwr &= ~(SPI_SRAM_CMD_BITLEN_M | SPI_SRAM_CMD_VALUE_M);
    dwr |= (7U << SPI_SRAM_CMD_BITLEN_S) | (PSRAM_CMD_WRITE_QUAD & SPI_SRAM_CMD_VALUE_M);
    SPI_MEM_SRAM_DWR_CMD(0) = dwr;

    /* CS setup/hold timing for external RAM (SPI0 and SPI1 share it). */
    ac  = SPI_MEM_SMEM_AC(0);
    ac &= ~((0x1FU << SPI_SMEM_CS_SETUP_TIME_S) |
            (0x1FU << SPI_SMEM_CS_HOLD_TIME_S));
    ac |= SPI_SMEM_CS_SETUP | SPI_SMEM_CS_HOLD;
    SPI_MEM_SMEM_AC(0) = ac;

    /* Let SPI0 drive CS1 (PSRAM) as well as CS0 (flash). */
    SPI_MEM_MISC(0) &= ~SPI_MISC_CS1_DIS;
}

/** Map @p size bytes of PSRAM at ::PSRAM_BASE in the shared MMU table. */
static void psram_map(uint32_t size)
{
    uint32_t base = MMU_ENTRY_ID(PSRAM_BASE);
    uint32_t pages = size / 0x10000UL;
    uint32_t i;

    for (i = 0; i < pages; i++)
    {
        REG32(MMU_TABLE_BASE + (base + i) * 4) =
            (i & 0x3FFFUL) | MMU_ACCESS_SPIRAM;
    }
}

/** Turn the data cache on for the core's D-bus and drop stale lines. */
static void psram_dcache_on(uint32_t size)
{
    EXTMEM_DCACHE_CTRL1 |= EXTMEM_DCACHE_SHUT_CORE0;
    EXTMEM_DCACHE_CTRL  |= EXTMEM_DCACHE_ENABLE;

    /* Invalidate the whole mapped region (SYNC_SIZE counts cache blocks).
     * Bound the wait so a mis-set size cannot wedge the boot. */
    EXTMEM_DCACHE_SYNC_CTRL = 0;
    EXTMEM_DCACHE_SYNC_ADDR = PSRAM_BASE;
    EXTMEM_DCACHE_SYNC_SIZE = size / 32U;
    EXTMEM_DCACHE_SYNC_CTRL = EXTMEM_DCACHE_INVALIDATE_ENA;
    {
        uint32_t n = 1000000U;

        while (!(EXTMEM_DCACHE_SYNC_CTRL & EXTMEM_DCACHE_SYNC_DONE) && n > 0)
        {
            n--;
        }
    }
    EXTMEM_DCACHE_SYNC_CTRL = 0;

    EXTMEM_DCACHE_CTRL1 &= ~EXTMEM_DCACHE_SHUT_CORE0;
}

/** Write a handful of patterns across the die and read them back. */
static int psram_selftest(uint32_t size)
{
    volatile uint32_t *base = (volatile uint32_t *)PSRAM_BASE;
    uint32_t nwords = size / 4;
    uint32_t at[4];
    uint32_t pat[4];
    int i;

    at[0] = 0;
    at[1] = nwords / 4;
    at[2] = nwords / 2;
    at[3] = nwords - 1;
    pat[0] = 0xA5A5A5A5UL;
    pat[1] = 0x5A5A5A5AUL;
    pat[2] = 0xDEADBEEFUL;
    pat[3] = 0x12345678UL;

    for (i = 0; i < 4; i++)
    {
        base[at[i]] = pat[i];
    }
    for (i = 0; i < 4; i++)
    {
        if (base[at[i]] != pat[i])
        {
            return SYSERR;
        }
    }

    /* A short linear run catches stuck address lines. */
    for (i = 0; i < 64; i++)
    {
        base[at[1] + (uint32_t)i] = 0x1000U + (uint32_t)i;
    }
    for (i = 0; i < 64; i++)
    {
        if (base[at[1] + (uint32_t)i] != 0x1000U + (uint32_t)i)
        {
            return SYSERR;
        }
    }
    return OK;
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                       */
/* ------------------------------------------------------------------ */

int psramInit(void)
{
    uint8_t idbuf[8];
    uint32_t id, density, size;
    uint64_t full_eid;
    uint32_t eid_47_16;

    if (psram_present)
    {
        return OK;
    }
    psram_init_status = PSRAM_INIT_RUNNING;

    psram_dbg_ctrl = SPI_MEM_CTRL(1);
    psram_dbg_clk = SPI_MEM_CLOCK(1);

    /* Control #1: read the flash JEDEC id over CS0 before we touch a single
     * register.  Our own boot image came out of that chip, so this must
     * return the flash's real id; if it does not, the fault is in the
     * low-level transaction and has nothing to do with the PSRAM. */
    idbuf[0] = idbuf[1] = idbuf[2] = 0;
    (void)psram_exec(0, 0x9FU, 8, 0, 0, 0, NULL, 0, idbuf, 24);
    psram_dbg_flash[0] = idbuf[0];
    psram_dbg_flash[1] = idbuf[1];
    psram_dbg_flash[2] = idbuf[2];

    /* CS1, and the two data pads the flash does not need in DIO mode. */
    psram_pad_init(PSRAM_CS_GPIO, 1);
    psram_pad_init(PSRAM_SD2_GPIO, 0);
    psram_pad_init(PSRAM_SD3_GPIO, 0);
    (void)FLASH_CS_GPIO;

    /* Run the MSPI clock slow enough that the die needs no timing tuning.
     * The divider is relative to the MSPI core clock the ROM selected. */
    psram_slow_clock(4);

    /* CS setup/hold for external RAM (shared by SPI0 and SPI1). */
    {
        uint32_t ac;

        ac  = SPI_MEM_SMEM_AC(0);
        ac &= ~((0x1FU << SPI_SMEM_CS_SETUP_TIME_S) |
                (0x1FU << SPI_SMEM_CS_HOLD_TIME_S));
        ac |= SPI_SMEM_CS_SETUP | SPI_SMEM_CS_HOLD;
        SPI_MEM_SMEM_AC(0) = ac;
    }

    /* Control #2: the same flash read, now after our clock and mode setup. */
    psram_spi_mode(0);
    idbuf[0] = idbuf[1] = idbuf[2] = 0;
    (void)psram_exec(0, 0x9FU, 8, 0, 0, 0, NULL, 0, idbuf, 24);
    psram_dbg_flash2[0] = idbuf[0];
    psram_dbg_flash2[1] = idbuf[1];
    psram_dbg_flash2[2] = idbuf[2];

    /* Drop the PSRAM die back to single-line (harmless on a cold boot). */
    psram_spi_mode(1);
    if (SYSERR == psram_exec(1, PSRAM_CMD_EXIT_QPI, 8, 0, 0, 0, NULL, 0, NULL, 0))
    {
        psram_init_status = PSRAM_INIT_SPI_TIMEOUT;
        return SYSERR;
    }
    psram_spi_mode(0);

    /* Device id: KGD must be 0x5D for a known-good die.  Retry once, as the
     * first read after a warm reset can come back empty. */
    id = 0;
    (void)psram_read_id(1, idbuf, 24);
    id = (uint32_t)idbuf[0] | ((uint32_t)idbuf[1] << 8) | ((uint32_t)idbuf[2] << 16);
    if (((id >> 8) & 0xFFU) != PSRAM_ID_KGD)
    {
        (void)psram_read_id(1, idbuf, 24);
        id = (uint32_t)idbuf[0] | ((uint32_t)idbuf[1] << 8) | ((uint32_t)idbuf[2] << 16);
        if (((id >> 8) & 0xFFU) != PSRAM_ID_KGD)
        {
            psram_dbg_id[0] = idbuf[0];
            psram_dbg_id[1] = idbuf[1];
            psram_dbg_id[2] = idbuf[2];
            psram_init_status = PSRAM_INIT_NO_ID;
            return SYSERR;
        }
    }
    psram_dbg_id[0] = idbuf[0];
    psram_dbg_id[1] = idbuf[1];
    psram_dbg_id[2] = idbuf[2];

    /* Density from the extended id, with the 64-Mbit trial special case. */
    if (((id >> 16) & 0xFFU) == 0x26U)
    {
        size = 8UL * 1024UL * 1024UL;
    }
    else
    {
        density = (id >> 21) & 0x7U;
        size = (density == 0) ? (2UL * 1024UL * 1024UL)
             : (density == 1) ? (4UL * 1024UL * 1024UL)
             : (density == 2) ? (8UL * 1024UL * 1024UL)
             : 0UL;
        if (0 == size)
        {
            psram_init_status = PSRAM_INIT_BAD_DENSITY;
            return SYSERR;
        }
    }

    /* Some APS3204 parts in 2T mode masquerade as 8 MiB; check the extended
     * id and knock them down to 4 MiB if so. */
    if (size == 8UL * 1024UL * 1024UL)
    {
        full_eid = 0;
        psram_read_id(1, (uint8_t *)&full_eid, 48);
        eid_47_16 = __builtin_bswap32((uint32_t)((full_eid >> 16) & 0xFFFFFFFFUL));
        if (((eid_47_16 >> 5) & 0xFFFFFUL) == 0x8A445UL)
        {
            size = 4UL * 1024UL * 1024UL;
        }
    }

    /* Reset the die, then latch quad mode. */
    psram_exec(1, PSRAM_CMD_RESET_EN, 8, 0, 0, 0, NULL, 0, NULL, 0);
    psram_exec(1, PSRAM_CMD_RESET,    8, 0, 0, 0, NULL, 0, NULL, 0);
    psram_delay(100);
    psram_exec(1, PSRAM_CMD_ENTER_QPI, 8, 0, 0, 0, NULL, 0, NULL, 0);
    psram_delay(10);

    /* Teach the cache controller the quad commands and map the die. */
    psram_cache_phases();
    psram_map(size);
    psram_dcache_on(size);

    if (SYSERR == psram_selftest(size))
    {
        psram_init_status = PSRAM_INIT_SELFTEST;
        return SYSERR;
    }

    psram_size = size;
    psram_present = 1;
    psram_init_status = PSRAM_INIT_OK;

    return OK;
}
