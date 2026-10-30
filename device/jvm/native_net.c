/*
 * native_net.c — natives de esp32p4.Net para el ESP32-S3 (sobre el W5500).
 *
 * Mínimo por ahora: la IPv4 de la interfaz y el estado del enlace.  Donde el
 * P4 usaba el WiFi (wifi_get_ip_str), aquí se consulta la interfaz Ethernet
 * de Xinu y el registro PHYCFGR del W5500.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <thread.h>
#include <device.h>
#include <monitor.h>
#include <network.h>
#include <w5500.h>

#include "simple_jvm.h"
#include "java_lib.h"
#include "jvm_io.h"

/* esp32p4.Net.ip(): la IPv4 de la interfaz, como String Java. */
int esp32p4_net_ip(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    struct netif *iface = netLookup(ETH0);
    char buf[32];

    (void)p;
    (void)type;
    if (iface != NULL)
    {
        netaddrsprintf(buf, &iface->ip);
    }
    else
    {
        strcpy(buf, "0.0.0.0");
    }
    pushInt(stack, heap_new_string(buf));
    return 0;
}

/* esp32p4.Net.isConnected(): 1 cuando el PHY del W5500 ve enlace. */
int esp32p4_net_isConnected(StackFrame *stack, SimpleConstantPool *p,
                            char *type)
{
    struct w5500 *ethptr = &w5500tab[0];
    int up = 0;

    (void)p;
    (void)type;
    if (ethptr->ready)
    {
        unsigned char phycfgr;

        lock(ethptr->lock);
        phycfgr = w5500ReadReg(W5500_PHYCFGR, W5500_BSB_COMMON);
        unlock(ethptr->lock);
        up = (phycfgr & W5500_PHY_LNK) ? 1 : 0;
    }
    pushInt(stack, up);
    return 0;
}
