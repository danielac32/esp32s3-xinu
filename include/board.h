/**
 * @file board.h
 *
 * ESP32-S3 board wiring used by this Xinu port.
 *
 * This is the single place where physical GPIO numbers are declared.  The
 * individual drivers keep their historical TFT_PIN_*, W5500_PIN_* and
 * SD_PIN_* spellings as aliases, so existing code keeps working while new
 * code can use the BOARD_* names.
 *
 * Board summary:
 *
 *   - ST7735 128x160 TFT, bit-banged SPI
 *   - W5500 Ethernet, hardware FSPI (SPI2)
 *   - microSD, SDMMC 4-bit
 *   - BOOT button
 *   - discrete RGB LED (common cathode, active high)
 *   - UART0 console
 *
 * GPIO15 is shared by the TFT reset and the W5500 reset.  The TFT driver
 * owns the line and pulses it during boot; the W5500 driver must not drive
 * it afterwards.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _BOARD_H_
#define _BOARD_H_

/* ------------------------------------------------------------------ */
/* UART0 console                                                      */
/* ------------------------------------------------------------------ */
#define BOARD_UART0_TXD_PIN     43
#define BOARD_UART0_RXD_PIN     44

/* ------------------------------------------------------------------ */
/* ST7735 TFT (bit-banged SPI)                                        */
/* ------------------------------------------------------------------ */
#define BOARD_TFT_CS_PIN        21
#define BOARD_TFT_DC_PIN        16
#define BOARD_TFT_MOSI_PIN      17
#define BOARD_TFT_SCLK_PIN      18
#define BOARD_TFT_RST_PIN       15      /* shared with W5500 reset */

/* ------------------------------------------------------------------ */
/* W5500 Ethernet (FSPI / SPI2, hardware SPI)                         */
/* ------------------------------------------------------------------ */
#define BOARD_W5500_CS_PIN      10
#define BOARD_W5500_MOSI_PIN    11
#define BOARD_W5500_SCK_PIN     12
#define BOARD_W5500_MISO_PIN    13
#define BOARD_W5500_IRQ_PIN     14
#define BOARD_W5500_RST_PIN     15      /* same pad as TFT reset */

/* ------------------------------------------------------------------ */
/* microSD (SDMMC, 4-bit)                                             */
/* ------------------------------------------------------------------ */
#define BOARD_SD_CLK_PIN        36
#define BOARD_SD_CMD_PIN        35
#define BOARD_SD_D0_PIN         37
#define BOARD_SD_D1_PIN         38
#define BOARD_SD_D2_PIN         33
#define BOARD_SD_D3_PIN         34

/* ------------------------------------------------------------------ */
/* BOOT button                                                        */
/* ------------------------------------------------------------------ */
#define BOARD_BUTTON_PIN        0       /* active low, external pull-up */

/* ------------------------------------------------------------------ */
/* Discrete RGB LED                                                   */
/* ------------------------------------------------------------------ */
/* The LED is wired as three independent GPIOs.  Keep the colour order in
 * one place: the shell command and the Java demo use these macros. */
#define BOARD_RGB_R_PIN         2
#define BOARD_RGB_G_PIN         4
#define BOARD_RGB_B_PIN         6
#define BOARD_RGB_ACTIVE_HIGH   1

/* ------------------------------------------------------------------ */
/* Flash / PSRAM pins reserved by the module (do not use as GPIO)     */
/* ------------------------------------------------------------------ */
#define BOARD_FLASH_CS_PIN      29
#define BOARD_PSRAM_CS_PIN      26
#define BOARD_PSRAM_SD2_PIN     27
#define BOARD_PSRAM_SD3_PIN     28

#endif /* _BOARD_H_ */
