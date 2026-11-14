# Carencias del SaguiOS vistas al portar NetSurf (D184)

NetSurf 3.11 (el navegador de https://www.netsurf-browser.org/, unas 200.000 líneas de C con sus
bibliotecas) se ha compilado para el SaguiOS como **un programa de terceros que no se toca**: todo el
código del upstream está tal cual (bajado y comprobado por `tools/netsurf_fuentes.sh`) y lo que falta
lo da una capa nuestra en `userland/terceros/netsurf/` (`libc/` y `plataforma/`). Este documento es lo
que esa capa tuvo que poner: **lo que un programa grande de C escrito para POSIX espera y el SaguiOS
todavía no da**.

**Hasta dónde llegó:** compila, enlaza (`/bin/netsurf`, 1,6 MB), **arranca**, **dibuja páginas locales**
(`file:`, HTML + CSS externo + PNG + JPEG) y **navega por la red** (`http:` y `https:` con un fetcher
propio sobre la HTTP y el TLS de la libsagui), con ratón, rueda, teclado y enlaces. Prueba:
`tests/h153_netsurf.py`. Lo que no va: JavaScript (NetSurf usa Duktape; no se compiló), SVG, WebP,
caracteres no ASCII escritos con el teclado, transparencia de los PNG y descargas sin bloquear la
ventana.

## Lo que ya se ha resuelto en el sistema (v7.2, mejora 094)

La tabla de abajo es la foto de cuando se portó NetSurf (D184). Desde entonces:

- **Punto 1 (libc) — D210:** la `libc/` de NetSurf es ahora la **libc común** del sistema, en
  `userland/libc/` (los nombres `libc/X` de la tabla son hoy `userland/libc/X`). La usan NetSurf, el
  OpenTyrian (sin su `libc_ot.c`), el SQLite (sin `sqlite_libc.c` ni sus cabeceras mínimas) y el
  MicroPython (sin `libc_sagui.c`), y desde la v7.3 también **QuickJS** (`qjs`, el Cipó; sin
  `qjs_port.c` ni sus cabeceras) y **GNU Prolog** (sin `libc_gp.c`/`numeros_gp.c`). Ya no queda
  ningún port con libc propia.
- **Puntos 6 y 7 (printf y strtod exactos) — D210:** `userland/libc/numeros.c`: `strtod`/`strtof`
  con **redondeo correcto** (el doble más cercano siempre, al par en los empates; también
  hexadecimales, `inf`/`nan` y `errno = ERANGE` como la glibc) y `%e`/`%f`/`%g` con las **cifras
  exactas** del doble redondeadas como la glibc (`%.17g` y vuelta da el mismo doble). Enteros
  grandes, sin coma flotante. `tests/h210_numeros_libc.py` lo compara con la glibc del PC (millones
  de dobles al azar y los casos difíciles: puntos medios con más de 800 cifras, subnormales,
  2^-1074, 1e23, 0.1). `sscanf("%f")` a un `float` usa `strtof` (sin doble redondeo) y `%Lf` lee el
  `long double`.
- **Puntos 23, 34 y 35 (señales, salida, procesos) — D210:** `sigaction` y las máscaras (guardan, no
  hacen nada), `kill`, `waitpid`/`system`/`pipe` sobre `executar`/`esperar`, `fork`/`exec` con
  `ENOSYS`, `setjmp`/`longjmp` (`saltos.c`), `getrusage`/`times`, `pwd.h`; `exit()` y `fflush(NULL)`
  vacían **todos** los `FILE` abiertos (antes solo `stdout`/`stderr`), y leer de una tubería sin
  escritor es fin de fichero, no error. Todo lo pedía GNU Prolog.
- **Punto 3 (malloc) — D209:** el `aloca`/`libera` de la libsagui es por clases de tamaño (O(1)) y
  **devuelve al sistema** la memoria que se libera (llamada `SIS_MEMORIA`). La `libc/memoria.c` de
  NetSurf ya no tiene reservador propio: envuelve el de la libsagui.
- **Punto 9 (división de 64 bits):** en la libsagui, una sola copia (mejora 094, v7.1); la de
  `libc/div64.c` se ha quitado.
- **Punto 13 (mmap) — D209:** `mmap` anónimo de verdad (páginas propias que `munmap` devuelve, también
  por trozos) y `mmap` de fichero como copia privada leída al mapear (sin páginas compartidas con la
  caché del disco: `MAP_SHARED` con escritura no se puede).

## Cómo leer la tabla

- **Estado en el SaguiOS**: *no existe* / *parcial* (hay algo, con otra interfaz o incompleto) /
  *existe* (con la interfaz de la C o casi).
- **Cómo se cubrió**: *implementación* (código de verdad en la capa, utilizable por otros), *emulación*
  (funciona para lo que pide NetSurf, no en general), *stub* (no hace nada útil), *sustitución* (otro
  componente del SaguiOS en su lugar).
- **Prioridad** para añadirlo al sistema de verdad (no al port): **alta** = lo va a pedir cualquier
  programa de terceros grande y hoy cada port lo reescribe; **media** = lo piden muchos; **baja** =
  específico o fácil de emular.
- Ficheros: `libc/X` y `plataforma/X` son de `userland/terceros/netsurf/`.

## La tabla

| # | Componente | Quién lo pide en NetSurf | Estado en el SaguiOS | Cómo se cubrió en el port | Prioridad |
| --- | --- | --- | --- | --- | --- |
| 1 | **Una libc** (las cabeceras estándar `stdio.h`, `stdlib.h`, `string.h`, `unistd.h`... y su biblioteca) | todo | **no existe**: la libsagui tiene su propia API en portugués (`tam_texto`, `abrir`, `ler`, `aloca`) y solo `memcpy`/`memset`/`memmove`/`memcmp` con nombre de C. Cada port lleva su libc: MicroPython (`libc_sagui.c`), SQLite (`terceros/sqlite/libc`, `sqlite_libc.c`), OpenTyrian (`libc_ot.c`, 1.100 líneas) y ahora NetSurf (`libc/`, 34 cabeceras y ~3.300 líneas) | implementación (`libc/*.h`, `libc/*.c`) | **alta** |
| 2 | **Un entorno de compilación cruzada** (sysroot con cabeceras, `pkg-config`) | el sistema de construcción del upstream | **no existe**. Y una trampa: `~/.local/saguitools/env.sh` exporta `CPATH` a las cabeceras del anfitrión, y `CPATH` **vale aunque se compile con `-nostdinc`**: una cabecera que falte en la libc del port se coge de la glibc sin aviso. Al principio NetSurf «compilaba» entero así | el `netsurf.mk` compila con `env -u CPATH -u C_INCLUDE_PATH`. **El OpenTyrian tiene el mismo riesgo** (no comprobado si le afecta) | **alta** (el `CPATH`), media (el sysroot) |
| 3 | `malloc`/`free`/`realloc`/`calloc` rápidos | libcss, libdom, hubbub: decenas de miles de reservas pequeñas por página | **parcial**: `aloca`/`libera` de la libsagui es una lista con búsqueda «el primero que quepa» **desde el principio** (O(n) por reserva; O(n²) al cargar una página) | implementación: reservador por clases de tamaño (O(1)) sobre trozos de 256 KB de la libsagui (`libc/memoria.c`) | **alta** |
| 4 | **Pila de más de 64 KB** | el layout (recursivo por la profundidad del árbol de cajas), el CSS, el DOM | **no**: 64 KB fijos (`PILHA_USUARIO_TAM`), sin crecimiento bajo demanda | `main()` corre sobre una pila de 4 MB del heap (`plataforma/principal_ns.c`), el truco de la D183 del OpenTyrian | **alta**: pila que crezca bajo demanda (o tamaño en la cabecera del ELF) |
| 5 | `stdio` con búferes: `FILE`, `fopen` (modos `r`/`w`/`a`/`+`), `fdopen`, `fileno`, `fgets`, `getline`, `fseek`/`ftell`, `setbuf`, `tmpfile`, `stdin`/`stdout`/`stderr` | opciones (`Choices`), cookies, historial, mensajes, la caché | **no existe** (descriptores sin búfer: `abrir`/`ler`/`escrever`/`posicionar`) | implementación (`libc/es.c`, sobre la de OpenTyrian, con búfer de escritura) | **alta** (parte de la libc) |
| 6 | `printf` completo: `%f %e %g`, `%lld`/`%llu` (64 bits), `%zu`, `%*.*s`, `%#x`; `vasprintf`; `sscanf` con `%[...]`, `%f`/`%lf`, anchos | registros, CSS (`%f`), `about:` | **parcial**: `formatar_texto` de la libsagui solo `%s %c %d %u %x %p`, sin reales ni 64 bits | implementación (`libc/es.c`) | **alta** |
| 7 | Números: `strtod`/`strtof`, `strtol`/`strtoll`/`strtoull` con `errno=ERANGE`, `qsort` O(n log n), `bsearch` | CSS, HTML (`width="..."`), urldb, las tablas | **parcial**: `texto_para_int`, `ordenar_ponteiros` (solo punteros) | implementación (`libc/texto.c`; `qsort` es heapsort: el de OpenTyrian era por inserción, cuadrático). `strtod` sin redondeo exacto | **alta** (libc) |
| 8 | `errno` y los códigos `E*` | todo lo de ficheros | **no existe** (las llamadas devuelven `ERRO_*` negativos) | implementación: un `errno` global y la traducción `ERRO_*` → `E*` (`libc/posix.c`). Con hilos haría falta uno por hilo | **alta** (libc); media el `errno` por hilo |
| 9 | La división de 64 bits (`__divdi3`, `__udivdi3`, `__moddi3`...) | el compilador, en cuanto hay `uint64_t` (tiempos, tamaños) | **no**: el userland no enlaza la libgcc; cada port copia sus ayudantes (`sqlite_libc.c` → OpenTyrian → NetSurf) | copia (`libc/div64.c`) | **media**: ponerlos una vez en la libsagui |
| 10 | Constructores globales (`__attribute__((constructor))`, `.init_array`) | libnsfb registra así sus superficies | **no**: `inicio.asm` salta a `principal` sin llamarlos y `programa.ld` no conserva `.init_array` | guion de enlace propio (`plataforma/netsurf.ld`) y bucle en `principal_ns.c` | **media** (C++ y muchas bibliotecas los usan) |
| 11 | `stat`/`fstat`/`lstat`/`fstatat` con `st_mode`, `st_size`, `st_mtime` | el fetcher `file:`, la caché, los listados de carpeta | **parcial**: `info_arquivo` (tipo y tamaño) y `datas_arquivo` (fechas) por separado; **sin `fstat` por descriptor**, sin inodo ni permisos | emulación (`libc/posix.c`): `fstat` solo sabe el tamaño (posicionar al final); el inodo es un resumen de la ruta | media |
| 12 | `opendir`/`readdir`/`scandir`/`dirfd`, `unlinkat` | el fetcher `file:` (listados), borrar la caché | **parcial**: `listar(ruta, índice)`; sin descriptores de carpeta | emulación: `dirfd` da números que solo entienden `fstatat`/`unlinkat` de esta libc | baja |
| 13 | `mmap` de un fichero | el fetcher `file:` | **no existe** | emulación: reserva y lee el fichero entero; solo lectura (`libc/posix.c`) | media (los ficheros grandes se copian enteros) |
| 14 | `pread`/`pwrite`, `open` con `O_EXCL`, `ftruncate`, `fsync` | libnsutils, la caché en disco | **parcial**: `posicionar`, `truncar`, `sincronizar`; sin `pread` atómico | emulación (posicionar + leer + volver) | baja |
| 15 | Tiempo: `time`, `gettimeofday`, `clock_gettime(CLOCK_MONOTONIC)`, `nanosleep` | el planificador de NetSurf (`schedule`), la caché, las cookies | **parcial**: `hora_utc` (segundos y ms) y `tempo_ms` (ms de **32 bits**: vuelve a 0 cada 49,7 días) | implementación (`libc/tempo.c`; el monótono se extiende a 64 bits mirando la vuelta) | media: un reloj monótono de 64 bits en el kernel |
| 16 | `time_t` de 64 bits | fechas de cookies y cabeceras HTTP | **no**: los segundos son `unsigned` de 32 bits | `time_t` es `long` de 32 bits (2038) | baja (hoy), alta antes de 2038 |
| 17 | `gmtime`/`localtime`/`mktime`/`timegm`/`strftime`/`strptime` | fechas HTTP, listados `file:` | **parcial**: el huso horario es bueno (D080, `fuso.h`), pero con API propia (`formatar_data`) | implementación sobre `fuso_*` | media (libc) |
| 18 | `setlocale`/`localeconv` | `netsurf.c` (`setlocale(LC_ALL, "")`) | **no existe** | stub: solo el locale `"C"` (que es lo que NetSurf necesita para el CSS) | baja |
| 19 | `iconv` | convertir texto entre UTF-8 y otras codificaciones (formularios, nombres) | **no existe** (solo `utf8_para_latin1`) | implementación mínima: UTF-8, Latin-1, Windows-1252, ASCII, UTF-16 y UTF-32 (`libc/iconv.c`). Las páginas no pasan por aquí: libparserutils trae sus propios decodificadores | media |
| 20 | `regcomp`/`regexec` (POSIX) | guardar una página completa (`save_complete.c`) | **parcial**: `er_compilar`/`er_buscar` de la libsagui (D097), sin subexpresiones | envoltorio (`libc/regex.c`); solo `pmatch[0]` | baja |
| 21 | **zlib** (`gzopen`/`gzgets`, `inflate` por trozos) | los ficheros de mensajes, los mensajes incluidos en el binario; en un navegador normal, el `Content-Encoding: gzip` | **parcial**: `inflar`/`gzip_descomprimir` de la libsagui (D103), de una vez y en memoria | emulación (`libc/zlib.c`): `gz*` de solo lectura; `inflate` descomprime todo en la primera llamada. El fetcher no pide gzip | media (sin ella, cada página baja sin comprimir) |
| 22 | `getopt`/`getopt_long` | `main` del frontend | **parcial**: `obter_opcao` (API propia) | implementación (`libc/posix.c`) | baja |
| 23 | Señales (`signal`, `raise`) | `signal(SIGPIPE, SIG_IGN)` | **no existen** | stub: se guarda el tratador; `raise` lo llama | baja (para NetSurf) |
| 24 | `select`/`poll` sobre varios descriptores y E/S sin bloqueo | **cualquier cliente de red con interfaz**: el fetcher de curl de NetSurf espera en `select` | **no existe**: `disponivel(fd)` (uno a uno) y `esperar_rede(visto, ms)` (cualquier socket), sin `O_NONBLOCK` en `conectar` | `select` es un stub que solo duerme; el fetcher propio **bloquea** la ventana mientras baja cada recurso | **alta** para programas de red |
| 25 | **Sockets BSD** (`socket`, `connect`, `getaddrinfo`, `struct sockaddr_in`) | libcurl (no se compiló); `urldb` solo usa `inet_aton`/`inet_pton` | **parcial**: API propia (`criar_socket`, `conectar(fd, ip, porta)`, `resolver`) | solo las cabeceras y `inet_*` (`libc/arpa/inet.h`, `libc/posix.c`) | media (sin ellos, ningún programa de red de terceros compila sin reescribir su red) |
| 26 | **libcurl** (HTTP/1.1, HTTPS, redirecciones, cookies, gzip, conexiones persistentes) | todos los `http:`/`https:` | **no existe**; hay un cliente HTTP (`http.c`) y TLS 1.2/1.3 con validación de certificados (`libtls`, D062) | **sustitución**: fetcher propio (`plataforma/fetch_http.c`) con la interfaz de los fetchers de NetSurf; un `curl/curl.h` vacío porque `content/fetch.c` lo incluye siempre | media (una libcurl, o un shim de su API sobre `http.c`+`libtls`, abriría muchos ports) |
| 27 | **libpng / libjpeg** | las imágenes | **parcial**: la libsagui decodifica PNG, JPEG, BMP y PNM (`imagem_decodificar`), con API propia y **sin canal alfa** | **sustitución**: manejador de contenido propio (`plataforma/imagem_sagui.c`) para `image/png` y `image/jpeg` | media (alfa en la libsagui; una API compatible con libpng ayudaría a otros ports) |
| 28 | Fuentes vectoriales (FreeType) | el texto de las páginas | **no existe** como biblioteca (el motor web del Cipó tiene sus fuentes, D141) | la fuente de mapa de bits interna del frontend framebuffer (`font_internal`, una sola fuente, sin tamaños de verdad) | media |
| 29 | Teclado con **códigos de tecla** y tecla soltada | libnsfb (todo el frontend framebuffer trabaja con teclas físicas de un teclado de EE. UU. y Mayús/Ctrl como teclas aparte) | **parcial**: los eventos traen el **carácter ya traducido** (Latin-1) y no hay evento de tecla soltada (`teclas_seguradas` da el mapa) | `plataforma/superficie_sagui.c` convierte cada carácter en «Mayús abajo, tecla, Mayús arriba». **Los caracteres que no están en el teclado de EE. UU. (ñ, á, ç...) no se pueden escribir** | media (un evento con el código de tecla y el de soltar) |
| 30 | Ventanas mayores de 1000×700 | el tamaño por defecto de NetSurf (800×600 cabe) | **parcial**: `JANELA_LARG_MAX`/`JANELA_ALT_MAX` | se recorta el tamaño pedido | baja |
| 31 | Hilos POSIX (`pthread_*`) | NetSurf no los usa (hay `content/handlers/javascript` y la caché en disco que podrían) | **parcial**: `thread_create`, `mutex_t`, `semaforo_t`, futex (D167), con API propia | no hizo falta | media (un `pthread.h` sobre la API de la D167 es corto) |
| 32 | `dlopen` | no (NetSurf enlaza todo estático) | no existe | — | baja |
| 33 | `getenv`/`setenv`/`unsetenv` | `HOME` (dónde guarda `~/.netsurf`) | **parcial**: `obter_env` y `environ`; sin `setenv` | implementación (`libc/posix.c`); si no hay `HOME` se pone `/home` | baja |
| 34 | `atexit`/`exit` (que vacíe los `FILE`), `abort`, `assert` | todo | **parcial**: `sair` | implementación | baja (libc) |
| 35 | `uname`, `gethostname`, `getpid`, `getuid` | el `User-Agent`, el registro | **parcial**: `meu_pid`; sin `uname` | implementación con valores fijos | baja |
| 36 | `realpath`, `getcwd`, `mkstemp`/`mkdtemp`, `tmpfile` | rutas de recursos y opciones | **parcial**: `pasta_atual`, `mudar_pasta` | implementación (`libc/posix.c`, `libc/es.c`) | baja |
| 37 | Matemática de precisión simple (`sqrtf`, `floorf`, `roundf`, `lround`...) | el layout, libcss | **parcial**: la libm del MicroPython (D038) solo es de doble precisión y no está en la libsagui | implementación sobre las dobles (`libc/matematica.c`) | baja |
| 38 | Herramientas del anfitrión para generar código (`gperf`, libpng del anfitrión, `HTML::Entities` de perl, zlib del anfitrión) | el sistema de construcción del upstream | no están en el entorno local | sustituidas por `tools/netsurf_gerar.py` (tabla de elementos, imágenes de la barra, mensajes, testament) | baja |

## Lo que recomiendo hacer primero en el sistema

1. **Una libc compartida de verdad** (`userland/libc/`, cabeceras estándar + implementación), que
   junte lo que hoy está repetido en cuatro ports: `stdio` con búfer, `printf` con reales y 64 bits,
   `strtod`, `qsort`, `errno`, `stat`, `opendir`, `time`/`strftime`, `getopt`, `assert`, `atexit` y los
   ayudantes de 64 bits de la libgcc. Las de MicroPython, SQLite, OpenTyrian y NetSurf se pueden ir
   pasando a ella. Es lo que más reduce el coste del siguiente port. Esta `libc/` de NetSurf es un buen
   punto de partida (está escrita para ser general, no para NetSurf).
2. **Quitar `CPATH` de `env.sh`** para la compilación del userland (o compilar el userland con
   `env -u CPATH`): con él, un port puede compilar contra la glibc del anfitrión sin que nadie se dé
   cuenta (punto 2 de la tabla).
3. **Pila de usuario que crezca bajo demanda** (o un tamaño de pila en el ELF): hoy cada port grande
   (OpenTyrian, NetSurf) se monta una pila en el heap.
4. **Un reservador de memoria O(1)** en la libsagui (el de `libc/memoria.c` vale): los programas del
   sistema también lo agradecerán cuando crezcan.
5. **E/S de red sin bloqueo con espera en varios descriptores** (`select`/`poll`, o un `esperar_eventos`
   que junte ventana + sockets): sin ella, cualquier navegador o cliente con interfaz se congela mientras
   baja algo. Con ella, el fetcher de NetSurf pasaría a ser incremental.
6. Después: constructores en `inicio.asm`, alfa en los PNG de la libsagui, eventos de teclado con código
   de tecla y tecla soltada, zlib por trozos (para pedir `gzip` al servidor), `pthread.h` sobre la D167,
   y una capa de sockets BSD sobre la API de red.

## Datos del port

- **Versión:** NetSurf 3.11 (`netsurf-all-3.11.tar.gz`, 28-12-2023, 9.080.781 bytes, SHA-256
  `4dea880ff3c2f698bfd62c982b259340f9abcd7f67e6c8eb2b32c61f71644b7b`), de
  https://download.netsurf-browser.org/netsurf/releases/source-full/.
- **Lo que se compila del upstream:** libwapcaplet, libparserutils, libhubbub, libdom (con la
  «binding» de hubbub), libcss, libnsutils, libnsbmp, libnsgif, libutf8proc, libnsfb (sin superficies:
  la nuestra la sustituye) y el núcleo de NetSurf con el frontend framebuffer: 461 + 145 ficheros, más
  31 generados. Apagados: libcurl, OpenSSL, libpng, libjpeg, JPEG XL, WebP, SVG, sprites de RISC OS,
  libnspsl, libnslog y Duktape (JavaScript).
- **Lo nuestro:** `libc/` (34 cabeceras y 10 ficheros .c, ~3.300 líneas), `plataforma/` (superficie de libnsfb,
  arranque, fetcher http/https, imágenes PNG/JPEG, guion de enlace), `netsurf.mk`,
  `tools/netsurf_fuentes.sh` y `tools/netsurf_gerar.py`. **Ningún fichero del upstream modificado.**
- **Tamaño:** `/bin/netsurf` 1,6 MB (con el TLS) + 0,1 MB de recursos en `/usr/share/netsurf`.
