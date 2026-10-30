/**
 * @file tftHw.c
 *
 * Bit-banged SPI plumbing and panel bring-up for the ST7735 TFT.
 *
 * The panel hangs off four GPIOs: chip select, data/command, MOSI and
 * clock.  The bus is clocked in software (xt_spi_xfer()) because the
 * hardware SPI units are needed elsewhere and the panel tolerates any
 * clock rate, so there is no gain in taking one over.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <clock.h>
#include <kernel.h>
#include <platform.h>
#include <stddef.h>
#include <stdint.h>
#include <xtensa.h>
#include <string.h>
#include <tft.h>
#include "st7735.h"

static const char *TAG = "st7735";
static uint16_t s_fb[TFT_BUF_SIZE];



/* The panel has no data output, so this pin number is never read. */
#define TFT_NO_MISO     31

 
/** Shift one byte out over the panel's bus. */
static inline uint8_t tftXfer(uint8_t tx)
{
    return xt_spi_xfer(TFT_PIN_SCLK, TFT_PIN_MOSI, TFT_NO_MISO, tx);
}

/** Shift @p len bytes out over the panel's bus. */
static void tftPutBytes(const uint8_t *buf, uint len)
{
    uint i;

    for (i = 0; i < len; i++)
    {
        (void)tftXfer(buf[i]);
    }
}

static void spi_data(const uint8_t *data, size_t len)
{

    xt_gpio_set_level(TFT_PIN_DC, 1);   /* datos */
    xt_gpio_set_level(TFT_PIN_CS, 0);
    tftPutBytes(data, (uint)len);
    xt_gpio_set_level(TFT_PIN_CS, 1);
}

/** Shift one 16 bit pixel out, high byte first, as the panel expects. */
static inline void tftPutPixel(uint16_t px)
{
    (void)tftXfer((uint8_t)(px >> 8));
    (void)tftXfer((uint8_t)(px & 0xFF));
}

void tftHwCmdData(uint8_t cmd, const uint8_t *data, uint len)
{
    xt_gpio_set_level(TFT_PIN_DC, 0);   /* command byte */
    xt_gpio_set_level(TFT_PIN_CS, 0);
    (void)tftXfer(cmd);

    if (len > 0)
    {
        xt_gpio_set_level(TFT_PIN_DC, 1);       /* parameter bytes */
        tftPutBytes(data, len);
    }
    xt_gpio_set_level(TFT_PIN_CS, 1);
}

void tftHwCmd(uint8_t cmd)
{
    tftHwCmdData(cmd, NULL, 0);
}



static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t ca[4] = { x0 >> 8, x0, x1 >> 8, x1 };
    uint8_t ra[4] = { y0 >> 8, y0, y1 >> 8, y1 };
    tftHwCmdData(ST7735_CASET, ca, 4);
    tftHwCmdData(ST7735_RASET, ra, 4);
    tftHwCmd(ST7735_RAMWR);
}


 
void tftHwInit(void)
{
    memset(s_fb, 0, sizeof(s_fb));

    xt_gpio_output(TFT_PIN_CS, GPIO_PULL_NONE);
    xt_gpio_output(TFT_PIN_DC, GPIO_PULL_NONE);
    xt_gpio_output(TFT_PIN_MOSI, GPIO_PULL_NONE);
    xt_gpio_output(TFT_PIN_SCLK, GPIO_PULL_NONE);
    xt_gpio_output(TFT_PIN_RST, GPIO_PULL_NONE);

    xt_gpio_set_level(TFT_PIN_CS, 1);
    xt_gpio_set_level(TFT_PIN_DC, 1);
    xt_gpio_set_level(TFT_PIN_SCLK, 0);
    xt_gpio_set_level(TFT_PIN_MOSI, 0);

    /* GPIO15 is the reset line shared with the W5500.  This driver owns
     * it -- the W5500 driver leaves it alone -- and must pulse it here.
     * Without a real hardware reset the panel comes up blank (white).
     * This is the same timing the board's vendor firmware uses. */
    xt_gpio_set_level(TFT_PIN_RST, 0);
    udelay(500);
    xt_gpio_set_level(TFT_PIN_RST, 1);
    udelay(500);
 
    tftHwCmd(ST_SWRESET);
    udelay(ST_RESET_US);
    tftHwCmd(ST_SLPOUT);
    udelay(ST_SLPOUT_US);

    tftHwCmdData(ST_MADCTL, (const uint8_t[]){ ST_MADCTL_MY | ST_MADCTL_MX }, 1);
    tftHwCmdData(ST_COLMOD, (const uint8_t[]){ 0x05 }, 1);      /* 16 bpp */

    tftHwCmd(ST_NORON);
    udelay(ST_NORON_US);

   // tftHwRect(0, 0, TFT_WIDTH, TFT_HEIGHT, TFT_BLACK);

    tftHwCmd(ST_DISPON);
    udelay(ST_DISPON_US);
}

void st7735_reinit(void)
{
 
    /* Reset hardware */
    xt_gpio_set_level(TFT_PIN_RST, 0);
    udelay(50000);
    xt_gpio_set_level(TFT_PIN_RST, 1);
    udelay(50000);
 
    /* Init sequence */
    tftHwCmd(ST7735_SWRESET);
    udelay(1000);
    tftHwCmd(ST7735_SLPOUT);
    udelay(1000);
    tftHwCmdData(ST7735_MADCTL, (uint8_t[]){MADCTL_MY | MADCTL_MX}, 1);
    tftHwCmdData(ST7735_COLMOD, (uint8_t[]){0x05}, 1);
    tftHwCmd(ST7735_NORON);
    udelay(1000);
    st7735_clear(COLOR_BLACK);
    st7735_flush();
    tftHwCmd(ST7735_DISPON);
    udelay(1000);
}

/* -------------------------------------------------------------- */
/* Framebuffer: almacena en BIG-ENDIAN (listo para SPI)            */
/* -------------------------------------------------------------- */
uint16_t *st7735_get_fb(void) { return s_fb; }

static inline uint16_t to_be(uint16_t c) { return __builtin_bswap16(c); }


void st7735_flush(void)
{
    set_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
    spi_data((const uint8_t *)s_fb, sizeof(s_fb));
}

void st7735_flush_area(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    if (x0 >= TFT_WIDTH || y0 >= TFT_HEIGHT || x1 >= TFT_WIDTH || y1 >= TFT_HEIGHT) return;
    set_window(x0, y0, x1, y1);
    /* Enviar fila por fila, ya que s_fb es lineal */
    for (uint16_t y = y0; y <= y1; y++) {
        const uint8_t *row = (const uint8_t *)&s_fb[y * TFT_WIDTH + x0];
        spi_data(row, (x1 - x0 + 1) * 2);
    }
}

void st7735_clear(uint16_t color)
{
    uint16_t c = to_be(color);
    for (size_t i = 0; i < TFT_BUF_SIZE; i++) s_fb[i] = c;
}

void st7735_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x < TFT_WIDTH && y < TFT_HEIGHT)
        s_fb[y * TFT_WIDTH + x] = to_be(color);
}


void st7735_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    uint16_t c = to_be(color);
    for (uint16_t i = 0; i < w && (x + i) < TFT_WIDTH; i++)
        s_fb[y * TFT_WIDTH + x + i] = c;
}

void st7735_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    uint16_t c = to_be(color);
    for (uint16_t i = 0; i < h && (y + i) < TFT_HEIGHT; i++)
        s_fb[(y + i) * TFT_WIDTH + x] = c;
}

void st7735_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    st7735_hline(x, y, w, color);
    st7735_hline(x, y + h - 1, w, color);
    st7735_vline(x, y, h, color);
    st7735_vline(x + w - 1, y, h, color);
}

void st7735_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t c = to_be(color);
    for (uint16_t j = y; j < y + h && j < TFT_HEIGHT; j++) {
        for (uint16_t i = x; i < x + w && i < TFT_WIDTH; i++)
            s_fb[j * TFT_WIDTH + i] = c;
    }
}

void st7735_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        st7735_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void st7735_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        st7735_pixel(x0+x, y0+y, color); st7735_pixel(x0+y, y0+x, color);
        st7735_pixel(x0-y, y0+x, color); st7735_pixel(x0-x, y0+y, color);
        st7735_pixel(x0-x, y0-y, color); st7735_pixel(x0-y, y0-x, color);
        st7735_pixel(x0+y, y0-x, color); st7735_pixel(x0+x, y0-y, color);
        y++;
        if (err < 0) err += 2*y + 1;
        else { x--; err += 2*(y - x) + 1; }
    }
}

void st7735_fill_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        st7735_hline(x0-x, y0+y, 2*x+1, color);
        st7735_hline(x0-x, y0-y, 2*x+1, color);
        st7735_hline(x0-y, y0+x, 2*y+1, color);
        st7735_hline(x0-y, y0-x, 2*y+1, color);
        y++;
        if (err < 0) err += 2*y + 1;
        else { x--; err += 2*(y - x) + 1; }
    }
}

/* -------------------------------------------------------------- */
/* Font 5x8 embedded (ASCII 32-126)                               */
/* Cada caracter: 5 bytes, cada byte = una columna, bit0=fila 0    */
/* -------------------------------------------------------------- */
static const uint8_t font5x8[] = {
    0x00,0x00,0x00,0x00,0x00, // space
    0x00,0x00,0x5F,0x00,0x00, // !
    0x00,0x07,0x00,0x07,0x00, // "
    0x14,0x7F,0x14,0x7F,0x14, // #
    0x24,0x2A,0x7F,0x2A,0x12, // $
    0x23,0x13,0x08,0x64,0x62, // %
    0x36,0x49,0x55,0x22,0x50, // &
    0x00,0x05,0x03,0x00,0x00, // '
    0x00,0x1C,0x22,0x41,0x00, // (
    0x00,0x41,0x22,0x1C,0x00, // )
    0x14,0x08,0x3E,0x08,0x14, // *
    0x08,0x08,0x3E,0x08,0x08, // +
    0x00,0x50,0x30,0x00,0x00, // ,
    0x08,0x08,0x08,0x08,0x08, // -
    0x00,0x60,0x60,0x00,0x00, // .
    0x20,0x10,0x08,0x04,0x02, // /
    0x3E,0x51,0x49,0x45,0x3E, // 0
    0x00,0x42,0x7F,0x40,0x00, // 1
    0x42,0x61,0x51,0x49,0x46, // 2
    0x21,0x41,0x45,0x4B,0x31, // 3
    0x18,0x14,0x12,0x7F,0x10, // 4
    0x27,0x45,0x45,0x45,0x39, // 5
    0x3C,0x4A,0x49,0x49,0x30, // 6
    0x01,0x71,0x09,0x05,0x03, // 7
    0x36,0x49,0x49,0x49,0x36, // 8
    0x06,0x49,0x49,0x29,0x1E, // 9
    0x00,0x36,0x36,0x00,0x00, // :
    0x00,0x56,0x36,0x00,0x00, // ;
    0x08,0x14,0x22,0x41,0x00, // <
    0x14,0x14,0x14,0x14,0x14, // =
    0x00,0x41,0x22,0x14,0x08, // >
    0x02,0x01,0x51,0x09,0x06, // ?
    0x32,0x49,0x79,0x41,0x3E, // @
    0x7E,0x11,0x11,0x11,0x7E, // A
    0x7F,0x49,0x49,0x49,0x36, // B
    0x3E,0x41,0x41,0x41,0x22, // C
    0x7F,0x41,0x41,0x22,0x1C, // D
    0x7F,0x49,0x49,0x49,0x41, // E
    0x7F,0x09,0x09,0x09,0x01, // F
    0x3E,0x41,0x49,0x49,0x7A, // G
    0x7F,0x08,0x08,0x08,0x7F, // H
    0x00,0x41,0x7F,0x41,0x00, // I
    0x20,0x40,0x41,0x3F,0x01, // J
    0x7F,0x08,0x14,0x22,0x41, // K
    0x7F,0x40,0x40,0x40,0x40, // L
    0x7F,0x02,0x0C,0x02,0x7F, // M
    0x7F,0x04,0x08,0x10,0x7F, // N
    0x3E,0x41,0x41,0x41,0x3E, // O
    0x7F,0x09,0x09,0x09,0x06, // P
    0x3E,0x41,0x51,0x21,0x5E, // Q
    0x7F,0x09,0x19,0x29,0x46, // R
    0x46,0x49,0x49,0x49,0x31, // S
    0x01,0x01,0x7F,0x01,0x01, // T
    0x3F,0x40,0x40,0x40,0x3F, // U
    0x1F,0x20,0x40,0x20,0x1F, // V
    0x3F,0x40,0x38,0x40,0x3F, // W
    0x63,0x14,0x08,0x14,0x63, // X
    0x07,0x08,0x70,0x08,0x07, // Y
    0x61,0x51,0x49,0x45,0x43, // Z
    0x00,0x7F,0x41,0x41,0x00, // [
    0x02,0x04,0x08,0x10,0x20, // backslash
    0x00,0x41,0x41,0x7F,0x00, // ]
    0x04,0x02,0x01,0x02,0x04, // ^
    0x40,0x40,0x40,0x40,0x40, // _
    0x00,0x01,0x02,0x04,0x00, // `
    0x20,0x54,0x54,0x54,0x78, // a
    0x7F,0x48,0x44,0x44,0x38, // b
    0x38,0x44,0x44,0x44,0x20, // c
    0x38,0x44,0x44,0x48,0x7F, // d
    0x38,0x54,0x54,0x54,0x18, // e
    0x08,0x7E,0x09,0x01,0x02, // f
    0x0C,0x52,0x52,0x52,0x3E, // g
    0x7F,0x08,0x04,0x04,0x78, // h
    0x00,0x44,0x7D,0x40,0x00, // i
    0x20,0x40,0x44,0x3D,0x00, // j
    0x7F,0x10,0x28,0x44,0x00, // k
    0x00,0x41,0x7F,0x40,0x00, // l
    0x7C,0x04,0x18,0x04,0x78, // m
    0x7C,0x08,0x04,0x04,0x78, // n
    0x38,0x44,0x44,0x44,0x38, // o
    0x7C,0x14,0x14,0x14,0x08, // p
    0x08,0x14,0x14,0x18,0x7C, // q
    0x7C,0x08,0x04,0x04,0x08, // r
    0x48,0x54,0x54,0x54,0x20, // s
    0x04,0x3F,0x44,0x40,0x20, // t
    0x3C,0x40,0x40,0x20,0x7C, // u
    0x1C,0x20,0x40,0x20,0x1C, // v
    0x3C,0x40,0x30,0x40,0x3C, // w
    0x44,0x28,0x10,0x28,0x44, // x
    0x0C,0x50,0x50,0x50,0x3C, // y
    0x44,0x64,0x54,0x4C,0x44, // z
    0x00,0x08,0x36,0x41,0x00, // {
    0x00,0x00,0x7F,0x00,0x00, // |
    0x00,0x41,0x36,0x08,0x00, // }
    0x10,0x08,0x08,0x10,0x08, // ~
};

void st7735_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg)
{
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = &font5x8[(c - 32) * 5];
    uint16_t fg_be = to_be(fg);
    uint16_t bg_be = to_be(bg);

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 5; col++) {
            uint16_t px = (glyph[col] & (1 << row)) ? fg_be : bg_be;
            uint16_t px_x = x + col;
            uint16_t px_y = y + row;
            if (px_x < TFT_WIDTH && px_y < TFT_HEIGHT)
                s_fb[px_y * TFT_WIDTH + px_x] = px;
        }
        /* 1 pixel de spacing entre caracteres */
        uint16_t sp_x = x + 5;
        if (sp_x < TFT_WIDTH && (y + row) < TFT_HEIGHT)
            s_fb[(y + row) * TFT_WIDTH + sp_x] = bg_be;
    }
}

void st7735_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg)
{
    while (*s) {
        st7735_char(x, y, *s, fg, bg);
        x += 6;
        if (x + 5 >= TFT_WIDTH) { x = 0; y += 9; }
        s++;
    }
}

void st7735_string_scaled(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale)
{
    while (*s) {
        for (uint8_t sy = 0; sy < scale; sy++)
            for (uint8_t sx = 0; sx < scale; sx++)
                st7735_char(x + sx * 6, y + sy * 9, *s, fg, bg);
        x += 6 * scale;
        if (x + 5 * scale >= TFT_WIDTH) { x = 0; y += 9 * scale; }
        s++;
    }
}


