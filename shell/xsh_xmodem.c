/**
 * @file xsh_xmodem.c
 *
 * Receive a file over the serial line using the XMODEM (CRC) protocol.
 * Based on the public-domain xmodem implementation by Georges Menie.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stdio.h>
#include <string.h>
#include <shell.h>
#include <device.h>
#include <tty.h>
#include <fat_filelib.h>
#include <xsh_fs.h>

#define SOH  0x01
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18

static unsigned short crc16_ccitt(const unsigned char *buf, int len)
{
    unsigned short crc = 0;
    int i, j;
    for (i = 0; i < len; i++)
    {
        crc ^= (unsigned short)buf[i] << 8;
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

shellcmd xsh_xmodem(int nargs, char *args[])
{
    if (!fs_need())
        return SHELL_ERROR;
    if (nargs < 3 || strcmp(args[1], "recv") != 0)
    {
        printf("Usage: xmodem recv <file>\n");
        return SHELL_ERROR;
    }

    char *p = full_path(args[2]);
    FL_FILE *f = fl_fopen(p, "w");
    if (!f)
    {
        printf("xmodem: cannot create '%s'\n", p);
        return SHELL_ERROR;
    }

    /* Raw (uncooked) input is required for binary transfer. */
    control(stdin, TTY_CTRL_SET_IFLAG, TTY_IRAW, NULL);

    unsigned char xbuff[133];
    unsigned char packetno = 1;
    int len = 0;
    int retry, retrans = 25;
    unsigned char trychar = 'C';

    for (;;)
    {
        for (retry = 0; retry < 16; retry++)
        {
            if (trychar)
                putc(stdout, trychar);

            int c = getc(stdin);
            if (c >= 0)
            {
                switch (c)
                {
                case SOH:
                    goto start_recv;
                case EOT:
                    putc(stdout, ACK);
                    control(stdin, TTY_CTRL_CLR_IFLAG, TTY_IRAW, NULL);
                    fl_fclose(f);
                    printf("\nxmodem: done (%d bytes)\n", len);
                    return SHELL_OK;
                case CAN:
                {
                    int c2 = getc(stdin);
                    if (c2 == CAN)
                    {
                        putc(stdout, ACK);
                        control(stdin, TTY_CTRL_CLR_IFLAG, TTY_IRAW, NULL);
                        fl_fclose(f);
                        fl_remove(p);
                        printf("\nxmodem: canceled\n");
                        return SHELL_ERROR;
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
        if (trychar == 'C')
        {
            trychar = NAK;
            continue;
        }
        /* sync error */
        putc(stdout, CAN);
        putc(stdout, CAN);
        putc(stdout, CAN);
        control(stdin, TTY_CTRL_CLR_IFLAG, TTY_IRAW, NULL);
        fl_fclose(f);
        fl_remove(p);
        printf("\nxmodem: sync error\n");
        return SHELL_ERROR;

    start_recv:
        if (trychar == 'C')
            trychar = 0;

        xbuff[0] = SOH;
        for (int i = 1; i < 133; i++)
        {
            int d = getc(stdin);
            if (d < 0)
                goto reject;
            xbuff[i] = (unsigned char)d;
        }

        if (xbuff[1] == (unsigned char)(~xbuff[2]) &&
            (xbuff[1] == packetno || xbuff[1] == (unsigned char)(packetno - 1)))
        {
            unsigned short crc = crc16_ccitt(&xbuff[3], 128);
            unsigned short tcrc = (xbuff[131] << 8) | xbuff[132];
            if (crc == tcrc)
            {
                if (xbuff[1] == packetno)
                {
                    fl_fwrite(&xbuff[3], 1, 128, f);
                    len += 128;
                    packetno++;
                    retrans = 25;
                }
                if (--retrans <= 0)
                {
                    putc(stdout, CAN);
                    putc(stdout, CAN);
                    putc(stdout, CAN);
                    control(stdin, TTY_CTRL_CLR_IFLAG, TTY_IRAW, NULL);
                    fl_fclose(f);
                    fl_remove(p);
                    printf("\nxmodem: too many retries\n");
                    return SHELL_ERROR;
                }
                putc(stdout, ACK);
                continue;
            }
        }

    reject:
        putc(stdout, NAK);
    }
}
