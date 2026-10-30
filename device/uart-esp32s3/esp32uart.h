/**
 * @file esp32uart.h
 *
 * Register layout and bit fields for the ESP32-S3 UART.
 *
 * The bit positions come from the ESP32-S3 SVD; the UART's registers are
 * contiguous from 0x00 to 0x78, so they can be described by a struct that
 * is overlaid on the device's CSR address (0x60000000 for UART0).
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _ESP32UART_H_
#define _ESP32UART_H_

#include <stddef.h>
#include <stdint.h>
#include <xtensa.h>

/**
 * ESP32-S3 UART register block.
 */
struct esp32_uart_csreg
{
    volatile uint32_t fifo;             /* 0x00 */
    volatile uint32_t int_raw;          /* 0x04 */
    volatile uint32_t int_st;           /* 0x08 */
    volatile uint32_t int_ena;          /* 0x0C */
    volatile uint32_t int_clr;          /* 0x10 */
    volatile uint32_t clkdiv;           /* 0x14 */
    volatile uint32_t rx_filt;          /* 0x18 */
    volatile uint32_t status;           /* 0x1C */
    volatile uint32_t conf0;            /* 0x20 */
    volatile uint32_t conf1;            /* 0x24 */
    volatile uint32_t lowpulse;         /* 0x28 */
    volatile uint32_t highpulse;        /* 0x2C */
    volatile uint32_t rxd_cnt;          /* 0x30 */
    volatile uint32_t flow_conf;        /* 0x34 */
    volatile uint32_t sleep_conf;       /* 0x38 */
    volatile uint32_t swfc_conf0;       /* 0x3C */
    volatile uint32_t swfc_conf1;       /* 0x40 */
    volatile uint32_t txbrk_conf;       /* 0x44 */
    volatile uint32_t idle_conf;        /* 0x48 */
    volatile uint32_t rs485_conf;       /* 0x4C */
    volatile uint32_t at_cmd_precnt;    /* 0x50 */
    volatile uint32_t at_cmd_postcnt;   /* 0x54 */
    volatile uint32_t at_cmd_gaptout;   /* 0x58 */
    volatile uint32_t at_cmd_char;      /* 0x5C */
    volatile uint32_t mem_conf;         /* 0x60 */
    volatile uint32_t mem_tx_status;    /* 0x64 */
    volatile uint32_t mem_rx_status;    /* 0x68 */
    volatile uint32_t fsm_status;       /* 0x6C */
    volatile uint32_t pospulse;         /* 0x70 */
    volatile uint32_t negpulse;         /* 0x74 */
    volatile uint32_t clk_conf;         /* 0x78 */
};

/* The UART's bit clock source.  This port deliberately does not touch the
 * SoC clock tree: the boot ROM leaves the crystal (40 MHz) as the CPU and
 * APB source and selects the crystal as the UART's clock, programming the
 * baud divider for it.  The selector and the frequency below therefore have
 * to stay in step with each other -- see uartHwInit(). */
#define ESP32_UART_SCLK_SEL   UART_CLK_CONF_SCLK_SEL_XTAL
#define ESP32_UART_SCLK_FREQ  40000000U

/* The default console pins.  UART0's TXD/RXD are IO_MUX "function 1" on
 * GPIO43 and GPIO44, which is also their power-on default. */
#define ESP32_UART0_TXD_PIN   43
#define ESP32_UART0_RXD_PIN   44

/* Convenience aliases so the driver reads like the generic uart code. */
#define UART_CONF0_TX_BITS    (UART_BIT_NUM_8 << UART_CONF0_BIT_NUM_POS)
#define UART_CONF0_STOP_BITS  (UART_STOP_BIT_1 << UART_CONF0_STOP_BIT_POS)

#endif /* _ESP32UART_H_ */
