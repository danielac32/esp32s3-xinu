/**
 * @file w5500Control.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: device control.
 *
 * Besides the driver's own queries, this implements the interface the Xinu
 * network stack expects of a network device (see <network.h>): the MTU, the
 * length of the link layer header, and the hardware and broadcast
 * addresses, plus the <ether.h> control codes used by the DHCP client.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <ether.h>               /* ETH_CTRL_* */
#include <ethernet.h>
#include <monitor.h>
#include <network.h>
#include <stddef.h>
#include <string.h>

#include <w5500.h>

/* Copy the six byte station address out of the control block. */
static void w5500GetMac(struct w5500 *ethptr, uchar *dst)
{
    lock(ethptr->lock);
    memcpy(dst, ethptr->mac, ETH_ADDR_LEN);
    unlock(ethptr->lock);
}

/**
 * @ingroup w5500
 *
 * Manipulate and query the Ethernet device.
 *
 * @param devptr W5500 device table entry
 * @param func control function to execute
 * @param arg1 argument to the control function
 * @param arg2 unused
 * @return depends on @p func, SYSERR for an unknown function
 */
devcall w5500Control(device *devptr, int func, long arg1, long arg2)
{
    struct w5500 *ethptr;
    int sock = devptr->minor;
    uint i;

    (void)arg2;

    ethptr = &w5500tab[sock];

    if (isbadmon(ethptr->lock))
    {
        return SYSERR;
    }

    switch (func)
    {
        /* ---------------------------------------------------------- */
        /* Interface used by the Xinu network stack                    */
        /* ---------------------------------------------------------- */

    case NET_GET_MTU:
        return W5500_MTU;

    case NET_GET_LINKHDRLEN:
        return ETH_HDR_LEN;

    case NET_GET_HWADDR:
    case NET_GET_HWBRC:
    {
        struct netaddr *addr = (struct netaddr *)arg1;

        if (NULL == addr)
        {
            return SYSERR;
        }

        addr->type = NETADDR_ETHERNET;
        addr->len = ETH_ADDR_LEN;
        if (NET_GET_HWBRC == func)
        {
            memset(addr->addr, 0xFF, ETH_ADDR_LEN);
        }
        else
        {
            w5500GetMac(ethptr, addr->addr);
        }
        return OK;
    }

        /* ---------------------------------------------------------- */
        /* The interface the DHCP client uses (see <ether.h>)          */
        /* ---------------------------------------------------------- */

    case ETH_CTRL_GET_MAC:
    case W5500_CTRL_MAC_GET:
    {
        uchar *dst = (uchar *)arg1;

        if (NULL == dst)
        {
            return SYSERR;
        }
        w5500GetMac(ethptr, dst);
        return OK;
    }

    case ETH_CTRL_SET_MAC:
    case W5500_CTRL_MAC_SET:
    {
        const uchar *src = (const uchar *)arg1;

        if (NULL == src)
        {
            return SYSERR;
        }
        if (!ethptr->ready)
        {
            return SYSERR;
        }
        lock(ethptr->lock);
        for (i = 0; i < ETH_ADDR_LEN; i++)
        {
            ethptr->mac[i] = src[i];
            w5500WriteReg((uint16_t)(W5500_SHAR + i), W5500_BSB_COMMON,
                          src[i]);
        }
        unlock(ethptr->lock);
        return OK;
    }

        /* ---------------------------------------------------------- */
        /* Driver specific queries                                      */
        /* ---------------------------------------------------------- */

    case W5500_CTRL_LINK:
        lock(ethptr->lock);
        i = (w5500ReadReg(W5500_PHYCFGR, W5500_BSB_COMMON) & W5500_PHY_LNK)
            ? 1 : 0;
        unlock(ethptr->lock);
        return i;

    case W5500_CTRL_PHYCFGR:
        lock(ethptr->lock);
        i = w5500ReadReg(W5500_PHYCFGR, W5500_BSB_COMMON);
        unlock(ethptr->lock);
        return i;

    default:
        return SYSERR;
    }
}
