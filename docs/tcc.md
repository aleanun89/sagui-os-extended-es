# TCC dentro de SaguiOS (D384, mejora 142)

`/apps/bin/tcc` es **TCC 0.9.28rc** (Tiny C Compiler, rama `mob` del repositorio oficial
<https://repo.or.cz/tinycc.git>, commit `f830a169` del 2026-11-13, bajado del espejo de GitHub con SHA-256),
licencia **LGPL-2.1-or-later** (`lib/libtcc1.c`: GPL-2.0-or-later con excepción de enlace). La última etiqueta, 0.9.27,
es de 2017; desde entonces el proyecto solo publica la rama `mob` (versión «0.9.28rc» desde 2023), que es lo que
empaquetan las distribuciones.

Guía de uso: [`rootfs/ayuda/tcc.md`](../rootfs/ayuda/tcc.md) (`/ayuda/tcc.md`); modificar AoEN:
[`aoen-modificar.md`](aoen-modificar.md).

## Cómo se construye

```
tools/tcc_fuentes.sh        # fuentes (no van a git) en userland/terceros/tcc/fuentes, con los parches aplicados
make                        # /apps/bin/tcc, /apps/tcc y /apps/aoen/fuentes (TCC=0 lo deja fuera)
```

En git: `userland/terceros/tcc/` (`tcc.mk`, `parches/`, `plataforma/`, `ejemplos/`, `aoen/`, `leeme.txt`,
`autocompilar.sh`) y `tools/tcc_fuentes.sh`, `tools/tcc_simbolos.py`. Sin fuentes, `make` avisa y sigue.

Se construyen **dos compiladores** de las mismas fuentes y con la misma configuración (`plataforma/config.h`,
`CONFIG_SAGUIOS`):

| | Qué es | Para qué |
|---|---|---|
| `build/tcc/anfitrion/i386-tcc` | TCC del PC (gcc del anfitrión), cruzado a SaguiOS | compila `libtcc1.a`, `bcheck.o`, `bt-*.o`, `runmain.o` (como el `lib/Makefile` de TCC) y la parte «pc» de h392 |
| `/apps/bin/tcc` | TCC compilado con gcc contra la libc común y la x87 (`-O2`) | el compilador de dentro |

Parches (`userland/terceros/tcc/parches`):
- `0001-saguios.patch`: los programas a `0x80000000` (`ELF_START_ADDR`, D049: el kernel no carga nada más abajo);
  enlace estático por defecto; `__saguios__` predefinido (además de `__linux__`, como el gcc con el que se compila
  SaguiOS); `-run` y libtcc con la libc del propio programa (ver abajo) y `principal()` como `main()`.
- `0002-bcheck-saguios.patch`: `-b` en SaguiOS (ver abajo).

Capa propia (`plataforma/`, GPL-3.0-only): `config.h` (rutas `/apps/tcc`, `CONFIG_TCC_STATIC`, sin hilos, trazas y
`-b` sí, `tccdefs.h` leído de `/apps/tcc/include` en tiempo de ejecución, `CONFIG_RUNMEM_RO=1`: W^X en `-run`),
`tcc_libc_extra.c` (`ldexpl` y `strtold`, que la libc común no tiene), `principal_main.c` (`principal()` que llama a
`main()` y sale con `exit()`, que vacía stdio), `tcc_sagui_run.c` (la tabla de símbolos), `include/stdint.h` (TCC
no trae uno; con gcc se usa el suyo) y `include/sys/ucontext.h` (lo mínimo para que compile el código de trazas).

## Enlace: estático (decisión)

Los programas que compila TCC son **ELF32 ET_EXEC estáticos a 0x80000000**, como los de `/bin`. Se descartó el
enlace dinámico de D378 (`ld-sagui.so.1`, `libc.so.1`...) para esta versión porque:
- es más **robusto**: no depende del ABI de las `.so` (que puede cambiar de soname) ni de cómo genera TCC la PLT
  (enlace perezoso, `DT_HASH`, relocaciones `R_386_COPY` de datos) frente a lo que hoy admite `ld-sagui`;
- la **SDL 2** y sus capas no existen como `.so` (los ports la enlazan estática) y AoEN la necesita;
- un `hola` ocupa 125 KB y AoEN 800 KB: aceptable en `/apps`.

`/apps/tcc/lib`:

| Fichero | Contenido |
|---|---|
| `crt1.o` | `inicio.asm` de la libsagui (`_inicio`, en `.text`: NASM deja `.text.inicio` sin permiso de ejecución) más `_start` |
| `crti.o`, `crtn.o` | vacíos (TCC los pide) |
| `libc.a` | la libc común, su libm (x87) y la libsagui **entera**, más `principal_main.o`: `tcc hola.c` no necesita `-l` |
| `libm.a`, `libsagui.a`, `libpthread.a`, `libdl.a` | archivos con un objeto vacío: `-lm` y compañía se aceptan |
| `libSDL2.a` (+ `_image`, `_ttf`, `_mixer`) | la SDL 2 de SaguiOS (`sdl2_sagui.o`, `sdl2_rwops.o`) y sus capas (D374) |
| `libtcc1.a`, `runmain.o` | el soporte de TCC (división de 64 bits, `alloca`...) y el arranque de `-run` |
| `bcheck.o`, `bcheck_run.o`, `bt-exe.o`, `bt-log.o` | `-b` y sus trazas |
| `libtcc.a` | TCC como biblioteca (con la tabla de símbolos) |

`/apps/tcc/include`: las de TCC (`stddef.h`, `stdarg.h`, `float.h`, `tccdefs.h`...), las de la libc común, `stdint.h`,
`libtcc.h`; `include/sagui` (userland/inc), `include/kernel` (las cuatro que piden: `abi.h`, `fuso.h`, `sha256.h`,
`tipos.h`) y `include/SDL2` (también `SDL2/SDL.h`). El orden de búsqueda es el de gcc al compilar los ports: libc,
userland/inc, kernel/inc. Un cambio en `sagui.h`: `NULL` con `#ifndef` (TCC avisa si se redefine distinto).

## Licencia de lo que compilas

Un programa enlazado con `crt1.o` o `libc.a` de `/apps/tcc/lib` lleva dentro código de la libsagui (`inicio.asm`,
`chamadas.c`, `memoria.c`...) y de la libc común, que son **GPL-3.0-only**; lo mismo con la SDL 2 de SaguiOS
(`libSDL2*.a`). Por eso **todo programa que distribuyas enlazado así queda bajo la GPL-3.0-only**: un programa
GPL-2.0-only o propietario no puede distribuirse enlazado con ellas (para uso propio, sin distribuirlo, no hay límite).
Lo de TCC no añade restricciones: `libtcc1.a` (GPL-2.0-or-later) lleva una excepción de enlace, y `bcheck.o` (`-b`),
`bt-*.o` y `libtcc.a` son LGPL-2.1-or-later. La mejora futura 143 (una libsagui con licencia MIT) lo resolverá.

## `-run` y libtcc

En Linux lo que se compila en memoria usa la libc del propio tcc con `dlsym()`. Aquí el tcc es estático: al
compilarlo, `tools/tcc_simbolos.py` saca con `nm` los símbolos globales de la libc, su libm y la base de la libsagui
(los objetos de `libsagui.so.1`) y genera `tcc_simbolos.c` (`{"printf", &printf}`, con etiquetas de ensamblador para
no chocar con ninguna cabecera; 845 símbolos). En `tcc_relocate`, `tcc_sagui_antes_de_run` los define **débiles y
absolutos** antes de añadir `libc.a`: así no se carga una segunda copia de `printf` o `stdout` (con su propio búfer,
que nadie vaciaría) y lo que el programa defina manda. Lo que no está en la tabla (SDL, la parte «medios» de la
libsagui) sale de los `.a` y lo que pide se resuelve con la libc del programa. Si no hay `main` pero sí
`principal`, `main` es `principal`.

Con libtcc es igual: `libtcc.a` lleva la tabla, así que el código compilado en memoria por un programa usa la libc de
ese programa (ejemplo `libtcc_hola.c`).

## `-b`: comprobación de límites

`bcheck.c` de TCC está escrito para Linux (hilos, `dlsym(RTLD_NEXT)` para envolver `malloc`, TLS, `syscall`) y los
BSD. El parche 0002 añade una rama `__saguios__`: sin hilos ni señales ni TLS; `malloc`, `calloc`, `free` y `realloc`
se redirigen sin `dlsym` a las de la libsagui (`aloca`, `aloca_zerado`, `libera`, `realoca`), que son las que usa el
`malloc` de la libc común. Un puntero que no salió del `malloc` de `-b` (de la libsagui, por ejemplo) se libera con
`libera`, sin aviso. En Linux un acceso fuera de límites devuelve un puntero inválido y el `SIGSEGV` termina el
programa; SaguiOS no entrega esa señal al programa (`signal.h` de la libc común), así que se sale con `exit(255)` tras
el mensaje. `-b` necesita las trazas de TCC (`CONFIG_TCC_BACKTRACE`, `bt-exe.o`, `bt-log.o`) para escribir el fichero
y la línea: están activadas por eso. Lo que sigue sin servir es la traza de un fallo del procesador (división por cero,
puntero nulo): el kernel termina el proceso con su mensaje de siempre.

Límites: un programa con `-b` no debe usar hilos; `posix_memalign`/`aligned_alloc` no están envueltas (si un programa
las usa, `malloc` queda definido dos veces y el enlace falla).

## Autocompilación

`/apps/tcc/fuentes` lleva los fuentes de TCC ya parcheados (solo i386), `config.h`, `tcc_sagui_run.c`,
`tcc_libc_extra.c`, `tcc_simbolos.c` y `compilar.sh`: `tcc -DONE_SOURCE=1 -I. -o tcc tcc.c tcc_libc_extra.c
tcc_simbolos.c`. El tcc resultante (730 KB, sin optimizar) compila y ejecuta igual (h392).

## Tiempos (h392, QEMU, 512 MB)

| | KVM (Ryzen 9 9900X) | TCG |
|---|---|---|
| AoEN entero (20 ficheros, 12 000 líneas + stb_image) | 0,03-0,04 s | 0,39-0,41 s |
| TCC compilándose a sí mismo | 0,03 s | 0,30 s |

Medido con `time sh compilar.sh` dentro de SaguiOS (el `time` de SaguiOS tiene resolución de 10 ms).

## Pendiente

- `-bt` como opción propia (trazas de fallos: SaguiOS no entrega señales del procesador al programa) y los
  compiladores cruzados (`i386-win32-tcc`...): no en esta versión.
- Enlace dinámico contra `libc.so.1`/`libsagui.so.1` (D378) cuando esas bibliotecas lleven la SDL y TCC genere lo
  que `ld-sagui` acepta.
- Mods de AoEN con libtcc y una aplicación «laboratorio de C» (mejora 142, fase posterior).
- `make` dentro de SaguiOS (hoy los proyectos usan un guion de sagsh, como `compilar.sh`).
