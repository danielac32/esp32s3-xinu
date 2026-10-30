/**
 * @file w5500Spi.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: bus framing.
 *
 * The chip answers a four byte SPI header followed by the payload:
 *
 *     [ addr_hi ][ addr_lo ][ control ][ data... ]
 *
 * where the control byte is
 *
 *     bit  7..3   BSB  (register block select)
 *     bit  2      RWB  (0 = read, 1 = write)
 *     bit  1..0   OM   (00 = variable length data mode, framed by CS)
 *
 * Two back ends are provided:
 *
 *   W5500_SPI_HW = 1  the ESP32-S3 GPSPI (SPI2/FSPI) peripheral drives
 *                     SCK/MOSI/MISO through its IO_MUX function, so a
 *                     whole transaction of up to 64 bytes is shifted by
 *                     the hardware while the CPU only polls the done bit.
 *                     CS stays a plain GPIO so transactions can be framed
 *                     by software.  Pins 10..13 happen to be the FSPI
 *                     default route (CS/SCK/MOSI/MISO respectively).
 *
 *   W5500_SPI_HW = 0  the original bit-banged path, kept as a fallback.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdint.h>
#include <string.h>
#include <xtensa.h>
#include <interrupt.h>

#include <w5500.h>

#ifndef W5500_SPI_HW
#define W5500_SPI_HW        1
#endif

/** Bus clock.  The APB clock is 20 MHz on this port; the GPSPI divider
 *  divides it down.  5 MHz is APB/4 and stays well inside the W5500's
 *  SPI limit; the bit-banged fallback is far slower. */
#ifndef W5500_SPI_HZ
#define W5500_SPI_HZ        5000000U
#endif

#define W5500_APB_HZ        20000000U

#define W5500_BUF_SIZE      (W5500_BUF_KB * 1024)
#define W5500_BUF_MASK      (W5500_BUF_SIZE - 1)

static inline void w5500Select(void)
{
    xt_gpio_set_level(W5500_PIN_CS, 0);
    /* Give the chip (and the GPIO) time to settle before the first clock. */
    __asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"
                      "nop; nop; nop; nop; nop; nop; nop; nop;");
}

static inline void w5500Deselect(void)
{
    xt_gpio_set_level(W5500_PIN_CS, 1);
    /* Hold CS high long enough for the chip to latch the transaction. */
    __asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"
                      "nop; nop; nop; nop; nop; nop; nop; nop;"
                      "nop; nop; nop; nop; nop; nop; nop; nop;"
                      "nop; nop; nop; nop; nop; nop; nop; nop;");
}

#if W5500_SPI_HW

/* ------------------------------------------------------------------ */
/*  Hardware GPSPI back end                                           */
/* ------------------------------------------------------------------ */

/**
 * Put one pad on its FSPI IO_MUX function.  This bypasses the GPIO
 * matrix entirely: the peripheral drives SCK/MOSI and samples MISO
 * with no software involvement.
 */
static void w5500SpiPad(uint pin, int input)
{
    uint32_t mux = REG32(IO_MUX_GPIO(pin));

    mux &= ~(IO_MUX_MCU_SEL_MASK | IO_MUX_FUN_WPU | IO_MUX_FUN_WPD
             | IO_MUX_FUN_IE | IO_MUX_FILTER_EN);
    mux |= (IO_MUX_MCU_SEL_FSPI << IO_MUX_MCU_SEL_POS);
    mux |= (2U << IO_MUX_FUN_DRV_POS);          /* ~20 mA drive */
    if (input)
    {
        mux |= IO_MUX_FUN_IE;
    }
    REG32(IO_MUX_GPIO(pin)) = mux;
}

/**
 * @ingroup w5500
 *
 * Bring up the GPSPI (SPI2/FSPI) master in mode 0.  CS is left to the
 * GPIO helper above; every hardware CS line is disabled so the
 * peripheral never touches a pad on its own.
 */
void w5500SpiInit(void)
{
    uint32_t div_n;
    uint32_t div_h;
    uint32_t reg;

    /* APB clock + reset for the peripheral. */
    REG32(SYSTEM_PERIP_CLK_EN0) |= SYS_CLK_EN_SPI2 | SYS_CLK_EN_SPI2_DMA;
    REG32(SYSTEM_PERIP_RST_EN0) |= SYS_CLK_EN_SPI2;
    {
        volatile int i;
        for (i = 0; i < 100; i++)
        {
            __asm__ volatile ("nop");
        }
    }
    REG32(SYSTEM_PERIP_RST_EN0) &= ~SYS_CLK_EN_SPI2;

    /* CS is a plain GPIO. */
    xt_gpio_output(W5500_PIN_CS, GPIO_PULL_NONE);
    xt_gpio_set_level(W5500_PIN_CS, 1);

    /* SCK, MOSI and MISO come from the peripheral. */
    w5500SpiPad(W5500_PIN_SCK, 0);
    w5500SpiPad(W5500_PIN_MOSI, 0);
    w5500SpiPad(W5500_PIN_MISO, 1);

    /* Module clock from APB, gated on. */
    REG32(SPI2_BASE + SPI_CLK_GATE_REG) =
        SPI_CLK_GATE_CLK_EN | SPI_CLK_GATE_MST_CLK_ACTIVE |
        SPI_CLK_GATE_MST_CLK_SEL_APB;

    /* All chip selects disabled: software drives CS. */
    REG32(SPI2_BASE + SPI_MISC_REG) = SPI_MISC_CS_DIS_MASK;

    /* Mode 0: clock idles low, data changes on the falling edge. */
    REG32(SPI2_BASE + SPI_USER_REG) =
        SPI_USER_DOUTDIN | SPI_USER_USR_MOSI | SPI_USER_USR_MISO;
    REG32(SPI2_BASE + SPI_USER1_REG) = 0;
    REG32(SPI2_BASE + SPI_USER2_REG) = 0;
    REG32(SPI2_BASE + SPI_CTRL_REG) = 0;        /* MSB first, 1 line */

    /* f = APB / (pre * n); n is 1..64, pre is 1..16. */
    {
        uint32_t n = W5500_APB_HZ / W5500_SPI_HZ;
        uint32_t pre = 1;

        if (n < 2)
        {
            n = 2;
        }
        if (n > 64)
        {
            n = 64;
        }
        div_n = n - 1;
        div_h = ((n + 1) / 2) - 1;
        reg = (div_n << SPI_CLOCK_CLKCNT_L_POS)
            | (div_h << SPI_CLOCK_CLKCNT_H_POS)
            | (div_n << SPI_CLOCK_CLKCNT_N_POS)
            | ((pre - 1) << SPI_CLOCK_CLKDIV_PRE_POS);
    }
    REG32(SPI2_BASE + SPI_CLOCK_REG) = reg;

    /* Flush the CPU FIFOs left over from the ROM/bootloader. */
    REG32(SPI2_BASE + SPI_DMA_CONF_REG) =
        SPI_DMA_CONF_BUF_AFIFO_RST | SPI_DMA_CONF_RX_AFIFO_RST;
    REG32(SPI2_BASE + SPI_DMA_CONF_REG) = 0;
}

/**
 * Shift at most SPI_CPU_MAX_LEN bytes.  Bytes are packed little endian
 * into the 16 word buffer, which is how the peripheral transmits them
 * (byte 0 first, MSB first inside every byte).
 *
 * A bounded spin keeps a misconfigured peripheral from hanging the
 * whole board, which is what a previous SPI2 attempt did.
 */
static int w5500HwXfer(const uint8_t *tx, uint8_t *rx, uint len)
{
    uint i;
    uint32_t spin;

    if (len == 0 || len > SPI_CPU_MAX_LEN)
    {
        return SYSERR;
    }

    REG32(SPI2_BASE + SPI_DMA_CONF_REG) =
        SPI_DMA_CONF_BUF_AFIFO_RST | SPI_DMA_CONF_RX_AFIFO_RST;
    REG32(SPI2_BASE + SPI_DMA_CONF_REG) = 0;

    for (i = 0; i < len; i += 4)
    {
        uint32_t word = 0;
        uint n = len - i;

        if (n > 4)
        {
            n = 4;
        }
        memcpy(&word, tx + i, n);
        REG32(SPI2_BASE + SPI_W0_REG + i) = word;
    }

    REG32(SPI2_BASE + SPI_MS_DLEN_REG) = (len * 8U) - 1U;

    /* Synchronize the configuration into the SPI clock domain. */
    REG32(SPI2_BASE + SPI_CMD_REG) = SPI_CMD_UPDATE;
    for (spin = 0; spin < 100000U; spin++)
    {
        if (!(REG32(SPI2_BASE + SPI_CMD_REG) & SPI_CMD_UPDATE))
        {
            break;
        }
    }
    if (spin >= 100000U)
    {
        return SYSERR;
    }

    /* Run it and wait for the hardware to clear USR. */
    REG32(SPI2_BASE + SPI_CMD_REG) = SPI_CMD_USR;
    for (spin = 0; spin < 1000000U; spin++)
    {
        if (!(REG32(SPI2_BASE + SPI_CMD_REG) & SPI_CMD_USR))
        {
            break;
        }
    }
    if (spin >= 1000000U)
    {
        return SYSERR;
    }

    if (rx != NULL)
    {
        for (i = 0; i < len; i += 4)
        {
            uint32_t word = REG32(SPI2_BASE + SPI_W0_REG + i);
            uint n = len - i;

            if (n > 4)
            {
                n = 4;
            }
            memcpy(rx + i, &word, n);
        }
    }
    return OK;
}

/** Write the three byte header plus payload, splitting large payloads. */
static void w5500HeaderWrite(uint16_t addr, uint8_t bsb, const uint8_t *src,
                             uint len)
{
    uint8_t tx[SPI_CPU_MAX_LEN];
    uint first = SPI_CPU_MAX_LEN - 3;
    uint chunk = (len < first) ? len : first;

    tx[0] = (uint8_t)(addr >> 8);
    tx[1] = (uint8_t)(addr & 0xFF);
    tx[2] = (uint8_t)((bsb << 3) | W5500_RWB_WRITE);
    if (chunk > 0)
    {
        memcpy(tx + 3, src, chunk);
    }
    if (SYSERR == w5500HwXfer(tx, NULL, 3 + chunk))
    {
        return;
    }

    src += chunk;
    len -= chunk;
    while (len > 0)
    {
        chunk = (len > SPI_CPU_MAX_LEN) ? SPI_CPU_MAX_LEN : len;
        if (SYSERR == w5500HwXfer(src, NULL, chunk))
        {
            return;
        }
        src += chunk;
        len -= chunk;
    }
}

/** Read the header, then the payload, splitting large payloads. */
static void w5500HeaderRead(uint16_t addr, uint8_t bsb, uint8_t *dst,
                            uint len)
{
    uint8_t tx[SPI_CPU_MAX_LEN];
    uint8_t rx[SPI_CPU_MAX_LEN];
    uint first = SPI_CPU_MAX_LEN - 3;
    uint chunk = (len < first) ? len : first;

    memset(tx, 0, sizeof(tx));
    tx[0] = (uint8_t)(addr >> 8);
    tx[1] = (uint8_t)(addr & 0xFF);
    tx[2] = (uint8_t)((bsb << 3) | W5500_RWB_READ);
    if (SYSERR == w5500HwXfer(tx, rx, 3 + chunk))
    {
        return;
    }
    memcpy(dst, rx + 3, chunk);

    dst += chunk;
    len -= chunk;
    while (len > 0)
    {
        chunk = (len > SPI_CPU_MAX_LEN) ? SPI_CPU_MAX_LEN : len;
        memset(tx, 0, chunk);
        if (SYSERR == w5500HwXfer(tx, rx, chunk))
        {
            return;
        }
        memcpy(dst, rx, chunk);
        dst += chunk;
        len -= chunk;
    }
}

/**
 * @ingroup w5500
 *
 * Read one byte from a chip register.
 */
uint8_t w5500ReadReg(uint16_t addr, uint8_t bsb)
{
    uint8_t tx[4];
    uint8_t rx[4];

    tx[0] = (uint8_t)(addr >> 8);
    tx[1] = (uint8_t)(addr & 0xFF);
    tx[2] = (uint8_t)((bsb << 3) | W5500_RWB_READ);
    tx[3] = 0;

    w5500Select();
    w5500HwXfer(tx, rx, 4);
    w5500Deselect();
    return rx[3];
}

/**
 * @ingroup w5500
 *
 * Write one byte to a chip register.
 */
void w5500WriteReg(uint16_t addr, uint8_t bsb, uint8_t value)
{
    uint8_t tx[4];

    tx[0] = (uint8_t)(addr >> 8);
    tx[1] = (uint8_t)(addr & 0xFF);
    tx[2] = (uint8_t)((bsb << 3) | W5500_RWB_WRITE);
    tx[3] = value;

    w5500Select();
    w5500HwXfer(tx, NULL, 4);
    w5500Deselect();
}

uint16_t w5500Read16(uint16_t addr, uint8_t bsb)
{
    uint8_t tx[5];
    uint8_t rx[5];

    tx[0] = (uint8_t)(addr >> 8);
    tx[1] = (uint8_t)(addr & 0xFF);
    tx[2] = (uint8_t)((bsb << 3) | W5500_RWB_READ);
    tx[3] = 0;
    tx[4] = 0;

    w5500Select();
    w5500HwXfer(tx, rx, 5);
    w5500Deselect();
    return (uint16_t)((rx[3] << 8) | rx[4]);
}

void w5500Write16(uint16_t addr, uint8_t bsb, uint16_t value)
{
    uint8_t tx[5];

    tx[0] = (uint8_t)(addr >> 8);
    tx[1] = (uint8_t)(addr & 0xFF);
    tx[2] = (uint8_t)((bsb << 3) | W5500_RWB_WRITE);
    tx[3] = (uint8_t)(value >> 8);
    tx[4] = (uint8_t)(value & 0xFF);

    w5500Select();
    w5500HwXfer(tx, NULL, 5);
    w5500Deselect();
}

/**
 * @ingroup w5500
 *
 * Copy bytes out of a socket buffer.
 *
 * Socket buffer addresses are masked to the size of that socket's buffer
 * and a transfer that reaches the end continues at offset 0, so the
 * transfer is split here instead of relying on the chip's own wrap.
 */
void w5500ReadBuf(uint16_t addr, uint8_t bsb, void *buf, uint len)
{
    uint8_t *dst = (uint8_t *)buf;

    while (len > 0)
    {
        uint chunk = W5500_BUF_SIZE - (addr & W5500_BUF_MASK);

        if (chunk > len)
        {
            chunk = len;
        }

        w5500Select();
        w5500HeaderRead((uint16_t)(addr & W5500_BUF_MASK), bsb, dst, chunk);
        w5500Deselect();

        dst  += chunk;
        addr  = (uint16_t)(addr + chunk);
        len  -= chunk;
    }
}

/**
 * @ingroup w5500
 *
 * Counterpart of w5500ReadBuf() for the transmit buffers.
 */
void w5500WriteBuf(uint16_t addr, uint8_t bsb, const void *buf, uint len)
{
    const uint8_t *src = (const uint8_t *)buf;

    while (len > 0)
    {
        uint chunk = W5500_BUF_SIZE - (addr & W5500_BUF_MASK);

        if (chunk > len)
        {
            chunk = len;
        }

        w5500Select();
        w5500HeaderWrite((uint16_t)(addr & W5500_BUF_MASK), bsb, src, chunk);
        w5500Deselect();

        src  += chunk;
        addr  = (uint16_t)(addr + chunk);
        len  -= chunk;
    }
}

#else /* !W5500_SPI_HW: original bit-banged back end */

static inline uint8_t w5500Xfer(uint8_t tx)
{
    irqmask im = disable();
    uint8_t rx = xt_spi_xfer(W5500_PIN_SCK, W5500_PIN_MOSI, W5500_PIN_MISO, tx);
    restore(im);
    return rx;
}

/* Clock out the three byte header of an access to (addr, bsb). */
static void w5500Header(uint16_t addr, uint8_t bsb, int writing)
{
    w5500Xfer((uint8_t)(addr >> 8));
    w5500Xfer((uint8_t)(addr & 0xFF));
    w5500Xfer((uint8_t)((bsb << 3) | (writing ? W5500_RWB_WRITE
                                               : W5500_RWB_READ)));
}

void w5500SpiInit(void)
{
    xt_gpio_output(W5500_PIN_CS, GPIO_PULL_NONE);
    xt_gpio_output(W5500_PIN_SCK, GPIO_PULL_NONE);
    xt_gpio_output(W5500_PIN_MOSI, GPIO_PULL_NONE);
    xt_gpio_input(W5500_PIN_MISO, GPIO_PULL_NONE);

    xt_gpio_set_level(W5500_PIN_CS, 1);
    xt_gpio_set_level(W5500_PIN_SCK, 0);
    xt_gpio_set_level(W5500_PIN_MOSI, 0);
}

uint8_t w5500ReadReg(uint16_t addr, uint8_t bsb)
{
    uint8_t value;

    w5500Select();
    w5500Header(addr, bsb, FALSE);
    value = w5500Xfer(0x00);
    w5500Deselect();
    return value;
}

void w5500WriteReg(uint16_t addr, uint8_t bsb, uint8_t value)
{
    w5500Select();
    w5500Header(addr, bsb, TRUE);
    w5500Xfer(value);
    w5500Deselect();
}

uint16_t w5500Read16(uint16_t addr, uint8_t bsb)
{
    uint16_t hi, lo;

    w5500Select();
    w5500Header(addr, bsb, FALSE);
    hi = w5500Xfer(0x00);
    lo = w5500Xfer(0x00);
    w5500Deselect();
    return (uint16_t)((hi << 8) | lo);
}

void w5500Write16(uint16_t addr, uint8_t bsb, uint16_t value)
{
    w5500Select();
    w5500Header(addr, bsb, TRUE);
    w5500Xfer((uint8_t)(value >> 8));
    w5500Xfer((uint8_t)(value & 0xFF));
    w5500Deselect();
}

void w5500ReadBuf(uint16_t addr, uint8_t bsb, void *buf, uint len)
{
    uint8_t *dst = (uint8_t *)buf;

    while (len > 0)
    {
        uint chunk = W5500_BUF_SIZE - (addr & W5500_BUF_MASK);
        uint i;

        if (chunk > len)
        {
            chunk = len;
        }

        w5500Select();
        w5500Header((uint16_t)(addr & W5500_BUF_MASK), bsb, FALSE);
        for (i = 0; i < chunk; i++)
        {
            dst[i] = w5500Xfer(0x00);
        }
        w5500Deselect();

        dst  += chunk;
        addr  = (uint16_t)(addr + chunk);
        len  -= chunk;
    }
}

void w5500WriteBuf(uint16_t addr, uint8_t bsb, const void *buf, uint len)
{
    const uint8_t *src = (const uint8_t *)buf;

    while (len > 0)
    {
        uint chunk = W5500_BUF_SIZE - (addr & W5500_BUF_MASK);
        uint i;

        if (chunk > len)
        {
            chunk = len;
        }

        w5500Select();
        w5500Header((uint16_t)(addr & W5500_BUF_MASK), bsb, TRUE);
        for (i = 0; i < chunk; i++)
        {
            w5500Xfer(src[i]);
        }
        w5500Deselect();

        src  += chunk;
        addr  = (uint16_t)(addr + chunk);
        len  -= chunk;
    }
}

#endif /* W5500_SPI_HW */
