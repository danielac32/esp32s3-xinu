/**
 * @file exception.c
 *
 * Xtensa exception handling for the ESP32-S3 port.
 *
 * The level-1 vector in vectors.S delivers CPU exceptions here (the
 * level-3 vector handles interrupts separately).  Two things can happen:
 *
 *   1. A SYSCALL instruction: this port's software-trap mechanism.  The
 *      handler runs the registered kernel function and arranges for "rfe"
 *      to resume at the instruction following the trap.
 *
 *   2. Anything else (illegal instruction, load/store fault, ...): the
 *      failing context is dumped and the machine is halted.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <kernel.h>
#include <platform.h>
#include <stddef.h>
#include <stdint.h>
#include <xtensa.h>
#include <interrupt.h>

/* EXCCAUSE values (Xtensa XEA2). */
#define EXCCAUSE_ILLEGAL_INSTRUCTION   0
#define EXCCAUSE_SYSCALL               1
#define EXCCAUSE_INSTRUCTION_FETCH     2
#define EXCCAUSE_LOADSTORE             3
#define EXCCAUSE_LEVEL1_INTERRUPT      4
#define EXCCAUSE_ALLOCA                5
#define EXCCAUSE_DIVIDE_BY_ZERO        6
#define EXCCAUSE_PRIVILEGED            8
#define EXCCAUSE_LOADSTORE_ALIGN       9
#define EXCCAUSE_INSTR_PIF_DATA        12
#define EXCCAUSE_LOADSTORE_PIF_DATA    13
#define EXCCAUSE_INSTR_PIF_ADDR        14
#define EXCCAUSE_LOADSTORE_PIF_ADDR    15

/*
 * Offsets into the saved-register frame that _xt_level1 builds on the
 * faulting thread's stack (see vectors.S):
 *
 *   0: a0      1: a2      2: a3      3: a4     4: a5
 *   5: a6      6: a7      7: a8      8: a9     9: a10
 *  10: a11    11: a12    12: a13    13: a14   14: a15
 */
#define FRAME_A0                       0
#define FRAME_A2                       1
#define FRAME_A3                       2
#define FRAME_A4                       3
#define FRAME_A5                       4

/* Size of the SYSCALL instruction, in bytes. */
#define SYSCALL_INSN_LEN               3

#define XINU_NSYSCALL                 32
#define XINU_NEXCEPTION               64

static syscall_handler_t syscallTable[XINU_NSYSCALL];
static exception_handler_t exceptionTable[XINU_NEXCEPTION];

/* ------------------------------------------------------------------
 * Bring-up reporting hook.
 *
 * Faults can happen before sysinit() has initialized and opened the
 * console, in which case kprintf() cannot report anything.  These helpers
 * bypass the device layer and push bytes straight into the UART0 TX FIFO,
 * which the boot ROM has already programmed for 115200 8N1.
 * ------------------------------------------------------------------ */
#define XT_DBG_FIFO (*(volatile uint32_t *)0x60000000UL)
#define XT_DBG_STATUS (*(volatile uint32_t *)0x6000001CUL)

void xt_dbg_putc(char c);

void xt_dbg_putc(char c)
{
    /* NOTE: do not add a spin/retry loop here.  A bounded C loop around a
     * volatile peripheral read gets compiled into the Xtensa LOOP
     * instruction, whose zero-iteration exit is not a real "break", and a
     * bad UART_STATUS field makes it spin for an absurdly long time.  The
     * FIFO is 128 bytes and our diagnostics are short, so write directly. */
    XT_DBG_FIFO = (uint32_t)(unsigned char)c;
}

static void xt_dbg_ch(char c)
{
    xt_dbg_putc(c);
}

static void xt_dbg_hex(uint32_t v)
{
    int i;

    for (i = 28; i >= 0; i -= 4)
    {
        uint32_t d = (v >> i) & 0xFU;
        xt_dbg_ch((char)(d < 10 ? ('0' + (int)d) : ('a' + (int)d - 10)));
    }
}

static void xt_dbg_puts(const char *s)
{
    while ('\0' != *s)
    {
        xt_dbg_ch(*s++);
    }
}

/**
 * Register the handler for software trap @p num.  The handler receives the
 * trap's three arguments and its return value becomes the trap's result.
 */
void syscall_register(uint32_t num, syscall_handler_t fn)
{
    if (num < XINU_NSYSCALL)
    {
        syscallTable[num] = fn;
    }
}

/**
 * Register a handler for exception cause @p cause.  The handler is expected
 * to either fix EPC1 itself or never return.
 */
void exception_register(uint32_t cause, exception_handler_t fn)
{
    if (cause < XINU_NEXCEPTION)
    {
        exceptionTable[cause] = fn;
    }
}

/**
 * Raise software trap @p num from kernel code.
 *
 * The trap number travels in a2 and the arguments in a3/a4/a5, mirroring
 * the CALL0 argument registers so that the trap handler can treat them as a
 * normal C call.
 */
syscall syscall_invoke(uint32_t num, long a, long b, long c)
{
    register long r2 __asm__("a2") = (long)num;
    register long r3 __asm__("a3") = a;
    register long r4 __asm__("a4") = b;
    register long r5 __asm__("a5") = c;

    __asm__ volatile ("syscall"
                      : "+r"(r2)
                      : "r"(r3), "r"(r4), "r"(r5)
                      : "memory");

    return (syscall)r2;
}

static const char *exccause_name(uint32_t cause)
{
    switch (cause)
    {
    case EXCCAUSE_ILLEGAL_INSTRUCTION: return "IllegalInstruction";
    case EXCCAUSE_SYSCALL:             return "Syscall";
    case EXCCAUSE_INSTRUCTION_FETCH:   return "InstructionFetchError";
    case EXCCAUSE_LOADSTORE:           return "LoadStoreError";
    case EXCCAUSE_ALLOCA:              return "Alloca";
    case EXCCAUSE_DIVIDE_BY_ZERO:      return "IntegerDivideByZero";
    case EXCCAUSE_PRIVILEGED:          return "Privileged";
    case EXCCAUSE_LOADSTORE_ALIGN:     return "LoadStoreAlignment";
    case EXCCAUSE_INSTR_PIF_DATA:      return "InstrPIFDataError";
    case EXCCAUSE_LOADSTORE_PIF_DATA:  return "LoadStorePIFDataError";
    case EXCCAUSE_INSTR_PIF_ADDR:      return "InstrPIFAddrError";
    case EXCCAUSE_LOADSTORE_PIF_ADDR:  return "LoadStorePIFAddrError";
    default:                           return "Unknown";
    }
}

/**
 * C entry point for the level-1 vector.
 *
 * @param exccause  value of the EXCCAUSE special register
 * @param frame     pointer to the saved register frame on the faulting
 *                  thread's stack
 */
void xt_exception(uint32_t exccause, uint32_t *frame)
{
    exception_handler_t eh;

    /* Report straight to the UART before touching the console device,
     * which may not exist yet. */
    xt_dbg_puts("\r\n[exc cause=");
    xt_dbg_hex(exccause);
    xt_dbg_puts(" epc1=");
    xt_dbg_hex(RSR(epc1));
    xt_dbg_puts(" excvaddr=");
    xt_dbg_hex(RSR(excvaddr));
    xt_dbg_puts(" ps=");
    xt_dbg_hex(RSR(ps));
    xt_dbg_puts(" frame=");
    xt_dbg_hex((uint32_t)(uintptr_t)frame);
    xt_dbg_puts("]\r\n");

    if (EXCCAUSE_SYSCALL == exccause)
    {
        syscall_handler_t fn;
        syscall ret = SYSERR;
        uint32_t num = frame[FRAME_A2];

        if (num < XINU_NSYSCALL)
        {
            fn = syscallTable[num];
            if (NULL != fn)
            {
                ret = fn((long)frame[FRAME_A3], (long)frame[FRAME_A4],
                         (long)frame[FRAME_A5]);
            }
        }

        /* Hand the result back in a2 and step over the trap so that "rfe"
         * resumes at the next instruction. */
        frame[FRAME_A2] = (uint32_t)ret;
        WSR(epc1, RSR(epc1) + SYSCALL_INSN_LEN);
        return;
    }

    /* Give a registered handler a chance to deal with the fault. */
    eh = (exccause < XINU_NEXCEPTION) ? exceptionTable[exccause] : NULL;
    if (NULL != eh)
    {
        eh(exccause, frame);
        return;
    }

    /* ---- unrecoverable ---- */
    kprintf("\r\n*** EXCEPTION ***\r\n");
    kprintf("  cause   : %u (%s)\r\n", exccause, exccause_name(exccause));
    kprintf("  epc1    : 0x%08X\r\n", RSR(epc1));
    kprintf("  ps      : 0x%08X\r\n", RSR(ps));
    kprintf("  excvaddr: 0x%08X\r\n", RSR(excvaddr));
    kprintf("  a0      : 0x%08X\r\n", frame[FRAME_A0]);
    kprintf("  a2      : 0x%08X\r\n", frame[FRAME_A2]);
    kprintf("  a3      : 0x%08X\r\n", frame[FRAME_A3]);
    kprintf("Halting.\r\n");

    halt();
}
