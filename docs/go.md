# Go en SaguiOS (TinyGo, D364)

SaguiOS compila programas en Go con **TinyGo**. Corren como cualquier programa de usuario (ELF de 32 bits, Pentium sin SSE por
defecto) y se instalan en `/apps/go/`.

## Instalar (en el anfitrión, sin sudo)

```bash
tools/tinygo_entorno.sh instalar     # TinyGo 0.44.0 + Go 1.26.11 en ~/.local/saguitools (descarga ~190 MB, SHA-256)
tools/tinygo_entorno.sh estado
```
Es idempotente. Nada va a git. `env.sh` añade `tinygo/bin` y `go/bin` al `PATH` si existen.

## Compilar y ejecutar

```bash
. ~/.local/saguitools/env.sh
tools/go-sagui build -o /tmp/hola ./userland/go/ejemplos/hola     # envoltorio de tinygo con el objetivo de SaguiOS
make                                                              # compila todos los ejemplos a /apps/go/ si TinyGo está instalado
make go-ejemplos                                                  # solo los ejemplos (build/userland/go/bin/)
```
En SaguiOS: `/apps/go/hola`. Sin TinyGo, `make` salta Go con un aviso. `GO=0` lo desactiva. `SAGUI_GO_SSE2=1` genera código con
SSE2 (solo para máquinas con SSE2). Un programa propio es un paquete `main` con su `go.mod`; `go-sagui build` acepta las
opciones de `tinygo build` (`-opt=z`, `-no-debug`...). La primera compilación tarda más (TinyGo compila su biblioteca).

## Cómo funciona

- `userland/go/alvos/saguios.json`: objetivo de TinyGo (`goos=linux`, `goarch=386`, etiqueta `saguios`, planificador de tareas,
  GC conservador, `ld.lld` con `userland/programa.ld`).
- `userland/go/runtime/glue.c`: punto de entrada, lo que falta de la libc (señales, futex, `getrandom`, reloj de 64 bits), la
  reserva del montón y las funciones `sg_*` del paquete `syscall`.
- `userland/go/syscall/`: sustituye al `syscall` de Go (que haría `int 0x80` con números de Linux) en una copia del GOROOT.
- `userland/go/runtime/parchear_tinygo.py`: parche del montón en una copia de TINYGOROOT.
- `build/userland/libgo.a` = libsagui + libc común + `glue.c`.

## Qué funciona

`fmt`, `strings`, `strconv`, `sort`, `bufio`, `math`, `math/rand`, `errors`, `unicode`, `encoding/json`, `os` (argumentos, entorno,
`Stdin/Stdout/Stderr`, `Open/Create/ReadFile/WriteFile/ReadDir/Stat/Mkdir/Remove/Rename`, `os.Exit`), `time` (`Now`, `Sleep`,
`Ticker`, `Timer`, `After`), goroutines, canales, `select`, `sync` (`Mutex`, `WaitGroup`, `Once`).

## Limitaciones

- TinyGo: `reflect` parcial, sin cgo, sin `plugin`, recolector conservador, goroutines cooperativas en un solo hilo (una que no se
  bloquea ni cede acapara la CPU), paquetes grandes de la biblioteca estándar incompletos.
- SaguiOS: reloj monótono de 1 ms; sin señales (`os/signal`), sin `fork`/`os/exec`, sin enlaces simbólicos, permisos aceptados pero
  no aplicados por `chmod`/`chown`; `os.Open` de una carpeta da un descriptor propio (3000+) válido solo para `ReadDir`.
- **Red (`net`): fase 2.** El `net` de TinyGo usa su propio `netdev` y no se ha adaptado a `kernel/rede/socket.c`: no hay ejemplo de TCP todavía.
- Memoria: montón de hasta 256 MB de direcciones, páginas asignadas al crecer.

## Fase 2: Go oficial (`GOOS=saguios`)

El compilador de Go oficial necesita del kernel: **TLS por hilo** (registro de segmento para el puntero de la goroutine actual,
`SIS_HILO_CRIAR` no lo da), **espera de E/S** (poll/epoll o equivalente sobre `SIS_ESPERAR_REDE` y descriptores), **señales**
(preempción asíncrona y `os/signal`), y un puerto de `runtime`/`syscall`/`internal/poll` con los números de `abi.h`. Eso toca el
núcleo (planificador, memoria) y se plantea como una versión propia; hasta entonces, TinyGo.

## Pruebas

`python3 tests/h366_tinygo.py` (se salta con aviso si TinyGo no está instalado): compila los ejemplos, arranca QEMU y comprueba
la salida de cada uno.

## Texto y acentos

Go escribe UTF-8; el terminal de SaguiOS es ISO-8859-1. `glue.c` convierte solo en descriptores de terminal
(`TIPO_TERMINAL`): la salida de UTF-8 a Latin-1 (lo que no cabe sale como `?`; las secuencias partidas entre dos `write`
se completan con el siguiente) y la entrada del teclado de Latin-1 a UTF-8. Ficheros y tuberías pasan byte a byte (los
ficheros quedan en UTF-8). Ejemplo: `userland/go/ejemplos/acentos`, comprobado en `tests/h366_tinygo.py`.
