/**
 * @file     main.c
 */
/* Embedded Xinu, Copyright (C) 2009, 2013.  All rights reserved. */

#include <device.h>
#include <platform.h>
#include <shell.h>
#include <stdio.h>
#include <thread.h>
#include <tft.h>
#include <version.h>
#include <psram.h>
#include <rgb.h>
#include "st7735.h"
static void print_os_info(void);

thread main(void)
{
    int console_ok = 0;

    /* Print information about the operating system  */
    print_os_info();
    

   // control(RGB0, RGB_CTRL_SET, (long)0xff0000, 0);
    /* Set up the console TTY over SERIAL0.  The serial console is the
     * only real text console; the TFT is write-only and ETH0 is a packet
     * device, so neither can be handed to shell() directly. */
    if (OK == open(CONSOLE, SERIAL0))
    {
        console_ok = 1;
    }
    else
    {
        kprintf("WARNING: Can't open CONSOLE over SERIAL0\r\n");
    }

    /* Bring up the display before anything else and before the network
     * device.  The panel's reset line (GPIO15) is shared with the W5500,
     * and this driver owns it: the pulse it produces here also resets the
     * W5500, whose driver never drives the line itself. */
    if (SYSERR == open(TFT0))
    {
        kprintf("WARNING: failed to open %s\r\n", devtab[TFT0].name);
    }
    else
    {
        control(TFT0, TFTCTL_CLEAR, TFT_BLACK, 0);
        control(TFT0, TFTCTL_SETCOLOR, TFT_WHITE, TFT_BLACK);
        control(TFT0, TFTCTL_CURSOR, 4, 4);
        write(TFT0, "Xinu ESP32-S3", 13);
        control(TFT0, TFTCTL_SETCOLOR, TFT_YELLOW, TFT_BLACK);
        control(TFT0, TFTCTL_CURSOR, 4, 20);
        write(TFT0, "Network: starting", 17);
        control(TFT0, TFTCTL_FLUSH, 0, 0);
        sleep(1000);
    }   

    /* Open the network device. */
    if (SYSERR == open(ETH0))
    {
        kprintf("WARNING: failed to open %s\r\n", devtab[ETH0].name);
    }

    /* Bring up the network and start the servers.  netup runs to
     * completion right here, before any shell exists, so its DHCP chatter
     * comes out in one piece.  telnetserver and httpd create their own
     * worker threads and return immediately. */
    {
        /* netup [<DEVICE> [<IP> [<MASK> [<GATEWAY>]]]]
         *   Brings up a network interface.  With no args: ETH0 + DHCP. */
        char *netup_argv[] = {"netup", NULL};

        /* telnetserver [-d device] [-p port] [-n count] [-h]
         *   Spawns Telnet servers.  -n 1 = one server thread. */
        char *telnet_argv[] = {"telnetserver", "-n", "1", NULL};

        /* httpd [port]
         *   Starts the HTTP server.  Port 80 by default. */
        char *httpd_argv[] = {"httpd", "80", NULL};

        xsh_netup(1, netup_argv);
        xsh_telnetserver(3, telnet_argv);
        xsh_httpd(2, httpd_argv);
    }

    /* Start the serial shell.  Telnet and web shells are created by their
     * respective servers started above. */
    if (console_ok)
    {
        tid_typ tid = create(shell, INITSTK, INITPRIO, "SHELL0", 3,
                             CONSOLE, CONSOLE, CONSOLE);
        if (SYSERR != tid)
        {
            ready(tid, RESCHED_NO);
        }
        else
        {
            kprintf("WARNING: Failed to create SHELL0\r\n");
        }
    }

    return 0;
}

extern void _start(void);

static void print_os_info(void)
{
    kprintf(VERSION);
    kprintf("\r\n\r\n");

    kprintf("%*ld bytes physical memory.\r\n",
            10, (uintptr_t)platform.maxaddr - (uintptr_t)platform.minaddr);

    kprintf("%*ld bytes reserved system area.\r\n",
            10, (uintptr_t)&_end - (uintptr_t)platform.minaddr);

    kprintf("%*ld bytes Xinu code.\r\n", 10, (uintptr_t)&_etext - (uintptr_t)_start);

    kprintf("%*ld bytes stack space.\r\n",
            10, (uintptr_t)memheap - (uintptr_t)&_end);

    kprintf("%*ld bytes heap space.\r\n",
            10, (uintptr_t)platform.maxaddr - (uintptr_t)memheap);

    if (psram_present && psram_size > 0)
    {
        kprintf("%*ld bytes external PSRAM (0x%08lX).\r\n",
                10, (uintptr_t)psram_size, (uintptr_t)PSRAM_BASE);
    }
    else
    {
        kprintf("%*s PSRAM st%d fl %02X%02X%02X/%02X%02X%02X id %02X%02X%02X\r\n",
                10, "no", psram_init_status,
                psram_dbg_flash[0], psram_dbg_flash[1], psram_dbg_flash[2],
                psram_dbg_flash2[0], psram_dbg_flash2[1], psram_dbg_flash2[2],
                psram_dbg_id[0], psram_dbg_id[1], psram_dbg_id[2]);
        kprintf("          ctrl %08lX clk %08lX misc %08lX spin %lu\r\n",
                (ulong)psram_dbg_ctrl, (ulong)psram_dbg_clk,
                (ulong)psram_dbg_misc, (ulong)psram_dbg_spin);
    }
    kprintf("\r\n");
}
