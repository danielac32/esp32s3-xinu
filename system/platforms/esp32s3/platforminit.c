/**
 * @file platforminit.c
 *
 * ESP32-S3 hardware platform specific initialization.
 *
 * Runs first thing in nulluser(), while interrupts are still disabled
 * and before Xinu's memory manager is up.  Its job is to bring the SoC
 * into a known state: kill the watchdogs (which would otherwise reset
 * the chip a few hundred milliseconds after the ROM hands control to
 * us) and publish the platform parameters.
 *
 * Clock: this port does not touch the SoC clock tree.  The boot ROM
 * leaves the CPU running from the crystal rather than from the PLL, and
 * the UART takes its bit clock straight from the crystal too, so there is
 * nothing here to mismatch.  See ESP32S3_CPU_FREQ_HZ for how the CPU
 * frequency was established.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <platform.h>
#include <string.h>
#include <stdint.h>
#include <kernel.h>
#include <interrupt.h>
#include <xtensa.h>
#include <psram.h>

/* Frequency of the Xtensa cycle counter (CCOUNT), in Hz.  This is what
 * udelay() and the system timer are measured against.
 *
 * The boot ROM selects neither the PLL nor the 40 MHz crystal directly for
 * the CPU: it runs the core at half the crystal.  The number below was
 * measured on hardware rather than read out of the clock registers, because
 * the ESP32-S3's clock tree is easy to misread (XTAL, APB and CCOUNT do not
 * all tick at the same rate).  Spinning until CCOUNT had advanced
 * 20,000,000 counts took 1.0006 s, so CCOUNT runs at 20 MHz. */
#define ESP32S3_CPU_FREQ_HZ  20000000UL

/* Internal SRAM available to the application: the data-window half of the
 * shared D/IRAM bank, starting right above the physical end of the code
 * (see ld.script).  The region stops below the boot ROM's own buffers,
 * stacks and .data, which live at 0x3FCD7E00 .. 0x3FCF0000. */
#define ESP32S3_DRAM_BASE    0x3FCA0000UL
#define ESP32S3_DRAM_END     0x3FCD0000UL

extern void *memheap;

/* Top of the stack used while the kernel is panicking.  start.S computes
 * this just above the kernel image so that a panic can still print even if
 * the running thread's stack is corrupt. */
void *panicstk;

/* Called from start.S before .bss is cleared, so it must not touch any
 * global state: it only writes to absolute peripheral registers. */
void xt_disable_watchdogs(void);

int platforminit(void)
{
    strlcpy(platform.family, "esp32s3", PLT_STRMAX);
    strlcpy(platform.name, "ESP32-S3", PLT_STRMAX);

    /* Make sure the UART/SPI peripheral clocks are running. */
    REG32(SYSTEM_PERIP_CLK_EN0) |= SYS_CLK_EN_UART0 | SYS_CLK_EN_UART_MEM
                                 | SYS_CLK_EN_SPI2 | SYS_CLK_EN_SPI3
                                 | SYS_CLK_EN_TIMERGROUP;

    /* The SoC clock tree is left exactly as the boot ROM configured it:
     * switching the CPU to the PLL wedged the core and the UART during
     * bring-up, and the crystal-derived clock is plenty for the UART,
     * the timers and the bit-banged SPI buses. */

    /* The ROM leaves the TG0 watchdog armed; start.S already disabled it,
     * but doing it again here is cheap and keeps platforminit correct on
     * its own. */
    xt_disable_watchdogs();

    platform.minaddr = (void *)ESP32S3_DRAM_BASE;
    platform.maxaddr = (void *)ESP32S3_DRAM_END;
    platform.clkfreq = ESP32S3_CPU_FREQ_HZ;

    /* The generic UART driver does not use these on this platform. */
    platform.uart_dll = 0;
    platform.uart_irqnum = XT_IRQ_UART0;

    platform.serial_low = 0;
    platform.serial_high = 0;

    /* Bring up the in-package PSRAM, if there is one.  This fails soft:
     * a board without usable PSRAM just keeps running from internal SRAM. */
    if (SYSERR == psramInit())
    {
        psram_present = 0;
        psram_size = 0;
    }

    return OK;
}

/**
 * Disable the RTC watchdog, the "super" watchdog and the Timer Group 0
 * main watchdog.  Any of them will reset the chip if left armed.
 */
void xt_disable_watchdogs(void)
{
    /* Super watchdog (SWD). */
    REG32(RTC_CNTL_SWD_WPROTECT) = RTC_CNTL_SWD_WKEY;
    REG32(RTC_CNTL_WDTCONFIG1) = 0;
    REG32(RTC_CNTL_SWD_CONF) = (1U << 30);   /* SWD_AUTO_FEED_EN */
    REG32(RTC_CNTL_SWD_WPROTECT) = 0;

    /* RTC watchdog. */
    REG32(RTC_CNTL_WDTWPROTECT) = RTC_CNTL_WDT_WKEY;
    REG32(RTC_CNTL_WDTCONFIG0) = 0;
    REG32(RTC_CNTL_WDTWPROTECT) = 0;

    /* Timer Group 0 main watchdog. */
    REG32(TIMG0_WDTWPROTECT) = TIMG0_WDT_WKEY;
    REG32(TIMG0_WDTCONFIG0) = 0;
    REG32(TIMG0_WDTWPROTECT) = 0;
}
