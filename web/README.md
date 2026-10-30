# Dashboard, manual, gestor de archivos y terminal web

Todo se sirve desde la microSD con el `httpd` de Xinu:

| Fichero | Tamaño | Contenido |
|---|---:|---|
| `index.html` | ~77 KB (76 586 B) | Dashboard: hardware, pines, memoria, red, concurrencia, drivers, JVM, shell, gestor de archivos, terminal web, endpoints/tareas, pruebas y pendientes |
| `manual.html` | ~88 KB (87 860 B) | Manual del kernel navegable, generado desde `docs/MANUAL-KERNEL.md` |

No usan CDN ni recursos externos: todo el CSS y el JavaScript van embebidos.
La placa no necesita Internet para servirlos.

## Mapa de endpoints y tareas

Todo lo sirve el mismo `httpd` del puerto 80. No hay un servidor aparte para
la API ni para el terminal: `httpdServer()` acepta la conexión, lee la
petición y decide entre fichero estático y API.

### Endpoints

| Método y ruta | Función que lo atiende | Fichero:línea | Qué hace |
|---|---|---|---|
| `GET /` y ficheros de la SD | `serve_static()` | `shell/xsh_httpd.c:220` | `/` se traduce a `/index.html`; el resto se abre desde la raíz de la SD |
| `GET /api/files?path=/dir` | `api_files()` | `shell/httpapi.c:263` | Lista un directorio en JSON |
| `GET /api/download?path=/file` | `api_download()` | `shell/httpapi.c:313` | Descarga un fichero con `Content-Disposition` |
| `PUT /api/file?path=/file` | `api_upload()` | `shell/httpapi.c:352` | Sube el cuerpo binario; exige `Content-Length` |
| `DELETE /api/file?path=/file` | `api_delete()` | `shell/httpapi.c:432` | Borra un fichero |
| `POST /api/mkdir?path=/dir` | `api_mkdir()` | `shell/httpapi.c:455` | Crea una carpeta |
| `POST /api/rmdir?path=/dir` | `api_rmdir()` | `shell/httpapi.c:470` | Borra una carpeta vacía |
| `POST /api/exec` | `api_exec()` | `shell/httpapi.c:485` | Encola un comando y devuelve `202` con el id |
| `GET /api/job?id=N` | `api_job()` | `shell/httpapi.c:561` | Devuelve estado y salida de un trabajo |
| Cualquier otra `/api/...` | `httpApiHandle()` | `shell/httpapi.c:593` | Despacha por ruta y método; si no coincide, `404` JSON |

El despacho de la API está en `httpApiHandle()` (`shell/httpapi.c:593-648`).
`httpdServer()` lo llama para toda ruta que empiece por `/api/`
(`shell/xsh_httpd.c:350-362`). Las rutas que no empiezan por `/api/` se sirven
como ficheros estáticos y solo aceptan `GET`; otro método recibe `405`.

### Quién crea cada tarea

| Tarea | La crea | Pila | Prioridad | Papel |
|---|---|---|---:|---|
| `MAIN` | `nulluser()` → `create(main, ...)` (`system/initialize.c:68`) | `INITSTK` 4096 B | 20 | Abre consola/TFT/ETH0, llama a `bootServices()` y crea la shell serie |
| `BOOT1` | `bootServices()` → `create(bootsvc_run, ...)` (`system/boot.c:105`) | `SHELL_CMDSTK` 4096 B | `BOOTSVC_PRIO` = 19 | Ejecuta `telnetserver -n 1` |
| `BOOT2` | igual que `BOOT1` | 4096 B | 19 | Ejecuta `httpd 80` |
| `httpd` | `xsh_httpd()` → `create(httpdServer, ...)` (`shell/xsh_httpd.c:397`) | `HTTPD_STK` 8192 B | `INITPRIO` 20 | Acepta una conexión, lee, despacha y libera |
| `webshell` | `webtermInit()` → `create(webtermTask, ...)` (`shell/webterm.c:94`) | `WEBTERM_STK` 16384 B (PSRAM) | `WEBTERM_PRIO` 18 | Espera en `jobsem` y ejecuta el trabajo |
| hijo de comando | `webtermRun()` → `create(commandtab[i].procedure, ...)` (`shell/webterm.c:301`) | `SHELL_CMDSTK` 4096 B | `WEBTERM_CMD_PRIO` 18 | Ejecuta una orden no incorporada con `stdout`/`stderr` en `MEMTTY0` |
| `JVM` | `xsh_java()` → `create(jvmTask, ...)` (`shell/xsh_java.c:90`) | `JVM_STK` 65536 B (PSRAM) | `INITPRIO` 20 | Ejecuta la clase Java y hereda los descriptores del comando |
| `httpdRel` | `httpdRelease()` → `create(httpdReleaser, ...)` (`shell/xsh_httpd.c:279`) | `INITSTK` 4096 B | 20 | Cierra el socket sin bloquear el siguiente `accept` |
| `SHELL0` | `main()` → `create(shell, ...)` (`system/main.c:85`) | `INITSTK` 4096 B | 20 | Shell de la consola serie |
| `MEMTTY0` | No es una tarea: `sysinit()` llama a `devtab[i].init()` (`system/initialize.c:156`) | — | — | Driver de captura; reserva 8 KB en PSRAM |

La cadena completa del terminal es:

```
ROM 0x0 → start.S → nulluser()
  → platforminit() + sysinit()       devtab[].init() incluye MEMTTY0
  → create(MAIN)                     system/initialize.c:68
      → bootServices()               system/main.c:73
          → netup                    wait=TRUE: corre en MAIN
          → BOOT1 → xsh_telnetserver
          → BOOT2 → xsh_httpd → create(httpd)   shell/xsh_httpd.c:397
      → create(SHELL0)               system/main.c:85
httpdServer()                        shell/xsh_httpd.c:289
  → webtermInit()                    shell/webterm.c:59
      → moncreate() + semcreate(0)
      → psram_memget(8256)           snapshot de salida
      → create(webshell)             shell/webterm.c:94
  → bucle accept/read/despachar
POST /api/exec → api_exec() → webtermSubmit()
  → signal(jobsem) → webshell → webtermRun()
      → builtin: se ejecuta en webshell
      → no builtin: create(hijo)     shell/webterm.c:301
          → si es java: create(JVM)  shell/xsh_java.c:90
```

`bootsvc_tab[]` está en `system/boot.c:37`; `netup` lleva `wait = TRUE` y se
ejecuta dentro de `MAIN`; `telnetserver` y `httpd` llevan `wait = FALSE` y
reciben un hilo `BOOTn`. La shell serie se crea después, pero el scheduler
puede ejecutar antes a los servicios; no hay una dependencia de orden entre
ellos una vez que `netup` ha terminado.

### Flujo de `POST /api/exec` y `GET /api/job`

1. `httpd` lee las cabeceras en `req[HTTPD_RQMAX + 1]` (2048 B), localiza el
   final y llama a `httpApiHandle()`. Si el cliente envió parte del cuerpo en
   la misma lectura, se pasa como `prebody`.
2. `api_exec()` acepta el comando en el cuerpo (`text/plain`) o en
   `?cmd=...`. Recorta espacios y saltos de línea; el máximo es 159 B.
3. `webtermSubmit()` toma `jobmon`, comprueba que no haya otro trabajo,
   asigna un id, copia el comando, pone `RUNNING`, limpia `memtty` y hace
   `signal(jobsem)`.
4. `webshell` estaba dormido en `wait(jobsem)`. Copia el comando bajo el
   monitor, pone su `stdin` en `DEVNULL` y su `stdout`/`stderr` en `MEMTTY0`,
   y ejecuta. Los comandos incorporados corren en el propio `webshell`; los
   demás se crean como hijo y se esperan con `receive()`.
5. La salida se acumula en el buffer de 8 KB de `MEMTTY0`. Al terminar,
   `webshell` pone el estado en `DONE` bajo `jobmon`.
6. `GET /api/job?id=N` llama a `webtermStatus()`. Si sigue en `RUNNING`
   responde `202` con `X-Job-State: running` y cuerpo vacío. Si terminó,
   `webtermSnapshot()` copia la salida a otro buffer de PSRAM y responde
   `200 text/plain` con `X-Job-State: done` y `X-Job-Truncated: 0|1`.

### Recursos y límites

| Elemento | Valor | Dónde |
|---|---:|---|
| Puerto HTTP | 80 | `HTTPD_PORT`, `shell/xsh_httpd.c:25` |
| Buffer de petición | 2048 B | `HTTPD_RQMAX`, `shell/xsh_httpd.c:26` |
| Pila de `httpd` | 8192 B | `HTTPD_STK`, `shell/xsh_httpd.c:27` |
| Timeout de recepción | 2000 ms | `HTTPD_RCVTIMEO`, `shell/xsh_httpd.c:31` |
| Longitud de comando | 159 B útiles | `WEB_CMD_MAX` 160, `include/webterm.h:13` |
| Buffer de salida | 8192 B | `MEMTTY_CAP`, `include/memtty.h:14` |
| Snapshot de salida | 8256 B | `WEB_SNAP_CAP`, `shell/webterm.c:39` |
| Pila de `webshell` | 16384 B | `WEBTERM_STK`, `shell/webterm.c:34` |
| Pila de comando | 4096 B | `SHELL_CMDSTK`, `include/shell.h:16` |
| Pila de JVM | 65536 B | `JVM_STK`, `include/thread.h:43` |
| Subida máxima | 16 MB | `API_MAX_UPLOAD`, `shell/httpapi.c:29` |
| Buffer de subida | 1024 B | `API_BUF`, `shell/httpapi.c:28` |
| Path de la API | 128 B | `FS_BUF_LEN`, `include/xsh_fs.h:13` |
| Path/query HTTP | 192 B cada uno | `struct http_request`, `include/xsh_http.h` |
| Dispositivo de captura | `MEMTTY0` = índice 25 | `xinu.conf`, `NDEVS` = 26 |

`httpd` atiende **una conexión a la vez**. Tras escribir la respuesta, crea
`httpdRel` para cerrar el socket y vuelve al `accept`; así no espera el FIN
del cliente, pero dos peticiones simultáneas siguen sin poder servirse en
paralelo. `httpd` configura `TCP_CTRL_RCVTIMEO` a 2 s para que una
preconexión idle del navegador no lo deje bloqueado.

### Dónde añadir o cambiar algo

- **Una ruta nueva de la API:** añade el `if` de ruta y método en
  `httpApiHandle()` (`shell/httpapi.c:593`) y la función `api_*` que responde.
  No hace falta tocar `boot.c` ni crear otra tarea.
- **Un trabajo asíncrono nuevo:** sigue el patrón de `webterm.c`: estado bajo
  monitor, semáforo para despertar al worker y un `create()` para el worker.
  No ejecutes I/O de red con el monitor cogido.
- **Cambiar pilas o prioridades:** `HTTPD_STK`/`INITPRIO` en
  `shell/xsh_httpd.c`, `WEBTERM_STK`/`WEBTERM_PRIO` en `shell/webterm.c`,
  `SHELL_CMDSTK` en `include/shell.h`.
- **Cambiar el límite de comando:** `WEB_CMD_MAX` en `include/webterm.h` y el
  buffer `cmd` de `api_exec()`; el mensaje de error de 413 está en
  `shell/httpapi.c`.
- **Cambiar la página:** edita `web/index.html` y súbela como `index.new`,
  luego `rm index.html` y `mv index.new index.html` (ver “Subir a la placa”).

## API de archivos

El mismo `httpd` atiende `/api/` además de los ficheros estáticos. Las rutas
son absolutas dentro de la SD y `full_path()` resuelve `.` y `..`, así que una
petición no puede salir de la tarjeta.

| Método | Ruta | Efecto |
|---|---|---|
| `GET` | `/api/files?path=/dir` | Lista un directorio en JSON |
| `GET` | `/api/download?path=/file` | Descarga un fichero |
| `PUT` | `/api/file?path=/file` | Sube un fichero (cuerpo binario) |
| `DELETE` | `/api/file?path=/file` | Borra un fichero |
| `POST` | `/api/mkdir?path=/dir` | Crea una carpeta |
| `POST` | `/api/rmdir?path=/dir` | Borra una carpeta vacía |

Ejemplos:

```bash
curl 'http://192.168.1.137/api/files?path=/'
curl -X POST 'http://192.168.1.137/api/mkdir?path=/docs'
curl -T local.bin 'http://192.168.1.137/api/file?path=/docs/local.bin'
curl -o local.bin 'http://192.168.1.137/api/download?path=/docs/local.bin'
curl -X DELETE 'http://192.168.1.137/api/file?path=/docs/local.bin'
curl -X POST 'http://192.168.1.137/api/rmdir?path=/docs'
```

Códigos y cuerpos:

| Endpoint | Éxito | Errores |
|---|---|---|
| `GET /api/files` | `200 application/json`, sin `Content-Length` (cierra al terminar) | `400 invalid path`; `404 cannot open directory` |
| `GET /api/download` | `200 application/octet-stream` + `Content-Disposition` | `400 invalid path` o `is a directory`; `404 cannot open file` |
| `PUT /api/file` | `201 {"ok":true,"path":"...","size":N}` | `400` path/body incompleto; `409 path is a directory`; `411 Content-Length is required`; `413 file exceeds the 16 MB limit`; `500` no se pudo crear/escribir |
| `DELETE /api/file` | `200 {"ok":true}` | `400` path; `404 file not found`; `409` es directorio; `500` no se pudo borrar |
| `POST /api/mkdir` | `201 {"ok":true}` | `400` path; `409 directory exists`; `500 cannot create directory` |
| `POST /api/rmdir` | `200 {"ok":true}` | `400` path o no es directorio; `409` no está vacío o no se puede borrar |

Los errores de la API son JSON con la forma
`{"ok":false,"error":"..."}` (`send_json_error()` en `shell/httpapi.c`). Un
método incorrecto devuelve `405` con `{"ok":false,"error":"use ..."}`. Una
ruta `/api/` desconocida devuelve `404` con `unknown endpoint`.

Notas:

- La subida usa `Content-Length` y se escribe en trozos de 1 KB; el límite es
  de 16 MB. `curl -T` puede enviar `Expect: 100-continue`; el servidor lo
  atiende.
- Si la subida se corta, el fichero parcial se borra. Si el destino ya
  existía, se pierde: la subida no es atómica.
- La descarga añade `Content-Disposition` con un nombre saneado; los
  caracteres fuera de `[A-Za-z0-9._-]` se sustituyen por `_`.
- El `path` debe empezar por `/`; `full_path()` resuelve `.` y `..` y no deja
  salir de la raíz de la SD. El parámetro se decodifica (`%XX` y `+`), con un
  máximo de 511 B por valor y 127 B para el path resuelto.
- **No hay autenticación.** Cualquiera que llegue al puerto 80 puede leer,
  subir y borrar. Úsalo solo en una LAN de confianza.

## API del terminal

El terminal web no ejecuta el comando dentro del hilo de `httpd`: lo encola y
un hilo `webshell` lo ejecuta con `stdout`/`stderr` apuntando al dispositivo
`MEMTTY0`. La página consulta el trabajo hasta que termina.

| Método | Ruta | Efecto |
|---|---|---|
| `POST` | `/api/exec` | Encola un comando. Cuerpo `text/plain`, máximo 159 B. Devuelve `202` con `{"ok":true,"id":N,"state":"running"}` |
| `GET` | `/api/job?id=N` | `202` con `X-Job-State: running` si sigue; `200 text/plain` con la salida y `X-Job-State: done` al terminar |

Cabeceras de `GET /api/job`:

- `X-Job-State: running|done`
- `X-Job-Truncated: 0|1` (la salida se corta a 8 KB)
- `Content-Length` y `Connection: close`

Códigos del terminal:

| Petición | Código | Cuerpo / cabeceras |
|---|---|---|
| `POST /api/exec` correcto | `202 Accepted` | `{"ok":true,"id":N,"state":"running"}` |
| `POST /api/exec` con otro trabajo activo | `409 Conflict` | `{"ok":false,"error":"terminal busy"}` |
| `POST /api/exec` > 159 B | `413 Payload Too Large` | `{"ok":false,"error":"command exceeds 159 bytes"}` |
| `POST /api/exec` vacío o incompleto | `400 Bad Request` | `empty command` o `incomplete body` |
| `POST /api/exec` sin terminal | `503 Service Unavailable` | `terminal not ready` |
| `GET /api/job` en ejecución | `202 Accepted` | `X-Job-State: running`, cuerpo vacío |
| `GET /api/job` terminado | `200 OK` | `text/plain`, `X-Job-State: done`, salida |
| `GET /api/job` id desconocido | `404 Not Found` | `{"ok":false,"error":"unknown job"}` |
| `GET /api/job` sin `id` | `400 Bad Request` | `{"ok":false,"error":"missing id"}` |

`POST /api/exec` acepta el comando en el cuerpo (`text/plain`), o en
`?cmd=...` si no hay cuerpo. Recorta espacios/saltos al principio y al final;
si queda vacío, no se encola. `GET /api/job` usa `atoi()` para el id, así que
un valor no numérico se convierte en `0` y termina en `404 unknown job`.

Ejemplos:

```bash
# Encolar
curl -sS -X POST --data 'ps' http://192.168.1.137/api/exec
# {"ok":true,"id":1,"state":"running"}

# Consultar (las cabeceras dicen el estado; el cuerpo es la salida)
curl -sS -D - 'http://192.168.1.137/api/job?id=1'
```

Notas:

- Solo hay un trabajo a la vez. Si se envía otro mientras corre, la API
  responde `409 Conflict` con `{"ok":false,"error":"terminal busy"}`.
- La salida se captura de `printf`/`fprintf` y de los hilos hijos; `java`
  hereda los descriptores del comando (se corrigió `xsh_java.c`, que antes
  creaba la JVM con la consola por defecto).
- No es un terminal interactivo: `telnet`, `nc` y `xmodem` no funcionan;
  tampoco se admiten `<`, `>` ni `&`. `exit` se rechaza para no matar al
  hilo `webshell`.
- El `cd` del terminal web es propio de la sesión: se guarda el `fs_curdir`
  global, se pone el del terminal, se ejecuta y se restaura. Durante el
  comando, un `cd` simultáneo por serie/telnet comparte la variable global.
- `httpd` configura `TCP_CTRL_RCVTIMEO` a 2 s. Así una preconexión idle del
  navegador (Chrome abre una y no envía nada) no deja bloqueado al servidor
  de una sola conexión: `tcpRead()` agota el timeout y libera el socket.
- **Sin autenticación:** el terminal ejecuta comandos con privilegios de
  kernel. No expongas el puerto 80 a Internet.

## Interfaz

La pestaña **Archivos** del dashboard permite:

- navegar con migas de pan y botón de subir;
- subir con el selector o arrastrando ficheros sobre la zona de drop, con
  progreso por fichero;
- descargar con un clic;
- borrar ficheros y carpetas, con confirmación;
- crear carpetas;
- avisar antes de sobrescribir un fichero existente.

La pestaña **Terminal** permite:

- escribir comandos, con historial (flechas arriba/abajo) y `Ctrl+L`;
- botones rápidos para `ps`, `free`, `ls /`, `df`, `route`, `tcpstat` y
  `java StrBuild`;
- ver el estado (`enviando…`, `ejecutando…`, `listo`, `error`);
- confirmar antes de `reset`, `kill`, `rm`, `rmdir`, etc.;
- limpiar la salida en local con `Limpiar` o con el comando `clear`.

## Regenerar `manual.html`

El conversor `md2html.py` entiende lo que usa el manual: bloques de código,
tablas, listas, citas, encabezados y formato en línea.

```bash
cd /home/daniel/s3-xinu
python3 web/md2html.py docs/MANUAL-KERNEL.md web/manual.html
```

El índice se genera automáticamente a partir de los `h2`/`h3`; no hay que
mantener anclas a mano.

## Subir a la placa

1. Sirve esta carpeta desde el host (en la red de pruebas, el host es
   `192.168.1.144`):

   ```bash
   cd /home/daniel/s3-xinu/web
   python3 -m http.server 8001
   ```

2. En la consola de la placa (serie o telnet):

   ```
   wget http://192.168.1.144:8001/index.html index.new
   rm index.html
   mv index.new index.html
   wget http://192.168.1.144:8001/manual.html manual.html
   ls
   ```

   Para `index.html` conviene subirlo como `index.new` y luego reemplazarlo:
   mientras `wget` reescribe el fichero que el `httpd` está sirviendo, la FAT
   lo desenlaza y las peticiones pueden recibir un 404 transitorio. El `mv` de
   Xinu copia y borra, así que la ventana existe igualmente; hazlo cuando no
   estés usando el dashboard.

3. Comprueba el tamaño y el `md5` contra los ficheros del host. Abre
   `http://192.168.1.137/index.html`.

> El `httpd` sirve desde la raíz de la SD: los ficheros deben quedar como
> `/index.html` y `/manual.html`.

## Notas

- `wget` sobrescribe el fichero existente; no hace falta borrarlo antes.
- Si el `telnetserver` se queda sin aceptar tras cortar una transferencia a
  medias, usa la consola serie y vuelve a lanzar `telnetserver -n 1`.
- Al cambiar pruebas o pendientes, actualiza `index.html`, vuelve a subirlo y
  deja el mismo cambio en `docs/MANUAL-KERNEL.md` si afecta al manual.
