/**
 * @file w5500Command.c
 *
 * W5500 SPI Ethernet controller for the ESP32-S3: socket commands.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <clock.h>
#include <device.h>
#include <stdint.h>

#include <w5500.h>

/**
 * @ingroup w5500
 *
 * Issue a socket command and wait for the chip to accept it.  The chip
 * clears Sn_CR itself when it has latched the command, so reading back a
 * zero is the acknowledgement.
 *
 * @param sock    socket number
 * @param command W5500_CR_* command
 * @return OK when the command was taken, TIMEOUT otherwise
 */
int w5500Command(int sock, uint8_t command)
{
    uint i;

    w5500WriteReg(W5500_SN_CR, W5500_BSB_SOCK_REG(sock), command);

    for (i = 0; i < W5500_TIMEOUT_MS; i++)
    {
        if (0 == w5500ReadReg(W5500_SN_CR, W5500_BSB_SOCK_REG(sock)))
        {
            return OK;
        }
        udelay(1000);
    }
    return TIMEOUT;
}
