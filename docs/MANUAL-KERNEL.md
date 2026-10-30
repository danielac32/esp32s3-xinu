# Manual del kernel Xinu — ESP32-S3

### Concurrencia, C y cómo modificar el kernel

Este manual está escrito contra **este árbol** (`/home/daniel/s3-xinu`), no
contra el Xinu genérico. Todos los nombres de funciones, constantes y ficheros
existen aquí. La idea es que puedas leerlo, ejecutar los ejemplos y perder el
miedo a tocar `system/`, `network/` y `device/`.

> **Cómo usarlo.** Lee las partes 1 a 6 en orden. Haz los ejercicios de la
> parte 9 con el manual al lado. La parte 8 es la guía de trabajo para cuando
> quieras añadir algo al kernel.

---

## Índice

- [0. El mapa del árbol y el arranque](#0-el-mapa-del-árbol-y-el-arranque)
- [1. Refresco de C con este kernel](#1-refresco-de-c-con-este-kernel)
- [2. Semáforos](#2-semáforos)
- [3. Monitores](#3-monitores)
- [4. Mailboxes](#4-mailboxes)
- [5. Mensajes de hilo (`send`/`receive`)](#5-mensajes-de-hilo-sendreceive)
- [6. Cómo elegir (y no liarse)](#6-cómo-elegir-y-no-liarse)
- [7. Estudio de caso: un ping completo](#7-estudio-de-caso-un-ping-completo)
- [8. Cómo modificar el kernel](#8-cómo-modificar-el-kernel)
- [9. Ejercicios guiados](#9-ejercicios-guiados)
- [Apéndice A. API de bolsillo](#apéndice-a-api-de-bolsillo)
- [Apéndice B. Constantes](#apéndice-b-constantes)
- [Apéndice C. Glosario](#apéndice-c-glosario)

---

## 0. El mapa del árbol y el arranque

### 0.1 Directorios

| Directorio | Qué hay |
|---|---|
| `loader/platforms/esp32s3/` | `start.S`, vectores, arranque en `0x0` |
| `system/` | Núcleo: hilos, semáforos, monitores, memoria, reloj, `boot.c` |
| `system/arch/xtensa/` | `ctxsw.S`, manejo de interrupciones de Xtensa |
| `system/platforms/esp32s3/` | Reloj, UART de bajo nivel, plataforma |
| `device/` | Drivers: `uart-esp32s3`, `tty`, `w5500`, `sd`, `tft`, `button`, `jvm`, `telnet` |
| `network/` | `net`, `ipv4`, `arp`, `icmp`, `tcp`, `udp`, `route`, `dhcp` |
| `mailbox/` | Implementación de mailboxes |
| `fs/` | FAT sobre SD |
| `shell/` | Comandos `xsh_*.c` y la tabla `commandtab[]` |
| `jvm/` | Simple JVM (portable) |
| `include/` | Cabeceras del kernel |
| `compile/` | Makefile, `platforms/esp32s3/xinu.conf`, `ld.script` |
| `apps/` | Aplicaciones de usuario |
| `lib/` | `printf`, cadenas, `malloc`, etc. (sin libc del sistema) |

### 0.2 Arranque

```
ROM bootloader (carga la imagen en 0x0)
   -> start.S
      -> nulluser()                    system/initialize.c
         -> platforminit()             PSRAM, relojes, UART
         -> sysinit()
            -> tablas: thrtab, semtab, montab, memlist, PSRAM
            -> mailboxInit()
            -> devtab[i].init()        <- w5500Init(), sdInit(), tftInit()...
            -> netInit()               <- pool de buffers, arp/icmp/tcpTimer
         -> enable()                   interrupciones
         -> create(main)               system/main.c
            -> shell / servicios de arranque (bootsvc_tab[])
```

Dos detalles que importan:

- `w5500Init()` corre **antes** de `netInit()`. El chip queda listo, pero la
  pila de red todavía no existe.
- `netup` es un **servicio de arranque** (`system/boot.c`) que ejecuta el
  comando `netup` del shell. Ese comando hace DHCP y llama a `netUp(ETH0, ...)`,
  que es quien crea el hilo `ETH0recv00`.

### 0.3 Compilar y flashear

```sh
cd /home/daniel/s3-xinu
find . -name '*.o' -delete
rm -rf compile/.deps
cd compile
source ../env.sh
make PLATFORM=esp32s3 -j4
```

Flashear (sin bootloader de ESP-IDF, la imagen va en `0x0`):

```sh
esptool --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
  write-flash --flash-mode dio --flash-freq 80m --flash-size 8MB \
  0x0 xinu.boot
```

---

## 1. Refresco de C con este kernel

Este kernel se compila con `-nostdinc`, `-fno-builtin`, `-fno-strict-aliasing`
y sin libc. Eso cambia varias costumbres respecto a un programa de Linux.

### 1.1 Tipos del kernel

En `include/stddef.h`:

```c
typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;
typedef char           bool;
typedef unsigned long  size_t;

typedef int syscall;    /* valor de retorno de una llamada del kernel */
typedef int devcall;    /* valor de retorno de un driver              */
typedef int shellcmd;   /* valor de retorno de un comando de shell    */
typedef int thread;     /* valor de retorno de un hilo                */
typedef void interrupt; /* manejador de interrupción                  */
typedef int message;    /* contenido de un mensaje                    */
typedef int tid_typ;    /* identificador de hilo                      */
```

Constantes universales:

```c
#define OK        1
#define SYSERR   (-1)
#define TIMEOUT  (-3)
#define NULL      0
#define TRUE      1
#define FALSE     0
```

`syscall` **no es una instrucción de trap**: es solo un `int`. `read()`,
`write()` y compañía son funciones normales que despachan por `devtab[]`.

`NULL` está definido como `0`, no como `((void *)0)`. Por eso verás
comparaciones como `NULL == pkt` con punteros y también con enteros. Funciona,
pero no es C estándar fino.

### 1.2 Punteros: el paquete de red

`struct packet` (`include/network.h`) es el ejemplo perfecto:

```c
struct packet
{
    struct netif *nif;      /* interfaz por la que llegó        */
    uint len;               /* longitud total                   */
    uchar *linkhdr;         /* cabecera Ethernet                */
    uchar *nethdr;          /* cabecera IP                      */
    uchar *curr;            /* posición actual dentro del buffer*/
    uchar pad[2];
    uchar data[1];          /* datos; el buffer real es más grande */
};
```

`data[1]` es el truco clásico de **struct con cola flexible**: el buffer se
reserva con `netGetbuf()` como `sizeof(struct packet) + NET_MAX_PKTLEN`, y se
accede a `pkt->data[...]` más allá del último campo declarado.

La pila de red mueve `curr` hacia delante y hacia atrás:

```c
pkt->curr += ((ip->ver_ihl & IPv4_IHL) << 2);  /* saltar cabecera IP */
pkt->curr -= ICMP_HEADER_LEN;                   /* hacer sitio para ICMP */
```

Aritmética de punteros, no índices. Si te equivocas en un byte, el protocolo
entero se rompe. De ahí los `IPv4_TRACE()` y las comprobaciones de longitud.

Conversión puntero ↔ entero (lo verás en las mailboxes):

```c
mailboxSend(arpqueue, (int)pkt);              /* guardar el puntero como int */
pkt = (struct packet *)mailboxReceive(arpqueue); /* recuperarlo */
```

En 32 bits cabe. Lo correcto en C sería `intptr_t`, pero el kernel usa `int`.
Cuando una función que devuelve puntero falla, devuelve `(void *)SYSERR`, y por
eso se comprueba así:

```c
pkt = netGetbuf();
if (SYSERR == (int)pkt) { ... }
```

### 1.3 Structs y tablas

El kernel es una colección de **tablas de estructuras**:

```c
struct thrent thrtab[NTHREAD];   /* hilos        */
struct sement semtab[NSEM];      /* semáforos    */
struct monent montab[NMON];      /* monitores    */
struct mbox   mboxtab[NMAILBOX]; /* mailboxes    */
```

Un `tid`, un `semaphore`, un `monitor` y una `mailbox` son **índices** a esas
tablas, no punteros. Por eso son `int`/`uint` y por eso `isbadtid()`,
`isbadsem()`, `isbadmon()` son macros de rango y estado.

### 1.4 Arrays y cadenas

No hay libc. Las funciones están en `lib/` y se declaran en `include/string.h`:

```c
memcpy(dst, src, n);
bzero(pkt, sizeof(struct packet) + NET_MAX_PKTLEN);
strlcpy(thrptr->name, name, TNMLEN);   /* copia segura, siempre acaba en \0 */
strcmp(a, b);
```

`strlcpy` es la que se usa para nombres de hilo y de dispositivo. No hay
`strcpy` "segura": usa `strlcpy` o `memcpy` con longitud explícita.

### 1.5 Funciones, `static` y prototipos

- `static` en una función o global = **solo visible en ese fichero**. Casi todos
  los helpers de los drivers son `static`.
- Todas las funciones públicas tienen prototipo en `include/*.h`. El Makefile
  compila con `-Wstrict-prototypes`, así que `f()` sin `void` avisa.
- El valor de retorno `SYSERR` es `-1`. Si tu función puede devolver `-1` como
  valor válido, tienes un problema de diseño (ver mailbox).

### 1.6 Punteros a función: `devtab` y `commandtab`

El kernel no tiene `switch` gigantes para los dispositivos. Usa una tabla de
punteros a función generada desde `xinu.conf`:

```c
const device devtab[NDEVS] = {
    { /* ETH0 */
        .name = "ETH0",
        .init = w5500Init,
        .open = w5500Open,
        .read = w5500Read,
        .write = w5500Write,
        ...
    },
};
```

`system/conf.c` es **generado**. No lo edites: edita
`compile/platforms/esp32s3/xinu.conf` y vuelve a compilar.

Lo mismo con el shell (`shell/shell.c`):

```c
const struct centry commandtab[] = {
    {"ping", FALSE, xsh_ping},
    {"ps",   FALSE, xsh_ps},
    ...
};
```

`ncommand` se calcula con `sizeof(commandtab) / sizeof(struct centry)`. Añadir
un comando es añadir una línea a esa tabla y un prototipo en `include/shell.h`.

### 1.7 Variádicas: `create()`

```c
tid_typ create(void *procaddr, uint ssize, int priority,
               const char *name, int nargs, ...);
```

Ejemplo real:

```c
tid = create(netRecv, NET_THR_STK, NET_THR_PRIO, "ETH0recv00", 1, netptr);
```

`nargs` dice cuántos argumentos siguen. `setupStack()` (específico de la
arquitectura) los copia a la pila del nuevo hilo. En 32 bits cada argumento
ocupa una palabra, así que un puntero también se pasa por aquí.

El hilo debe declararse con la firma correcta:

```c
thread netRecv(struct netif *netptr) { ... }
```

Si te equivocas de firma, el compilador no te avisa (es varargs) y el hilo lee
basura. Es un error clásico.

### 1.8 Macros

```c
#define isbadsem(s) ((s >= NSEM) || (SFREE == semtab[s].state))
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define REG32(addr)  (*(volatile uint32_t *)(addr))
```

Cuidado con los efectos secundarios: `isbadsem(i++)` evalúa `i` dos veces. Usa
una variable.

`min`/`max` en `kernel.h` usan `typeof` y un bloque de declaración (extensión de
GCC). Funcionan, pero no son C99 puro.

### 1.9 `volatile` y registros de hardware

Los registros del ESP32-S3 se tocan así (`include/xtensa.h`):

```c
#define REG32(addr) (*(volatile uint32_t *)(addr))

REG32(SYSTEM_PERIP_CLK_EN0) |= SYS_CLK_EN_SPI2;
```

`volatile` impide que el compilador elimine o reordene el acceso. **Sin
`volatile`, un bucle de espera sobre un registro se optimiza a infinito o a
nada.** Todos los accesos a MMIO del port lo llevan.

### 1.10 `disable()` / `restore()`

```c
irqmask im;

im = disable();        /* guarda el estado y enmascara interrupciones */
...sección crítica corta...
restore(im);           /* restaura exactamente el estado anterior     */
```

No es un contador: cada `disable()` devuelve la máscara anterior y `restore()`
la repone. Se pueden anidar si guardas cada máscara en su variable. **No** hagas
`enable()` a mano dentro de una sección crítica.

Uso típico: proteger una estructura de datos muy corta o comprobar+actuar de
forma atómica. Para esperar, no uses `disable()`: usa un semáforo.

### 1.11 Memoria

- `memget(n)` / `memfree(p, n)`: heap del kernel en SRAM interna.
- `psram_memget` / `psram_memfree`: heap en PSRAM.
- `stkget(n)` / `stkfree`: pilas de hilo. En este port **prefiere PSRAM** y cae
  a SRAM si no hay.
- `create()` usa `stkget()`, así que las pilas de los hilos viven en PSRAM.
- `mailboxAlloc()` usa `memget()`: la cola de mensajes está en SRAM interna.

Consecuencia práctica: una mailbox de 1000 enteros son 4 KB del heap interno.
No es gratis.

### 1.12 Errores de C que más duelen aquí

1. **`SYSERR` vs valor válido.** `-1` es error. Si tu dato puede ser `-1`, no
   uses `-1` como “fin de cola” sin documentarlo (ver mailbox).
2. **`sizeof` de un puntero.** `sizeof(pkt->data)` es 1 (es `uchar data[1]`),
   no el tamaño del buffer.
3. **Signed/unsigned.** `monitor`, `semaphore` y `mailbox` son `unsigned int`.
   Comparar con `SYSERR` (`-1`) funciona por conversión, pero no es elegante.
4. **Pasarse de longitud.** El bug de `wget` con el header de 512 bytes se
   arregló subiendo `hdr[512]` a `hdr[1024]`; un byte de más o de menos y el
   fichero sale corrupto.
5. **No inicializar.** Las variables globales van a `.bss` (cero), pero las
   locales de pila no. `bzero()` es tu amigo.
6. **Aliasing.** El Makefile usa `-fno-strict-aliasing` justamente para que
   estos casts funcionen como la gente espera.
7. **Pila pequeña.** `INITSTK` son 4096 bytes. Un `char buf[8192]` local revienta
   el hilo. Usa `memget`/PSRAM para buffers grandes.

---

## 2. Semáforos

### 2.1 Qué hay dentro

`include/semaphore.h`:

```c
struct sement
{
    char    state;   /* SFREE o SUSED          */
    int     count;   /* contador (puede ser < 0) */
    qid_typ queue;   /* cola de hilos dormidos  */
};
```

Un semáforo es **un contador y una cola de espera**. Nada más. No lleva datos y
no sabe quién lo tiene.

### 2.2 API

```c
semaphore semcreate(int count);   /* count >= 0; SYSERR si no hay slots */
syscall   wait(semaphore sem);    /* --count; si < 0, duerme            */
syscall   signal(semaphore sem);  /* ++count; si había < 0, despierta   */
syscall   signaln(semaphore sem, int n);
syscall   semcount(semaphore sem);
syscall   semfree(semaphore sem);
```

`semfree()` despierta a todos los que esperan y marca el slot como libre.

### 2.3 Los tres usos

**a) Llave (mutex binario)** — `semcreate(1)`

```c
semaphore llave = semcreate(1);

wait(llave);
/* sección crítica */
signal(llave);
```

**b) Evento / aviso** — `semcreate(0)`

```c
semaphore listo = semcreate(0);

/* hilo A: produce */
...trabajo...
signal(listo);

/* hilo B: consume */
wait(listo);      /* duerme hasta que A avise */
...usar el resultado...
```

**c) Contador de recursos** — `semcreate(N)`

```c
semaphore huecos = semcreate(4);   /* 4 recursos libres */

wait(huecos);    /* coge uno; si no hay, duerme */
...usar...
signal(huecos);  /* devuelve uno */
```

El pool de buffers de red es exactamente esto (`system/bfpalloc.c`):

```c
bfpptr->freebuf = semcreate(0);
...
signaln(bfpptr->freebuf, nbuf);    /* N buffers disponibles */
```

`netGetbuf()` hace `wait(bfpptr->freebuf)` y `netFreebuf()` hace
`signaln(..., 1)`. Si no hay buffers, el hilo de recepción duerme en vez de
girar en vacío.

### 2.4 Traza de contadores

`semcreate(1)`:

| Operación | `count` antes | `count` después | Efecto |
|---|---:|---:|---|
| `wait()` | 1 | 0 | entra |
| `wait()` | 0 | -1 | **duerme** |
| `signal()` | -1 | 0 | despierta a uno |
| `signal()` | 0 | 1 | queda un permiso |

`count` negativo = **número de hilos dormidos**. Por eso `semcount()` puede
devolver un negativo; no es un error.

`semcreate(0)`:

| Operación | antes | después | Efecto |
|---|---:|---:|---|
| `wait()` | 0 | -1 | duerme |
| `signal()` | -1 | 0 | despierta |

El hilo que despierta **no vuelve a decrementar**: su `wait()` ya lo hizo antes
de dormir. Por eso el contador queda en 0, no en -1.

### 2.5 El error clásico con semáforos

Un semáforo **no tiene dueño**. Cualquiera puede hacer `signal()`, y si lo hace
cuando no hay nadie esperando, el contador sube:

```c
semaphore s = semcreate(1);

/* hilo A */
wait(s);        /* count 0 */
...
signal(s);      /* count 1 */

/* hilo B, por error, también hace signal() */
signal(s);      /* count 2  <-- roto */

/* ahora dos hilos pueden entrar a la vez */
```

Ese es el motivo por el que existe el monitor.

### 2.6 Trampas

- **No duermas dentro de una sección crítica** protegida con un semáforo: si el
  hilo duerme, nadie más entra.
- **No liberes un semáforo en uso.** `semfree()` despierta a los que esperan y
  ellos continúan como si nada; si además hay un `signal()` pendiente, el
  resultado es impredecible.
- **No uses `wait()` en un ISR.** Un manejador de interrupción no puede dormir.
  `signal()` desde un ISR es tentador, pero en este port `ready(...,
  RESCHED_YES)` puede intentar un cambio de contexto dentro de la interrupción,
  y eso es justo el punto frágil de `ctxsw.S`. Si necesitas avisar desde un ISR,
  deja una bandera y despierta a un hilo fuera.
- **No hay herencia de prioridad.** Si un hilo de prioridad baja tiene la llave
  y uno de prioridad alta la espera, los hilos de prioridad media pueden
  adelantar al dueño. Mantén las secciones críticas cortas; si el caso es grave,
  sube temporalmente la prioridad del dueño con `chprio()`.

---

## 3. Monitores

### 3.1 Qué añade sobre un semáforo

`include/monitor.h`:

```c
struct monent
{
    char     state;   /* MFREE o MUSED     */
    tid_typ  owner;   /* hilo dueño, o NOOWNER */
    uint     count;   /* profundidad de lock   */
    semaphore sem;    /* semáforo interno (1)  */
};
```

Un monitor es **un semáforo de 1 + dueño + contador de recursión**. Su objetivo
es proteger una **región** (una secuencia de operaciones), no solo una variable.

### 3.2 Cómo funciona

`lock()` (`system/lock.c`), en esencia:

```c
if (NOOWNER == monptr->owner)
{
    monptr->owner = thrcurrent;
    monptr->count = 1;
    wait(monptr->sem);          /* consume el 1 inicial */
}
else if (thrcurrent == monptr->owner)
{
    monptr->count++;            /* recursivo: no espera */
}
else
{
    wait(monptr->sem);          /* otro lo tiene: duerme */
    monptr->owner = thrcurrent;
    monptr->count = 1;
}
```

`unlock()`:

```c
monptr->count--;
if (0 == monptr->count)
{
    monptr->owner = NOOWNER;
    signal(monptr->sem);        /* despierta al siguiente */
}
```

Traza:

| Operación | `owner` | `count` | Efecto |
|---|---|---:|---|
| `lock()` | yo | 1 | entra |
| `lock()` otra vez | yo | 2 | **no** se bloquea |
| `unlock()` | yo | 1 | no libera |
| `unlock()` | NOOWNER | 0 | despierta a otro |

`moncount(m)` devuelve la profundidad actual (0 si está libre). Si un hilo muere
con el monitor cogido, `moncount()` te dice cuántos `unlock()` hacen falta para
recuperarlo.

### 3.3 Semáforo(1) vs monitor: las diferencias que importan

| Pregunta | Semáforo(1) | Monitor |
|---|---|---|
| ¿Sabe quién lo tiene? | No | Sí (`owner`) |
| ¿Se puede anidar? | No: el segundo `wait` se bloquea | Sí: `count++` |
| ¿Se puede sobre-liberar? | Sí: dos `signal` dejan `count=2` | No: `unlock` con `count=0` da `SYSERR` |
| ¿Se puede recuperar si muere el dueño? | No hay forma de saberlo | `moncount()` dice cuántos `unlock` faltan |
| ¿Se ve en `sem`/`mon`? | `sem`: contador y si hay waiters | `mon`: dueño y profundidad |
| Coste | mínimo | 1 semáforo + entrada |

**Cuidado:** en esta implementación, `unlock()` **no comprueba** que el llamante
sea el dueño. Un hilo distinto puede decrementar el contador del dueño. La
protección es “por convención”: solo el dueño hace `unlock`. No escribas código
que dependa de que el kernel te lo impida.

**Lo que un monitor NO es:** no es una variable de condición. No hay
`monwait()`/`monsignal()`. Si necesitas “esperar a que se cumpla X”, tienes dos
opciones:

```c
/* Opción 1: soltar el monitor y esperar un semáforo */
unlock(m);
wait(evento);
lock(m);

/* Opción 2: bucle con sleep (menos elegante, pero simple) */
while (!condicion)
{
    unlock(m);
    sleep(1);
    lock(m);
}
```

### 3.4 Ejemplo real: el bus SPI

El W5500 es **un** dispositivo en **un** bus. Una transacción no es un solo
registro: es “leo el tamaño, leo la cabecera, leo los datos”. Si otro hilo se
cuela en medio, el chip recibe una mezcla y devuelve basura.

`device/w5500/w5500Read.c`:

```c
for (tries = 0; tries < W5500_RX_TRIES; tries++)
{
    lock(ethptr->lock);

    rxsize = w5500Read16(W5500_SN_RX_RSR, ...);
    if (0 == rxsize)
    {
        unlock(ethptr->lock);   /* esperar FUERA del monitor */
        sleep(1);
        continue;
    }

    rd = w5500Read16(W5500_SN_RX_RD, ...);
    w5500ReadBuf(rd, ..., hdr, 2);
    ...
    unlock(ethptr->lock);
    return len;
}
```

Dos cosas que aprender de aquí:

1. El monitor protege **la secuencia completa**, no cada acceso suelto.
2. La espera (`sleep`) va **fuera** del monitor. Si durmieras con la llave
   cogida, el `httpd` no podría transmitir y la red se pararía.

La FAT tiene su propio monitor (`fs/Src/fatPort.c`, `s_fat_mon`) por la misma
razón: las operaciones de fichero no son reentrantes.

### 3.5 Reglas de oro con monitores

1. Un `lock` por cada `unlock`, en el mismo nivel.
2. No duermas, no hagas E/S larga, no llames a `mailboxReceive` con el monitor
   cogido.
3. Si necesitas dos monitores, define un **orden global** y respétalo siempre.
   `lock(A); lock(B);` está bien; en otro sitio `lock(B); lock(A);` es un
   interbloqueo.
4. No liberes un monitor en uso. `monfree()` con dueño o con waiters deja el
   estado inconsistente. Libéralo al apagar, no en caliente.
5. Si el recurso es un contador o un aviso, no hace falta monitor: un semáforo
   es más barato y más claro.

---

## 4. Mailboxes

### 4.1 Qué hay dentro

`include/mailbox.h`:

```c
struct mbox
{
    semaphore sender;    /* huecos libres  (empieza en N) */
    semaphore receiver;  /* mensajes listos (empieza en 0) */
    uint max;            /* capacidad                      */
    uint count;          /* mensajes dentro                */
    uint start;          /* índice del primero             */
    uchar state;         /* MAILBOX_FREE / MAILBOX_ALLOC   */
    int *msgs;           /* array circular en el heap      */
};
```

Es un **array circular + dos semáforos**. No hay magia.

`mailboxSend`:

```c
wait(mbxptr->sender);                        /* si está llena, duerme */
mbxptr->msgs[(start + count) % max] = msg;
mbxptr->count++;
signal(mbxptr->receiver);                    /* avisa al consumidor */
```

`mailboxReceive`:

```c
wait(mbxptr->receiver);                      /* si está vacía, duerme */
msg = mbxptr->msgs[start];
start = (start + 1) % max;
count--;
signal(mbxptr->sender);                      /* libera un hueco */
```

### 4.2 API

```c
mailbox mailboxAlloc(uint count);     /* count = nº de mensajes */
syscall mailboxSend(mailbox box, int msg);
syscall mailboxReceive(mailbox box);  /* devuelve el mensaje o SYSERR */
syscall mailboxCount(mailbox box);
syscall mailboxFree(mailbox box);
```

- `mailboxAlloc` reserva `count * sizeof(int)` con `memget` (heap interno) y
  crea los dos semáforos.
- `mailboxSend` se bloquea si está llena (**backpressure**).
- `mailboxReceive` se bloquea si está vacía.
- `mailboxFree` libera los semáforos y la memoria. No la uses mientras haya
  hilos esperando.

### 4.3 Ejemplo real: ARP e ICMP

`network/arp/arpInit.c`:

```c
arpqueue = mailboxAlloc(ARP_NQUEUE);
ready(create((void *)arpDaemon, ARP_THR_STK, ARP_THR_PRIO,
             "arpDaemon", 0), RESCHED_NO);
```

`network/arp/arpRecv.c` (corre en el hilo de recepción):

```c
mailboxSend(arpqueue, (int)pkt);   /* deja el paquete y vuelve */
```

`network/arp/arpDaemon.c`:

```c
pkt = (struct packet *)mailboxReceive(arpqueue);
arpSendReply(pkt);
netFreebuf(pkt);
```

Y exactamente igual con ICMP (`network/icmp/icmpInit.c`, `icmpRecv.c`,
`icmpDaemon.c`).

**Por qué está así:** el hilo de recepción no puede entretenerse respondiendo.
Si lo hiciera, el buffer RX del W5500 se llenaría y la red se pararía. La
mailbox convierte “responder” en un trabajo de otro hilo.

### 4.4 “¿Cómo sabe un proceso que el mensaje es para él?”

**No lo sabe, y no hace falta.** Una mailbox **no tiene destinatario**. Es una
cola con un número; el que tiene el número puede meter o sacar.

El patrón es **una mailbox por servicio**, no una por proceso:

- `arpqueue` es *la cola del servicio ARP*.
- `icmpqueue` es *la cola del servicio ICMP*.
- El dueño del servicio es el único que hace `mailboxReceive`.
- Los productores solo conocen el número y hacen `mailboxSend`.

Si varios hilos hacen `receive` de la misma mailbox, cada mensaje va a **uno**
de ellos (se reparten el trabajo), no a todos. Para difusión necesitas varias
mailboxes o copias.

Si de verdad necesitas punto a punto, no uses una mailbox: usa
`send(tid, msg)` (siguiente sección) o dale a cada proceso su propia mailbox y
publica el número.

### 4.5 Trampas

- **`-1` es ambiguo.** `mailboxReceive` devuelve `SYSERR` (`-1`) en error, pero
  `-1` también puede ser un mensaje válido. Usa IDs ≥ 0 y un valor de fin
  distinto, o un flag aparte.
- **Es una cola acotada.** Si el consumidor es más lento que los productores,
  los productores se bloquean. Eso es una feature (control de flujo), pero
  asegúrate de que el consumidor no dependa de uno de esos productores para
  avanzar, o tendrás un interbloqueo.
- **El mensaje es un `int`.** Si mandas un puntero, define **quién libera** la
  memoria. En ARP, el demonio hace `netFreebuf()`; el productor no vuelve a
  tocarlo.
- **No liberes la mailbox con waiters.** `mailboxFree` hace `semfree` de los dos
  semáforos; los que estaban esperando se despiertan y ven `state != ALLOC`,
  pero es mejor no llegar ahí.
- **La cola está en SRAM interna.** `mailboxAlloc(1000)` son 4 KB del heap del
  kernel. Para colas grandes, usa otra estructura en PSRAM.

---

## 5. Mensajes de hilo (`send`/`receive`)

### 5.1 Qué es

Cada entrada de `thrtab[]` tiene **un solo slot**:

```c
message msg;      /* el mensaje   */
bool    hasmsg;   /* ¿hay mensaje? */
```

```c
syscall send(tid_typ tid, message msg);
message receive(void);
message recvclr(void);
message recvtime(int ticks);
```

- `send(tid, msg)` **sí tiene destinatario**: deposita el mensaje en el slot de
  ese `tid`. Si el hilo está en `THRRECV`, lo despierta.
- `send` **falla** si `tid` no existe, está libre, o **ya tiene un mensaje**
  (`hasmsg`). Es un slot, no una cola.
- `receive()` se bloquea si no hay mensaje.
- `recvtime(ticks)` se bloquea como máximo `ticks` (con
  `CLKTICKS_PER_SEC = 1000`, 1000 = 1 segundo) y devuelve `TIMEOUT`.
- `recvclr()` limpia el mensaje pendiente.

### 5.2 Ejemplo: ping

`icmpRecv` (camino de respuesta):

```c
id = net2hs(echo->id);        /* el id que puso el ping = su tid */
...
send(id, (message)pkt);       /* despierta al hilo que pingó    */
```

`xsh_ping`:

```c
if (OK != icmpEchoRequest(&target, gettid(), i)) { ... }
...
if (TIMEOUT != recvtime(intervalticks))
{
    /* recoger la respuesta */
}
```

El ping **mete su propio `tid` en el campo `id` del ICMP**. Cuando llega el
reply, `icmpRecv` lo saca y hace `send()` a ese `tid`. Esa es toda la
“dirección”: viaja dentro del protocolo.

Detalle fino: como `send` es un solo slot, `icmpRecv` no lo usa como cola.
Guarda el paquete en `echotab[]` (una cola por `tid`) y usa `send` solo como
**despertador**. Si el hilo ya tenía un mensaje, el paquete se queda en la cola
y se procesa igualmente.

### 5.3 Cuándo usarlo

- Cuando la respuesta debe ir a **un hilo concreto** y conoces su `tid`.
- Cuando el emisor no puede permitirse bloquearse si el receptor ya tiene un
  mensaje pendiente (ping solo tiene uno en vuelo).
- **No** cuando necesitas una cola: usa mailbox.

---

## 6. Cómo elegir (y no liarse)

### 6.1 Tabla

| Necesitas… | Usa… | Porque… |
|---|---|---|
| Que dos hilos no toquen lo mismo a la vez | **Monitor** | Dueño, recursivo, se ve en `mon` |
| “Espera a que ocurra X” | **Semáforo(0)** | El evento queda como permiso |
| Contar recursos (buffers, plazas) | **Semáforo(N)** | `wait`/`signal` es el contador |
| Pasar trabajos a un servicio | **Mailbox** | Cola acotada + backpressure |
| Responder a un hilo concreto | **`send(tid)`** | Direccionado por `tid` |
| Proteger una secuencia con E/S | **Monitor** | La sección crítica es la secuencia |

### 6.2 Árbol de decisión

```
¿Hay que mover datos de un hilo a otro?
├── NO  -> ¿Hace falta exclusión mutua sobre un recurso?
│         ├── SÍ -> monitor (o semáforo(1) si eres disciplinado)
│         └── NO -> ¿Solo avisar o contar?
│                   └── semáforo(0) / semáforo(N)
└── SÍ  -> ¿El receptor es un servicio (uno cualquiera) o un hilo concreto?
          ├── servicio -> mailbox
          └── concreto -> send(tid)
```

### 6.3 Semáforo vs monitor, en una frase

**El semáforo es un contador; el monitor es un contador con dueño y recursión.**
Si solo necesitas “esperar/avisar”, semáforo. Si necesitas “esta secuencia no se
mezcla con otra”, monitor.

Y el detalle que te confundía: **un semáforo de 1 y un monitor parecen lo mismo
cuando todo va bien**. La diferencia aparece cuando algo va mal: un `signal` de
más, un `unlock` anidado, un hilo que muere con la llave. El monitor está
pensado para eso; el semáforo no.

### 6.4 Patrones útiles

**Productor/consumidor con mailbox**

```c
#define N 8
mailbox cola = mailboxAlloc(N);

/* productor */
mailboxSend(cola, trabajo);

/* consumidor */
trabajo = mailboxReceive(cola);
```

**Señal de terminación**

```c
/* productor */
mailboxSend(cola, -1);   /* fin */

/* consumidor */
while ((msg = mailboxReceive(cola)) != -1) { ... }
```

**Esperar a que N hilos terminen**

```c
semaphore hechos = semcreate(0);

/* cada worker, al terminar */
signal(hechos);

/* el padre */
for (i = 0; i < N; i++) wait(hechos);
```

**Proteger un contador y un índice juntos**

```c
lock(m);
buffer[idx] = dato;
idx++;
unlock(m);
```

Los dos campos son una sola invariante; si los tocas por separado, el lector
puede ver `idx` actualizado y `dato` viejo.

---

## 7. Estudio de caso: un ping completo

### 7.1 Capas

```
 ping / httpd / telnet / wget          <- hilos de usuario
        |         |
      ICMP      TCP / UDP
        |         |
        +--> IPv4 <+
              |
   ARP -----> netSend / netRecv        <- struct netif (IP, mask, MAC, MTU, dev)
              |
        read(ETH0) / write(ETH0)       <- devtab[ETH0]
              |
        w5500Read / w5500Write         <- lock(ethptr->lock)  [monitor]
              |
        registros SPI2 (w5500Spi.c)    <- sin locks, hardware
              |
        W5500 en MACRAW                <- solo tramas, no entiende IP
              |
            cable
```

El W5500 **no hace ARP, ni IP, ni TCP**. Está en MACRAW: entrega y recibe
tramas Ethernet. Todo lo demás son hilos de Xinu.

### 7.2 El host hace ping a la placa

1. La trama llega al W5500 y queda en su buffer RX.
2. `ETH0recv00` está dormido en `read(ETH0)` → `w5500Read`, que hace polling de
   `Sn_RX_RSR` cada 10 ms. Al haber trama: `lock(ethptr->lock)`, lee cabecera +
   datos por SPI, `unlock`.
3. `netRecv` tiene el paquete en un buffer del pool (`netGetbuf`). Ethertype
   IPv4 → `ipv4Recv`.
4. `ipv4Recv` valida, comprueba que la IP es nuestra, **aprende la MAC del
   emisor** (`arpLearn`) y despacha: ICMP → `icmpRecv`.
5. `icmpRecv` ve `ICMP_ECHO` y **no responde**. Hace:
   ```c
   mailboxSend(icmpqueue, (int)pkt);
   return OK;
   ```
6. `icmpDaemon` estaba dormido en `mailboxReceive(icmpqueue)`. Se despierta,
   hace `icmpEchoReply(pkt)` → `icmpSend` → `ipv4Send` → `netSend`.
7. `netSend` pone la cabecera Ethernet, busca la MAC (ya aprendida, no bloquea)
   y hace `write(ETH0, ...)` → `w5500Write` → SPI → `SEND`.
8. El host recibe el reply.

Si el hilo de recepción respondiera él mismo, el buffer RX se llenaría mientras
construye la respuesta. Por eso hay mailbox y demonio.

### 7.3 La placa hace ping al host

1. `xsh_ping` llama a `icmpEchoRequest(&target, gettid(), seq)`. **Su `tid` va
   en el campo `id` del ICMP.**
2. `icmpSend` → `ipv4Send` → `netSend` → `arpLookup`.
3. Si no conoce la MAC: crea una entrada `ARP_UNRESOLVED`, apunta su `tid` en
   `entry->waiting[]`, manda el ARP request y se duerme en `recvtime()`.
4. Llega el ARP reply → `ETH0recv00` → `netRecv` → `arpRecv`. Actualiza la tabla
   y llama a `arpNotify`, que hace `send(tid, ...)` a cada hilo de `waiting[]`.
5. El ping se despierta, ya tiene la MAC y manda el ICMP.
6. Se duerme en `recvtime(intervalticks)` esperando el echo reply.
7. Llega el reply → `icmpRecv` ve `ICMP_ECHOREPLY`, saca el `id` (= `tid`), guarda
   el paquete en `echotab` y hace `send(id, pkt)`. El ping se despierta y
   calcula el RTT.

### 7.4 Quién usa qué

| Mecanismo | Dónde | Para qué |
|---|---|---|
| Semáforo | `bfpalloc`/`bufget` | Contar buffers libres |
| Semáforo | dentro de mailbox y monitor | Implementarlos |
| Monitor | `ethptr->lock` | Que el SPI no se mezcle |
| Monitor | `s_fat_mon` | Que la FAT no se mezcle |
| Mailbox | `arpqueue` | Trabajos ARP sin bloquear recepción |
| Mailbox | `icmpqueue` | Trabajos ICMP sin bloquear recepción |
| `send`/`receive` | reply de ping | Respuesta a un `tid` concreto |
| `send`/`receive` | `arpNotify` | Avisar a quien espera resolución |

**La misma pila usa las dos cosas en el mismo flujo.** Cuando es trabajo de un
servicio, mailbox; cuando es respuesta a alguien, `send(tid)`. Ahí está la
respuesta a tu duda.

---

## 8. Cómo modificar el kernel

### 8.1 Añadir un fichero `.c`

Cada directorio tiene un `Makerules` con la lista `C_FILES`. Por ejemplo,
`system/Makerules`:

```make
C_FILES += misemaforo.c
```

Si creas un **directorio nuevo** (por ejemplo `midrv/`), tienes que añadirlo a
`APPCOMPS` (código) o `DEVICES` (driver) en
`compile/platforms/esp32s3/platformVars`:

```make
APPCOMPS := shell apps mailbox network fs midrv
```

y el directorio debe tener su `Makerules` con `COMP` y `C_FILES`.

### 8.2 Añadir un comando al shell

Cuatro pasos:

1. Crea `shell/xsh_midemo.c`:

```c
#include <kernel.h>
#include <shell.h>
#include <stdio.h>
#include <thread.h>

shellcmd xsh_midemo(int nargs, char *args[])
{
    (void)nargs;
    (void)args;

    printf("hola desde midemo\n");
    return SHELL_OK;
}
```

2. Añade el prototipo en `include/shell.h`:

```c
shellcmd xsh_midemo(int, char *[]);
```

3. Añade la entrada en `shell/shell.c`:

```c
{"midemo", FALSE, xsh_midemo},
```

4. Añade el fichero a `shell/Makerules`:

```make
C_FILES += xsh_midemo.c
```

Vuelve a compilar. `ncommand` se recalcula solo porque es
`sizeof(commandtab) / sizeof(struct centry)`.

### 8.3 Añadir un dispositivo

1. Escribe el driver con las funciones que uses: `fooInit`, `fooOpen`,
   `fooClose`, `fooRead`, `fooWrite`, `fooGetc`, `fooPutc`, `fooControl`.
2. Declara el tipo y la instancia en `compile/platforms/esp32s3/xinu.conf`:

```
foo:
    on HARDWARE -i fooInit  -o fooOpen  -c fooClose
                -r fooRead  -g fooGetc  -p fooPutc
                -w fooWrite -n fooControl

FOO0 is foo on HARDWARE
```

3. Añade el directorio a `DEVICES` en `platformVars`.
4. No edites `system/conf.c`: es generado.

### 8.4 Añadir un hilo al arranque

**Opción A: servicio de shell** (lo más simple). En `system/boot.c`:

```c
static const struct bootsvc bootsvc_tab[] = {
    {"netup", TRUE, {NULL}},
    {"telnetserver", FALSE, {"-n", "1", NULL}},
    {"httpd", FALSE, {"80", NULL}},
    {"midemo", FALSE, {NULL}},
    {NULL, FALSE, {NULL}}
};
```

El segundo campo es `wait`: si es `TRUE`, el comando se ejecuta en el hilo de
arranque y hay que esperar a que termine antes de lanzar el siguiente (así se
hace `netup`); si es `FALSE`, se lanza en su propio hilo. Los argumentos son
los mismos que escribirías en el shell. El servicio se ejecuta como si lo
escribieras en el shell.

**Opción B: hilo propio** en una función de inicialización:

```c
tid = create((void *)miHilo, INITSTK, INITPRIO, "miHilo", 0);
if (SYSERR != tid)
{
    ready(tid, RESCHED_NO);
}
```

Si el hilo es un servicio permanente, ponle una prioridad adecuada y un bucle
`while (TRUE)` con una espera bloqueante (mailbox, semáforo o `sleep`). **Nunca**
un bucle que gire sin bloquear: se come la CPU y el shell no responde.

### 8.5 Cambiar la configuración

Edita `compile/platforms/esp32s3/xinu.conf`:

```
#define NSEM      128
#define NMON      16
#define NMAILBOX  10
#define NTHREAD   50
#define NTCP      10
```

`include/conf.h` y `system/conf.c` se generan desde ahí. No los edites a mano.

### 8.6 Depurar

| Comando | Qué enseña |
|---|---|
| `ps` | Hilos, prioridad, estado |
| `sem` | Semáforos usados, contador, si hay waiters |
| `mon` | Monitores usados, dueño, profundidad |
| `free` | Memoria libre (heap y PSRAM) |
| `memstat` | Estado del heap |
| `netstat` | Interfaces y contadores |
| `tcpstat` | Conexiones TCP |
| `arp` | Tabla ARP |
| `route` | Rutas |
| `udpstat` | Sockets UDP |

Para trazas temporales: `printf()` en contexto de tarea, `kprintf()` solo en
arranque/inicialización (el shell y los hilos usan `printf`).

### 8.7 Errores típicos de compilación

| Error | Causa |
|---|---|
| `undefined reference to foo` | Falta `foo.c` en el `Makerules` |
| `implicit declaration of function` | Falta el prototipo en `include/` |
| `conflicting types for 'x'` | Firma distinta entre `.h` y `.c` |
| `section .iram0.text will not fit` | Demasiado código en IRAM; mira `ld.script` |
| `region dram overflowed` | Variables globales de más; usa PSRAM o reduce buffers |

---

## 9. Ejercicios guiados

### Ejercicio 1 — `semdemo`: semáforo como llave

**Objetivo:** ver la diferencia entre incrementar un contador con y sin
semáforo.

Crea un comando `semdemo` que:

1. Cree un semáforo con `semcreate(1)`.
2. Lance dos hilos que hagan 100 000 veces `contador++` dentro de
   `wait`/`signal`.
3. Espere a que terminen (con un semáforo de terminación) y imprima el
   resultado.
4. Repite la prueba **sin** semáforo.

**Preguntas:** ¿sale siempre 200 000? ¿Qué pasa si cambias `semcreate(1)` por
`semcreate(0)`? ¿Y si añades un `signal()` de más?

**Para observar:** `sem` mientras corre.

### Ejercicio 2 — `mondemo`: monitor sobre la consola

**Objetivo:** ver una sección crítica que abarca varios `printf`.

Crea `mondemo` con dos hilos que impriman informes de tres líneas. Hazlo primero
sin monitor y luego con monitor. La salida sin monitor debe mezclarse; con
monitor debe salir cada informe completo.

**Preguntas:** ¿por qué no basta con proteger cada `printf`? ¿Qué pasa si un
hilo hace `sleep()` dentro del monitor? (Pruébalo y mira `ps`.)

### Ejercicio 3 — `mbdemo`: productor/consumidor

**Objetivo:** entender el bloqueo y el control de flujo.

Crea `mbdemo` con:

- una mailbox de 4 mensajes;
- dos productores que envían 10 trabajos cada uno (`(id << 8) | i`);
- un consumidor que tarda 50 ms por trabajo;
- un mensaje `-1` por cada productor para terminar.

**Preguntas:** ¿cuándo se bloquean los productores? ¿Qué muestra `sem`? ¿Qué pasa
si el consumidor no llama a `mailboxReceive`? ¿Y si pones capacidad 1?

### Ejercicio 4 — `pingtrace`: instrumentar la pila

**Objetivo:** seguir un ping por dentro.

Añade contadores globales en `icmpRecv` (requests y replies) y en `arpRecv`
(requests y replies), y un comando `pingtrace` que los imprima. Lanza un ping
desde el host y otro desde la placa.

**Preguntas:** ¿qué contador sube en cada caso? ¿Por qué el reply del host no
pasa por `arpRecv` si la MAC ya se aprendió? ¿Qué pasa si borras la entrada ARP
y vuelves a hacer ping?

### Ejercicio 5 — HTTP con acceptor y mailbox

**Objetivo:** aplicar el patrón de la parte 6 al pendiente real.

Diseña (sin implementar todavía) un `httpd` con:

- un hilo **aceptador** que hace `open()` pasivo y `mailboxSend(cola, tcpdev)`;
- un hilo **worker** que hace `mailboxReceive(cola)` y sirve la petición;
- una cola de 4.

**Criterios de aceptación:**

1. 100 peticiones secuenciales siguen funcionando.
2. Dos peticiones simultáneas no reciben RST: la segunda espera en la cola.
3. `free` no muestra hilos ni semáforos filtrados al final.
4. El listener no desaparece mientras se sirve.

**Pista:** el worker es el único que toca la FAT, así que no necesitas otro
monitor para la SD. Si la cola se llena, el aceptador se bloquea en
`mailboxSend` (eso es el control de flujo).

---

## 10. Caso real: terminal web

El dashboard tiene una pestaña **Terminal** que ejecuta comandos del shell
desde el navegador. No es un `telnet` con otro color: el `httpd` de Xinu
atiende una conexión a la vez y no puede quedarse dentro de un comando largo.
Este caso reúne casi todo lo anterior: un hilo trabajador, un monitor, un
semáforo, un dispositivo pseudo-TTY, memoria PSRAM y un timeout en la pila
TCP.

### 10.1 El problema

Si `httpd` hiciera directamente:

```c
/* MAL: bloquea el único servidor mientras el comando corre */
system("ping -c 10 192.168.1.1");
```

mientras el ping tarda, el navegador no puede ni consultar el estado. Además,
la salida del comando iría a la consola serie, no a la página.

La solución separa tres cosas:

1. **Aceptar** el comando rápido y devolver un identificador de trabajo.
2. **Ejecutarlo** en otro hilo, con su salida capturada.
3. **Consultar** el estado desde el navegador con peticiones cortas.

### 10.2 Arquitectura

```
navegador
   │  POST /api/exec  (texto del comando)
   ▼
httpd (prioridad 20)
   │  webtermSubmit(): monitor jobmon, estado RUNNING
   │  signal(jobsem)
   ▼
webshell (prioridad 18)  ── wait(jobsem) ──► webtermRun()
   │                                         │
   │  fdesc[1] = fdesc[2] = MEMTTY0          │ builtin: ejecuta en el worker
   │                                         │ no-builtin: create() + receive()
   ▼                                         ▼
memtty (PSRAM, 8 KB)  ◄── printf/fprintf ── comando
   │
   │  GET /api/job?id=N
   ▼
httpd: webtermStatus() + webtermSnapshot()
   │  202 mientras corre, 200 text/plain al terminar
   ▼
navegador: pinta la salida y vuelve a "listo"
```

Ficheros reales:

- `shell/webterm.c` — cola de un trabajo, monitor y worker.
- `include/webterm.h` — estados y API interna.
- `device/memtty/memtty.c` — dispositivo de captura.
- `shell/httpapi.c` — `POST /api/exec` y `GET /api/job`.
- `shell/xsh_httpd.c` — configura el timeout de recepción.
- `device/tcp/tcpRead.c` — lectura con timeout opcional.
- `web/index.html` — pestaña Terminal y polling.

### 10.2.1 Endpoints y tareas

No hay un servidor aparte para la API. `httpdServer()` acepta la conexión y
`httpApiHandle()` despacha por ruta y método:

| Método y ruta | Función | Fichero:línea |
|---|---|---|
| `GET /` y ficheros de la SD | `serve_static()` | `shell/xsh_httpd.c:220` |
| `GET /api/files` | `api_files()` | `shell/httpapi.c:263` |
| `GET /api/download` | `api_download()` | `shell/httpapi.c:313` |
| `PUT /api/file` | `api_upload()` | `shell/httpapi.c:352` |
| `DELETE /api/file` | `api_delete()` | `shell/httpapi.c:432` |
| `POST /api/mkdir` | `api_mkdir()` | `shell/httpapi.c:455` |
| `POST /api/rmdir` | `api_rmdir()` | `shell/httpapi.c:470` |
| `POST /api/exec` | `api_exec()` | `shell/httpapi.c:485` |
| `GET /api/job` | `api_job()` | `shell/httpapi.c:561` |
| ruta `/api/` desconocida | `httpApiHandle()` | `shell/httpapi.c:593` |

La cadena de tareas es:

| Tarea | La crea | Pila | Prioridad |
|---|---|---|---:|
| `MAIN` | `nulluser()` (`system/initialize.c:68`) | `INITSTK` 4096 B | 20 |
| `BOOT1` / `BOOT2` | `bootServices()` (`system/boot.c:105`) | `SHELL_CMDSTK` 4096 B | 19 |
| `httpd` | `xsh_httpd()` (`shell/xsh_httpd.c:397`) | `HTTPD_STK` 8192 B | 20 |
| `webshell` | `webtermInit()` (`shell/webterm.c:94`) | `WEBTERM_STK` 16384 B | 18 |
| hijo de comando | `webtermRun()` (`shell/webterm.c:301`) | `SHELL_CMDSTK` 4096 B | 18 |
| `JVM` | `xsh_java()` (`shell/xsh_java.c:90`) | `JVM_STK` 65536 B | 20 |
| `httpdRel` | `httpdRelease()` (`shell/xsh_httpd.c:279`) | `INITSTK` 4096 B | 20 |
| `SHELL0` | `main()` (`system/main.c:85`) | `INITSTK` 4096 B | 20 |

`MEMTTY0` no es una tarea: `sysinit()` ejecuta `devtab[i].init()`
(`system/initialize.c:156`), que llama a `memttyInit()` y reserva 8 KB en
PSRAM. El despacho de la API está en `httpApiHandle()`
(`shell/httpapi.c:593-648`) y se invoca desde `httpdServer()`
(`shell/xsh_httpd.c:350-362`). Los detalles de respuesta y los límites están
en `web/README.md`.

### 10.3 Estado del trabajo: monitor + semáforo

El estado compartido es pequeño:

```c
struct webjob
{
    int id;
    volatile int state;          /* IDLE, RUNNING o DONE */
    char cmd[WEB_CMD_MAX];       /* 160 bytes */
};
```

`webtermSubmit()` hace la transición a `RUNNING` dentro del monitor y despierta
al worker:

```c
lock(jobmon);
if (WEB_STATE_RUNNING == job.state)
{
    unlock(jobmon);
    return -1;                   /* la API responde 409 busy */
}
id = ++job.id;
memcpy(job.cmd, cmd, len);
job.cmd[len] = '\0';
job.state = WEB_STATE_RUNNING;
memttyReset();
unlock(jobmon);

signal(jobsem);
return id;
```

El worker espera fuera del monitor:

```c
while (TRUE)
{
    wait(jobsem);                /* dormido hasta que hay trabajo */

    lock(jobmon);
    strlcpy(cmd, job.cmd, sizeof(cmd));
    unlock(jobmon);

    webtermRun(cmd);             /* aquí corre el comando */

    lock(jobmon);
    job.state = WEB_STATE_DONE;
    unlock(jobmon);
}
```

**Por qué los dos primitivos.** El monitor protege el estado y el comando; el
semáforo es solo la campana que despierta al worker. Se podría hacer todo con
un semáforo, pero entonces cada lectura del estado tendría que recordar qué
contadores protegen qué campos. El monitor da exclusión mutua y deja el código
legible.

**Regla práctica:** nunca hacer I/O de red mientras se tiene `jobmon`. La API
copia la salida a un buffer de instantánea en PSRAM, suelta el monitor y
escribe el socket después. Así un cliente lento no bloquea al worker.

### 10.4 Capturar la salida sin tocar la consola

Xinu guarda los descriptores por hilo:

```c
#define stdout (thrtab[thrcurrent].fdesc[1])
#define stderr (thrtab[thrcurrent].fdesc[2])
```

`webtermRun()` los cambia solo para el hilo que ejecuta el comando:

```c
saved0 = thrtab[thrcurrent].fdesc[0];
saved1 = thrtab[thrcurrent].fdesc[1];
saved2 = thrtab[thrcurrent].fdesc[2];
thrtab[thrcurrent].fdesc[0] = DEVNULL;   /* sin entrada interactiva */
thrtab[thrcurrent].fdesc[1] = MEMTTY0;
thrtab[thrcurrent].fdesc[2] = MEMTTY0;
```

Para un comando no incorporado, el shell crea un hijo. `webtermRun()` copia
los mismos descriptores al hijo **antes** de `ready()`:

```c
child = create(commandtab[i].procedure, SHELL_CMDSTK,
               WEBTERM_CMD_PRIO, commandtab[i].name, 2, ntok, tok);
thrtab[child].fdesc[0] = DEVNULL;
thrtab[child].fdesc[1] = MEMTTY0;
thrtab[child].fdesc[2] = MEMTTY0;
ready(child, RESCHED_YES);
while (receive() != child)
    ;
```

`create()` inicializa los descriptores a `CONSOLE`. Por eso un comando que
crea otro hilo debe copiarlos explícitamente. El caso concreto era `java`:
`xsh_java.c` creaba la JVM y esta escribía en la consola serie aunque el
comando se lanzara desde el terminal web. El arreglo fue copiar
`stdin`/`stdout`/`stderr` del hilo actual al hijo de la JVM antes de `ready()`.

El dispositivo `memtty` es deliberadamente simple:

```c
devcall memttyPutc(device *devptr, char ch)
{
    if (NULL == mt_buf)
        return SYSERR;
    if (mt_len < MEMTTY_CAP)
        mt_buf[mt_len++] = ch;
    else
        mt_trunc = 1;
    return OK;
}
```

- El buffer de 8 KB se reserva en PSRAM; si no hay PSRAM, cae al heap.
- `memttyRead()` devuelve 0 y `memttyGetc()` devuelve `-1`: el terminal web no
  es interactivo.
- `memttyReset()` se llama al aceptar un trabajo, antes de despertar al
  worker. La disciplina del monitor garantiza que nadie lee mientras se
  escribe y nadie escribe mientras se lee.
- `webtermSnapshot()` copia a otro buffer de PSRAM y añade
  `[salida truncada]` si se superaron los 8 KB.

El dispositivo se declara en `compile/platforms/esp32s3/xinu.conf`:

```
memtty:
    on SOFTWARE -i memttyInit -o memttyOpen -c memttyClose
                -r memttyRead -g memttyGetc -p memttyPutc
                -w memttyWrite -n memttyControl

MEMTTY0 is memtty on SOFTWARE
```

`config` genera `system/conf.c` e `include/conf.h`; `NDEVS` pasa a 26 y
`MEMTTY0` queda en el índice 25. El driver se añade a `DEVICES` en
`platformVars` y a `device/memtty/Makerules`.

### 10.5 El directorio de trabajo

`fs_curdir` es global. El terminal web guarda el directorio de la sesión web,
pone el suyo, ejecuta y al terminar guarda el nuevo y restaura el global:

```c
strlcpy(saved_cwd, fs_curdir, sizeof(saved_cwd));
strlcpy(fs_curdir, webterm_cwd, sizeof(fs_curdir));

/* ... ejecutar ... */

strlcpy(new_cwd, fs_curdir, sizeof(new_cwd));
strlcpy(fs_curdir, saved_cwd, sizeof(fs_curdir));
strlcpy(webterm_cwd, new_cwd, sizeof(webterm_cwd));
```

Así `cd /java` seguido de `ls` funciona dentro del terminal web y no cambia
el directorio de la shell serie. Durante la ejecución la variable sigue siendo
global: si otro hilo hace `cd` en ese instante, hay una carrera de ventana
corta. Una sesión realmente aislada necesitaría un `struct session` con su
propio `cwd` y pasarlo a los comandos, que hoy leen la variable global.

### 10.6 El preconnect del navegador y `TCP_CTRL_RCVTIMEO`

Al cargar la página, Chrome puede abrir una conexión especulativa al mismo
origen y no enviar nada. El `httpd` de una sola conexión la acepta y se queda
en:

```c
read(tcpdev, req + used, HTTPD_RQMAX - used);  /* bloqueado */
```

Mientras tanto, el `POST /api/exec` real recibe RST: el servidor no está
escuchando. La página mostraba `Failed to fetch`.

La pila TCP no tenía timeout de lectura. Se añadió:

- campo `int rcvtimeo` en `struct tcb` (0 = bloqueo, valor en ms);
- control `TCP_CTRL_RCVTIMEO` en `tcpControl.c`;
- en `tcpRead()`, si `rcvtimeo > 0`, se hace polling cada 10 ms:

```c
wait(tcbptr->mutex);
while (0 == tcbptr->icount)
{
    check = stateCheck(tcbptr);      /* libera el mutex si hay error */
    if (check != OK)
        return SYSERR;
    if (elapsed >= tcbptr->rcvtimeo)
    {
        signal(tcbptr->mutex);
        return TCP_ERR_READTO;
    }
    signal(tcbptr->mutex);
    sleep(10);
    elapsed += 10;
    wait(tcbptr->mutex);
}
```

`httpd` lo activa tras el `open()` pasivo:

```c
control(tcpdev, TCP_CTRL_RCVTIMEO, HTTPD_RCVTIMEO, 0);  /* 2000 ms */
```

La ruta normal (sin timeout) sigue usando el semáforo `readers`; telnet no
cambia. El polling de 10 ms es un compromiso: no hay `semwait` con timeout en
Xinu y añadirlo tocaría el núcleo de semáforos. Para un servidor de laboratorio
es suficiente y mantiene el cambio local.

La UI también reintenta los errores de red al hacer polling, porque el
`httpd` tarda un instante en volver a crear el socket de escucha después de
cerrar una conexión.

### 10.7 La API HTTP

`POST /api/exec` acepta el comando en el cuerpo (`text/plain`, máximo 159
bytes) y responde:

```json
{"ok":true,"id":7,"state":"running"}
```

Si ya hay un trabajo: `409 Conflict` con
`{"ok":false,"error":"terminal busy"}`.

`GET /api/job?id=7` usa cabeceras para el estado y el cuerpo para la salida:

```
HTTP/1.0 202 Accepted
X-Job-State: running
Content-Length: 0

HTTP/1.0 200 OK
Content-Type: text/plain; charset=utf-8
X-Job-State: done
X-Job-Truncated: 0
Content-Length: 910

TID NAME             STATE PRIO ...
```

No se envuelve la salida en JSON: así no hay que escapar 8 KB ni duplicar el
buffer. La página lee `X-Job-State`, espera 300 ms y vuelve a preguntar.

### 10.8 Límites actuales

- **Un trabajo a la vez.** No hay cola; el segundo comando recibe 409.
- **Sin cancelación.** Un `sleep 60` deja el terminal ocupado hasta que
  termine. `kill` desde la consola serie puede matar el comando, pero el
  estado del trabajo no se entera.
- **Sin salida en vivo.** La API solo entrega la salida cuando el estado es
  `DONE`. El polling muestra "ejecutando…", no las líneas parciales.
- **Sin comandos interactivos.** `telnet`, `nc` y `xmodem` necesitan una
  consola real; el `stdin` del trabajo es `DEVNULL`.
- **Sin redirecciones ni `&`.** Se rechazan antes de ejecutar.
- **`exit` rechazado.** Mataría al hilo `webshell` y el terminal dejaría de
  responder hasta reiniciar.
- **Sin autenticación.** Cualquiera en la LAN puede ejecutar `reset`, `kill`
  o borrar ficheros. Es una herramienta de laboratorio, no un servicio
  expuesto a Internet.

### 10.9 Ejercicios sobre este caso

1. **Cola de dos trabajos.** Añade un mailbox de `struct webjob` y un segundo
   buffer de salida. ¿Qué protege el monitor y qué protege el mailbox? ¿Qué
   pasa si el usuario consulta el trabajo 1 mientras corre el 2?
2. **Cancelar.** Añade `POST /api/cancel` con el id. El worker debe matar al
   hijo (o al comando incorporado) y pasar a `CANCELLED`. Piensa qué recursos
   pueden quedar abiertos: ficheros FAT, sockets, semáforos.
3. **Autenticación.** Añade una cabecera `X-Token` y compárala con un token
   guardado en la SD. ¿Dónde se valida: en `httpd`, en `httpApiHandle` o en
   cada endpoint? ¿Qué pasa con el terminal y con la subida de ficheros?
4. **Salida en vivo.** Haz que `GET /api/job` copie también mientras está
   `RUNNING`. ¿Cómo evitas leer un byte a medias mientras `memttyPutc` lo
   escribe? ¿Basta el monitor o hace falta un buffer circular con índice de
   escritura?
5. **WebSocket.** Compara el polling actual con un WebSocket + `xterm.js`.
   ¿Qué gana la UI? ¿Qué cuesta en RAM, en hilos y en complejidad de la pila
   TCP de Xinu?
6. **Sesiones.** Diseña un `struct session` con `cwd` y token. ¿Cómo pasas la
   sesión a `ls`, `cd` y `pwd` sin cambiar todos los comandos?

### 10.10 Pruebas realizadas

| Prueba | Resultado |
|---|---|
| `POST /api/exec` + polling con `ps` | 200, salida completa de 910 B |
| `free` | 200, heap libre ≈ 51.9 KB, sin truncar |
| `java StrBuild` | 200, salida de la JVM capturada |
| `ping -c 2 192.168.1.1` | 200, salida del ping capturada |
| `cd /java`, `pwd`, `ls` | El `cd` persiste en el terminal web; la shell serie no cambia |
| `exit` | Rechazado con mensaje; el worker sigue vivo |
| Comando inexistente | `command not found` capturado |
| Segundo comando mientras corre `sleep 3` | `409 terminal busy` |
| Socket idle contra el puerto 80 | Se libera a los 2 s; la siguiente petición responde 200 |
| UI real en Chrome headless | `ps`, `free` y `java StrBuild` seguidos; estado `listo` |

---

## Apéndice A. API de bolsillo

### Semáforos

```c
semaphore semcreate(int count);      /* SYSERR si no hay slots o count < 0 */
syscall   wait(semaphore s);         /* OK / SYSERR                        */
syscall   signal(semaphore s);       /* OK / SYSERR                        */
syscall   signaln(semaphore s, int n);
syscall   semcount(semaphore s);     /* contador (puede ser negativo)      */
syscall   semfree(semaphore s);
```

### Monitores

```c
monitor moncreate(void);             /* SYSERR si no hay slots */
syscall lock(monitor m);
syscall unlock(monitor m);
syscall moncount(monitor m);         /* profundidad, 0 si libre */
syscall monfree(monitor m);
```

### Mailboxes

```c
mailbox mailboxAlloc(uint count);
syscall mailboxSend(mailbox b, int msg);
syscall mailboxReceive(mailbox b);
syscall mailboxCount(mailbox b);
syscall mailboxFree(mailbox b);
```

### Mensajes de hilo

```c
syscall send(tid_typ tid, message msg);
message receive(void);
message recvclr(void);
message recvtime(int ticks);          /* TIMEOUT si expira */
```

### Hilos

```c
tid_typ create(void *fn, uint stk, int prio,
               const char *name, int nargs, ...);
int     ready(tid_typ tid, bool resched);
syscall sleep(uint ms);
syscall yield(void);
syscall kill(int tid);
tid_typ gettid(void);
syscall getprio(tid_typ tid);
syscall chprio(tid_typ tid, int prio);   /* system/chprio.c; añade el
                                            prototipo a thread.h si lo usas */
```

### Memoria

```c
void *memget(uintptr_t n);
syscall memfree(void *p, uintptr_t n);
void *psram_memget(uintptr_t n);
syscall psram_memfree(void *p, uintptr_t n);
void *stkget(uintptr_t n);
syscall stkfree(void *p, uintptr_t n);
```

---

## Apéndice B. Constantes

| Constante | Valor | Dónde |
|---|---:|---|
| `NTHREAD` | 50 | `xinu.conf` |
| `NSEM` | 128 | `xinu.conf` |
| `NMON` | 16 | `xinu.conf` |
| `NMAILBOX` | 10 | `xinu.conf` |
| `NPOOL` | 8 | `xinu.conf` |
| `INITSTK` | 4096 | `include/thread.h` |
| `MINSTK` | 128 | `include/thread.h` |
| `INITPRIO` | 20 | `include/thread.h` |
| `NET_THR_PRIO` | 30 | `include/network.h` |
| `CLKTICKS_PER_SEC` | 1000 | `include/clock.h` |
| `NET_MAX_PKTLEN` | 1598 | `include/network.h` |
| `NET_POOLSIZE` | 24 | `include/network.h` |
| `TNMLEN` | 16 | `include/thread.h` |

Prioridad: **número mayor = más prioridad**. `NET_THR_PRIO` (30) corre por
encima de `INITPRIO` (20, el shell).

---

## Apéndice C. Glosario

- **Hilo (thread):** unidad de ejecución con pila propia. En Xinu no hay
  procesos con memoria separada: todos comparten el espacio de direcciones.
- **TID:** índice en `thrtab[]`.
- **Sección crítica:** código que no debe ejecutarse a la vez que otro.
- **Mutex:** exclusión mutua (una llave).
- **Semáforo:** contador + cola de espera. `wait`/`signal`.
- **Monitor:** mutex con dueño y recursión. `lock`/`unlock`.
- **Mailbox:** cola acotada de `int` con dos semáforos. Sin destinatario.
- **Mensaje de hilo:** un `int` en el slot de un `tid` concreto.
- **Backpressure:** cuando la cola se llena, el productor se bloquea.
- **Inversión de prioridad:** un hilo de prioridad alta espera a uno de
  prioridad baja que, a su vez, es adelantado por uno de prioridad media. Xinu
  no tiene herencia de prioridad.
- **ISR:** manejador de interrupción. No puede dormir.
- **PSRAM:** memoria externa del ESP32-S3, usada para pilas y buffers grandes.
- **MACRAW:** modo del W5500 en el que entrega tramas Ethernet crudas; no hace
  IP/TCP.

---

*Fin del manual. Si algo no cuadra con el código, gana el código: este documento
está pensado para leerse con `system/`, `network/` y `device/` abiertos al lado.*
