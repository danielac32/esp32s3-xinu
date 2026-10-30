# Pendientes — ESP32-S3 / Xinu

Estado: SPI2 hardware a 5 MHz, red verificada, dashboard, gestor de archivos
y terminal web publicados en la SD.

## Red / W5500

- [x] **SPI2 a 5 MHz** (`W5500_SPI_HZ` en `device/w5500/w5500Spi.c`,
      APB 20 MHz / 4). Verificado el 2026-10-21: ping 0 % (2.8–6.9 ms),
      `wget` de 16 KB y 100 KB con md5 idéntico en la SD.
- [ ] **HTTP concurrente**: el servidor atiende una conexión a la vez.
      Mientras sirve, no hay listener y los SYN paralelos reciben RST.
      Propuesta: hilo aceptador + cola (mailbox) de `tcpdev` y un hilo
      trabajador que sirva en serie (evita además concurrencia en la FAT).
- [x] **Preconexión idle**: `httpd` configura `TCP_CTRL_RCVTIMEO` a 2 s.
      `tcpRead()` hace polling cada 10 ms cuando el timeout está activo, así
      una conexión que no envía nada se libera y el servidor vuelve a
      escuchar. Verificado con un socket idle: la petición concurrente falla
      mientras dura y funciona tras el timeout.
- [ ] **XMODEM con ficheros grandes**: falló una vez a los ~8 KB (bloque 69).
      Revisar si el receptor aborta por timeout al escribir en la SD;
      los ficheros pequeños siguen funcionando.
- [ ] `telnetserver -n 1` deja de aceptar si el cliente corta a mitad de una
      transferencia; recuperar por consola serie (`telnetserver -n 1`) o
      reiniciar. No afecta a `wget`, que por serie sigue funcionando.
- [x] `wget` con 100 KB verificado (102 400 B, md5 `a88cd902…`). Falta
      probar varios MB.
- [ ] Revisar el checksum de `tcpSendRst` con captura (ya corregido, sin
      validación independiente).

## Stack TCP

- [x] `tcpRead()`: timeout de recepción configurable (`TCP_CTRL_RCVTIMEO`,
      en ms; 0 = bloqueo). `httpd` lo usa con 2 s para no quedarse colgado
      con una preconexión idle. La ruta con timeout hace polling con
      `sleep(10)`; la ruta normal sigue usando el semáforo `readers`.
- [ ] `tcpRead()`: en las salidas de error de `stateCheck()` sale sin
      liberar el mutex del TCB (latente).
- [ ] `TIME_WAIT` bajado a 1 s (`TCP_TWOMSL`) para no agotar la tabla;
      valor no estándar, revisar si aparece algún duplicado raro.
- [ ] `tcpAllocPassive()` reclama `TIMEWAIT`, `FIN-WAIT-2` y `CLOSING`.
      Comprobar el caso de un cliente que nunca confirma el FIN.
- [ ] `NTCP = 10`, `NSEM = 128` (antes 3 y 50). Vigilar consumo si se
      añaden más servicios.

## JVM

- [ ] **Sin GC**: el heap se agota en bucles largos. Mitigado con
      `JVM_HEAP_SLOTS = 2048`; evitar concatenación de `String` por iteración.
- [ ] `Thread.sleep(long)` registrado contra un nativo que hace `pop` de un
      `int` (desajuste de firma). Usar `esp32p4.Sys.sleep(int)` o corregirlo.
- [ ] Subir el resto de clases de `jvm/dist/java` por XMODEM y ejecutar las
      pruebas de `p4-xinu/jvm/tests/run.sh`.
- [ ] `double` sin validar.

## Núcleo / arquitectura

- [ ] `ctxsw()` invocado desde contexto de interrupción sigue guardando los
      registros del manejador en vez de los del hilo interrumpido
      (hay un parche defensivo; falta rehacer `ctxsw.S`/`vectors.S`).
- [ ] **No** volver a enmascarar interrupciones con `PS=15` dentro de
      `ctxsw.S`: rompe el arranque.
- [ ] CPU a 20 MHz; árbol de relojes sin tocar (un intento anterior colgó
      núcleo y UART).
- [ ] `resched()` debe seguir en `clkhandler.c` y `wakeup.c` (DHCP depende).

## Documentación / web

- [x] `docs/MANUAL-KERNEL.md`: manual de C, concurrencia, red y modificaciones.
- [x] `web/index.html`: dashboard técnico (~77 KB) servido por `httpd`.
- [x] `web/manual.html`: manual navegable enlazado desde el dashboard.
- [x] Gestor de archivos web: `GET /api/files`, `GET /api/download`,
      `PUT /api/file`, `DELETE /api/file`, `POST /api/mkdir`,
      `POST /api/rmdir`. Probado con 4 KB y 100 KB, md5 idéntico.
- [x] UI del gestor: navegar, subir por selector/arrastre, descargar,
      borrar, crear y eliminar carpetas.
- [x] Terminal web: `POST /api/exec` + `GET /api/job`, hilo `webshell`,
      dispositivo `MEMTTY0` (PSRAM), monitor `jobmon` + semáforo `jobsem`,
      salida de 8 KB con marca de truncado y `cd` propio de la sesión.
      Probado con `ps`, `free`, `java StrBuild`, `ping`, `exit` rechazado,
      segundo comando → 409 y UI real con Chrome headless.
- [x] Mapa de endpoints y tareas: ruta → función → fichero:línea → hilo,
      cadena `MAIN`/`BOOTn`/`httpd`/`webshell`/hijo/`JVM`/`httpdRel`, pilas,
      prioridades y límites en `web/README.md`, `README_ESP32S3.md`,
      `docs/MANUAL-KERNEL.md` y la sección “Endpoints y tareas” del dashboard.
- [ ] Terminal: cancelar un comando largo y entregar salida en vivo (hoy se
      entrega al terminar; el polling solo devuelve el estado).
- [ ] Autenticación para `/api/`; hoy no hay y la placa debe estar en una
      LAN de confianza.
- [ ] Subida atómica: el `PUT` trunca/crea el destino y, si falla, borra el
      parcial. Si el destino ya existía, se pierde.
- [ ] Mantener el dashboard al día cuando cambien pruebas o pendientes.

## Fase 2

- [ ] LVGL sobre el ST7735 con un `lv_conf.h` recortado.
- [ ] Cargador de `.bin` PIC.
