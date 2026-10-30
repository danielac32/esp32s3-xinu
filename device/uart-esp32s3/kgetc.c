/**
 * @file kgetc.c
 *
 * Synchronously read a character from the ESP32-S3 UART.  This blocks
 * until a character is available.  The interrupt handler is not used.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <uart.h>
#include "esp32uart.h"

syscall kgetc(device *devptr)
{
    struct uart *uartptr = &uarttab[devptr->minor];
    volatile struct esp32_uart_csreg *regptr = devptr->csr;
    uint32_t uart_im;
    uchar c;

    /* Save and disable UART interrupts so the handler cannot consume the
     * character we are waiting for. */
    uart_im = regptr->int_ena;
    regptr->int_ena = 0;

    while (UART_STATUS_RXFIFO_CNT(regptr->status) == 0)
    {
        /* Do nothing */
    }

    c = (uchar)(regptr->fifo & 0xFFU);
    uartptr->cin++;

    regptr->int_ena = uart_im;
    return c;
}
