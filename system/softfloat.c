/**
 * @file softfloat.c
 *
 * Coma flotante en software para el ESP32-S3.
 *
 * Este puerto compila con `-mabi=call0` y con coma flotante en software (el
 * toolchain no habilita la FPU del LX7 por omisión), de modo que toda
 * operación con `float` o `double` se resuelve mediante llamadas a libgcc:
 * `__adddf3`, `__muldf3`, `__divsf3`, ...  El libgcc que reparte el toolchain
 * está construido para la ABI de ventanas de registros: sus rutinas de coma
 * flotante empiezan con `entry a1, 16` y terminan con `retw.n`, que exigen
 * rotar la ventana; desde código call0 la ventana nunca se gira, así que
 * enlazarlas mata el core en el primer `double`.  Este archivo provee esas
 * rutinas en C plano, compiladas con las mismas banderas que el resto del
 * kernel, cumpliendo IEEE 754 binary64: redondeo a par, subnormales,
 * infinitos y NaN.
 *
 * Se implementan exactamente los símbolos que el compilador emite para este
 * puerto (comprobar con `nm -u compile/xinu.elf | grep '^ *U __'`):
 *
 *   __adddf3  __subdf3  __muldf3  __divdf3     aritmética binary64
 *   __fixdfsi __fixunsdfsi                     doble  -> entero
 *   __floatsidf __floatunsidf                  entero -> doble
 *   __extendsfdf2                              simple -> doble
 *   __ltdf2 __ledf2 __gtdf2 __gedf2            comparaciones
 *   __eqdf2 __nedf2 __cmpdf2
 *   __divsf3                                   división de precisión simple
 *
 * Más los auxiliares de 64 bits de libgcc por si el compilador llegara a
 * pedirlos.
 *
 * Las convenciones de las comparaciones se dedujeron del código que GCC
 * genera para `a < b`, `a > b` y `a >= b` (verificado con objdump):
 *
 *   __ltdf2: GCC extrae el bit de signo del resultado, luego se cumple
 *            "a<b" si y sólo si el resultado es negativo.  Un NaN debe
 *            devolver algo no negativo para que la comparación sea falsa.
 *   __gtdf2: GCC comprueba `res >= 1`, luego "a>b" ⟺ resultado ≥ 1.
 *   __gedf2: GCC calcula `(res<0 ? -1 : 0) + 1`, luego "a>=b" ⟺ res ≥ 0.
 */

#include <kernel.h>
#include <stdint.h>

typedef int32_t   s32;
typedef uint32_t  u32;
typedef int64_t   s64;
typedef uint64_t  u64;

/* ---------- formato binary64 ------------------------------------------ */

#define DBITS   52
#define DBIAS   1023
#define DMASK   ((((u64)1) << DBITS) - 1)
#define DIMP    (((u64)1) << DBITS)
#define DEXP(x) ((s32)((((u64)(x)) >> DBITS) & 0x7FFu))
#define DMANT(x) (((u64)(x)) & DMASK)
#define DSIGN(x) (((u64)(x)) >> 63)

static const u64 D_QNAN = 0x7FF8000000000000ull;
static const u64 D_INF  = 0x7FF0000000000000ull;

/* ---------- formato binary32 ------------------------------------------ */

#define FBITS   23
#define FBIAS   127
#define FMASK   ((((u32)1) << FBITS) - 1)
#define FIMP    (((u32)1) << FBITS)
#define FEXP(x) ((s32)((((u32)(x)) >> FBITS) & 0xFFu))
#define FMANT(x) (((u32)(x)) & FMASK)
#define FSIGN(x) (((u32)(x)) >> 31)

static const u32 F_QNAN = 0x7FC00000u;
static const u32 F_INF  = 0x7F800000u;

/* ---------- utilidades ------------------------------------------------- */

/*
 * Bits más significativos a cero.  Se escribe a mano porque `__builtin_clz`
 * se traduce en una llamada a `__clzsi2` si el toolchain no tiene la opción
 * CLZ del ISA, y ese símbolo tampoco está disponible.
 */
static int clz32(u32 x)
{
    int n = 0;

    if (x == 0) {
        return 32;
    }
    if ((x & 0xFFFF0000u) == 0) {
        n += 16;
        x <<= 16;
    }
    if ((x & 0xFF000000u) == 0) {
        n += 8;
        x <<= 8;
    }
    if ((x & 0xF0000000u) == 0) {
        n += 4;
        x <<= 4;
    }
    if ((x & 0xC0000000u) == 0) {
        n += 2;
        x <<= 2;
    }
    if ((x & 0x80000000u) == 0) {
        n += 1;
    }
    return n;
}

static int clz64(u64 x)
{
    u32 hi = (u32)(x >> 32);

    if (hi != 0) {
        return clz32(hi);
    }
    return 32 + clz32((u32)x);
}

/*
 * Extrae los 32 bits menos significativos de `x >> n`, con `n` acotado a
 * [0,64] por el llamador.  Evita el desplazamiento variable de 64 bits, que
 * el compilador traduciría en otra llamada a libgcc.
 */
static u32 shr64(u64 x, int n)
{
    u32 lo = (u32)x;
    u32 hi = (u32)(x >> 32);

    if (n >= 64) {
        return 0;
    }
    if (n >= 32) {
        return hi >> (n - 32);
    }
    if (n == 0) {
        return lo;
    }
    return (lo >> n) | (hi << (32 - n));
}

static int disnan(u64 x)
{
    return (DEXP(x) == 0x7FF) && (DMANT(x) != 0);
}

static int disinf(u64 x)
{
    return (DEXP(x) == 0x7FF) && (DMANT(x) == 0);
}

static int diszero(u64 x)
{
    return (DEXP(x) == 0) && (DMANT(x) == 0);
}

/*
 * Decodifica un doble y lo deja normalizado: la mantisa ocupa los bits
 * 0..55 con el bit implícito en el 55 y tres bits de guarda por debajo
 * (ver `dpack`), y `*e` es el exponente sin sesgo, de modo que el valor vale
 * 2^*e * m / 2^55.  Devuelve falso si el valor es cero, NaN o infinito.
 */
static int ddecode(u64 x, u64 *m, s32 *e)
{
    s32 ee = DEXP(x);
    u64 mm = DMANT(x);

    if (ee == 0) {
        if (mm == 0) {
            return 0;
        }
        /* Subnormal: se desplaza la mantisa hasta el bit 55, que es donde
         * vive el implícito en la forma normalizada. */
        ee = (1 - DBIAS) - (clz64(mm) - (63 - DBITS));
        *m = mm << (clz64(mm) - 8);
    } else {
        mm |= DIMP;
        ee -= DBIAS;
        *m = mm << 3;
    }

    *e = ee;
    return 1;
}

/*
 * Redondea `m >> sh` (sh > 0) al entero más próximo, con desempate a par.
 * `sticky` avisa de restos que no están representados en `m`.  Se usa para
 * los resultados subnormales, donde el número de bits significativos depende
 * del exponente y hay que redondear una sola vez.
 */
static u64 shrround(u64 m, int sh, int sticky)
{
    u64 r;
    int g;
    int rd;
    int st;

    if (sh <= 0) {
        return m;
    }
    if (sh > 56) {
        return 0;
    }
    r  = m >> sh;
    g  = (int)((m >> (sh - 1)) & 1u);
    rd = (sh >= 2) ? (int)((m >> (sh - 2)) & 1u) : 0;
    st = (sticky != 0);
    if (sh >= 3) {
        st = st | ((m & ((((u64)1) << (sh - 2)) - 1)) != 0);
    }
    if (g != 0 && (rd != 0 || st != 0 || ((r & 1u) != 0))) {
        r += 1;
    }
    return r;
}

/*
 * Empaqueta un resultado finito de precisión doble.
 *
 * `m` lleva la mantisa con el bit implícito en el bit 55 y TRES bits de
 * guarda por debajo: el bit 2 es el de redondeo, el bit 1 el «round» y el
 * bit 0 recoge cualquier resto (sticky).  Hacen falta los tres para que el
 * redondeo a par sea exacto.  `sticky` añade restos externos.  `e` es el
 * exponente sin sesgo y `sign` el signo ya desplazado.
 */
static u64 dpack(u64 m, int sticky, s32 e, u64 sign)
{
    u64 r;
    s32 eb;
    int g;
    int rd;
    int st;

    eb = e + DBIAS;

    /* Resultado subnormal: la mantisa vale `m >> (4 - eb)`; se redondea
     * desde la mantisa completa para no redondear dos veces. */
    if (eb <= 0) {
        u64 rr;

        if (eb < -(56 - 4)) {
            return sign;
        }
        rr = shrround(m, 4 - eb, sticky);
        if (rr >= DIMP) {
            return sign | (((u64)1) << DBITS);   /* mínimo normal */
        }
        return sign | (rr & DMASK);
    }

    r  = m >> 3;
    g  = (int)((m >> 2) & 1u);
    rd = (int)((m >> 1) & 1u);
    st = (int)(m & 1u) | (sticky != 0);

    if (g != 0 && (rd != 0 || st != 0 || ((r & 1u) != 0))) {
        r += 1;
        if (r == (DIMP << 1)) {         /* acarreo: 2^53 */
            r >>= 1;
            eb += 1;
        }
    }
    if (r == 0) {
        return sign;
    }
    if (eb >= 0x7FF) {
        return sign | D_INF;
    }
    return sign | (((u64)eb) << DBITS) | (r & DMASK);
}

/* ---------- suma y resta ------------------------------------------------ */

/*
 * `a + b` (sub == 0) o `a - b` (sub != 0).
 */
static u64 addsub(u64 a, u64 b, int sub)
{
    u64 sa = DSIGN(a);
    u64 sb = DSIGN(b) ^ (u64)sub;
    u64 ma, mb;
    s32 ea, eb;
    s32 sh;
    int lost;

    /* ---- infinitos y NaN ---- */
    if (disnan(a)) {
        return D_QNAN | (sa << 63);
    }
    if (disnan(b)) {
        return D_QNAN | (sb << 63);
    }
    if (disinf(a) || disinf(b)) {
        if (disinf(a) && disinf(b) && (sa != sb)) {
            return D_QNAN | (sa << 63);     /* infinito - infinito */
        }
        if (disinf(a)) {
            return (sa << 63) | D_INF;
        }
        return (sb << 63) | D_INF;
    }

    /* ---- ceros: el resultado toma el signo del otro operando ---- */
    if (diszero(a)) {
        if (diszero(b)) {
            return (sa & sb) << 63;         /* 0 + 0 / 0 - 0 */
        }
        return (sb << 63) | (b & ~(((u64)1) << 63));
    }
    if (diszero(b)) {
        return (sa << 63) | (a & ~(((u64)1) << 63));
    }

    if (!ddecode(a, &ma, &ea) || !ddecode(b, &mb, &eb)) {
        return (sa << 63);                  /* nunca se alcanza */
    }

    /* ---- se alinean dejando delante la de mayor exponente ---- */
    if (ea < eb) {
        u64 tm = ma;
        u64 ts = sa;
        s32 te = ea;

        ma = mb;
        mb = tm;
        sa = sb;
        sb = ts;
        ea = eb;
        eb = te;
    }

    /* Alineamos la mantisa menor.  `lost` avisa de que la parte baja no era
     * cero: en la suma hay que redondear hacia arriba y en la resta hacia
     * abajo, de ahí que se guarde aparte. */
    lost = 0;
    sh = ea - eb;
    if (sh > 56) {
        lost = (mb != 0);
        mb = lost ? 1 : 0;
    } else if (sh > 0) {
        lost = (mb & ((((u64)1) << sh) - 1)) != 0;
        mb >>= sh;
        if (lost) {
            mb |= 1;
        }
    }

    if (sa == sb) {
        /* Suma: el bit 56 puede subir, entonces pasa a guard+round+sticky. */
        u64 sum = ma + mb;

        if (lost) {
            sum |= 1;
        }
        if ((sum & (((u64)1) << 56)) != 0) {
            sum = (sum >> 1) | (sum & 1u);
            ea += 1;
        }
        return dpack(sum, 0, ea, sa << 63);
    } else {
        /* Resta: el signo lo marca la mantisa de mayor magnitud. */
        u64 d;
        u64 sign;

        if (ma >= mb) {
            d = ma - mb;
            sign = sa;
        } else {
            d = mb - ma;
            sign = sb;
        }
        if (lost) {
            /* La parte baja que se perdió hay que restarla, no sumarla. */
            if (d == 0) {
                return 0;               /* el resto es despreciable */
            }
            d -= 1;
            d |= 1;
        }
        if (d == 0) {
            /* Cancelación exacta: IEEE manda +0 salvo en la resta de dos
             * ceros, ya resuelta más arriba. */
            return ((sa != sb) ? 0 : sa) << 63;
        }
        sh = clz64(d) - 8;              /* normaliza hasta el bit 55 */
        d <<= sh;
        ea -= sh;
        return dpack(d, 0, ea, sign << 63);
    }
}

u64 __adddf3(u64 a, u64 b)
{
    return addsub(a, b, 0);
}

u64 __subdf3(u64 a, u64 b)
{
    return addsub(a, b, 1);
}

/* ---------- producto --------------------------------------------------- */

u64 __muldf3(u64 a, u64 b)
{
    u64 sign = DSIGN(a) ^ DSIGN(b);
    u64 ma, mb;
    s32 ea, eb;
    u32 ma0, ma1, mb0, mb1;
    u64 p00, mid, lo, hi;
    u64 m;
    s32 e;

    if (disnan(a) || disnan(b)) {
        return D_QNAN | (sign << 63);
    }
    if (disinf(a) || disinf(b)) {
        if (diszero(a) || diszero(b)) {
            return D_QNAN | (sign << 63);   /* infinito * 0 */
        }
        return D_INF | (sign << 63);
    }
    if (diszero(a) || diszero(b)) {
        return sign << 63;
    }

    if (!ddecode(a, &ma, &ea) || !ddecode(b, &mb, &eb)) {
        return sign << 63;
    }

    /* Mantisas de 53 bits (bit implícito en el 52) para el producto. */
    ma >>= 3;
    mb >>= 3;

    ma1 = (u32)(ma >> 32);
    ma0 = (u32)ma;
    mb1 = (u32)(mb >> 32);
    mb0 = (u32)mb;

    p00 = ((u64)ma0) * mb0;
    mid = ((u64)ma0) * mb1 + ((u64)ma1) * mb0 + (p00 >> 32);
    lo  = (mid << 32) | (p00 & 0xFFFFFFFFu);
    hi  = ((u64)ma1) * mb1 + (mid >> 32);

    /* `hi:lo` es el producto exacto, en [2^104, 2^106). */
    if (hi >= (((u64)1) << 41)) {               /* >= 2^105 */
        m = (lo >> 50) | (hi << 14);            /* implícito en el bit 55 */
        if ((lo & ((((u64)1) << 50) - 1)) != 0) {
            m |= 1;
        }
        e = ea + eb + 1;
    } else {
        m = (lo >> 49) | (hi << 15);
        if ((lo & ((((u64)1) << 49) - 1)) != 0) {
            m |= 1;
        }
        e = ea + eb;
    }

    return dpack(m, 0, e, sign << 63);
}

/* ---------- cociente --------------------------------------------------- */

/*
 * Divide dos mantisas normalizadas de 53 bits (bit implícito en el 52) y
 * devuelve un cociente de 63 bits con el bit entero siempre en el bit 62,
 * más el exponente ajustado y la bandera «resto != 0», que aporta el bit
 * pegajoso.  El llamador deja en `*e` la diferencia de exponentes.
 *
 * Se hace la división bit a bit con desplazamiento y resta: el cociente
 * sobra para redondear tanto a doble como a simple y evita cualquier
 * llamada a `__udivdi3`.
 */
static u64 divcore(u64 ma, u64 mb, s32 *e, int *rest)
{
    u64 num;
    u64 q = 1;
    u64 r;
    int i;

    if (ma >= mb) {
        num = ma;
    } else {
        num = ma << 1;
        *e -= 1;
    }
    r = num - mb;

    for (i = 0; i < 62; i++) {
        r <<= 1;
        q <<= 1;
        if (r >= mb) {
            r -= mb;
            q |= 1;
        }
    }

    *rest = (r != 0);
    return q;
}

u64 __divdf3(u64 a, u64 b)
{
    u64 sign = DSIGN(a) ^ DSIGN(b);
    u64 ma, mb;
    s32 ea, eb;
    u64 q;
    u64 m;
    int rest;

    if (disnan(a) || disnan(b)) {
        return D_QNAN | (sign << 63);
    }
    if (disinf(a)) {
        if (disinf(b)) {
            return D_QNAN | (sign << 63);   /* infinito / infinito */
        }
        return D_INF | (sign << 63);
    }
    if (disinf(b)) {
        return sign << 63;                  /* finito / infinito */
    }
    if (diszero(b)) {
        if (diszero(a)) {
            return D_QNAN | (sign << 63);   /* 0 / 0 */
        }
        return D_INF | (sign << 63);        /* x / 0 */
    }
    if (diszero(a)) {
        return sign << 63;
    }

    if (!ddecode(a, &ma, &ea) || !ddecode(b, &mb, &eb)) {
        return sign << 63;
    }

    ma >>= 3;
    mb >>= 3;
    eb = ea - eb;
    q = divcore(ma, mb, &eb, &rest);

    /* `q` tiene 63 bits; se pasa a la forma de 56 con guard+round+sticky
     * descartando los 7 bits menos significativos. */
    m = q >> 7;
    if (rest || ((q & 0x7Fu) != 0)) {
        m |= 1;
    }
    return dpack(m, 0, eb, sign << 63);
}

/* ---------- conversiones a y desde entero ------------------------------ */

u64 __floatsidf(s32 v)
{
    u32 x;
    u64 m;
    u64 sign;
    s32 e;

    if (v == 0) {
        return 0;
    }
    if (v < 0) {
        x = 0u - (u32)v;             /* magnitud; correcto también con INT_MIN */
        sign = ((u64)1) << 63;
    } else {
        x = (u32)v;
        sign = 0;
    }
    e = 31 - clz32(x);              /* posición del bit más significativo */
    /* La mantisa es exacta: 32 bits caben sin redondeo en 53. */
    m = ((u64)x) << (DBITS - e);
    return sign | ((((u64)(DBIAS + e)) << DBITS) | (m & DMASK));
}

u64 __floatunsidf(u32 v)
{
    u64 m;
    s32 e;

    if (v == 0) {
        return 0;
    }
    e = 31 - clz32(v);
    m = ((u64)v) << (DBITS - e);
    return ((((u64)(DBIAS + e)) << DBITS) | (m & DMASK));
}

s32 __fixdfsi(u64 a)
{
    s32 e;
    u64 m;
    u32 r;
    int neg = (int)DSIGN(a);

    if (disnan(a) || disinf(a)) {
        return neg ? (-0x7FFFFFFF - 1) : 0x7FFFFFFF;
    }
    if (!ddecode(a, &m, &e)) {
        return 0;
    }

    /* El valor es 2^e * m / 2^55 con m en [2^55, 2^56), luego ya supera
     * 2^31 en cuanto e llega a 31. */
    if (e > 31) {
        return neg ? (-0x7FFFFFFF - 1) : 0x7FFFFFFF;
    }

    r = shr64(m, 55 - e);           /* trunca hacia cero, como exige C */
    if (neg) {
        if (r > 0x80000000u) {
            return -0x7FFFFFFF - 1;
        }
        return (s32)(0u - r);
    }
    if (r > 0x7FFFFFFFu) {
        return 0x7FFFFFFF;
    }
    return (s32)r;
}

u32 __fixunsdfsi(u64 a)
{
    s32 e;
    u64 m;

    if (disnan(a) || disinf(a) || DSIGN(a)) {
        return (DSIGN(a) && !disnan(a) && !disinf(a)) ? 0 : 0xFFFFFFFFu;
    }
    if (!ddecode(a, &m, &e)) {
        return 0;
    }
    if (e > 31) {
        return 0xFFFFFFFFu;
    }
    return shr64(m, 55 - e);
}

/* ---------- precisión simple -> doble ---------------------------------- */

/*
 * Convierte un `float` en bits de doble.  La conversión es siempre exacta.
 * Para un subnormal simple el valor vale 2^-149 * m, lo que deja escribir el
 * resultado directamente.
 */
u64 __extendsfdf2(u32 a)
{
    u32 sa = FSIGN(a);
    s32 fe = FEXP(a);
    u32 fm = FMANT(a);
    u64 md;
    s32 ed;

    if (fe == 0xFF) {
        if (fm != 0) {
            return D_QNAN | (((u64)sa) << 63);
        }
        return D_INF | (((u64)sa) << 63);
    }
    if (fe == 0) {
        if (fm == 0) {
            return ((u64)sa) << 63;
        }
        ed = (31 - clz32(fm)) - 149;                    /* = k - 149 */
        md = ((u64)(fm << (FBITS - 1 - (31 - clz32(fm))))) << 30;
    } else {
        ed = fe - FBIAS;
        md = (((u64)FIMP) | ((u64)fm)) << (DBITS - FBITS);
    }

    return (((u64)sa) << 63) |
           ((((u64)(ed + DBIAS)) << DBITS) | (md & DMASK));
}

/* ---------- división de precisión simple ------------------------------- */

/*
 * Empaqueta un cociente de precisión simple.  `m` sigue la misma convención
 * que en `dpack` pero con el bit implícito en el 26: 24 bits significativos
 * y tres de guarda.
 */
static u32 fpack(u64 m, int sticky, s32 e, u32 sign)
{
    u32 r;
    s32 eb;
    int g;
    int rd;
    int st;

    eb = e + FBIAS;
    if (eb <= 0) {
        u64 rr;

        if (eb < -(56 - 4)) {
            return sign;
        }
        rr = shrround(m, 4 - eb, sticky);
        if (rr >= FIMP) {
            return sign | FIMP;         /* mínimo normal */
        }
        return sign | ((u32)rr & FMASK);
    }

    r  = (u32)(m >> 3);
    g  = (int)((m >> 2) & 1u);
    rd = (int)((m >> 1) & 1u);
    st = (int)(m & 1u) | (sticky != 0);

    if (g != 0 && (rd != 0 || st != 0 || ((r & 1u) != 0))) {
        r += 1;
        if (r == (FIMP << 1)) {
            r >>= 1;
            eb += 1;
        }
    }
    if (r == 0) {
        return sign;
    }
    if (eb >= 0xFF) {
        return sign | F_INF;
    }
    return sign | (((u32)eb) << FBITS) | (r & FMASK);
}

u32 __divsf3(u32 a, u32 b)
{
    u32 sign = FSIGN(a) ^ FSIGN(b);
    s32 ea = FEXP(a);
    s32 eb = FEXP(b);
    u32 ma = FMANT(a);
    u32 mb = FMANT(b);
    u64 qa, qb, q, m;
    s32 Ea, Eb, e;
    int rest;

    if (ea == 0xFF || eb == 0xFF) {
        if (ea == 0xFF && ma != 0) {
            return F_QNAN | (sign << 31);
        }
        if (eb == 0xFF && mb != 0) {
            return F_QNAN | (sign << 31);
        }
        if (ea == 0xFF && eb == 0xFF) {
            return F_QNAN | (sign << 31);   /* infinito / infinito */
        }
        if (ea == 0xFF) {
            return F_INF | (sign << 31);    /* infinito / finito */
        }
        return sign << 31;                  /* finito / infinito */
    }
    if (eb == 0 && mb == 0) {
        if (ea == 0 && ma == 0) {
            return F_QNAN | (sign << 31);   /* 0 / 0 */
        }
        return F_INF | (sign << 31);        /* x / 0 */
    }
    if (ea == 0 && ma == 0) {
        return sign << 31;
    }

    /* Se normalizan a 53 bits (bit implícito en el 52) para reutilizar el
     * mismo núcleo de división del doble.  La conversión es exacta. */
    if (ea == 0) {
        Ea = (31 - clz32(ma)) - 149;
        qa = ((u64)(ma << (FBITS - 1 - (31 - clz32(ma))))) << 30;
    } else {
        Ea = ea - FBIAS;
        qa = (((u64)FIMP) | ((u64)ma)) << (DBITS - FBITS);
    }
    if (eb == 0) {
        Eb = (31 - clz32(mb)) - 149;
        qb = ((u64)(mb << (FBITS - 1 - (31 - clz32(mb))))) << 30;
    } else {
        Eb = eb - FBIAS;
        qb = (((u64)FIMP) | ((u64)mb)) << (DBITS - FBITS);
    }

    e = Ea - Eb;
    q = divcore(qa, qb, &e, &rest);

    /* De los 63 bits del cociente se conservan los 27 más significativos y
     * el redondeo se hace una sola vez (nada de pasar por doble). */
    m = q >> 36;
    if (rest || ((q & ((((u64)1) << 36) - 1)) != 0)) {
        m |= 1;
    }
    return fpack(m, 0, e, sign << 31);
}

/* ---------- comparaciones ---------------------------------------------- */

/*
 * -1, 0 o +1 según a sea menor, igual o mayor que b.  Los operandos no son
 * NaN por el llamador.
 */
static int dcmp(u64 a, u64 b)
{
    u64 xa = a & ~(((u64)1) << 63);
    u64 xb = b & ~(((u64)1) << 63);

    if (xa == xb) {
        if (xa == 0) {
            return 0;               /* +0 == -0 */
        }
        return (DSIGN(a) == DSIGN(b)) ? 0 : (DSIGN(a) ? -1 : 1);
    }
    if (DSIGN(a) != DSIGN(b)) {
        return DSIGN(a) ? -1 : 1;
    }
    if (DSIGN(a)) {
        return (xa > xb) ? -1 : 1;
    }
    return (xa > xb) ? 1 : -1;
}

int __cmpdf2(u64 a, u64 b)
{
    if (disnan(a) || disnan(b)) {
        return 1;
    }
    return dcmp(a, b);
}

int __ltdf2(u64 a, u64 b)
{
    if (disnan(a) || disnan(b)) {
        return 1;                   /* desordenado: no es menor */
    }
    return dcmp(a, b);
}

int __ledf2(u64 a, u64 b)
{
    if (disnan(a) || disnan(b)) {
        return 1;
    }
    return dcmp(a, b);
}

int __gtdf2(u64 a, u64 b)
{
    if (disnan(a) || disnan(b)) {
        return -1;
    }
    return dcmp(a, b);
}

int __gedf2(u64 a, u64 b)
{
    if (disnan(a) || disnan(b)) {
        return -1;
    }
    return dcmp(a, b);
}

int __eqdf2(u64 a, u64 b)
{
    if (disnan(a) || disnan(b)) {
        return 1;
    }
    return (dcmp(a, b) == 0) ? 0 : 1;
}

int __nedf2(u64 a, u64 b)
{
    if (disnan(a) || disnan(b)) {
        return 1;
    }
    return (dcmp(a, b) == 0) ? 0 : 1;
}

/* ---------- auxiliares de libgcc de 64 bits ---------------------------- */

/*
 * El compilador puede recurrir a estos si la rutina que está generando hace
 * desplazamientos o productos de 64 bits variables.  Se escriben con
 * operaciones de 32 bits precisamente para no volver a pedirlos.
 */
u64 __lshrdi3(u64 a, int b)
{
    u32 lo = (u32)a;
    u32 hi = (u32)(a >> 32);

    if (b >= 64) {
        return 0;
    }
    if (b >= 32) {
        return (u64)(b == 32 ? hi : (hi >> (b - 32)));
    }
    if (b == 0) {
        return a;
    }
    return (((u64)(hi >> b)) << 32) | (u64)((lo >> b) | (hi << (32 - b)));
}

u64 __ashldi3(u64 a, int b)
{
    u32 lo = (u32)a;
    u32 hi = (u32)(a >> 32);

    if (b >= 64) {
        return 0;
    }
    if (b >= 32) {
        return (u64)(b == 32 ? lo : (lo << (b - 32))) << 32;
    }
    if (b == 0) {
        return a;
    }
    return (((u64)((hi << b) | (lo >> (32 - b)))) << 32) | (u64)(lo << b);
}

u64 __ashrdi3(u64 a, int b)
{
    u32 lo = (u32)a;
    s32 hi = (s32)(a >> 32);

    if (b >= 64) {
        return (u64)((u32)(hi >> 31)) << 32 | (u32)(hi >> 31);
    }
    if (b >= 32) {
        return (u64)((u32)(hi >> 31)) << 32 | (u32)(hi >> (b - 32));
    }
    if (b == 0) {
        return a;
    }
    return (((u64)((u32)(hi >> b))) << 32) |
           (u64)((lo >> b) | ((u32)hi << (32 - b)));
}

/*
 * Producto de dos enteros de 32 bits que da 64.  GCC lo pide para las
 * mantisas (32x32 -> 64), y hay que escribirlo con multiplicaciones de 16
 * bits para no volver a llamarse a sí mismo.
 */
u64 __umulsidi3(u32 a, u32 b)
{
    u32 a0 = a & 0xFFFFu, a1 = a >> 16;
    u32 b0 = b & 0xFFFFu, b1 = b >> 16;
    u32 p00 = a0 * b0;
    u32 p01 = a0 * b1;
    u32 p10 = a1 * b0;
    u32 p11 = a1 * b1;
    u32 mid = (p00 >> 16) + (p01 & 0xFFFFu) + (p10 & 0xFFFFu);
    u32 lo = (p00 & 0xFFFFu) | (mid << 16);
    u32 hi = p11 + (p01 >> 16) + (p10 >> 16) + (mid >> 16);

    return (((u64)hi) << 32) | (u64)lo;
}

u64 __muldi3(u64 a, u64 b)
{
    u32 a0 = (u32)a, a1 = (u32)(a >> 32);
    u32 b0 = (u32)b, b1 = (u32)(b >> 32);
    u64 p00 = ((u64)a0) * b0;
    u64 p01 = ((u64)a0) * b1;
    u64 p10 = ((u64)a1) * b0;
    u64 mid = p01 + p10 + (p00 >> 32);

    /* Sólo hacen falta los 64 bits bajos del producto. */
    return (mid << 32) | (p00 & 0xFFFFFFFFu);
}
