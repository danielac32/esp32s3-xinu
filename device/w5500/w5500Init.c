/**
 * @file w5500Init.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: device initialization.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <clock.h>
#include <device.h>
#include <kernel.h>
#include <monitor.h>
#include <stdlib.h>
#include <xtensa.h>

#include <w5500.h>

struct w5500 w5500tab[NW5500];

/* Pin directions and bus bring-up live in w5500Spi.c.  CS/SCK/MOSI are
 * ours to drive; the chip only ever drives MISO.  The reset line (GPIO15)
 * is shared with the TFT panel and is owned by the TFT driver, so we
 * deliberately leave it alone. */

/**
 * @ingroup w5500
 *
 * Initialize a W5500 device: create its monitor, reset the chip and put
 * its socket into MACRAW mode.
 *
 * GPIO15 is the reset line shared with the TFT panel.  That driver owns
 * and pulses it (it runs first at boot), so the panel and this chip both
 * see a hardware reset before either is initialized; here we only use
 * the chip's own software reset (MR.RST) and never touch GPIO15, which
 * would otherwise wipe the panel after it has been brought up.
 *
 * @param devptr W5500 device table entry
 * @return OK if the chip answered and the socket opened, otherwise SYSERR
 */
devcall w5500Init(device *devptr)
{
    struct w5500 *ethptr;
    uint i;
    uint8_t version = 0;

    ethptr = &w5500tab[devptr->minor];
    bzero(ethptr, sizeof(struct w5500));

    /* The lock is needed even if the chip turns out to be missing, so
     * that a failed probe cannot leave a caller waiting on nothing. */
    ethptr->lock = moncreate();
    if (isbadmon(ethptr->lock))
    {
        return SYSERR;
    }

    /* Locally administered station address, unique per device entry. */
    ethptr->mac[0] = 0x02;
    ethptr->mac[1] = 0x00;
    ethptr->mac[2] = 0x00;
    ethptr->mac[3] = 0x00;
    ethptr->mac[4] = 0x00;
    ethptr->mac[5] = (uchar)(0x01 + devptr->minor);

    w5500SpiInit();

    /* Software reset: MR.RST clears itself when the chip is done.  The
     * hardware reset on the shared GPIO15 line was already produced by
     * the TFT driver. */
    w5500WriteReg(W5500_MR, W5500_BSB_COMMON, W5500_MR_RST);
    for (i = 0; i < W5500_TIMEOUT_MS; i++)
    {
        if (!(w5500ReadReg(W5500_MR, W5500_BSB_COMMON) & W5500_MR_RST))
        {
            break;
        }
        udelay(1000);
    }

    /* The only identification the W5500 offers is its version.  Some
     * parts answer 0 for a little while right after a reset. */
    for (i = 0; i < W5500_TIMEOUT_MS; i++)
    {
        version = w5500ReadReg(W5500_VERSIONR, W5500_BSB_COMMON);
        if (W5500_CHIP_VERSION == version)
        {
            break;
        }
        udelay(1000);
    }
    ethptr->version = version;
    if (W5500_CHIP_VERSION != version)
    {
        kprintf("w5500: no chip (VERSIONR=0x%02x)\r\n", version);
        return SYSERR;
    }
    kprintf("w5500: VERSIONR=0x%02x, SPI ready\r\n", version);

    /* Retry time 2000 x 100 us and 8 retries, the chip's defaults. */
    w5500Write16(W5500_RTR, W5500_BSB_COMMON, 0x07D0);
    w5500WriteReg(W5500_RCR, W5500_BSB_COMMON, 0x08);

    /* Give every kilobyte of buffer memory to this socket. */
    w5500WriteReg(W5500_SN_TXBUF_SIZE, W5500_BSB_SOCK_REG(devptr->minor),
                  W5500_BUF_KB);
    w5500WriteReg(W5500_SN_RXBUF_SIZE, W5500_BSB_SOCK_REG(devptr->minor),
                  W5500_BUF_KB);

    for (i = 0; i < 6; i++)
    {
        w5500WriteReg((uint16_t)(W5500_SHAR + i), W5500_BSB_COMMON,
                      ethptr->mac[i]);
    }

    /* MACRAW mode with the MAC address filter on, so only frames
     * addressed to us plus broadcast and multicast are delivered. */
    w5500WriteReg(W5500_SN_IMR, W5500_BSB_SOCK_REG(devptr->minor), 0x00);
    w5500WriteReg(W5500_SN_MR, W5500_BSB_SOCK_REG(devptr->minor),
                  W5500_SMR_MACRAW | W5500_SMR_MFEN);

    if (OK != w5500Command(devptr->minor, W5500_CR_OPEN))
    {
        kprintf("w5500: OPEN timed out\r\n");
        return SYSERR;
    }

    for (i = 0; i < W5500_TIMEOUT_MS; i++)
    {
        if (W5500_SR_MACRAW ==
            w5500ReadReg(W5500_SN_SR, W5500_BSB_SOCK_REG(devptr->minor)))
        {
            ethptr->ready = TRUE;
            return OK;
        }
        udelay(1000);
    }

    kprintf("w5500: socket did not reach MACRAW (Sn_SR=0x%02x)\r\n",
            w5500ReadReg(W5500_SN_SR, W5500_BSB_SOCK_REG(devptr->minor)));
    return SYSERR;
}
