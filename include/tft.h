/**
 * @file tft.h
 *
 * ST7735 TFT display driver for the ESP32-S3.
 *
 * The panel is 128x160 with a 16 bit RGB565 interface on its own
 * bit-banged SPI bus.  There is no framebuffer: every drawing call
 * programs the panel's address window and clocks the pixels out
 * immediately, which costs nothing extra on a bus that is already the
 * bottleneck and saves 40 KiB of the board's 192 KiB of DRAM.
 *
 * Text written with write()/putc() goes to the character cell selected
 * by the cursor kept in the device's control block.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _TFT_H_
#define _TFT_H_

#include <device.h>
#include <monitor.h>
#include <stddef.h>
#include <stdint.h>
#include <conf.h>

/* Panel geometry. */
#define TFT_WIDTH   128
#define TFT_HEIGHT  160

/* Board wiring.  The reset line (GPIO15) is shared with the W5500, and
 * this driver owns it: the panel must see a real hardware reset pulse
 * here or it comes up blank/white.  The W5500 driver leaves the line
 * alone for that reason. */
#define TFT_PIN_CS      21
#define TFT_PIN_DC      16
#define TFT_PIN_MOSI    17
#define TFT_PIN_SCLK    18
#define TFT_PIN_RST     15

/* RGB565 colours. */
#define TFT_BLACK       0x0000
#define TFT_WHITE       0xFFFF
#define TFT_RED         0xF800
#define TFT_GREEN       0x07E0
#define TFT_BLUE        0x001F
#define TFT_YELLOW      0xFFE0
#define TFT_CYAN        0x07FF
#define TFT_MAGENTA     0xF81F
#define TFT_ORANGE      0xFD20
#define TFT_GRAY        0x8410

#define TFT_RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))

 
 
/** TFT control block, one per device. */
struct tft
{
    monitor lock;               /**< serializes access to the device      */
    bool opened;                /**< TRUE once tftOpen() has run          */
    bool ready;                 /**< TRUE once the panel is initialized   */
    uint cx;                    /**< cursor column, in pixels             */
    uint cy;                    /**< cursor row, in pixels                */
    uint16_t fg;                /**< colour of text pixels                */
    uint16_t bg;                /**< colour behind text pixels            */
};

#define CHAR_W  6
#define CHAR_H  9   /* 8 px font + 1 px spacing */
#define COLS    (TFT_WIDTH / CHAR_W)    /* 128 / 6 = 21 */
#define ROWS    (TFT_HEIGHT / CHAR_H)   /* 160 / 9 = 17 */

/* Control commands (para devcontrol) */
#define TFTCTL_CLEAR    0   /* limpia la pantalla */
#define TFTCTL_FLUSH    1   /* flush forzado del framebuffer */
#define TFTCTL_SETCOLOR 2   /* arg1=fg, arg2=bg (futuro) */
#define TFTCTL_HOME     3   /* cursor a (0,0) */
#define TFTCTL_CURSOR 4

extern struct tft tfttab[];

 
/* Driver functions */
devcall tftInit(device *);
devcall tftOpen(device *);
devcall tftClose(device *);
devcall tftRead(device *, void *, uint);
devcall tftWrite(device *, const void *, uint);
devcall tftGetc(device *);
devcall tftPutc(device *, char);
devcall tftControl(device *, int, long, long);

#endif                          /* _TFT_H_ */
