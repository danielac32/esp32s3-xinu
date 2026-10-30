/**
 * @file ttyPutc.c
 *
 */
/* Embedded Xinu, Copyright (C) 2009.  All rights reserved. */

#include <stddef.h>
#include <device.h>
#include <tty.h>
#include <interrupt.h>

/**
 * @ingroup tty
 *
 * Write a single character to a TTY.
 * @param devptr TTY device table entry
 * @param ch character to output
 * @return OK if character was written successfully, otherwise SYSERR
 */
devcall ttyPutc(device *devptr, char ch)
{
    irqmask im = disable();
    struct tty *ttyptr = NULL;
    device *phw = NULL;

    /* Setup and error check pointers to structures */
    ttyptr = &ttytab[devptr->minor];
    phw = ttyptr->phw;
    if (NULL == phw)
    {
        restore(im);
        return SYSERR;
    }

    /* Cook special characters */
    switch (ch)
    {
        /* Newline */
    case '\n':
        if (ttyptr->oflags & TTY_ONLCR)
        {
            if (SYSERR == (*phw->putc) (phw, '\r'))
            {
                return SYSERR;
            }
        }
        break;
        /* Carriage return */
    case '\r':
        if (ttyptr->oflags & TTY_OCRNL)
        {
            ch = '\n';
        }
        break;
    }

    /* Write character to underlying device */
    devcall _r = (*phw->putc) (phw, ch);
    restore(im);
    return _r;
}
