/**
 * @file kputc.c
 *
 * Synchronously write a character to the ESP32-S3 UART.  This blocks until
 * the character has been handed to the transmit FIFO.  The interrupt
 * handler is not used, which makes this safe to call before the kernel
 * scheduler is running.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <uart.h>
#include "esp32uart.h"

syscall kputc(uchar c, device *devptr)
{
    struct uart *uartptr = &uarttab[devptr->minor];
    volatile struct esp32_uart_csreg *regptr = devptr->csr;
    uint32_t uart_im;

    /* Save and disable UART interrupts so the handler cannot steal the
     * FIFO out from under us. */
    uart_im = regptr->int_ena;
    regptr->int_ena = 0;

    /* Wait for space in the transmit FIFO. */
    while (UART_STATUS_TXFIFO_CNT(regptr->status) >= UART_FIFO_LEN)
    {
        /* Do nothing */
    }

    regptr->fifo = c;
    uartptr->cout++;

    regptr->int_ena = uart_im;
    return c;
}
