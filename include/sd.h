/**
 * @file sd.h
 *
 * SD card driver for the ESP32-S3.
 *
 * The board wires its microSD socket to the ESP32-S3 SDMMC controller in
 * 4-bit mode, so the card is driven over the SDHOST block (0x60028000)
 * rather than over SPI.  SDHOST is a DesignWare MSHC: an SDHCI-like
 * command/response engine with a small data FIFO.
 *
 * This driver uses 1-bit mode during identification and 4-bit mode
 * afterwards, and moves data through the FIFO by hand (programmed I/O);
 * the internal DMA engine is left off so that no descriptors have to be
 * built.  Reads and writes are one 512 byte block at a time.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _SD_H_
#define _SD_H_

#include <stddef.h>
#include <stdint.h>
#include <conf.h>

#define SD_BLOCK_SIZE   512      /**< one sector, the only block size used */
#define SD_MAX_BLOCKS   0x00FFFFFFUL

/* ------------------------------------------------------------------ */
/* Device interface                                                    */
/* ------------------------------------------------------------------ */

/** Information about the card, for SD_CTRL_INFO. */
struct sdinfo
{
    uint32_t blocks;        /**< capacity in 512 byte blocks      */
    uint32_t card_clk_khz;  /**< bus clock in kHz while working   */
    uint16_t rca;           /**< relative card address            */
    uint8_t  present;       /**< 1 once a card answered           */
    uint8_t  highcap;       /**< 1 for SDHC/SDXC (block addressed)*/
    uint8_t  bus_width;     /**< 1 or 4 data lines                */
    uint8_t  reserved;
};

/* sdControl() functions */
#define SD_CTRL_PRESENT   0x30  /**< 1 when a card is usable            */
#define SD_CTRL_SEEK      0x31  /**< arg1 = LBA for the next read/write */
#define SD_CTRL_BLOCKS    0x32  /**< capacity in 512 byte blocks        */
#define SD_CTRL_HIGHCAP   0x33  /**< 1 for block addressed cards        */
#define SD_CTRL_INFO      0x34  /**< arg1 = struct sdinfo *             */
#define SD_CTRL_RESET     0x35  /**< drop the card and identify it again*/

/* Block level helpers, usable without going through the device table. */
int         sd_read_blocks(uint32_t lba, void *buf, uint32_t count);
int         sd_write_blocks(uint32_t lba, const void *buf, uint32_t count);
uint32_t    sd_capacity_blocks(void);
int         sd_is_up(void);

devcall sdInit(device *);
devcall sdOpen(device *);
devcall sdClose(device *);
devcall sdRead(device *, void *, uint);
devcall sdWrite(device *, const void *, uint);
devcall sdGetc(device *);
devcall sdPutc(device *, char);
devcall sdControl(device *, int, long, long);

#endif /* _SD_H_ */
