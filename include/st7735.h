/**
 * @file st7735.h
 *
 * Internal interface of the ST7735 TFT driver.
 *
 * Only the driver's own translation units include this file: it exposes
 * the panel's command set and the small set of routines that talk to it
 * over the bit-banged SPI bus.  The public interface lives in <tft.h>.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _ST7735_H_
#define _ST7735_H_

#include <stddef.h>
#include <stdint.h>
#include <conf.h>

/* ST7735 command set (the subset we use). */
#define ST_SWRESET      0x01    /**< software reset                     */
#define ST_SLPOUT       0x11    /**< sleep out                          */
#define ST_NORON        0x13    /**< normal display mode on             */
#define ST_INVOFF       0x20    /**< display inversion off              */
#define ST_DISPON       0x29    /**< display on                         */
#define ST_CASET        0x2A    /**< column address set                 */
#define ST_RASET        0x2B    /**< row address set                    */
#define ST_RAMWR        0x2C    /**< memory write                       */
#define ST_MADCTL       0x36    /**< memory access control              */
#define ST_COLMOD       0x3A    /**< interface pixel format             */
#define ST_FRMCTR1      0xB1    /**< frame rate control, normal mode    */
#define ST_FRMCTR2      0xB2    /**< frame rate control, idle mode      */
#define ST_FRMCTR3      0xB3    /**< frame rate control, partial mode   */
#define ST_INVCTR       0xB4    /**< display inversion control          */
#define ST_PWCTR1       0xC0    /**< power control 1                    */
#define ST_PWCTR2       0xC1    /**< power control 2                    */
#define ST_PWCTR3       0xC2    /**< power control 3                    */
#define ST_PWCTR4       0xC3    /**< power control 4                    */
#define ST_PWCTR5       0xC4    /**< power control 5                    */
#define ST_VMCTR1       0xC5    /**< VCOM control 1                     */
#define ST_GMCTRP1      0xE0    /**< positive gamma correction          */
#define ST_GMCTRN1      0xE1    /**< negative gamma correction          */

#define ST_MADCTL_MY    0x80    /**< row address order                  */
#define ST_MADCTL_MX    0x40    /**< column address order               */

/* Panel timing, in microseconds. */
#define ST_RESET_US     150000  /**< after software reset               */
#define ST_SLPOUT_US    255000  /**< after sleep out                    */
#define ST_NORON_US     10000   /**< after normal display mode on       */
#define ST_DISPON_US    100000  /**< after display on                   */

/* -------------------------------------------------------------- */
/* Comandos ST7735                                                 */
/* -------------------------------------------------------------- */
#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT  0x11
#define ST7735_NORON   0x13
#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_MADCTL  0x36
#define ST7735_COLMOD  0x3A

#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08

#define TFT_WIDTH   128
#define TFT_HEIGHT  160
#define TFT_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT)

/* Colores RGB565 */
#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_BLUE     0x001F
#define COLOR_YELLOW   0xFFE0
#define COLOR_CYAN     0x07FF
#define COLOR_MAGENTA  0xF81F
#define COLOR_ORANGE   0xFD20
#define COLOR_DARK_GRAY 0x4208
#define COLOR_LIGHT_GRAY 0xC618

#define RGB565(r, g, b) ((uint16_t)(((r) & 0xF8) << 8 | ((g) & 0xFC) << 3 | ((b) >> 3)))



/* Inicializacion */
void st7735_init(void);
void st7735_reinit(void);

/* Framebuffer */
uint16_t *st7735_get_fb(void);
void st7735_flush(void);
void st7735_flush_area(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/* Primitivas de dibujo (operan sobre el framebuffer) */
void st7735_clear(uint16_t color);
void st7735_pixel(uint16_t x, uint16_t y, uint16_t color);
void st7735_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void st7735_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void st7735_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7735_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7735_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void st7735_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void st7735_fill_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);

/* Texto (font 5x8, 1x scale) */
void st7735_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg);
void st7735_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg);
void st7735_string_scaled(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);

void tftHwInit(void);
void tftHwCmd(uint8_t cmd);
void tftHwCmdData(uint8_t cmd, const uint8_t *data, uint len);

 
#endif                          /* _ST7735_H_ */
