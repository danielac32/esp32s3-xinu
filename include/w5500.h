/**
 * @file w5500.h
 *
 * W5500 SPI Ethernet controller driver for the ESP32-S3.
 *
 * The chip answers a four byte SPI header followed by the payload:
 *
 *     [ addr_hi ][ addr_lo ][ control ][ data... ]
 *
 * where the control byte is
 *
 *     bit  7..3   BSB  (register block select)
 *     bit  2      RWB  (0 = read, 1 = write)
 *     bit  1..0   OM   (00 = variable length data mode, framed by CS)
 *
 * Register addresses inside a block are 16 bits, so the whole address
 * space is 8 blocks of 64 KiB.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _W5500_H_
#define _W5500_H_

#include <device.h>
#include <monitor.h>
#include <stddef.h>
#include <stdint.h>
#include <conf.h>

/* ------------------------------------------------------------------ */
/* Board wiring: pins 10..13 are the FSPI default route (CS/SCK/MOSI/
 * MISO), so the hardware GPSPI can drive them through IO_MUX. */
/* ------------------------------------------------------------------ */
#define W5500_PIN_CS      10
#define W5500_PIN_MOSI    11
#define W5500_PIN_SCK     12
#define W5500_PIN_MISO    13
#define W5500_PIN_IRQ     14
#define W5500_PIN_RST     15    /* shared with the TFT reset */

/* ------------------------------------------------------------------ */
/* SPI framing                                                        */
/* ------------------------------------------------------------------ */
#define W5500_BSB_COMMON        0x00    /**< common register block      */
#define W5500_BSB_SOCK_REG(n)   (1 + 4 * (n))   /**< socket n register  */
#define W5500_BSB_SOCK_TX(n)    (2 + 4 * (n))   /**< socket n TX buffer */
#define W5500_BSB_SOCK_RX(n)    (3 + 4 * (n))   /**< socket n RX buffer */

#define W5500_RWB_READ          0x00
#define W5500_RWB_WRITE         0x04

/* ------------------------------------------------------------------ */
/* Common registers                                                   */
/* ------------------------------------------------------------------ */
#define W5500_MR            0x0000  /* Mode Register                 */
#define W5500_GAR           0x0001  /* Gateway Address       (4)     */
#define W5500_SUBR          0x0005  /* Subnet Mask           (4)     */
#define W5500_SHAR          0x0009  /* Source Hardware Addr  (6)     */
#define W5500_SIPR          0x000F  /* Source IP Address     (4)     */
#define W5500_INTLEVEL      0x0013
#define W5500_IR            0x0015  /* Interrupt Register            */
#define W5500_IMR           0x0016  /* Interrupt Mask Register       */
#define W5500_SIR           0x0017  /* Socket Interrupt Register     */
#define W5500_SIMR          0x0018  /* Socket Interrupt Mask         */
#define W5500_RTR           0x0019  /* Retry Time            (2)     */
#define W5500_RCR           0x001B  /* Retry Count                   */
#define W5500_PHYCFGR       0x002E  /* PHY Configuration             */
#define W5500_VERSIONR      0x0039  /* Chip version                  */

/* MR bits */
#define W5500_MR_RST        0x80
#define W5500_MR_PB         0x10    /* ping block */
#define W5500_CHIP_VERSION  0x04

/* PHYCFGR bits */
#define W5500_PHY_LNK       0x01    /* 1 = link up      */
#define W5500_PHY_SPD       0x02    /* 1 = 100 Mbps     */
#define W5500_PHY_DPX       0x04    /* 1 = full duplex  */
#define W5500_PHY_RST       0x80    /* 0 = reset phy    */

/* ------------------------------------------------------------------ */
/* Socket 0 registers (offset inside the socket register block)        */
/* ------------------------------------------------------------------ */
#define W5500_SN_MR         0x0000  /* Socket Mode                   */
#define W5500_SN_CR         0x0001  /* Socket Command                */
#define W5500_SN_IR         0x0002  /* Socket Interrupt              */
#define W5500_SN_SR         0x0003  /* Socket Status                 */
#define W5500_SN_RXBUF_SIZE 0x001E
#define W5500_SN_TXBUF_SIZE 0x001F
#define W5500_SN_TX_FSR     0x0020  /* TX Free Size          (2)     */
#define W5500_SN_TX_RD      0x0022
#define W5500_SN_TX_WR      0x0024
#define W5500_SN_RX_RSR     0x0026  /* Received Size         (2)     */
#define W5500_SN_RX_RD      0x0028
#define W5500_SN_RX_WR      0x002A
#define W5500_SN_IMR        0x002C

/* Sn_MR protocol field */
#define W5500_SMR_CLOSED    0x00
#define W5500_SMR_TCP       0x01
#define W5500_SMR_UDP       0x02
#define W5500_SMR_MACRAW    0x04    /* MAC LAYER RAW SOCK (socket 0 only) */
#define W5500_SMR_MFEN      0x80    /* MAC filter enable                  */

/* Sn_CR commands */
#define W5500_CR_OPEN       0x01
#define W5500_CR_LISTEN     0x02
#define W5500_CR_CONNECT    0x04
#define W5500_CR_DISCON     0x08
#define W5500_CR_CLOSE      0x10
#define W5500_CR_SEND       0x20
#define W5500_CR_SEND_MAC   0x21
#define W5500_CR_RECV       0x40

/* Sn_SR values */
#define W5500_SR_CLOSED     0x00
#define W5500_SR_INIT       0x13
#define W5500_SR_UDP        0x22
#define W5500_SR_MACRAW     0x42

/* Sn_IR bits */
#define W5500_SIR_CON       0x01
#define W5500_SIR_DISCON    0x02
#define W5500_SIR_RECV      0x04
#define W5500_SIR_TIMEOUT   0x08
#define W5500_SIR_SENDOK    0x10

/* ------------------------------------------------------------------ */
/* Driver interface                                                   */
/* ------------------------------------------------------------------ */

#define W5500_CTRL_MAC_GET  0x10    /**< read the 6 byte MAC address   */
#define W5500_CTRL_MAC_SET  0x11    /**< write the 6 byte MAC address  */
#define W5500_CTRL_LINK     0x12    /**< 1 when the PHY reports link   */
#define W5500_CTRL_PHYCFGR  0x13    /**< raw PHYCFGR value             */

/* Largest Ethernet frame we accept (payload without the 4 byte FCS). */
#define W5500_MTU           1500
#define W5500_FRAME_MAX     (W5500_MTU + 14)

/**
 * Kilobyte count of socket buffer memory to request from the chip.  The
 * W5500 has 16 KiB of transmit and 16 KiB of receive memory in total, so
 * a single socket can own all of it.
 */
#define W5500_BUF_KB        16

/** How long an operation waits for the chip, in one millisecond steps. */
#define W5500_TIMEOUT_MS    1000

/* ------------------------------------------------------------------ */
/* Driver interface                                                   */
/* ------------------------------------------------------------------ */

/**
 * W5500 control block.  One entry exists per configured W5500 device and
 * the device's minor number doubles as the chip's socket number.
 */
struct w5500
{
    monitor lock;               /**< serializes access to the socket      */
    bool ready;                 /**< TRUE once the chip has answered     */
    bool opened;                /**< TRUE once w5500Open() has run       */
    uchar mac[6];               /**< station address                     */
    uchar version;              /**< value read back from W5500_VERSIONR */
};

extern struct w5500 w5500tab[];

/* SPI plumbing, shared between the driver's translation units. */
void    w5500SpiInit(void);
uint8_t w5500ReadReg(uint16_t addr, uint8_t bsb);
void    w5500WriteReg(uint16_t addr, uint8_t bsb, uint8_t value);
uint16_t w5500Read16(uint16_t addr, uint8_t bsb);
void    w5500Write16(uint16_t addr, uint8_t bsb, uint16_t value);
void    w5500ReadBuf(uint16_t addr, uint8_t bsb, void *buf, uint len);
void    w5500WriteBuf(uint16_t addr, uint8_t bsb, const void *buf, uint len);

/* Socket command handling, shared between w5500Init/Read/Write. */
int     w5500Command(int sock, uint8_t command);

devcall w5500Init(device *);
devcall w5500Open(device *);
devcall w5500Close(device *);
devcall w5500Read(device *, void *, uint);
devcall w5500Write(device *, const void *, uint);
devcall w5500Getc(device *);
devcall w5500Putc(device *, char);
devcall w5500Control(device *, int, long, long);
interrupt w5500Interrupt(void);

#endif /* _W5500_H_ */
