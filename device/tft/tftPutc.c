/**
 * @file tftPutc.c
 *
 * ST7735 TFT display driver for the ESP32-S3: draw one character at the
 * cursor.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <monitor.h>
#include "st7735.h"
#include <tft.h>
#include <string.h>

 
/**
 * @ingroup tft
 *
 * Draw one character at the cursor and advance it.  '\r' is ignored,
 * '\n' moves to the next line, '\t' is drawn as a space, and the display
 * scrolls (by clearing) when the cursor runs off the bottom.
 *
 * @param devptr tft device table entry
 * @param c      character to draw
 * @return OK on success, SYSERR if the display is not open
 */

static void tft_scroll(int minor)
{
    /* Copiar de la fila 1 a la fila 0 */
    uint16_t *fb = st7735_get_fb();
    int row_bytes = TFT_WIDTH * 2;
    memmove(fb, &fb[TFT_WIDTH], (TFT_HEIGHT - CHAR_H) * row_bytes);
    /* Limpiar la ultima fila */
    uint16_t bg_be = __builtin_bswap16(tfttab[minor].bg);
    for (int y = TFT_HEIGHT - CHAR_H; y < TFT_HEIGHT; y++)
        for (int x = 0; x < TFT_WIDTH; x++)
            fb[y * TFT_WIDTH + x] = bg_be;
}

static void tft_newline(int minor)
{
    tfttab[minor].cx = 0;
    tfttab[minor].cy += CHAR_H;
    if (tfttab[minor].cy + CHAR_H > TFT_HEIGHT) {
        tft_scroll(minor);
        tfttab[minor].cy = TFT_HEIGHT - CHAR_H;
    }
}

static void tft_putc_impl(int minor,char c)
{
    if (c == '\n' || c == '\r') {
        tft_newline(minor);
        return;
    }
    if (c == '\t') {
        /* Tab: avanzar al siguiente multiplo de 4 columnas */
        tfttab[minor].cx = ((tfttab[minor].cx / (CHAR_W * 4)) + 1) * (CHAR_W * 4);
        if (tfttab[minor].cx + CHAR_W > TFT_WIDTH) tft_newline(minor);
        return;
    }
    if (c == '\b') {
        if (tfttab[minor].cx >= CHAR_W) {
            tfttab[minor].cx -= CHAR_W;
            st7735_fill_rect(tfttab[minor].cx, tfttab[minor].cy, CHAR_W, CHAR_H, tfttab[minor].bg);
        }
        return;
    }

    /* Si llegamos al final de la linea, salto */
    if (tfttab[minor].cx + CHAR_W > TFT_WIDTH)
        tft_newline(minor);

    /* Dibujar caracter */
    st7735_char(tfttab[minor].cx, tfttab[minor].cy, c, tfttab[minor].fg, tfttab[minor].bg);
    tfttab[minor].cx += CHAR_W;

    /* Flush solo el area del caracter (no toda la pantalla) */
    st7735_flush_area(tfttab[minor].cx - CHAR_W, tfttab[minor].cy,
                      tfttab[minor].cx - 1, tfttab[minor].cy + CHAR_H - 1);
}


devcall tftPutc(device *devptr, char c)
{
    struct tft *tftptr;

    tftptr = &tfttab[devptr->minor];

    if ( isbadmon(tftptr->lock))
    {
        return SYSERR;
    }

    lock(tftptr->lock);
    tft_putc_impl(devptr->minor,c);
    unlock(tftptr->lock);
    return OK;
}
