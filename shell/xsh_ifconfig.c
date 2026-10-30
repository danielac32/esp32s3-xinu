/**
 * @file xsh_ifconfig.c
 *
 * Show the state of the Ethernet interface.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <string.h>
#include <shell.h>
#include <device.h>
#include <network.h>
#include <ipv4.h>

static void show_iface(int dev)
{
    struct netif *iface = netLookup(dev);
    char buf[32];

    if (!iface)
    {
        printf("%s: no such interface\n", devtab[dev].name);
        return;
    }

    printf("%s: flags=UP,BROADCAST,RUNNING\n", devtab[dev].name);
    netaddrsprintf(buf, &iface->ip);
    printf("        inet %s\n", buf);
    netaddrsprintf(buf, &iface->mask);
    printf("        netmask %s\n", buf);
    netaddrsprintf(buf, &iface->gateway);
    printf("        gateway %s\n", buf);
    netaddrsprintf(buf, &iface->hwaddr);
    printf("        ether %s\n", buf);
    printf("        mtu %u\n", iface->mtu);
}

shellcmd xsh_ifconfig(int nargs, char *args[])
{
    (void)args;

    if (nargs <= 1 || !strcmp(args[1], "-a"))
    {
        show_iface(ETH0);
        return SHELL_OK;
    }

    if (!strcmp(args[1], "--help"))
    {
        printf("Usage: ifconfig [-a]\n");
        printf("Shows the configured IPv4 address, netmask, gateway and MAC.\n");
        return SHELL_OK;
    }

    printf("ifconfig: unknown option '%s'\n", args[1]);
    return SHELL_ERROR;
}
