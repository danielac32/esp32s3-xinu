/**
 * @file w5500Read.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: receive one frame.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <kernel.h>
#include <monitor.h>
#include <stddef.h>
#include <thread.h>

#include <w5500.h>

/* How many 10 ms ticks a read is willing to wait for a frame before it
 * gives up and reports TIMEOUT. */
#define W5500_RX_TRIES      100

/**
 * @ingroup w5500
 *
 * Receive one raw Ethernet frame.  In MACRAW mode the chip prefixes each
 * frame with its length, in big endian, and that length counts the two
 * prefix bytes as well.
 *
 * Frames are collected by polling Sn_RX_RSR rather than by the chip's
 * INTn pin, so the read blocks (in 10 ms slices) until a frame arrives.
 *
 * The wait happens outside the monitor.  A caller is allowed to be killed
 * while it blocks here (the DHCP client kills its receiver thread), and a
 * thread that dies while holding the monitor would leave the controller
 * locked for every other thread.
 *
 * @param devptr W5500 device table entry
 * @param buf buffer for the frame, whose MAC header is included
 * @param len size of @p buf
 * @return number of frame bytes copied, SYSERR on error, TIMEOUT if no
 *         frame arrived in time
 */
devcall w5500Read(device *devptr, void *buf, uint len)
{
    struct w5500 *ethptr;
    int sock = devptr->minor;
    uint16_t rxsize;
    uint16_t rd;
    uint8_t hdr[2];
    uint frame_len;
    uint copy_len;
    uint tries;

    ethptr = &w5500tab[sock];

    if (!ethptr->ready || NULL == buf || 0 == len ||
        isbadmon(ethptr->lock))
    {
        return SYSERR;
    }

    for (tries = 0; tries < W5500_RX_TRIES; tries++)
    {
        lock(ethptr->lock);

        rxsize = w5500Read16(W5500_SN_RX_RSR, W5500_BSB_SOCK_REG(sock));
        if (0 == rxsize)
        {
            unlock(ethptr->lock);
            sleep(1);
            continue;
        }

        rd = w5500Read16(W5500_SN_RX_RD, W5500_BSB_SOCK_REG(sock));
        w5500ReadBuf(rd, W5500_BSB_SOCK_RX(sock), hdr, 2);
        frame_len = (uint)((hdr[0] << 8) | hdr[1]);

        if (frame_len < 2 || frame_len > (W5500_FRAME_MAX + 2))
        {
            /* Nonsense length: drop the record and resynchronise. */
            frame_len = 2;
        }

        copy_len = frame_len - 2;
        if (copy_len > len)
        {
            copy_len = len;
        }

        w5500ReadBuf((uint16_t)(rd + 2), W5500_BSB_SOCK_RX(sock), buf,
                     copy_len);

        /* Tell the chip how much was consumed, then commit with RECV. */
        w5500Write16(W5500_SN_RX_RD, W5500_BSB_SOCK_REG(sock),
                     (uint16_t)(rd + frame_len));
        w5500Command(sock, W5500_CR_RECV);

        unlock(ethptr->lock);

        return (devcall)(frame_len - 2);
    }

    return TIMEOUT;
}
