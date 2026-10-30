/**
 * @file uartHwPutc.c
 *
 * Hand one character to the ESP32-S3 UART transmit FIFO.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <uart.h>
#include "esp32uart.h"
#include <interrupt.h>

/**
 * Wait for room in the transmit FIFO and hand it one character.
 *
 * The port runs its transmitter synchronously (see uartWrite.c), so there is
 * no software output buffer to feed and no transmit interrupt to arm: this
 * routine blocks until the FIFO can accept the byte and then writes it.
 *
 * @param csr
 *      Pointer to the UART register block.
 * @param c
 *      Character to transmit.
 */
void uartHwPutc(void *csr, uchar c)
{
    irqmask im = disable();
    volatile struct esp32_uart_csreg *regptr = csr;

    /* Wait for space in the transmit FIFO.  UART_FIFO_LEN is deliberately
     * kept at the conservative value configured for this port; the hardware
     * FIFO is deeper, so this only limits throughput to the line rate. */
    while (UART_STATUS_TXFIFO_CNT(regptr->status) >= UART_FIFO_LEN)
    {
        /* Do nothing */
    }

    regptr->fifo = c;
    restore(im);
}
