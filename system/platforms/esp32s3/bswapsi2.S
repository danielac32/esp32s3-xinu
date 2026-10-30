/**
 * @file bswapsi2.S
 *
 * Compiler support routine for the ESP32-S3: 32 bit byte swap.
 *
 * GCC lowers __builtin_bswap32(), which the network code uses through
 * htonl()/ntohl(), into a call to __bswapsi2() unless the target has an
 * instruction for it.  That routine normally lives in libgcc, but this
 * port deliberately does not link libgcc: the toolchain only ships a
 * windowed-ABI libgcc, and a windowed callee cannot be called from CALL0
 * code.
 *
 * NOTE: this cannot be written in C.  GCC recognizes the shift/mask
 * idiom as a byte swap and lowers it back into a call to __bswapsi2, so a
 * C implementation calls itself until the stack runs out.
 */
/* Embedded Xinu, ported to ESP32-S3. */

    .text
    .align  4
    .globl  __bswapsi2
    .type   __bswapsi2, @function
__bswapsi2:
    /* a2 holds the value to swap.  Split it into its four bytes and put
     * them back in reverse order. */
    extui   a3, a2, 0, 8            /* byte 0 -> byte 3 */
    extui   a4, a2, 8, 8            /* byte 1 -> byte 2 */
    extui   a5, a2, 16, 8           /* byte 2 -> byte 1 */
    extui   a6, a2, 24, 8           /* byte 3 -> byte 0 */

    slli    a3, a3, 24
    slli    a4, a4, 16
    slli    a5, a5, 8

    or      a3, a3, a4
    or      a5, a5, a6
    or      a2, a3, a5

    ret.n
    .size   __bswapsi2, . - __bswapsi2
