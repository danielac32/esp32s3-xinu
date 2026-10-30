/**
 * @file w5500Interrupt.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: chip interrupt handler.
 *
 * The chip's INTn pin is not routed to the interrupt controller; w5500Read()
 * polls instead.  The handler exists so that the device can carry a vector
 * entry, and it clears a pending socket interrupt to stay harmless if the
 * pin is wired up later.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>

#include <w5500.h>

/**
 * @ingroup w5500
 *
 * Clear the socket's interrupt flags.
 */
interrupt w5500Interrupt(void)
{
    w5500WriteReg(W5500_SN_IR, W5500_BSB_SOCK_REG(0), 0xFF);
}
