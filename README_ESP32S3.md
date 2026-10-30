# s3-xinu: Embedded Xinu en ESP32-S3

Port de Embedded Xinu al ESP32-S3 (Xtensa LX7, ABI CALL0), **sin ESP-IDF y sin
bootloader**: la ROM carga la imagen en `0x0`.

> El diseño anterior con ESP-IDF en el núcleo 0, WiFi por IPC y cámara **no está
> implementado**. El port actual es bare-metal, de un solo núcleo, y el segundo
> núcleo queda libre para una fase posterior.

## Estado (2026-10-21)

- Kernel Xinu arranca y ejecuta el shell por UART0 y por telnet.
- Red real con W5500 en modo **MACRAW** por **SPI2 hardware a 5 MHz**.
- DHCP, ARP, IPv4, ICMP (ping), TCP y UDP.
- `httpd` en `:80` y `telnetserver` en `:23` como servicios de arranque.
- FAT sobre microSD (SDMMC 4 bits); `wget` host→SD y `httpd` SD→host.
- Simple JVM ejecutando las clases del proyecto P4 sin recompilar.
- Dashboard técnico en la SD: `web/index.html` y `web/manual.html`.
- Gestor de archivos web: listar, subir, descargar, borrar, crear y
  eliminar carpetas desde el navegador.
- Terminal web: `POST /api/exec` + polling de `GET /api/job`, con hilo
  `webshell` y captura de salida en `MEMTTY0` (PSRAM).

Pruebas verificadas en placa:

| Prueba | Resultado |
|---|---|
| Ping host → placa | 0 % pérdida, 2.8–6.9 ms |
| 100 peticiones HTTP secuenciales | 100/100, máx ≈ 0.10 s |
| `wget` 16 384 B → SD | md5 idéntico |
| `wget` 102 400 B → SD | md5 idéntico |
| Descarga SD → host por `httpd` | 200, 102 400 B, md5 idéntico |
| Gestor de archivos: mkdir, subir, bajar, borrar, rmdir | OK, 4 KB y 100 KB, md5 idéntico |
| Terminal web: `ps`, `free`, `java StrBuild`, `ping` | OK, salida capturada; 2.º comando → 409 |
| UI del terminal en Chrome headless | OK, varios comandos y estado `listo` |
| Preconexión idle contra `httpd` | Se libera a los 2 s; la siguiente petición responde 200 |
| JVM: `Loop`, `StrBuild`, `FloatTest`, `ComplexDemo` | OK |

## Arquitectura

```
┌──────────────────────────────────────────────────────────┐
│                     ESP32-S3 (un núcleo)                  │
│                                                           │
│  ROM bootloader ──► start.S ──► nulluser() ──► main()     │
│                                      │                    │
│           ┌──────────────────────────┴─────────────┐      │
│           │  scheduler, semáforos, monitores,      │      │
│           │  mailboxes, mensajes de hilo           │      │
│           ├────────────────────────────────────────┤      │
│           │  shell · httpd · telnet · JVM          │      │
│           ├────────────────────────────────────────┤      │
│           │  ARP · IPv4 · ICMP · TCP · UDP · DHCP  │      │
│           ├────────────────────────────────────────┤      │
│           │  drivers: W5500 · SD · TFT · UART ...  │      │
│           └────────────────────────────────────────┘      │
│                                                           │
│  IRAM 128 KiB · DRAM 160 KiB · PSRAM 8 MiB · Flash 8 MiB  │
└──────────────────────────────────────────────────────────┘
```

## Hardware

| Elemento | Detalle |
|---|---|
| SoC | ESP32-S3, Xtensa LX7, ABI CALL0 |
| Reloj | CPU y APB a 20 MHz |
| PSRAM | 8 MB en `0x3C000000` |
| Flash | 8 MB, imagen en `0x0` |
| Consola | UART0, 115200 8N1, GPIO43 TX / GPIO44 RX |
| Ethernet | W5500 SPI2: CS 10, MOSI 11, SCK 12, MISO 13, INT 14, RST 15 |
| TFT | ST7735 128×160: CS 21, DC 16, MOSI 17, SCLK 18, RST 15 |
| microSD | SDMMC 4 bits: CLK 36, CMD 35, D0 37, D1 38, D2 33, D3 34 |
| Botón | BOOT en GPIO0, polling, activo a nivel bajo |

**GPIO15 es del TFT.** Es el reset compartido con el W5500; el driver del TFT
lo posee y el W5500 usa reset por software (`MR.RST`). No conducir ese pin
desde el W5500.

## Compilar

```bash
cd /home/daniel/s3-xinu
find . -name '*.o' -delete
rm -rf compile/.deps
cd compile
source ../env.sh
make PLATFORM=esp32s3 -j4
```

Salida: `compile/xinu.elf` y `compile/xinu.boot`.

## Flashear

El CH340 de esta placa aparece como `/dev/ttyUSB1`:

```bash
esptool --chip esp32s3 \
  --port /dev/ttyUSB1 --baud 460800 \
  write-flash --flash-mode dio \
  --flash-freq 80m --flash-size 8MB \
  0x0 xinu.boot
```

No se usa bootloader de ESP-IDF: la imagen se escribe en `0x0`.

## Red

- W5500 en MACRAW (socket 0); ARP/IP/TCP/UDP los implementa Xinu.
- SPI2 hardware, `W5500_SPI_HZ = 5 MHz` (APB/4). Fallback bit-bang con
  `W5500_SPI_HW 0`.
- DHCP al arrancar; en la red de pruebas: `192.168.1.137/24`, gw
  `192.168.1.1`.
- `httpd` sirve la microSD; `wget` descarga del host a la SD.
- `NTCP 10`, `TIME_WAIT` 1 s para no agotar la tabla.

## Web

El `httpd` sirve desde la SD y atiende también la API. No hay un segundo
servidor:

- `/index.html` — dashboard técnico.
- `/manual.html` — manual navegable.
- `/api/files`, `/api/download`, `/api/file`, `/api/mkdir`, `/api/rmdir` —
  gestor de archivos.
- `/api/exec`, `/api/job` — terminal web.

### Endpoints y función que los atiende

| Método y ruta | Función | Fichero:línea | Hilo que responde |
|---|---|---|---|
| `GET /` y ficheros de la SD | `serve_static()` | `shell/xsh_httpd.c:220` | `httpd` |
| `GET /api/files` | `api_files()` | `shell/httpapi.c:263` | `httpd` |
| `GET /api/download` | `api_download()` | `shell/httpapi.c:313` | `httpd` |
| `PUT /api/file` | `api_upload()` | `shell/httpapi.c:352` | `httpd` (lee el cuerpo) |
| `DELETE /api/file` | `api_delete()` | `shell/httpapi.c:432` | `httpd` |
| `POST /api/mkdir` | `api_mkdir()` | `shell/httpapi.c:455` | `httpd` |
| `POST /api/rmdir` | `api_rmdir()` | `shell/httpapi.c:470` | `httpd` |
| `POST /api/exec` | `api_exec()` | `shell/httpapi.c:485` | `httpd` encola; ejecuta `webshell` |
| `GET /api/job` | `api_job()` | `shell/httpapi.c:561` | `httpd` lee; salida de `MEMTTY0` |
| ruta `/api/` desconocida | `httpApiHandle()` | `shell/httpapi.c:593` | `httpd` → 404 JSON |

El despacho está en `httpApiHandle()` (`shell/httpapi.c:593-648`).
`httpdServer()` lo llama para toda ruta que empiece por `/api/`
(`shell/xsh_httpd.c:350-362`). Las rutas estáticas solo aceptan `GET`; otro
método recibe `405`. Todas las respuestas son HTTP/1.0 con
`Connection: close`.

### Tareas y cadena de creación

| Tarea | La crea | Pila | Prioridad | Papel |
|---|---|---|---:|---|
| `MAIN` | `nulluser()` (`system/initialize.c:68`) | 4096 B | 20 | Arranque; llama a `bootServices()` |
| `BOOT1` | `bootServices()` (`system/boot.c:105`) | 4096 B | 19 | `telnetserver -n 1` |
| `BOOT2` | `bootServices()` (`system/boot.c:105`) | 4096 B | 19 | `httpd 80` |
| `httpd` | `xsh_httpd()` (`shell/xsh_httpd.c:397`) | 8192 B | 20 | Acepta, lee, despacha y libera |
| `webshell` | `webtermInit()` (`shell/webterm.c:94`) | 16384 B PSRAM | 18 | Espera en `jobsem` y ejecuta el trabajo |
| hijo de comando | `webtermRun()` (`shell/webterm.c:301`) | 4096 B | 18 | Comando no incorporado; fds a `MEMTTY0` |
| `JVM` | `xsh_java()` (`shell/xsh_java.c:90`) | 65536 B PSRAM | 20 | Ejecuta Java; hereda los fds del comando |
| `httpdRel` | `httpdRelease()` (`shell/xsh_httpd.c:279`) | 4096 B | 20 | Cierra el socket sin bloquear el `accept` |
| `SHELL0` | `main()` (`system/main.c:85`) | 4096 B | 20 | Shell de la consola serie |
| `MEMTTY0` | No es tarea; `devtab[i].init()` (`system/initialize.c:156`) | — | — | Captura de salida, 8 KB en PSRAM |

```
ROM 0x0 → start.S → nulluser()
  → platforminit() + sysinit()       devtab[].init() incluye MEMTTY0
  → create(MAIN)                     system/initialize.c:68
      → bootServices()               system/main.c:73
          → netup (wait=TRUE, en MAIN)
          → BOOT1 → xsh_telnetserver
          → BOOT2 → xsh_httpd → create(httpd)   shell/xsh_httpd.c:397
      → create(SHELL0)               system/main.c:85
httpdServer()                        shell/xsh_httpd.c:289
  → webtermInit()                    shell/webterm.c:59
      → moncreate() + semcreate(0) + psram_memget(8256)
      → create(webshell)             shell/webterm.c:94
  → accept → read → httpApiHandle()
POST /api/exec → api_exec() → webtermSubmit() → signal(jobsem)
  → webshell → webtermRun()
      → builtin: en webshell
      → no builtin: create(hijo)     shell/webterm.c:301
          → java: create(JVM)        shell/xsh_java.c:90
```

`bootsvc_tab[]` está en `system/boot.c:37`; `netup` lleva `wait = TRUE` y
corre dentro de `MAIN`; los servidores llevan `wait = FALSE` y reciben un
hilo `BOOTn`. La shell serie se crea después, pero el scheduler puede
ejecutar antes a los servicios.

### Flujo del terminal

1. `POST /api/exec` guarda el comando (máximo 159 B), pone el trabajo en
   `RUNNING` bajo `jobmon`, limpia `MEMTTY0` y hace `signal(jobsem)`.
2. `webshell` estaba en `wait(jobsem)`. Cambia su `stdin` a `DEVNULL` y su
   `stdout`/`stderr` a `MEMTTY0`, y ejecuta. Los builtins corren en el propio
   `webshell`; el resto se crea como hijo y se espera con `receive()`.
3. Al terminar, el estado pasa a `DONE`. `GET /api/job` devuelve `202` con
   `X-Job-State: running` mientras corre, y `200 text/plain` con
   `X-Job-State: done` y `X-Job-Truncated: 0|1` al terminar. La salida se
   corta a 8 KB.

### Recursos y límites

| Elemento | Valor |
|---|---:|
| Puerto | 80 (`HTTPD_PORT`) |
| Buffer de petición | 2048 B (`HTTPD_RQMAX`) |
| Pila de `httpd` | 8192 B (`HTTPD_STK`) |
| Timeout de recepción | 2000 ms (`HTTPD_RCVTIMEO`) |
| Comando máximo | 159 B (`WEB_CMD_MAX` 160) |
| Salida capturada | 8192 B (`MEMTTY_CAP`) |
| Snapshot | 8256 B (`WEB_SNAP_CAP`) |
| Pila de `webshell` | 16384 B (`WEBTERM_STK`) |
| Pila de comando | 4096 B (`SHELL_CMDSTK`) |
| Pila de JVM | 65536 B (`JVM_STK`) |
| Subida máxima | 16 MB (`API_MAX_UPLOAD`) |
| Buffer de subida | 1024 B (`API_BUF`) |
| Path de la API | 128 B (`FS_BUF_LEN`) |
| Path/query HTTP | 192 B cada uno (`struct http_request`) |
| `MEMTTY0` | índice 25, `NDEVS` 26 |

`httpd` atiende **una conexión a la vez**. Tras responder crea `httpdRel`
para cerrar el socket y vuelve al `accept`; dos peticiones simultáneas siguen
sin servirse en paralelo. `TCP_CTRL_RCVTIMEO` a 2 s evita que una preconexión
idle de Chrome bloquee el servidor.

El gestor permite navegar, subir por selector o arrastre, descargar, borrar y
crear/eliminar carpetas. El terminal acepta comandos no interactivos
(`ps`, `free`, `ls`, `ping`, `java`, …), mantiene su propio `cd` y rechaza
`exit`, redirecciones y `&`. **No hay autenticación**: úsalo solo en una LAN
de confianza. El detalle completo, los códigos de respuesta y las notas de
implementación están en `web/README.md` y en el capítulo 10 de
`docs/MANUAL-KERNEL.md`.

## Documentación

- `docs/MANUAL-KERNEL.md` — manual de C, concurrencia (semáforos, monitores,
  mailboxes, mensajes), red, drivers, cómo modificar el kernel y ejercicios.
- `web/index.html` — dashboard técnico que se sirve desde la placa.
- `web/manual.html` — manual navegable (generado desde el Markdown).
- `PENDIENTES.md` — estado y trabajo abierto.
- `web/README.md` — cómo regenerar y subir el dashboard.

## Estructura

```
system/            kernel: scheduler, ctxsw, initialize, boot
device/            drivers: w5500, sd, tft, uart, tty, telnet, jvm
network/           arp, ipv4, icmp, dhcp, net
jvm/               Simple JVM y parser de .class
shell/             comandos del shell
fs/                FAT
include/           cabeceras y configuración generada
compile/           Makefiles, linker script, xinu.boot
docs/              manual del kernel
web/               dashboard y manual HTML
```
