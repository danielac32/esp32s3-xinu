/**
 * @file uartWrite.c
 */

/* Embedded Xinu, Copyright (C) 2009, 2013.  All rights reserved. */

#include <uart.h>
#include <interrupt.h>

/**
 * @ingroup uartgeneric
 *
 * Write a buffer of data to a UART.
 *
 * The generic Xinu UART driver normally buffers output in software and lets
 * the interrupt handler push bytes into the hardware FIFO.  The ESP32-S3's
 * UART cannot drive that scheme: its transmit-FIFO status register does not
 * report the queue depth reliably and its "FIFO below threshold" interrupt
 * is level-sensitive on that same unreliable depth, so a buffered transmitter
 * can stop making progress.  This port therefore runs the transmitter
 * synchronously: uartHwPutc() waits for room in the FIFO and writes the byte
 * itself, exactly as kputc() does for the kernel console.  The call blocks
 * while the FIFO is full, which is the documented behaviour of a UART write
 * anyway.
 *
 * @param devptr
 *      Pointer to the device table entry for a UART.
 * @param buf
 *      Pointer to the buffer of data to write.
 * @param len
 *      Number of bytes to write.
 *
 * @return
 *      On success, returns the number of bytes written, which is @p len.
 *      Returns ::SYSERR if uartInit() has not yet been called.
 */
devcall uartWrite(device *devptr, const void *buf, uint len)
{
    struct uart *uartptr = &uarttab[devptr->minor];
    uint count;

    /* Make sure uartInit() has run.  */
    if (NULL == uartptr->csr)
    {
        return SYSERR;
    }

    for (count = 0; count < len; count++)
    {
        uartHwPutc(uartptr->csr, ((const uchar *)buf)[count]);
        uartptr->cout++;
    }

    /* The transmitter is not buffered in software, so it is always idle.  */
    uartptr->oidle = TRUE;

    return count;
}
