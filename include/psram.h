/**
 * @file psram.h
 *
 * ESP32-S3 quad PSRAM support.
 *
 * The in-package quad PSRAM (8 MiB) is brought up by psramInit() and mapped
 * on the ESP32-S3 data bus at PSRAM_BASE.  Until that succeeds psram_present
 * stays 0 and psram_size stays 0, and every caller here falls back to the
 * internal SRAM heap, so a board without (or with an unsupported) PSRAM
 * still boots normally.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#ifndef _PSRAM_H_
#define _PSRAM_H_

#include <stddef.h>
#include <stdint.h>

/** Data-bus address the whole PSRAM die is mapped at. */
#define PSRAM_BASE      0x3C000000UL

/** Largest die this port knows how to map (8 MiB). */
#define PSRAM_MAX_SIZE  (8UL * 1024UL * 1024UL)

/** Nonzero once psramInit() has mapped a usable die. */
extern int psram_present;

/** Usable size of the mapped die, in bytes (0 when absent). */
extern uint32_t psram_size;

/** Diagnostic result of the last psramInit() attempt (see PSRAM_INIT_*). */
extern int psram_init_status;

/** Raw device-id bytes from the last probe (debug aid). */
extern uint8_t psram_dbg_id[8];

/** Raw flash JEDEC id read over CS0 with the ROM's register state. */
extern uint8_t psram_dbg_flash[4];

/** Raw flash JEDEC id read over CS0 after our clock/mode setup. */
extern uint8_t psram_dbg_flash2[4];

/** SPI_MEM_CTRL(1) / SPI_MEM_CLOCK(1) as found at the start of the probe. */
extern uint32_t psram_dbg_ctrl;
extern uint32_t psram_dbg_clk;

/** Poll iterations of the last transaction (0 = never ran) and MISC(1). */
extern uint32_t psram_dbg_spin;
extern uint32_t psram_dbg_misc;

#define PSRAM_INIT_NOT_TRIED    0   /**< psramInit() has not run yet       */
#define PSRAM_INIT_OK           1   /**< die is up and self-tested          */
#define PSRAM_INIT_RUNNING      2   /**< psramInit() is in progress         */
#define PSRAM_INIT_SPI_TIMEOUT  (-1) /**< SPI1 transaction never completed  */
#define PSRAM_INIT_NO_ID        (-2) /**< no known-good device id read       */
#define PSRAM_INIT_BAD_DENSITY  (-3) /**< recognized die but unknown size    */
#define PSRAM_INIT_SELFTEST     (-4) /**< mapped but failed its read/write   */

/**
 * Bring the PSRAM die up in quad mode, teach the SPI0 cache controller the
 * quad read/write commands and map the die at ::PSRAM_BASE.
 *
 * @return ::OK on success, ::SYSERR (without changing psram_present) if the
 *         die is absent, unsupported or failed its self-test.
 */
int psramInit(void);

/**
 * Carve the region mapped by psramInit() into a free list owned by the PSRAM
 * allocator.  Must be called once the kernel memory manager is up.
 */
void psramMemInit(void);

/** Allocate from PSRAM.  Same contract as memget() (8-byte aligned). */
void *psram_memget(uintptr_t nbytes);

/** Free a block allocated by psram_memget().  Same contract as memfree(). */
syscall psram_memfree(void *memptr, uintptr_t nbytes);

/**
 * Allocate a downward-growing stack from PSRAM.  Returns the topmost word,
 * exactly like stkget(); free it with stkfree()/memfree().
 */
void *psram_stkget(uintptr_t nbytes);

/* ---- malloc-style interface -------------------------------------------
 * psram_memget()/psram_memfree() make the caller remember the block size.
 * These wrap them with the same 8-byte accounting header libxc's malloc()
 * uses, so callers (the Simple JVM, LVGL) get the familiar size-less free().
 * Blocks come from the PSRAM free list only; psram_free() silently ignores
 * pointers it did not hand out. */

/** Allocate @p size bytes from PSRAM, like malloc(). */
void *psram_malloc(size_t size);

/** Allocate zeroed storage for @p nmemb elements of @p size bytes. */
void *psram_calloc(size_t nmemb, size_t size);

/** Resize a block from psram_malloc(), keeping its contents. */
void *psram_realloc(void *ptr, size_t size);

/** Free a block from psram_malloc()/psram_calloc()/psram_realloc(). */
void psram_free(void *ptr);

/** Bytes still free in the PSRAM free list (0 before psramMemInit()). */
uint32_t psramFreeBytes(void);

/** Largest contiguous free block in the PSRAM free list. */
uint32_t psramLargestFree(void);
uint32_t psramAllocCount(void);
uint32_t psramAllocBytes(void);
uint32_t psramLargestAlloc(void);

/** Bytes handed out of the PSRAM free list. */
uint32_t psramUsedBytes(void);

#endif                          /* _PSRAM_H_ */
