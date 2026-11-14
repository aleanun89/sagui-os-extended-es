# 032 - MicroPython: biblioteca estándar, archivos, unicode y red

- **Estado:** hecha (fases 1 y 2 aquí; la fase 3, el módulo `sagui`, en la mejora 033)
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

El `python` de SaguiOS (mejora 004, D034, D038) solo hacía cuentas: no abría archivos, no
importaba módulos `.py`, no tenía `os`, `json` ni `re`, trataba el texto como bytes Latin-1
(`len("ñ") == 2` si venía de un archivo UTF-8) y no podía usar la red que ya tienen `curl`,
`wget` y Cipó. Con 768 KB de heap tampoco cabía nada grande. Para escribir programas útiles
dentro del propio sistema hace falta la biblioteca estándar de siempre.

## Propuesta (aplicada)

Detalle en `docs/decisoes.md`, D067 (archivos, `os`, `import`, biblioteca y memoria), D068
(texto: UTF-8 dentro, Latin-1 fuera) y D069 (`socket`, `ssl` y `requests`).

**Fase 1: lo básico y la biblioteca estándar**
- `open()` con los modos r/w/a/x, `b` y `+`, `with`, lectura por líneas e iteración,
  `seek`/`tell`; `io.StringIO`/`io.BytesIO`; `sys.stdin`/`stdout`/`stderr` como objetos de
  archivo (con `.buffer`), y `print(..., file=sys.stderr)` sale por el descriptor 2.
  Sin el VFS de MicroPython: objetos de archivo propios sobre las llamadas de SaguiOS
  (`arquivos_sagui.c`).
- `os` propio (`os_sagui.c`): `listdir`, `ilistdir`, `mkdir`, `makedirs`, `rmdir`,
  `remove`/`unlink`, `rename`, `getcwd`, `chdir`, `stat` (con `st_size`...), `environ`,
  `getenv`/`putenv`/`unsetenv`, `system` (con `sagsh -c`), `urandom`, `getpid`, `uname`,
  `sep` y `os.path` (`join`, `exists`, `isdir`, `isfile`, `basename`, `dirname`, `split`,
  `splitext`, `abspath`, `normpath`, `isabs`, `getsize`, `expanduser`).
- `import` de archivos `.py` y paquetes con `__init__.py`; `sys.path` = [carpeta del script
  (o `''`), `PYTHONPATH`..., `/lib/python`].
- Nivel `MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES` y, del `extmod` de MicroPython 1.24.1
  (copiados sin cambios), `json`, `re`, `random` (semilla del núcleo), `heapq`, `binascii`,
  `select` y `time`; `struct`, `collections`, `errno`, `array` del núcleo; `hashlib` propio
  (`md5`, `sha1`, `sha256`, `sha384`, `sha512`); `time` completo (`time()` con fracción,
  `localtime`/`gmtime`/`mktime` con el reloj de la placa, `strftime`, `monotonic`, `ticks_*`).
  `collections.defaultdict` en `/lib/python/collections.py`.
- `str` unicode: UTF-8 dentro de Python; el terminal, la entrada, los argumentos y los
  archivos en modo texto se convierten de/a Latin-1.
- Heap: empieza con 4 MB y crece solo hasta 256 MB; `-X heap=16M` o `PYTHONHEAP=16M` fijan un
  tamaño. `python -h` muestra la ayuda.

**Fase 2: red**
- `socket` (`rede_sagui.c`) sobre los sockets de SaguiOS: TCP cliente y servidor, UDP,
  `settimeout`/`setblocking`, `makefile`, `getaddrinfo`/`gethostbyname` con el DNS del
  sistema, `socket.timeout` y `socket.gaierror`; los sockets entienden `select`.
- `ssl` (`ssl_sagui.c` + `ponte_sagui.c`) sobre la libtls: `ssl.wrap_socket`,
  `SSLContext`, `create_default_context`, `load_verify_locations(cafile=, cadata=)`,
  `check_hostname`, `verify_mode`; valida con `/etc/ssl/certs` por defecto, y
  `cert_reqs=ssl.CERT_NONE` lo desactiva. Errores en español (`SSLCertVerificationError`).
- `requests` y `urllib.parse` en Python, en `/lib/python`: get/post/put/patch/delete/head con
  `headers`, `params`, `data`, `json`, `timeout`, `verify`, `auth`; respuesta con
  `status_code`, `headers`, `text`, `content`, `json()`, `ok`, `raise_for_status()`;
  redirecciones, chunked, http y https.

**Ejemplos:** `/ejemplos/archivos.py`, `/ejemplos/red.py` (servidor web) y
`/ejemplos/json_http.py`; módulo de ejemplo `/lib/python/saludos.py`.

## Criterios de aceptación

- [x] Archivos (escribir, leer, añadir, binario, `with`), `os` y `os.path`
- [x] `import` de un módulo propio, de un paquete y de `/lib/python`
- [x] json, re, random, struct, collections, hashlib (comparado con el del anfitrión),
      binascii y time
- [x] Unicode: acentos y ñ en el terminal (salida, `input()`, REPL) y en archivos
- [x] Heap grande (listas de varios MB) y `os.system`
- [x] TCP cliente y servidor, UDP y DNS; `requests` con JSON; HTTPS con la CA de prueba
      (falla sin ella; funciona con `verify=`/`cadata`/`cafile` y tras `certificados añadir`)
- [x] Tests automáticos: `tests/h61_python_stdlib.py` y `tests/h62_python_red.py`
- [x] Decisiones D067-D069 en `docs/decisoes.md`; licencias de lo copiado en
      `userland/micropython/LICENCA-MICROPYTHON.txt`
- [x] Fase 3: módulo `sagui` (ventanas, dibujo, eventos, sonido): hecha en la
      [mejora 033](033-python-sagui-tkinter.md), con tkinter encima (D083, D084)
- [ ] Probado en el G470

## Cifras

- `/bin/python`: 164.268 -> 287.468 bytes (+123 KB): ~15 KB los módulos del `extmod`, ~25 KB
  el pegamento propio (archivos, `os`, `socket`, `ssl`, `hashlib`), ~55 KB la libtls (la misma
  de `curl`), el resto el nivel EXTRA, unicode, los nombres (qstr) nuevos y `select`.
  El nivel EXTRA cuesta ~8 KB más que el BASIC con los mismos módulos activados uno a uno.
- `/lib/python`: 25 KB (`requests` 15 KB, `urllib/parse` 8 KB, `collections`, `saludos`).
- Imagen: 2.223.104 -> 2.391.552 bytes útiles (35,3 % -> 38,0 % de la meta de 6 MB).
- Velocidad: igual que antes (bucle de 300.000 `float` en 260-270 ms; Mandelbrot 110-160 ms).

## Diferencias conocidas con CPython

- `repr()` de un `str` escapa lo que no es ASCII (`'l\xednea'`); `print()` lo muestra bien.
- `str.upper()`/`lower()` solo cambian letras ASCII (`'ñ'.upper() == 'ñ'`).
- `re` no tiene `re.split` a nivel de módulo (sí `re.compile(...).split`); sin `crc32` en
  `binascii`; `deque` necesita `maxlen`; `os.stat` solo da tipo y tamaño.
- `requests.ConnectTimeout` no es también `ConnectionError` (MicroPython no permite heredar de
  dos excepciones nativas); sin cookies de sesión ni compresión.
- `ssl` es solo cliente; `ssl.wrap_socket` valida por defecto (en CPython no).
- Sin loopback en la pila de red: `127.0.0.1` no llega a ningún sitio.

## Fase 3 (hecha en la mejora 033): módulo `sagui`

Ventanas, dibujo, eventos, imágenes y `bip` desde Python: `userland/micropython/janela_sagui.c`
(D083), y encima un tkinter escrito en Python en `/lib/python/tkinter` (D084). Ver
[033-python-sagui-tkinter.md](033-python-sagui-tkinter.md) y `/ayuda/python-tk.md`.
