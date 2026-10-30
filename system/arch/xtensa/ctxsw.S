/**
 * @file ctxsw.S
 *
 * Context switch for Embedded Xinu on the ESP32-S3 (Xtensa LX7).
 *
 * This port uses the CALL0 ABI (-mabi=call0): there are no register
 * windows, only the 16 general purpose registers a0..a15, where
 * a0 is the return address and a1 is the stack pointer.
 *
 * Saved-context frame (64 bytes, 16 words):
 *
 *     offset  register
 *      0      a0    (return address)
 *      4      a2
 *      8      a3
 *     12      a4
 *     16      a5
 *     20      a6
 *     24      a7
 *     28      a8
 *     32      a9
 *     36      a10
 *     40      a11
 *     44      a12
 *     48      a13
 *     52      a14
 *     56      a15
 *     60      PS    (processor status: interrupt level, UM, ...)
 *
 * The frame must be 16-byte aligned, which it is as long as the stack
 * it is built on is 16-byte aligned (setupStack() guarantees this).
 *
 * C prototype:   void ctxsw(void **oldstkptr, void **newstkptr, uchar asid)
 *                -> a2 = oldstkptr, a3 = newstkptr, a4 = asid (ignored)
 */
/* Embedded Xinu, ported to ESP32-S3. */

    .text
    .globl  ctxsw
    .align  4
    .type   ctxsw, @function
ctxsw:
    /* ---- save the outgoing thread ---- */
    addi    sp, sp, -64

    s32i    a0,  sp, 0
    s32i    a2,  sp, 4
    s32i    a3,  sp, 8
    s32i    a4,  sp, 12
    s32i    a5,  sp, 16
    s32i    a6,  sp, 20
    s32i    a7,  sp, 24
    s32i    a8,  sp, 28
    s32i    a9,  sp, 32
    s32i    a10, sp, 36
    s32i    a11, sp, 40
    s32i    a12, sp, 44
    s32i    a13, sp, 48
    s32i    a14, sp, 52
    s32i    a15, sp, 56

    rsr.ps  a6
    s32i    a6,  sp, 60

    /* ---- *oldstkptr = sp ---- */
    s32i    sp, a2, 0

    /* ---- load the incoming thread ---- */
    l32i    sp, a3, 0

    l32i    a0,  sp, 0
    l32i    a2,  sp, 4
    l32i    a3,  sp, 8
    l32i    a4,  sp, 12
    l32i    a5,  sp, 16
    l32i    a6,  sp, 20
    l32i    a7,  sp, 24
    l32i    a8,  sp, 28
    l32i    a9,  sp, 32
    l32i    a10, sp, 36
    l32i    a11, sp, 40
    l32i    a12, sp, 44
    l32i    a13, sp, 48
    l32i    a14, sp, 52
    l32i    a15, sp, 56

    l32i    a6,  sp, 60          /* saved PS */
    wsr.ps  a6
    rsync

    addi    sp, sp, 64
    ret
    .size   ctxsw, . - ctxsw

/* ------------------------------------------------------------------ */
/*  Entry stub for freshly created threads.                            */
/*                                                                     */
/*  setupStack() points a0 here while a12 holds the thread body and     */
/*  a13 holds the return address that must be in a0 when the thread     */
/*  body is entered (normally userret()).                              */
/* ------------------------------------------------------------------ */
    .globl  _xt_thread_start
    .align  4
    .type   _xt_thread_start, @function
_xt_thread_start:
    mov     a0, a13
    jx      a12
    .size   _xt_thread_start, . - _xt_thread_start
