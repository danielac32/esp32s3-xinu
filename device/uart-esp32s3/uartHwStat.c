/**
 * @file uartHwStat.c
 *
 * Print hardware-specific statistics about the ESP32-S3 UART.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <uart.h>
#include <stdio.h>
#include "esp32uart.h"

void uartHwStat(void *csr)
{
    volatile struct esp32_uart_csreg *regptr = csr;
    uint32_t st = regptr->status;

    printf("\n  RXFIFO: %u  TXFIFO: %u\n",
           UART_STATUS_RXFIFO_CNT(st), UART_STATUS_TXFIFO_CNT(st));
}
