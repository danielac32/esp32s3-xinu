/**
 * @file xtensa.h
 *
 * Xtensa LX7 / ESP32-S3 architecture and memory-map definitions.
 *
 * Every address, bit position and field width in this file was taken from
 * the ESP32-S3 SVD (register descriptor) file, so it should match the
 * chip's Technical Reference Manual.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _XTENSA_H_
#define _XTENSA_H_

#include <stddef.h>
#include <stdint.h>

/* ================================================================== */
/*  Xtensa special-register access helpers                            */
/* ================================================================== */

#define RSR(reg)  ({ uint32_t __v;                                  \
        __asm__ volatile ("rsr %0, " #reg : "=a"(__v)); __v; })

#define WSR(reg, v) do { uint32_t __t = (uint32_t)(v);               \
        __asm__ volatile ("wsr %0, " #reg "\n\trsync" :: "a"(__t));  \
    } while (0)

/** Read the free-running cycle counter. */
#define XT_CCOUNT()        RSR(ccount)
/** Read the processor status register. */
#define XT_PS()            RSR(ps)
/** Read the pending-interrupt register. */
#define XT_INTERRUPT()     RSR(interrupt)
/** Read the interrupt-enable register. */
#define XT_INTENABLE()     RSR(intenable)
/** Read the exception cause. */
#define XT_EXCCAUSE()      RSR(exccause)

#define XT_EXCCAUSE_LEVEL1 4    /**< EXCCAUSE value for a level-1 interrupt */

/* ================================================================== */
/*  PS (Processor Status) register bits                               */
/* ================================================================== */

#define PS_INTLEVEL_MASK    0x0000000FU  /**< interrupt level (0 = enabled) */
#define PS_EXCM             0x00000010U  /**< exception mode                */
#define PS_UM               0x00000020U  /**< user mode                     */
#define PS_RING_MASK        0x00003000U

/* ================================================================== */
/*  CPU interrupt-line assignment                                     */
/* ================================================================== */
/*
 * Peripheral interrupt sources are routed to one of the 32 CPU interrupt
 * lines by writing the line number into INTERRUPT_CORE0_<src>_INTR_MAP.
 * The Xtensa core then assigns each line to an interrupt level.
 *
 * Every line below is a level-3 line.  Level 3 is the level of CCOMPARE1,
 * the core timer used for the Xinu tick, so a single vector handles the
 * timer and all peripherals, and a single instruction ("rfi 3") returns
 * from them.  Lines 11/15/22/23/27/29 are the level-3 lines on this core
 * (XCHAL_INT11/15/22/23/27/29_LEVEL == 3).
 */
#define XT_IRQ_TIMER        15    /**< CCOMPARE1 core timer     */
#define XT_IRQ_UART0        22    /**< UART0                    */
#define XT_IRQ_GPIO         23    /**< GPIO (button, W5500 IRQ) */

/* Peripheral interrupt "source" numbers, used by the interrupt matrix.
 * The map register for source N is at INTERRUPT_CORE0_BASE + N*4.  */
#define PERIPH_SRC_GPIO               16
#define PERIPH_SRC_UART0              27
#define PERIPH_SRC_SYSTIMER_TARGET0   57

/* ================================================================== */
/*  Base addresses                                                    */
/* ================================================================== */

#define UART0_BASE           0x60000000UL
#define UART1_BASE           0x60010000UL
#define UART2_BASE           0x6002E000UL
#define TIMG0_BASE           0x6001F000UL
#define TIMG1_BASE           0x60020000UL
#define SYSTIMER_BASE        0x60023000UL
#define SPI2_BASE            0x60024000UL
#define SPI3_BASE            0x60025000UL
#define GPIO_BASE            0x60004000UL
#define IO_MUX_BASE          0x60009000UL
/** SD/MMC host controller (DesignWare MSHC, SDHCI-like). */
#define SDHOST_BASE          0x60028000UL
#define RTC_CNTL_BASE        0x60008000UL
#define SYSTEM_BASE          0x600C0000UL
#define INTERRUPT_CORE0_BASE 0x600C2000UL
#define INTERRUPT_CORE1_BASE 0x600C2800UL

/*
 * The internal SRAM window.  SRAM1 is shared "D/IRAM": the same physical
 * 416 KiB is visible through two windows that differ by 0x6F0000, so
 * IRAM_BASE (0x40378000) and DRAM_BASE (0x3FC88000) describe the same
 * bytes.  Code and data must therefore occupy disjoint physical ranges;
 * see compile/platforms/esp32s3/ld.script for the split we use.
 */
#define IRAM_BASE            0x40378000UL
#define IRAM_END             0x403E0000UL
#define DRAM_BASE            0x3FC88000UL
#define DRAM_END             0x3FCF0000UL

/** Offset between a data-window address and its instruction-window alias. */
#define IRAM_DRAM_OFFSET     0x6F0000UL

/* ================================================================== */
/*  SYSTEM registers (0x600C0000)                                     */
/* ================================================================== */

#define SYSTEM_CPU_PERI_CLK_EN    (SYSTEM_BASE + 0x0008)
#define SYSTEM_CPU_PERI_RST_EN    (SYSTEM_BASE + 0x000C)
#define SYSTEM_CPU_PER_CONF       (SYSTEM_BASE + 0x0010)
#define SYSTEM_PERIP_CLK_EN0      (SYSTEM_BASE + 0x0018)
#define SYSTEM_PERIP_CLK_EN1      (SYSTEM_BASE + 0x001C)
#define SYSTEM_PERIP_RST_EN0      (SYSTEM_BASE + 0x0020)
#define SYSTEM_PERIP_RST_EN1      (SYSTEM_BASE + 0x0024)
#define SYSTEM_SYSCLK_CONF        (SYSTEM_BASE + 0x0060)

/* PERIP_CLK_EN0 / PERIP_RST_EN0 bits */
#define SYS_CLK_EN_TIMERS         (1U << 0)
#define SYS_CLK_EN_SPI01          (1U << 1)
#define SYS_CLK_EN_UART0          (1U << 2)
#define SYS_CLK_EN_WDG            (1U << 3)
#define SYS_CLK_EN_I2S0           (1U << 4)
#define SYS_CLK_EN_UART1          (1U << 5)
#define SYS_CLK_EN_SPI2           (1U << 6)
#define SYS_CLK_EN_TIMERGROUP     (1U << 13)
#define SYS_CLK_EN_TIMERGROUP1    (1U << 15)
#define SYS_CLK_EN_SPI3           (1U << 16)
#define SYS_CLK_EN_SPI2_DMA       (1U << 22)
#define SYS_CLK_EN_UART_MEM       (1U << 24)
#define SYS_CLK_EN_SPI3_DMA       (1U << 27)
#define SYS_CLK_EN_SYSTIMER       (1U << 29)

/* PERIP_CLK_EN1 / PERIP_RST_EN1 bits */
#define SYS_CLK_EN_SDIO_HOST      (1U << 7)
#define SYS_RST_EN_SDIO_HOST      (1U << 7)

/* SYSCLK_CONF.SOC_CLK_SEL values */
#define SYS_SOC_CLK_SEL_XTAL      0U
#define SYS_SOC_CLK_SEL_PLL       1U
#define SYS_SOC_CLK_SEL_RC_FAST   2U

/* ================================================================== */
/*  Interrupt matrix (INTERRUPT_CORE0, 0x600C2000)                    */
/* ================================================================== */

/** Map-register address for peripheral interrupt source @p src. */
#define INTR_MAP(src)             (INTERRUPT_CORE0_BASE + ((src) * 4))
#define INTR_PRO_STATUS0          (INTERRUPT_CORE0_BASE + 0x018C)
#define INTR_PRO_STATUS1          (INTERRUPT_CORE0_BASE + 0x0190)
#define INTR_PRO_STATUS2          (INTERRUPT_CORE0_BASE + 0x0194)
#define INTR_PRO_STATUS3          (INTERRUPT_CORE0_BASE + 0x0198)

/* ================================================================== */
/*  RTC_CNTL (0x60008000): watchdog                                   */
/* ================================================================== */

#define RTC_CNTL_OPTIONS0         (RTC_CNTL_BASE + 0x0000)
/* OPTIONS0: writing 1 to SW_SYS_RST resets the digital core and the CPU. */
#define XT_RTC_CNTL_SW_SYS_RST    (1UL << 31)
#define RTC_CNTL_WDTCONFIG0       (RTC_CNTL_BASE + 0x0098)
#define RTC_CNTL_WDTCONFIG1       (RTC_CNTL_BASE + 0x009C)
#define RTC_CNTL_WDTFEED          (RTC_CNTL_BASE + 0x00AC)
#define RTC_CNTL_WDTWPROTECT      (RTC_CNTL_BASE + 0x00B0)
#define RTC_CNTL_SWD_CONF         (RTC_CNTL_BASE + 0x00B4)
#define RTC_CNTL_SWD_WPROTECT     (RTC_CNTL_BASE + 0x00B8)
#define RTC_CNTL_SW_CPU_STALL     (RTC_CNTL_BASE + 0x00BC)

#define RTC_CNTL_WDT_WKEY         0x50D83AA1UL
#define RTC_CNTL_SWD_WKEY         0x8F1D312AUL

#define RTC_CNTL_WDT_EN           (1U << 31)
#define RTC_CNTL_SWD_DISABLE      (1U << 30)
#define RTC_CNTL_SWD_AUTO_FEED_EN (1U << 31)

/* ================================================================== */
/*  TIMG0 (0x6001F000): main watchdog                                 */
/* ================================================================== */

#define TIMG0_WDTCONFIG0          (TIMG0_BASE + 0x0048)
#define TIMG0_WDTWPROTECT         (TIMG0_BASE + 0x0064)
#define TIMG0_WDT_WKEY            0x50D83AA1UL

/* ================================================================== */
/*  GPIO (0x60004000)                                                 */
/* ================================================================== */

#define GPIO_OUT                  (GPIO_BASE + 0x0004)
#define GPIO_OUT_W1TS             (GPIO_BASE + 0x0008)
#define GPIO_OUT_W1TC             (GPIO_BASE + 0x000C)
#define GPIO_OUT1                 (GPIO_BASE + 0x0010)
#define GPIO_OUT1_W1TS            (GPIO_BASE + 0x0014)
#define GPIO_OUT1_W1TC            (GPIO_BASE + 0x0018)
#define GPIO_ENABLE               (GPIO_BASE + 0x0020)
#define GPIO_ENABLE_W1TS          (GPIO_BASE + 0x0024)
#define GPIO_ENABLE_W1TC          (GPIO_BASE + 0x0028)
#define GPIO_ENABLE1              (GPIO_BASE + 0x002C)
#define GPIO_ENABLE1_W1TS         (GPIO_BASE + 0x0030)
#define GPIO_ENABLE1_W1TC         (GPIO_BASE + 0x0034)
#define GPIO_IN                   (GPIO_BASE + 0x003C)
#define GPIO_IN1                  (GPIO_BASE + 0x0040)
#define GPIO_STATUS               (GPIO_BASE + 0x0044)
#define GPIO_STATUS_W1TS          (GPIO_BASE + 0x0048)
#define GPIO_STATUS_W1TC          (GPIO_BASE + 0x004C)
#define GPIO_STATUS1              (GPIO_BASE + 0x0050)
#define GPIO_STATUS1_W1TS         (GPIO_BASE + 0x0054)
#define GPIO_STATUS1_W1TC         (GPIO_BASE + 0x0058)
#define GPIO_PCPU_INT             (GPIO_BASE + 0x005C)
#define GPIO_PCPU_NMI_INT         (GPIO_BASE + 0x0060)

/** Per-pin configuration register (54 of them, 0x74 + 4*n). */
#define GPIO_PIN(n)               (GPIO_BASE + 0x0074 + ((n) * 4))
/** GPIO-matrix input selection for output index n (256 of them). */
#define GPIO_FUNC_IN_SEL_CFG(n)   (GPIO_BASE + 0x0154 + ((n) * 4))
/** GPIO-matrix output selection for GPIO n (54 of them). */
#define GPIO_FUNC_OUT_SEL_CFG(n)  (GPIO_BASE + 0x0554 + ((n) * 4))

/* GPIO_PIN(n) fields */
#define GPIO_PIN_INT_TYPE_POS     7
#define GPIO_PIN_INT_TYPE_MASK    (7U << 7)
#define GPIO_PIN_WAKEUP_ENABLE    (1U << 10)
#define GPIO_PIN_CONFIG_POS       11
#define GPIO_PIN_CONFIG_MASK      (3U << 11)
#define GPIO_PIN_INT_ENA_POS      13
#define GPIO_PIN_INT_ENA_MASK     (0x1FU << 13)

/* GPIO_PIN(n).INT_TYPE values */
#define GPIO_INT_DISABLE          0U   /**< no interrupt               */
#define GPIO_INT_POSEDGE          1U   /**< rising edge                */
#define GPIO_INT_NEGEDGE          2U   /**< falling edge               */
#define GPIO_INT_ANYEDGE          3U   /**< any edge                   */
#define GPIO_INT_LOW_LEVEL        4U   /**< low level                  */
#define GPIO_INT_HIGH_LEVEL       5U   /**< high level                 */

/* GPIO_PIN(n).CONFIG values */
#define GPIO_PULL_NONE            0U
#define GPIO_PULL_UP              1U
#define GPIO_PULL_DOWN            2U

/* GPIO_FUNC_OUT_SEL_CFG(n) fields */
#define GPIO_OUT_SEL_MASK         0x1FFU
#define GPIO_OUT_INV_SEL          (1U << 9)
#define GPIO_OUT_OEN_SEL          (1U << 10)
#define GPIO_OUT_OEN_INV_SEL      (1U << 11)

/* GPIO_FUNC_IN_SEL_CFG(n) fields */
#define GPIO_IN_SEL_MASK          0x3FU
#define GPIO_IN_INV_SEL           (1U << 6)
#define GPIO_IN_SEL               (1U << 7)

/** Signal index that connects a GPIO-matrix output back to GPIO_OUT. */
#define SIG_GPIO_OUT_IDX          256U

/* ================================================================== */
/*  IO_MUX (0x60009000)                                               */
/* ================================================================== */
/*
 * One register per pad, four bytes apart:
 *     IO_MUX_GPIO(n) = 0x60009000 + 0x04 + 4*n
 */
#define IO_MUX_PIN_CTRL           (IO_MUX_BASE + 0x0000)
#define IO_MUX_GPIO(n)            (IO_MUX_BASE + 0x0004 + ((n) * 4))
#define IO_MUX_DATE               (IO_MUX_BASE + 0x00FC)

#define IO_MUX_MCU_OE             (1U << 0)
#define IO_MUX_SLP_SEL            (1U << 1)
#define IO_MUX_MCU_WPD            (1U << 2)
#define IO_MUX_MCU_WPU            (1U << 3)
#define IO_MUX_MCU_IE             (1U << 4)
#define IO_MUX_FUN_WPD            (1U << 7)
#define IO_MUX_FUN_WPU            (1U << 8)
#define IO_MUX_FUN_IE             (1U << 9)
#define IO_MUX_FUN_DRV_POS        10
#define IO_MUX_FUN_DRV_MASK       (3U << 10)
#define IO_MUX_MCU_SEL_POS        12
#define IO_MUX_MCU_SEL_MASK       (7U << 12)
#define IO_MUX_FILTER_EN          (1U << 15)

/* MCU_SEL values.  Value 0 selects IO_MUX "function 1", which is the
 * peripheral assigned to the pad in the TRM's IO_MUX table; value 1
 * hands the pad to the GPIO matrix. */
#define IO_MUX_MCU_SEL_FUNC1      0U
#define IO_MUX_MCU_SEL_GPIO       1U

/* ================================================================== */
/*  UART (0x60000000)                                                 */
/* ================================================================== */
/*
 * Register names follow the SVD: FIFO, INT_RAW, INT_ST, INT_ENA, INT_CLR,
 * CLKDIV, RX_FILT, STATUS, CONF0, CONF1, ...
 */
#define UART_FIFO                 (UART0_BASE + 0x0000)
#define UART_INT_RAW              (UART0_BASE + 0x0004)
#define UART_INT_ST               (UART0_BASE + 0x0008)
#define UART_INT_ENA              (UART0_BASE + 0x000C)
#define UART_INT_CLR              (UART0_BASE + 0x0010)
#define UART_CLKDIV               (UART0_BASE + 0x0014)
#define UART_RX_FILT              (UART0_BASE + 0x0018)
#define UART_STATUS               (UART0_BASE + 0x001C)
#define UART_CONF0                (UART0_BASE + 0x0020)
#define UART_CONF1                (UART0_BASE + 0x0024)
#define UART_CLK_CONF             (UART0_BASE + 0x0078)

/* STATUS */
#define UART_STATUS_RXFIFO_CNT(s) (((s) >> 0) & 0x3FFU)
#define UART_STATUS_TXFIFO_CNT(s) (((s) >> 16) & 0x3FFU)

/* CLKDIV */
#define UART_CLKDIV_MASK          0x00000FFFU
#define UART_CLKDIV_FRAG_POS      20

/* CONF0 */
#define UART_CONF0_PARITY         (1U << 0)
#define UART_CONF0_PARITY_EN      (1U << 1)
#define UART_CONF0_BIT_NUM_POS    2
#define UART_CONF0_STOP_BIT_POS   4
#define UART_CONF0_TXD_BRK        (1U << 8)
#define UART_CONF0_LOOPBACK       (1U << 14)
#define UART_CONF0_RXFIFO_RST     (1U << 17)
#define UART_CONF0_TXFIFO_RST     (1U << 18)
#define UART_CONF0_MEM_CLK_EN     (1U << 28)

/* BIT_NUM values */
#define UART_BIT_NUM_5            0U
#define UART_BIT_NUM_6            1U
#define UART_BIT_NUM_7            2U
#define UART_BIT_NUM_8            3U

/* STOP_BIT_NUM values */
#define UART_STOP_BIT_1           1U
#define UART_STOP_BIT_1_5         2U
#define UART_STOP_BIT_2           3U

/* CONF1 */
#define UART_CONF1_RXFIFO_FULL_THRHD_POS 0
#define UART_CONF1_TXFIFO_EMPTY_THRHD_POS 10
#define UART_CONF1_RX_TOUT_THRHD_POS     17
#define UART_CONF1_DIS_RX_DAT_OVF (1U << 20)
#define UART_CONF1_RX_TOUT_EN     (1U << 23)

/* Interrupt bits (INT_RAW / INT_ST / INT_ENA / INT_CLR) */
#define UART_INT_RXFIFO_FULL      (1U << 0)
#define UART_INT_TXFIFO_EMPTY     (1U << 1)
#define UART_INT_PARITY_ERR       (1U << 2)
#define UART_INT_FRM_ERR          (1U << 3)
#define UART_INT_RXFIFO_OVF       (1U << 4)
#define UART_INT_BRK_DET          (1U << 7)
#define UART_INT_RXFIFO_TOUT      (1U << 8)
#define UART_INT_TX_DONE          (1U << 14)
#define UART_INT_ALL              0xFFFFFFFFU

/* CLK_CONF */
#define UART_CLK_CONF_SCLK_DIV_B_POS   0
#define UART_CLK_CONF_SCLK_DIV_A_POS   6
#define UART_CLK_CONF_SCLK_DIV_NUM_POS 12
#define UART_CLK_CONF_SCLK_SEL_POS     20
#define UART_CLK_CONF_SCLK_EN          (1U << 22)
#define UART_CLK_CONF_RST_CORE         (1U << 23)
#define UART_CLK_CONF_TX_SCLK_EN       (1U << 24)
#define UART_CLK_CONF_RX_SCLK_EN       (1U << 25)
#define UART_CLK_CONF_TX_RST_CORE      (1U << 26)
#define UART_CLK_CONF_RX_RST_CORE      (1U << 27)

/* UART_SCLK_SEL is an *encoded* field on the ESP32-S3, not a clock id:
 *   0 = reserved, 1 = APB, 2 = RC_FAST, 3 = XTAL.
 * The register's reset value selects XTAL and the boot ROM switches it to
 * APB, so the driver has to select APB explicitly.  (See ESP-IDF's
 * uart_ll_set_sclk() -- this encoding is not the same on every SoC.) */
#define UART_CLK_CONF_SCLK_SEL_MASK    0x3U
#define UART_CLK_CONF_SCLK_SEL_APB     1U
#define UART_CLK_CONF_SCLK_SEL_RC_FAST 2U
#define UART_CLK_CONF_SCLK_SEL_XTAL    3U

/* ================================================================== */
/*  SPI (SPI2 / SPI3) register offsets and fields                     */
/*                                                                    */
/*  Layout taken from the ESP32-S3 TRM / ESP-IDF spi_reg.h.  The      */
/*  GPSPI block starts at SPI2_BASE (0x60024000) and is also called   */
/*  FSPI in the IO_MUX tables.                                        */
/* ================================================================== */

#define SPI_CMD_REG               0x0000
#define SPI_ADDR_REG              0x0004
#define SPI_CTRL_REG              0x0008
#define SPI_CLOCK_REG             0x000C
#define SPI_USER_REG              0x0010
#define SPI_USER1_REG             0x0014
#define SPI_USER2_REG             0x0018
#define SPI_MS_DLEN_REG           0x001C
#define SPI_MISC_REG              0x0020
#define SPI_DMA_CONF_REG          0x0030
#define SPI_W0_REG                0x0098
#define SPI_SLAVE_REG             0x00E0
#define SPI_CLK_GATE_REG          0x00E8

/* CMD */
#define SPI_CMD_UPDATE            (1U << 23)
#define SPI_CMD_USR               (1U << 24)

/* CTRL */
#define SPI_CTRL_WR_BIT_ORDER_POS 25
#define SPI_CTRL_RD_BIT_ORDER_POS 23

/* CLOCK */
#define SPI_CLOCK_CLKCNT_L_POS    0
#define SPI_CLOCK_CLKCNT_H_POS    6
#define SPI_CLOCK_CLKCNT_N_POS    12
#define SPI_CLOCK_CLKDIV_PRE_POS  18
#define SPI_CLOCK_EQU_SYSCLK      (1U << 31)

/* USER */
#define SPI_USER_DOUTDIN          (1U << 0)
#define SPI_USER_CK_OUT_EDGE      (1U << 9)
#define SPI_USER_USR_MOSI         (1U << 27)
#define SPI_USER_USR_MISO         (1U << 28)
#define SPI_USER_USR_DUMMY        (1U << 29)
#define SPI_USER_USR_ADDR         (1U << 30)
#define SPI_USER_USR_COMMAND      (1U << 31)

/* MISC */
#define SPI_MISC_CS_DIS_MASK      0x3FU   /* CS0..CS5_DIS */
#define SPI_MISC_CK_IDLE_EDGE     (1U << 29)

/* DMA_CONF */
#define SPI_DMA_CONF_RX_AFIFO_RST (1U << 29)
#define SPI_DMA_CONF_BUF_AFIFO_RST (1U << 30)

/* CLK_GATE */
#define SPI_CLK_GATE_CLK_EN       (1U << 0)
#define SPI_CLK_GATE_MST_CLK_ACTIVE (1U << 1)
#define SPI_CLK_GATE_MST_CLK_SEL_APB (1U << 2)

/** IO_MUX MCU_SEL value that routes a pad to its FSPI (SPI2) function. */
#define IO_MUX_MCU_SEL_FSPI       4U

/** Maximum number of bytes moved by one GPSPI CPU-controlled transaction. */
#define SPI_CPU_MAX_LEN           64U

/* ================================================================== */
/*  Helpers                                                           */
/* ================================================================== */

#define REG32(addr)      (*(volatile uint32_t *)(addr))

/* ================================================================== */
/*  Simple GPIO helpers                                               */
/*                                                                    */
/*  These drive a pad as a plain general-purpose I/O through the      */
/*  GPIO matrix.  On the ESP32-S3 a pad used as GPIO needs:           */
/*    - IO_MUX: MCU_SEL = 1 (GPIO function), plus the pull resistors  */
/*    - GPIO_ENABLE_W1TS (output) / FUN_IE in IO_MUX (input)          */
/*  The GPIO-matrix output selector already points at GPIO_OUT after   */
/*  reset (FUNC_OUT_SEL_CFG = SIG_GPIO_OUT_IDX), so nothing else is   */
/*  needed for a plain output pin.                                    */
/* ================================================================== */

/* Pads 32..39 live in the "1" bank of the output/enable/input registers. */
#define XT_GPIO_BANK1(n)  ((n) >= 32)

static inline void xt_gpio_set_level(uint pin, uint level)
{
    uint32_t bit = 1UL << (pin & 31U);
    if (level)
    {
        REG32(XT_GPIO_BANK1(pin) ? GPIO_OUT1_W1TS : GPIO_OUT_W1TS) = bit;
    }
    else
    {
        REG32(XT_GPIO_BANK1(pin) ? GPIO_OUT1_W1TC : GPIO_OUT_W1TC) = bit;
    }
}

static inline uint xt_gpio_get_level(uint pin)
{
    uint32_t bit = 1UL << (pin & 31U);
    return (REG32(XT_GPIO_BANK1(pin) ? GPIO_IN1 : GPIO_IN) & bit) ? 1U : 0U;
}

/** Configure a pad as a push-pull output.  \p pull is GPIO_PULL_*. */
static inline void xt_gpio_output(uint pin, uint pull)
{
    uint32_t mux;
    uint32_t bit = 1UL << (pin & 31U);

    mux  = REG32(IO_MUX_GPIO(pin));
    mux &= ~(IO_MUX_MCU_SEL_MASK | IO_MUX_MCU_WPU | IO_MUX_MCU_WPD
             | IO_MUX_FUN_WPU | IO_MUX_FUN_WPD);
    mux |= IO_MUX_MCU_SEL_GPIO;
    if (pull == GPIO_PULL_UP)
    {
        mux |= IO_MUX_FUN_WPU;
    }
    else if (pull == GPIO_PULL_DOWN)
    {
        mux |= IO_MUX_FUN_WPD;
    }
    REG32(IO_MUX_GPIO(pin)) = mux;

    REG32(XT_GPIO_BANK1(pin) ? GPIO_ENABLE1_W1TS : GPIO_ENABLE_W1TS) = bit;
}

/** Configure a pad as an input.  \p pull is GPIO_PULL_*. */
static inline void xt_gpio_input(uint pin, uint pull)
{
    uint32_t mux;
    uint32_t bit = 1UL << (pin & 31U);

    mux  = REG32(IO_MUX_GPIO(pin));
    mux &= ~(IO_MUX_MCU_SEL_MASK | IO_MUX_MCU_WPU | IO_MUX_MCU_WPD
             | IO_MUX_FUN_WPU | IO_MUX_FUN_WPD | IO_MUX_FUN_IE);
    mux |= IO_MUX_MCU_SEL_GPIO | IO_MUX_FUN_IE;
    if (pull == GPIO_PULL_UP)
    {
        mux |= IO_MUX_FUN_WPU;
    }
    else if (pull == GPIO_PULL_DOWN)
    {
        mux |= IO_MUX_FUN_WPD;
    }
    REG32(IO_MUX_GPIO(pin)) = mux;

    REG32(XT_GPIO_BANK1(pin) ? GPIO_ENABLE1_W1TC : GPIO_ENABLE_W1TC) = bit;
}

/* ================================================================== */
/*  GPIO matrix                                                       */
/*                                                                    */
/*  Peripherals with no dedicated IO_MUX function (SDHOST is one)     */
/*  reach their pads through the GPIO matrix: the peripheral picks a  */
/*  signal index and the matrix connects that signal to a pad.        */
/* ================================================================== */

/**
 * Route a peripheral output signal to a pad.  The pad is put in GPIO
 * function and its output enable is left to the peripheral (OEN_SEL
 * clear), which is what a bidirectional bus such as SD CMD/DAT needs.
 * \p pull is GPIO_PULL_*.
 */
static inline void xt_gpio_matrix_out(uint pin, uint sig, uint pull)
{
    uint32_t mux = REG32(IO_MUX_GPIO(pin));

    mux &= ~(IO_MUX_MCU_SEL_MASK | IO_MUX_FUN_WPD | IO_MUX_FUN_WPU
             | IO_MUX_FUN_IE);
    mux |= IO_MUX_MCU_SEL_GPIO | IO_MUX_FUN_IE;
    if (pull == GPIO_PULL_UP)
    {
        mux |= IO_MUX_FUN_WPU;
    }
    else if (pull == GPIO_PULL_DOWN)
    {
        mux |= IO_MUX_FUN_WPD;
    }
    REG32(IO_MUX_GPIO(pin)) = mux;

    /* Keep the GPIO output enable set as well, so the pad drives no
     * matter which signal the hardware uses for the output enable. */
    REG32(XT_GPIO_BANK1(pin) ? GPIO_ENABLE1_W1TS : GPIO_ENABLE_W1TS)
        = 1UL << (pin & 31U);

    REG32(GPIO_FUNC_OUT_SEL_CFG(pin)) = (sig & GPIO_OUT_SEL_MASK);

    /* Let the peripheral sample the pad as well (card responses). */
    REG32(GPIO_FUNC_IN_SEL_CFG(sig)) = (pin & GPIO_IN_SEL_MASK) | GPIO_IN_SEL;
}

/* ================================================================== */
/*  Bit-banged SPI (mode 0)                                           */
/*                                                                    */
/*  The W5500 and the ST7735 both sit on private SPI buses that we    */
/*  drive by hand.  Bit-banging keeps the bring-up simple: it needs   */
/*  no SPI2/SPI3 clock setup, no DMA and no GPIO-matrix routing, at   */
/*  the cost of a few hundred kbit/s -- plenty for these parts.       */
/*                                                                    */
/*  CPOL = 0, CPHA = 0: MOSI is driven while SCK is low and the       */
/*  slave samples it on the rising edge; the slave drives MISO on the */
/*  falling edge, so we sample right after SCK goes high.             */
/* ================================================================== */

/* A few cycles of settling after each clock edge.  The ESP32-S3 GPIO  */
/* input register is synchronous to the APB clock, so sampling too     */
/* soon after the edge can read the previous bit.                      */
#define XT_SPI_DELAY()  do { __asm__ volatile ("nop; nop; nop; nop"); } while (0)

/** Shift one byte out and one byte in, MSB first, SPI mode 0. */
static inline uint8_t xt_spi_xfer(uint sck, uint mosi, uint miso, uint8_t tx)
{
    uint8_t rx = 0;
    int i;

    for (i = 7; i >= 0; i--)
    {
        xt_gpio_set_level(mosi, (tx >> i) & 1U);
        XT_SPI_DELAY();
        xt_gpio_set_level(sck, 1);
        XT_SPI_DELAY();
        rx = (uint8_t)((rx << 1) | (xt_gpio_get_level(miso) ? 1U : 0U));
        xt_gpio_set_level(sck, 0);
        XT_SPI_DELAY();
    }
    return rx;
}

#endif /* _XTENSA_H_ */
void halt(void);
