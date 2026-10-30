/**
 * @file     xsh_tft.c
 *
 * Shell command (tft) drives the ST7735 display.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <device.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <shell.h>
#include <tft.h>

/** Look up a colour by name; returns TRUE when the name was recognized. */
static bool tftColorName(const char *name, uint16_t *color)
{
    static const struct
    {
        const char *name;
        uint16_t color;
    } colors[] = {
        { "black", TFT_BLACK },
        { "white", TFT_WHITE },
        { "red", TFT_RED },
        { "green", TFT_GREEN },
        { "blue", TFT_BLUE },
        { "yellow", TFT_YELLOW },
        { "cyan", TFT_CYAN },
        { "magenta", TFT_MAGENTA },
        { "orange", TFT_ORANGE },
        { "gray", TFT_GRAY },
    };
    uint i;

    for (i = 0; i < (sizeof(colors) / sizeof(colors[0])); i++)
    {
        if (0 == strcmp(name, colors[i].name))
        {
            *color = colors[i].color;
            return TRUE;
        }
    }
    return FALSE;
}

/** Print the usage message. */
static void tftUsage(const char *cmd)
{
    printf("Usage: %s [--help]\n", cmd);
    printf("       %s demo\n", cmd);
    printf("       %s clear [<color>]\n", cmd);
    printf("       %s text <string>\n", cmd);
    printf("\n");
    printf("Description:\n");
    printf("\tDraws on the 128x160 ST7735 display.\n");
    printf("\tWith no arguments, draws a test pattern.\n");
    printf("\tColors: black, white, red, green, blue, yellow, cyan,\n");
    printf("\t        magenta, orange, gray\n");
}

/**
 * @ingroup shell
 *
 * Exercise the TFT display.
 * @param nargs number of arguments in args array
 * @param args  array of arguments
 * @return non-zero value on error
 */
shellcmd xsh_tft(int nargs, char *args[])
{
    uint16_t color;

    if (nargs == 2 && 0 == strcmp(args[1], "--help"))
    {
        tftUsage(args[0]);
        return SHELL_OK;
    }

    /* Make sure the display is usable.  It may already be open. */
    if (SYSERR == open(TFT0))
    {
        if (SYSERR == control(TFT0, TFT_CTRL_BACKLIGHT, 1, 0))
        {
            fprintf(stderr, "%s: display is not available\n", args[0]);
            return SHELL_ERROR;
        }
    }

    if (nargs == 1 || (nargs == 2 && 0 == strcmp(args[1], "demo")))
    {
        uint i;

        printf("Drawing test pattern...\n");

        control(TFT0, TFT_CTRL_CLEAR, TFT_BLACK, 0);

        /* Colour bars across the top. */
        for (i = 0; i < 128; i += 16)
        {
            static const uint16_t bars[8] = {
                TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN,
                TFT_CYAN, TFT_BLUE, TFT_MAGENTA, TFT_WHITE
            };

            tft_rect(i, 0, 16, 32, bars[i / 16]);
        }

        /* Border and text below the bars. */
        tft_rect(0, 32, 128, 1, TFT_WHITE);
        tft_rect(0, 158, 128, 1, TFT_WHITE);
        control(TFT0, TFT_CTRL_CURSOR, 4, 40);
        write(TFT0, "Embedded Xinu", 13);
        control(TFT0, TFT_CTRL_COLORS, TFT_GREEN, TFT_BLACK);
        control(TFT0, TFT_CTRL_CURSOR, 4, 52);
        write(TFT0, "ESP32-S3 + ST7735", 17);
        control(TFT0, TFT_CTRL_COLORS, TFT_WHITE, TFT_BLACK);
        control(TFT0, TFT_CTRL_CURSOR, 4, 72);
        write(TFT0, "Hello, world!", 13);

        printf("Done.\n");
        return SHELL_OK;
    }

    if (0 == strcmp(args[1], "clear"))
    {
        color = TFT_BLACK;
        if (nargs > 2 && !tftColorName(args[2], &color))
        {
            fprintf(stderr, "%s: unknown color '%s'\n", args[0], args[2]);
            return SHELL_ERROR;
        }
        control(TFT0, TFT_CTRL_CLEAR, color, 0);
        return SHELL_OK;
    }

    if (0 == strcmp(args[1], "text"))
    {
        int i;

        if (nargs < 3)
        {
            fprintf(stderr, "%s: text needs a string\n", args[0]);
            tftUsage(args[0]);
            return SHELL_ERROR;
        }

        control(TFT0, TFT_CTRL_CLEAR, TFT_BLACK, 0);
        control(TFT0, TFT_CTRL_CURSOR, 0, 0);

        for (i = 2; i < nargs; i++)
        {
            write(TFT0, args[i], strlen(args[i]));
            write(TFT0, " ", 1);
        }
        return SHELL_OK;
    }

    fprintf(stderr, "%s: unknown option '%s'\n", args[0], args[1]);
    tftUsage(args[0]);
    return SHELL_ERROR;
}
