/**
 * @file uartInterrupt.c
 *
 * Interrupt handler (lower half) for the ESP32-S3 UART.
 *
 * Only the receiver is interrupt driven.  This port runs its transmitter
 * synchronously (see uartWrite.c), so there is no transmit FIFO interrupt to
 * service.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <thread.h>
#include <device.h>
#include <uart.h>
#include <xtensa.h>
#include "esp32uart.h"

extern int resdefer;

/**
 * Read the UART's receive FIFO into the software input buffer and wake any
 * thread waiting for input.
 *
 * The caller must arrange for rescheduling afterwards: this routine runs with
 * ::resdefer raised so that signaln() cannot switch threads from underneath an
 * interrupt handler that has not finished.
 */
interrupt uartInterruptNoRsch(void)
{
    int u;
    struct uart *uartptr;
    volatile struct esp32_uart_csreg *regptr;
    uchar c;
    uint count;
    uint32_t st;

    resdefer = 1;               /* defer rescheduling until we are done */

    for (u = 0; u < NUART; u++)
    {
        uartptr = &uarttab[u];
        if (NULL == uartptr->csr)
        {
            continue;
        }
        regptr = (volatile struct esp32_uart_csreg *)uartptr->csr;

        /* INT_ST is INT_RAW masked by INT_ENA, i.e. exactly the events this
         * port asked for.  Read it before acknowledging. */
        st = regptr->int_st;

        /* Acknowledge *every* flag, not just the enabled ones.  INT_RAW
         * latches the transmitter flags too (TX_DONE, TXFIFO_EMPTY), and a
         * stale one keeps the UART requesting an interrupt. */
        regptr->int_clr = UART_INT_ALL;

        if (0 == st)
        {
            continue;
        }

        /* ---- receive -------------------------------------------------- */
        if (st & (UART_INT_RXFIFO_FULL | UART_INT_RXFIFO_TOUT))
        {
            uartptr->iirq++;
            count = 0;
            while (UART_STATUS_RXFIFO_CNT(regptr->status) > 0)
            {
                c = (uchar)(regptr->fifo & 0xFFU);
                if (uartptr->icount < UART_IBLEN)
                {
                    uartptr->in[(uartptr->istart +
                                 uartptr->icount) % UART_IBLEN] = c;
                    uartptr->icount++;
                    count++;
                }
                else
                {
                    uartptr->ovrrn++;
                }
            }
            uartptr->cin += count;
            if (count > 0)
            {
                signaln(uartptr->isema, count);
            }
        }

        if (st & UART_INT_RXFIFO_OVF)
        {
            uartptr->ovrrn++;
        }
        if (st & UART_INT_FRM_ERR)
        {
            uartptr->lserr++;
        }
    }
}

interrupt uartInterrupt(void)
{
    uartInterruptNoRsch();

    /* Wake up any threads that were signaled, now that the handler is
     * finished and cannot be re-entered. */
    if (--resdefer > 0)
    {
        resdefer = 0;
        resched();
    }
}
