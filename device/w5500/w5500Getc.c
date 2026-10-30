/**
 * @file w5500Getc.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: character interface.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>

#include <w5500.h>

/**
 * @ingroup w5500
 *
 * The W5500 is frame oriented; it has no character interface.
 *
 * @param devptr W5500 device table entry
 * @return SYSERR
 */
devcall w5500Getc(device *devptr)
{
    (void)devptr;
    return SYSERR;
}
