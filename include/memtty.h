/**
 * @file memtty.h
 *
 * Memory-backed TTY used by the web terminal to capture a command's
 * stdout/stderr without touching the serial console.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _MEMTTY_H_
#define _MEMTTY_H_

#include <device.h>

#define MEMTTY_CAP 8192

/* Driver entry points (conf.c takes their addresses). */
devcall memttyInit(device *);
devcall memttyOpen(device *, ...);
devcall memttyClose(device *);
devcall memttyRead(device *, void *, uint);
devcall memttyWrite(device *, const void *, uint);
devcall memttyGetc(device *);
devcall memttyPutc(device *, char);
devcall memttyControl(device *, int, long, long);

/* Clear the capture buffer.  The caller must ensure no thread is writing
 * to it (the web terminal's job monitor provides that guarantee). */
void memttyReset(void);

int memttyLength(void);
int memttyTruncated(void);

/* Copy at most max-1 bytes into dst and NUL-terminate.  Returns the number
 * of bytes copied. */
int memttyCopy(char *dst, int max);

#endif /* _MEMTTY_H_ */
