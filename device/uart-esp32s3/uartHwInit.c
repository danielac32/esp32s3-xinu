/**
 * @file uartHwInit.c
 *
 * Hardware initialization for the ESP32-S3 UART.  This is the
 * platform-specific half of the generic Xinu UART driver: uartInit()
 * takes care of the software control block and calls us to bring up the
 * hardware.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <uart.h>
#include <interrupt.h>
#include <platform.h>
#include <xtensa.h>
#include "esp32uart.h"

devcall uartHwInit(device *devptr)
{
    volatile struct esp32_uart_csreg *regptr = devptr->csr;
    uint32_t divider;
    uint32_t sclk;
    uint32_t clkconf;

    /* ---- clocks -------------------------------------------------------
     * The UART's register clock comes from the APB peripheral clock gate,
     * and its bit clock from the UART's own clock gate. */
    REG32(SYSTEM_PERIP_CLK_EN0) |= SYS_CLK_EN_UART0 | SYS_CLK_EN_UART_MEM;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~SYS_CLK_EN_UART0;

    /* UART_SCLK_SEL is an *encoded* field on this SoC (1 = APB, 2 =
     * RC_FAST, 3 = XTAL) and its reset value selects the crystal, so the
     * source has to be selected explicitly.  Leaving the field at 0 --
     * which is reserved, not "APB" -- turns the bit clock off and the UART
     * silently stops transmitting, so never write this register with a
     * bare constant: modify the field and leave DIV_A/DIV_B/DIV_NUM as
     * they are, since they only apply to the non-crystal sources. */
    clkconf  = regptr->clk_conf;
    clkconf &= ~(UART_CLK_CONF_SCLK_SEL_MASK << UART_CLK_CONF_SCLK_SEL_POS);
    clkconf |= (uint32_t)ESP32_UART_SCLK_SEL << UART_CLK_CONF_SCLK_SEL_POS;
    clkconf |= UART_CLK_CONF_SCLK_EN
             | UART_CLK_CONF_TX_SCLK_EN
             | UART_CLK_CONF_RX_SCLK_EN;
    regptr->clk_conf = clkconf;

    /* ---- pins ---------------------------------------------------------
     * TXD/RXD are IO_MUX function 1 on GPIO43/GPIO44.  The RX pad needs
     * its input buffer on, and a pull-up is handy when nothing is
     * connected. */
    REG32(IO_MUX_GPIO(ESP32_UART0_TXD_PIN)) =
        (IO_MUX_MCU_SEL_FUNC1 << IO_MUX_MCU_SEL_POS) | (2U << IO_MUX_FUN_DRV_POS);
    REG32(IO_MUX_GPIO(ESP32_UART0_RXD_PIN)) =
        (IO_MUX_MCU_SEL_FUNC1 << IO_MUX_MCU_SEL_POS) | IO_MUX_FUN_IE
        | IO_MUX_FUN_WPU | (2U << IO_MUX_FUN_DRV_POS);

    /* ---- frame format and FIFOs --------------------------------------- */
    regptr->conf0 |= UART_CONF0_RXFIFO_RST | UART_CONF0_TXFIFO_RST;
    regptr->conf0 &= ~(UART_CONF0_RXFIFO_RST | UART_CONF0_TXFIFO_RST);

    /* 8 data bits, 1 stop bit, no parity, FIFO memory clocked. */
    regptr->conf0 = UART_CONF0_MEM_CLK_EN
                  | (UART_BIT_NUM_8 << UART_CONF0_BIT_NUM_POS)
                  | (UART_STOP_BIT_1 << UART_CONF0_STOP_BIT_POS);

    /* ---- baud rate ----------------------------------------------------
     * The UART derives its bit clock from the source selected above.  The
     * divider is
     *
     *     divider = (sclk * 16) / baud
     *
     * with the low four bits going into the FRAG field and the rest into
     * CLKDIV. */
    sclk = ESP32_UART_SCLK_FREQ;
    divider = (sclk * 16U) / UART_BAUD;
    regptr->clkdiv = (divider >> 4)
                   | ((divider & 0xFU) << UART_CLKDIV_FRAG_POS);

    /* ---- FIFO thresholds and RX timeout -------------------------------
     * The receiver has to interrupt on every burst of input, not only when
     * the FIFO is nearly full: an interactive shell sees one or two bytes
     * at a time.  The FIFO-full threshold is therefore 1 and the RX
     * timeout (the "line went quiet mid-message" interrupt) is armed as
     * well so a lone byte is delivered. */
    regptr->conf1 = (1U << UART_CONF1_RXFIFO_FULL_THRHD_POS)
                  | (0U << UART_CONF1_TXFIFO_EMPTY_THRHD_POS)
                  | (10U << UART_CONF1_RX_TOUT_THRHD_POS)
                  | UART_CONF1_RX_TOUT_EN;

    /* ---- interrupt routing --------------------------------------------
     * The peripheral source has to be mapped onto a CPU interrupt line
     * before the Xtensa core can see it. */
    intc_route(PERIPH_SRC_UART0, devptr->irq);
    interruptVector[devptr->irq] = devptr->intr;

    regptr->int_clr = UART_INT_ALL;
    regptr->int_ena = UART_INT_RXFIFO_FULL | UART_INT_RXFIFO_TOUT
                    | UART_INT_RXFIFO_OVF | UART_INT_FRM_ERR;
    enable_irq(devptr->irq);

    return OK;
}
