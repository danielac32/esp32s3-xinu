# Compilar y extender el proyecto Xinu ESP32-S3

Este documento explica como compilar el kernel, las apps Java y como agregar codigo nuevo.

## 1. Compilar el kernel (desde la raiz del proyecto)

```bash
cd /home/daniel/s3-xinu/compile
source ../env.sh
make PLATFORM=esp32s3 -j4
```

Genera `compile/xinu.boot` (imagen unica, offset 0).

### Variables importantes en compile/platforms/esp32s3/platformVars

```make
APPCOMPS := shell apps mailbox network fs
DEVICES  := tty uart-esp32s3 w5500 raw udp tcp telnet sd tft button rgb jvm memtty
```

- `APPCOMPS`: componentes que se compilan (shell, apps, mailbox, network, fs).
- `DEVICES`: drivers que se incluyen en el kernel.

Si quitas `shell` de `APPCOMPS`, `HAVE_SHELL` pasa a 0 y `main.c` no compila (porque usa `shell`). Si quitas `tft` o `w5500` de `DEVICES`, `TFT0`/`ETH0` dejan de existir.

### Flashear

```bash
esptool --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
  write-flash --flash-mode dio --flash-freq 80m --flash-size 8MB \
  0x0 xinu.boot
```

## 2. Compilar las apps Java

```bash
cd /home/daniel/s3-xinu/jvm
./build-examples.sh
```

Las clases quedan en `jvm/dist/java/` (apps) y `jvm/dist/java/esp32p4/` (librerias nativas).

## 3. Agregar un nuevo .c al kernel

1. Crear el fichero en el directorio adecuado, por ejemplo `system/mi_modulo.c`.
2. Anadirlo a `C_FILES` en el `Makerules` correspondiente:

```make
# system/Makerules
C_FILES += memget.c memfree.c ... mi_modulo.c
```

3. Si es un driver, anadirlo tambien a `DEVICES` en `platformVars` y declararlo en `xinu.conf`.
4. Recompilar: `make PLATFORM=esp32s3 -j4`.

## 4. Agregar un comando a la shell

1. Crear `shell/xsh_foo.c` con la funcion `shellcmd xsh_foo(int nargs, char *args[])`.
2. Anadir a `shell/Makerules`:
   ```make
   C_FILES += xsh_foo.c
   ```
3. Declarar en `include/shell.h`:
   ```c
   shellcmd xsh_foo(int, char *[]);
   ```
4. Registrar en `shell/shell.c` dentro de `commandtab[]`:
   ```c
   {"foo", FALSE, xsh_foo},
   ```

## 5. Agregar un nuevo device

1. Declararlo en `compile/platforms/esp32s3/xinu.conf`:
   ```
   MI0      is mi     on HARDWARE
   ```
2. Anadir el tipo a `DEVICES` en `platformVars`:
   ```make
   DEVICES := ... mi
   ```
3. Implementar el driver en `device/mi/` con `miInit`, `miOpen`, `miRead`, etc.
4. Anadir el directorio a `device/Makerules`.
5. Recompilar.

## 6. Modificar el linker script

El script esta en `compile/platforms/esp32s3/ld.script`. Los valores actuales son:

```
iram (rwx) : ORIGIN = 0x40378000, LENGTH = 0x20000   /* 128 KiB */
dram (rwx) : ORIGIN = 0x3FCA8000, LENGTH = 0x28000   /* 160 KiB */
```

No ampliar IRAM a 140 KiB: la ROM carga mas bytes de los que admite y la placa se reinicia. Si el enlazador dice `region iram overflowed`, hay que liberar codigo (quitar comandos de shell, simplificar funciones).

## 7. Modificar la configuracion (conf.h)

`include/conf.h` es un fichero generado. No se edita a mano. Se genera desde `compile/platforms/esp32s3/xinu.conf` con `compile/config/config`. Para cambiar pines, contadores o constantes, edita `xinu.conf` y recompila.

## 8. Subir ficheros a la SD

Desde la consola Xinu:

```
cd /java
wget http://<host>:8001/App.class App.class
```

O por XMODEM:

```
xmodem recv /java/App.class
```

## 9. Ejecutar una app Java

```
java ColorShow
```

Desde el directorio `/java` de la SD.

## 10. Notas importantes

- La IRAM esta casi llena (~600 B libres). Cada cambio de codigo puede hacer que el enlazador falle. En ese caso, quita comandos de shell secundarios o simplifica codigo.
- El buffer TCP es de 1024 bytes. La ventana inicial anunciada al peer debe ser igual o menor que ese buffer (`TCP_INIT_WND = TCP_IBLEN` en `include/tcp.h`).
- El comando `mem` muestra el uso de memoria; `free` es mas detallado.
- Para documentacion completa, ver `web/docs.html` en la SD o en el host.