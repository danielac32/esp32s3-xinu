/**
 * @file memtty.c
 *
 * A tiny output-only TTY.  The web terminal points a command's stdout and
 * stderr at this device, runs the command, and then copies the captured
 * bytes into the HTTP response.  Input is always EOF: interactive commands
 * belong on the serial console or telnet.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <stddef.h>
#include <string.h>
#include <device.h>
#include <memory.h>
#include <psram.h>
#include <memtty.h>

static char *mt_buf;
static volatile int mt_len;
static volatile int mt_trunc;

devcall memttyInit(device *devptr)
{
    (void)devptr;

    if (NULL == mt_buf)
    {
        /* Prefer PSRAM, but keep a small kernel-heap fallback so the shell
         * still works if the external RAM was not detected. */
        mt_buf = (char *)psram_memget(MEMTTY_CAP);
        if (NULL == mt_buf)
            mt_buf = (char *)memget(MEMTTY_CAP);
        if (NULL == mt_buf)
            return SYSERR;
    }

    mt_len = 0;
    mt_trunc = 0;
    return OK;
}

devcall memttyOpen(device *devptr, ...)
{
    (void)devptr;
    return OK;
}

devcall memttyClose(device *devptr)
{
    (void)devptr;
    return OK;
}

/* This device only captures output.  A read returns EOF immediately. */
devcall memttyRead(device *devptr, void *buf, uint len)
{
    (void)devptr;
    (void)buf;
    (void)len;
    return 0;
}

devcall memttyGetc(device *devptr)
{
    (void)devptr;
    return -1;
}

devcall memttyPutc(device *devptr, char ch)
{
    (void)devptr;

    if (NULL == mt_buf)
        return SYSERR;
    if (mt_len < MEMTTY_CAP)
        mt_buf[mt_len++] = ch;
    else
        mt_trunc = 1;
    return OK;
}

devcall memttyWrite(device *devptr, const void *buf, uint len)
{
    const char *p = (const char *)buf;
    uint i;

    (void)devptr;

    if (NULL == mt_buf)
        return SYSERR;

    for (i = 0; i < len; i++)
    {
        if (mt_len < MEMTTY_CAP)
            mt_buf[mt_len++] = p[i];
        else
        {
            mt_trunc = 1;
            break;
        }
    }
    return (devcall)len;
}

devcall memttyControl(device *devptr, int func, long arg1, long arg2)
{
    (void)devptr;
    (void)func;
    (void)arg1;
    (void)arg2;
    return OK;
}

void memttyReset(void)
{
    mt_len = 0;
    mt_trunc = 0;
}

int memttyLength(void)
{
    return mt_len;
}

int memttyTruncated(void)
{
    return mt_trunc;
}

int memttyCopy(char *dst, int max)
{
    int n;

    if (NULL == dst || max <= 1)
        return 0;

    n = mt_len;
    if (n > max - 1)
        n = max - 1;
    if (n > 0 && NULL != mt_buf)
        memcpy(dst, mt_buf, (size_t)n);
    dst[n] = '\0';
    return n;
}
