/**
 * @file     xsh_w5500.c
 *
 * Shell command (w5500) reports the state of the on-board W5500 Ethernet
 * controller: the chip's version, its station address, the PHY link and
 * the MACRAW socket.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <w5500.h>

#if NW5500

/**
 * @ingroup shell
 *
 * Report the W5500 controller's state.
 *
 * @param nargs number of arguments in args array
 * @param args  array of arguments
 * @return 0 on success, 1 on error
 */
shellcmd xsh_w5500(int nargs, char *args[])
{
    struct w5500 *ethptr;
    uint8_t phycfgr;
    uint8_t sock_status;
    uint16_t rx_pending;
    int i;

    if (nargs == 2 && strcmp(args[1], "--help") == 0)
    {
        printf("Usage: %s\n\n", args[0]);
        printf("Description:\n");
        printf("\tDisplays the status of the W5500 Ethernet controller.\n");
        printf("Options:\n");
        printf("\t--help\t display this help and exit\n");
        return 0;
    }

    if (nargs > 1)
    {
        fprintf(stderr, "%s: too many arguments\n", args[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", args[0]);
        return 1;
    }

    ethptr = &w5500tab[0];

    if (!ethptr->ready)
    {
        printf("w5500: controller did not respond at boot\n");
        return 1;
    }

    /* The controller's monitor may have been left locked by a thread that
     * died while using the device; taking it unconditionally would hang
     * this command, and reading the registers without it would corrupt the
     * access the driver is making.  So report the monitor and only look at
     * the chip when the lock is free. */
    if (montab[ethptr->lock].count != 0)
    {
        printf("  monitor     : owner %d count %u -- still locked, so the\n"
               "                chip registers are not read (the driver may\n"
               "                be blocked, or simply polling)\n",
               (int)montab[ethptr->lock].owner,
               montab[ethptr->lock].count);
        return 0;
    }

    lock(ethptr->lock);

    printf("W5500 Ethernet controller\n");
    printf("-------------------------\n");
    printf("  version     : 0x%02x%s\n", ethptr->version,
           (W5500_CHIP_VERSION == ethptr->version) ? " (W5500)" : "");
    printf("  MAC address : ");
    for (i = 0; i < 6; i++)
    {
        printf("%02x%s", ethptr->mac[i], (i < 5) ? ":" : "");
    }
    printf("\n");

    printf("  SHAR        : ");
    for (i = 0; i < 6; i++)
    {
        printf("%02x%s", w5500ReadReg((uint16_t)(W5500_SHAR + i),
                                      W5500_BSB_COMMON),
               (i < 5) ? ":" : "");
    }
    printf("  (in the chip)\n");

    phycfgr = w5500ReadReg(W5500_PHYCFGR, W5500_BSB_COMMON);
    printf("  PHYCFGR     : 0x%02x  link %s, %s, %s duplex\n", phycfgr,
           (phycfgr & W5500_PHY_LNK) ? "up  " : "down",
           (phycfgr & W5500_PHY_SPD) ? "100 Mbps" : " 10 Mbps",
           (phycfgr & W5500_PHY_DPX) ? "full" : "half");

    sock_status = w5500ReadReg(W5500_SN_SR, W5500_BSB_SOCK_REG(0));
    printf("  Sn_SR       : 0x%02x%s\n", sock_status,
           (W5500_SR_MACRAW == sock_status) ? " (MACRAW)" : "");

    rx_pending = w5500Read16(W5500_SN_RX_RSR, W5500_BSB_SOCK_REG(0));
    printf("  Sn_RX_RSR   : %u bytes waiting\n", rx_pending);

    printf("  Sn_IR       : 0x%02x\n",
           w5500ReadReg(W5500_SN_IR, W5500_BSB_SOCK_REG(0)));
    printf("  Sn_TX_FSR   : %u bytes free\n",
           w5500Read16(W5500_SN_TX_FSR, W5500_BSB_SOCK_REG(0)));
    printf("  Sn_TX_WR    : 0x%04x\n",
           w5500Read16(W5500_SN_TX_WR, W5500_BSB_SOCK_REG(0)));
    printf("  Sn_RX_RD    : 0x%04x\n",
           w5500Read16(W5500_SN_RX_RD, W5500_BSB_SOCK_REG(0)));

    unlock(ethptr->lock);

    return 0;
}

#endif                          /* NW5500 */
