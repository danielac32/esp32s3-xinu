/**
 * @file w5500Write.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: transmit one frame.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>
#include <stddef.h>
#include <thread.h>

#include <w5500.h>

/* How many 10 ms ticks a write is willing to wait for room in the chip's
 * transmit buffer. */
#define W5500_TX_TRIES      100

/**
 * @ingroup w5500
 *
 * Queue one raw Ethernet frame for transmission.  The caller supplies the
 * whole frame, MAC header included; the chip appends the FCS.
 *
 * @param devptr W5500 device table entry
 * @param buf frame to send
 * @param len frame length, including the 14 byte MAC header
 * @return @p len on success, SYSERR on error, TIMEOUT if the chip's
 *         transmit buffer stayed full
 */
devcall w5500Write(device *devptr, const void *buf, uint len)
{
    struct w5500 *ethptr;
    int sock = devptr->minor;
    uint16_t free_size = 0;
    uint16_t wr;
    uint tries;

    ethptr = &w5500tab[sock];

    if (!ethptr->ready || NULL == buf || isbadmon(ethptr->lock))
    {
        return SYSERR;
    }
    if (len > W5500_FRAME_MAX || len < 14)
    {
        return SYSERR;
    }

    for (tries = 0; tries < W5500_TX_TRIES; tries++)
    {
        lock(ethptr->lock);

        free_size = w5500Read16(W5500_SN_TX_FSR, W5500_BSB_SOCK_REG(sock));
        if (free_size < len)
        {
            /* No room yet.  Wait outside the monitor so that the receive
             * path (and any other thread) keeps running. */
            unlock(ethptr->lock);
            sleep(1);
            continue;
        }

        wr = w5500Read16(W5500_SN_TX_WR, W5500_BSB_SOCK_REG(sock));
        w5500WriteBuf(wr, W5500_BSB_SOCK_TX(sock), buf, len);
        w5500Write16(W5500_SN_TX_WR, W5500_BSB_SOCK_REG(sock),
                     (uint16_t)(wr + len));

        if (OK != w5500Command(sock, W5500_CR_SEND))
        {
            unlock(ethptr->lock);
            return TIMEOUT;
        }

        unlock(ethptr->lock);
        return (devcall)len;
    }

    return TIMEOUT;
}
