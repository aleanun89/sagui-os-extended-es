# Compilar SaguiOS-Extended / Build SaguiOS-Extended / Compilar o SaguiOS-Extended

**Idioma / Language / Idioma:** [Español](#espa%C3%B1ol) · [English](#english) · [Português (Brasil)](#portugu%C3%AAs-brasil)

Las tres secciones tienen el mismo contenido. / The three sections have the same content. / As três seções têm o mesmo conteúdo.

---

## Español

Guía para compilar SaguiOS-Extended desde el código del repositorio público y generar tus propias imágenes. Las órdenes (`make`, `./gerar_versao.sh`...) y los nombres de fichero no se traducen. Todo lo que dice esta guía sale de leer el `Makefile`, `gerar_versao.sh`, `run_in_qemu.sh`, `tools/mkimagem.py` y los guiones de `tools/`.

### 1. Requisitos del PC

- **Sistema:** Linux de 64 bits con `apt` (Ubuntu 24.04 o parecido; `gcc-13` es lo que usa el envoltorio de `tools/entorno-local.sh`) o **WSL2** con Ubuntu, que es donde se desarrolla el proyecto. No hace falta compilador cruzado: el `gcc` normal del PC con `-m32` y `-ffreestanding` hace de compilador (decisión D001); por eso se necesita `gcc-multilib`.
- **Paquetes** (Ubuntu/Debian):

```bash
sudo apt install build-essential gcc-multilib g++ nasm binutils make python3 \
    mtools dosfstools e2fsprogs xz-utils curl perl \
    qemu-system-x86 qemu-utils
# opcional, solo para las pruebas automáticas: imagemagick ffmpeg zip sqlite3 exfatprogs
# opcional, para probar el arranque UEFI (D264: h264, run_in_qemu.sh --uefi): ovmf ovmf-ia32
```

**Opcional: programas en Go.** `tools/tinygo_entorno.sh instalar` baja TinyGo y Go a `~/.local/saguitools` (sin sudo); con eso `make` compila además los ejemplos de `userland/go/ejemplos/` a `/apps/go/`. Sin TinyGo se saltan con un aviso (`GO=0` los desactiva). Ver `docs/go.md`.

**Opcional: programas en Java.** `tools/java_entorno.sh instalar` baja un JDK 17 y Janino a `~/.local/saguitools` (sin sudo) y `tools/minijvm_fuentes.sh` baja miniJVM (commit fijo, SHA-256) a `userland/terceros/minijvm/fuentes/`; con eso `make` compila `/apps/bin/java`, `/apps/bin/javac`, la biblioteca de clases en `/apps/java/lib/` y los ejemplos de `userland/java/ejemplos/`. Sin las fuentes o sin el JDK se saltan con un aviso (`JAVA=0` los desactiva). Ver `docs/java.md`.

**Opcional: compilador de C dentro (TCC).** `tools/tcc_fuentes.sh` baja TCC 0.9.28rc (commit fijo, SHA-256; LGPL-2.1 o posterior) a `userland/terceros/tcc/fuentes/` y aplica sus parches; con eso `make` también compila `/apps/bin/tcc`, `/apps/tcc/` (cabeceras, `libc.a`, `libSDL2.a`, `libtcc.a`, ejemplos y los fuentes de TCC para compilarse a sí mismo) y `/apps/aoen/fuentes` (AoEN compilable dentro con `sh compilar.sh`). Sin las fuentes se salta con un aviso (`TCC=0` lo desactiva). Ver `docs/tcc.md`.

**Opcional: GNU make dentro (D413).** `tools/make_fuentes.sh` baja GNU make 4.4.1 (SHA-256 fijo; GPL-3.0-or-later) a `userland/terceros/make/fuentes/`; con eso `make` también compila `/apps/bin/make` y `/apps/make` (léeme, licencia y `ejemplos/proyecto`). Sin las fuentes se salta con un aviso (`MAKE_DENTRO=0` lo desactiva). Va fuera de `SEM_GPL=1`. Ver `docs/make.md`.

**Opcional: Laboratorio de C (D414).** Con las fuentes de TCC, `make` compila también `/apps/bin/labc`, el Laboratorio de C (editor con resaltado, ejecución con libtcc en memoria, errores con su línea y diez ejemplos en `/apps/labc/ejemplos`). No necesita nada más: se incluye desde `tcc.mk`, así que `TCC=0` y `SEM_GPL=1` también lo quitan, y `LABC=0` quita solo él. Ver `rootfs/ayuda/laboratorio-c.md`.

**Opcional: awk y Lua dentro (D440).** `tools/awk_fuentes.sh` baja The One True AWK 20261106 (SHA-256 fijo; licencia permisiva de Lucent) a `userland/terceros/awk/fuentes/`; con eso `make` instala `/bin/awk`. No hace falta bison: la gramática ya generada va en git. `tools/lua_fuentes.sh` baja Lua 5.4.9 (SHA-256 fijo; MIT) a `userland/terceros/lua/fuentes/`; con eso `make` instala `lua` y `luac` en `/apps/bin`, `liblua.a` para TCC y `liblua.so.5.4`. Sin las fuentes cada una se salta con un aviso (`AWK_DENTRO=0` y `LUA_DENTRO=0` las desactivan). Las dos entran también con `SEM_GPL=1`. Ver `docs/awk-lua.md`.

**Opcional: git dentro (D400).** `tools/git_fuentes.sh` baja libgit2 1.9.1 (etiqueta fija, SHA-256 comprobado) a `userland/terceros/git/fuentes/`; con eso `make` también compila `/apps/bin/git` (init, clone, fetch, pull, push, status, add, commit, log, diff, branch, checkout, remote y config; HTTP(S) con TLS, y SSH). Sin las fuentes se salta con un aviso (`GIT=0` lo desactiva). Va fuera de `SEM_GPL=1`. Ver `docs/git.md`.

| Paquete | Para qué |
| --- | --- |
| `gcc`, `gcc-multilib`, `binutils`, `make` | compilar el kernel y los programas en 32 bits |
| `g++` (con soporte `-m32`) | **opcional**: los programas de C++ (OpenJazz, Command & Conquer, DOSBox). Si no hay, el sistema se construye igual sin ellos |
| `nasm` | cargador de arranque y partes del kernel |
| `python3` | todas las herramientas de `tools/` (`mkimagem.py`, `mksgfs.py`...) |
| `mtools`, `dosfstools` | particiones FAT32 (`mcopy`, `mkfs.vfat`) |
| `e2fsprogs` | particiones ext2 (`mke2fs`) |
| `exfatprogs` (opcional) | pruebas de exFAT (`tests/h325_exfat.py`: `mkfs.exfat`, `fsck.exfat`); sin él esa prueba no se hace. `tools/entorno-local.sh` lo baja sin sudo |
| `xz-utils` | imágenes `.xz` y los datos comprimidos de OpenTyrian |
| `qemu-system-x86`, `qemu-utils` | arrancar el sistema (`qemu-system-i386`) y crear el `.vdi` (`qemu-img`) |
| `ovmf`, `ovmf-ia32` (opcional) | firmware UEFI de QEMU de 64 y 32 bits, para probar el arranque UEFI (`run_in_qemu.sh --uefi`, prueba h264; D264). `tools/entorno-local.sh` los baja sin sudo |
| `curl`, `perl` | solo los guiones de descarga (`curl`) y la preparación de NetSurf (`perl`, ver 7) |

- **Sin permisos de administrador (`sudo`):** `bash tools/entorno-local.sh` descarga con `apt-get download` los paquetes que falten (nasm, soporte de 32 bits de gcc, QEMU, mtools, dosfstools, e2fsprogs, g++...), los extrae en `~/.local/saguitools/root` y crea `~/.local/saguitools/env.sh`. Se usa así, en cada terminal nueva:

```bash
bash tools/entorno-local.sh               # una sola vez
. ~/.local/saguitools/env.sh && make      # cada vez que abras un terminal
```

  Pide que el `gcc-13` del sistema ya esté instalado (el envoltorio llama a `/usr/bin/gcc-13`). Con `sudo apt install` como arriba no hace falta nada de esto; `env.sh` es opcional y `make`, `run_in_qemu.sh` y `gerar_versao.sh` lo cargan solo si existe.
- **Espacio en disco:** la compilación (`build/`) ocupa en torno a 2 GB (en el equipo del mantenedor, 1,8 GB). `./gerar_versao.sh --todas` deja además en la carpeta de entrega otros 2 GB aproximados (las imágenes persistentes ext2 miden unos 900 MB cada una). Conviene tener **5 GB libres**.
- **Tiempo:** no se ha medido para esta guía. Depende de los núcleos del PC: la primera compilación es la lenta (el kernel, la libsagui, MicroPython, el navegador Cipó y los ports de terceros); las siguientes solo rehacen lo que cambia. Usa `make -j8` (o los núcleos que tengas).

### 2. Compilar y arrancar

```bash
make -j8                 # compila todo y genera build/saguios.img
./run_in_qemu.sh         # arranca build/saguios.img en QEMU con ventana
```

`make` sin objetivo compila `build/saguios.img` y las herramientas del PC. Otros objetivos útiles: `make vdi` (`build/saguios.vdi`, para VirtualBox), `make iso` (`build/saguios.iso`, CD arrancable), `make rodar` (QEMU sencillo con la imagen), `make limpar` (borra `build/`), `make test` (batería completa de pruebas en QEMU, muy larga).

Opciones de `./run_in_qemu.sh` (`--help` las lista todas):

| Opción | Qué hace |
| --- | --- |
| `--imagen ARCHIVO` | arranca esa imagen en vez de `build/saguios.img` (un `.img.xz` se descomprime antes en `build/`) |
| `--compilar` | ejecuta `make` antes de arrancar |
| `--smp`, `--nucleos N` | multinúcleo: hace falta una imagen creada con `mkimagem.py --smp`; con `--smp` y sin `--nucleos`, la máquina tiene 4 núcleos (o los que haya) |
| `--ioapic`, `--planificador P`, `--uacpi` | interrupciones por el IOAPIC; planificador (`rr`, `afinidad`, `equilibrio`, `prioridad`); intérprete ACPI e indicador de batería. Con cualquiera de estas, `--smp` o `--uacpi` se crea `build/saguios-opciones.img` al momento y la tuya no se toca |
| `--sonido`, `--ac97` | sonido por PulseAudio (en WSLg funciona): altavoz del PC y tarjeta Intel HDA (o AC'97 con `--ac97`) |
| `--usb`, `--cd`, `--pendrive ARCHIVO`, `--xhci`, `--hid-usb` | arrancar como pendrive USB o desde el CD (`build/saguios.iso`); conectar un pendrive de datos FAT32; controlador USB 3.0; teclado y ratón USB |
| `--mem MB`, `--cpu MODELO`, `--gpu MODELO`, `--vram MB` | memoria (2048 por defecto), modelo de CPU (`pentium3`...), tarjeta gráfica (`std`, `virtio`, `vmware`) y su memoria |
| `--usb-real VID:PID`, `--wifi-usb` | pasar un dispositivo USB real del PC (en WSL, con `usbipd`; ver `docs/usb-real.md`) |
| `--raton-relativo`, `--serie`, `--vnc`, `--guardar` | ratón como en un portátil real; log del kernel por este terminal; sin ventana (VNC en `localhost:5900`); conservar los cambios en el disco de arranque (por defecto se descartan) |
| `--sin-kvm` | sin aceleración KVM. Por defecto se usa KVM si existe `/dev/kvm`; si no, emulación por software (más lenta) |

### 3. Opciones de compilación

Se pasan en la línea de `make`: `make NETSURF=0 B=build-mia -j8`. Si cambias una opción, usa un `B=` distinto (o `make limpar`) para no mezclar objetos.

**Variables del `Makefile`:**

| Variable | Por defecto | Qué hace |
| --- | --- | --- |
| `B` | `build` (con `SEM_GPL=1`, `build-sem-gpl`; con `SEM_FIRMWARE=1`, `build-sem-firmware`) | directorio de salida de la compilación |
| `-j N` | (uno) | trabajos en paralelo de `make`. `gerar_versao.sh` usa por defecto todos los núcleos |
| `OTIMIZACAO` | `-O2` | nivel de optimización (`-O0`, `-O1`, `-O2`, `-O3`, `-Os`) |
| `PUBLICABLE` | `0` | `1` deja fuera lo que no puede ir en un binario GPL v3: NetSurf, GNUBoy y el r8169 derivado de Linux (el driver usa el inicio clásico propio). Lo pasa solo `gerar_versao.sh` si no usas `--con-input-share` |
| `SEM_GPL` | `0` | `1` deja fuera todos los ports de terceros con licencia GPL (OpenTyrian, NetSurf, GNU Prolog, OpenJazz, Vanilla Conquer, Quake, GNUBoy, DOSBox, Linux), y también Tzar, TCC, GNU make y el Laboratorio de C. Compila en `build-sem-gpl/` |
| `SEM_FIRMWARE` | `0` | `1` no mete el firmware binario (`firmware/`: el del pincho Wi-Fi RTL8188EU, los de los pinchos Wi-Fi USB y Bluetooth y el microcódigo de las Radeon R5xx, R6xx y R7xx para el 3D por la GPU). Compila en `build-sem-firmware/` |
| `MICROPYTHON` | `1` | `0` genera la imagen sin MicroPython (bastante más pequeña) |
| `NETSURF` | `1` | navegador NetSurf; solo entra si están las fuentes preparadas y el pegamento (ver 7). `NETSURF=0` lo quita |
| `GNUBOY` | `1` | emulador de Game Boy; solo entra si está descargado su código (`make priv_descargas` o `bash tools/gnuboy_fuentes.sh`, ver 5 y 7). `GNUBOY=0` lo quita |
| `MGBA` | `1` | emulador de Game Boy Advance, Game Boy y Game Boy Color con mGBA (MPL-2.0; sus fuentes van en git). También entra con `PUBLICABLE=1`. `MGBA=0` lo quita |
| `LAINES`, `GEARSYSTEM` | `1` | emuladores de NES (LaiNES) y de Master System, Game Gear y SG-1000 (Gearsystem), en C++ con SDL 2; sin `g++` no entran. `0` los quita; con `SEM_GPL=1` tampoco entran |
| `SHINE` | `1` | codificador MP3 shine (LGPL-2.0+, D339) del Conversor de audio (`convertir-audio`). `SHINE=0` lo quita (el programa sale igual, sin la salida MP3); con `SEM_GPL=1` tampoco entra |
| `QUAKE` | `1` | motor de Quake (`0` lo quita) |
| `OPENJAZZ`, `CONQUER`, `DOSBOX`, `GPROLOG` | `1` | OpenJazz, Command & Conquer (Vanilla Conquer), DOSBox (con el lanzador Recreativa) y GNU Prolog (`0` los quita) |
| `QUAKE_DATOS`, `OPENJAZZ_DATOS` | `1` | meter en `/jogos` los datos shareware que bajaron `tools/quake_datos.sh` y `tools/openjazz_datos.sh`. Si no se bajaron, no entra nada; `0` los ignora aunque estén bajados |
| `CNC_VIDEOS` | `0` | `1` mete también las películas de C&C (840 MB más: sube `JOGOS_MB`, por ejemplo `JOGOS_MB=2000`) |
| `INPUT_SHARE` | `input_share` | carpeta con tus juegos y apps (ver 6). `INPUT_SHARE=` (vacío) la ignora; también vale otra ruta |
| `JOGOS` | `1` | `0` quita la partición de juegos `SAGUI-JOGOS` de todas las imágenes |
| `JOGOS_MB` | `800` | tamaño de `SAGUI-JOGOS` en las imágenes persistentes, la de particiones y lo que crea el instalador (`0` = el justo) |
| `JOGOS_IMG_MB` | `0` | lo mismo para `saguios.img`, `-smp` y `-uacpi` (`0` = el justo para lo que lleva) |
| `APPS`, `APPS_MB` | `1`, `0` | `APPS=0` mete las aplicaciones dentro del ramdisk (`/apps`, imagen de un solo bloque); `APPS_MB` es el tamaño de la partición `SAGUI-APPS` (`0` = lo justo más un margen). `convertir-audio`, `traductor` y los exploradores de bases de datos van en la partición de aplicaciones (`/apps`); los exploradores ejecutan `/apps/bin/python`. |
| `RAIZ_DISCO` | `1` | (D379, desde la v8.5.3.1) las imágenes arrancan con un initramfs mínimo (`build/initramfs.sgfs`, < 1 MB) y el sistema entero, con `/apps`, en la partición `SAGUI-RAIZ` (ext2): live desde el medio (raíz de solo lectura y los cambios en la RAM, o en `SAGUI-PERSIST` en las persistentes); la ISO la copia a la RAM (toram). `0`: todas con el ramdisk de siempre (como hasta la v8.5.3) |
| `PERSIST_MB` | `256` | tamaño de `SAGUI-PERSIST` (la capa persistente de la live) en las imágenes persistentes |
| `RAMDISK` | `base` | `completo`: el ramdisk de siempre con todo el sistema y las aplicaciones dentro (como `APPS=0` y `RAIZ_DISCO=0`; es `saguios-completo.img`). Se pasa igual con `gerar_versao.sh`, por ejemplo `RAMDISK=completo ./gerar_versao.sh` |
| `DATOS_MB` | `64` | `/home` de la imagen persistente ext2 |
| `DATOS_FAT32_MB` | `2048` | `/home` de la imagen persistente FAT32 |
| `HOME_MB`, `TMP_MB`, `LOGS_MB` | `2048`, `256`, `128` | particiones del disco con particiones (`imagem-particoes`) |
| `PART_FS` | `fat32` | sistema de ficheros de esas particiones (`fat32` o `ext2`) |
| `PART_EXT` | (vacío) | `--primarias`: el formato de antes de la v7.3 (por defecto, todas como lógicas en una extendida) |
| `DESTINO`, `PRIV_DESTINO` | `entrega/latest` | carpeta de salida de `make variantes` y de `make priv_variantes` |
| `GERAR_OPC` | (vacío) | opciones extra para `gerar_versao.sh` desde esos dos objetivos (por ejemplo `GERAR_OPC="--probar"`) |
| `FUENTES_RED` | (sin definir) | `1`: si falta el zip de Typodermic, que lo baje (ver 5) |
| `FREECIV` | `1` | Freeciv 3.1.6 (cliente SDL2 y servidor local, en español, D374): solo entra si están preparadas las fuentes (ver 5). `FREECIV=0` lo quita; con `SEM_GPL=1` tampoco entra |
| `DOOM` | `1` | Chocolate Doom 3.1.1 con Heretic, Hexen y Strife (D383): solo entra si están preparadas las fuentes (`tools/doom_fuentes.sh`). `DOOM=0` lo quita; con `SEM_GPL=1` tampoco entra. Freedoom (BSD-3-Clause, `tools/doom_datos.sh`) va a `/apps/doom` también en las imágenes públicas; los WAD originales, solo por `input_share/games/DOOM` |
| `GIT` | `1` | git de SaguiOS (`/apps/bin/git`, D400): solo entra si están las fuentes de libgit2 (`tools/git_fuentes.sh`, ver 1 y 5). `GIT=0` lo quita; con `SEM_GPL=1` tampoco entra |
| `MAKE_DENTRO` | `1` | GNU make de SaguiOS (`/apps/bin/make`, D413): solo entra si están las fuentes (`tools/make_fuentes.sh`, ver 1 y 5). `MAKE_DENTRO=0` lo quita; con `SEM_GPL=1` tampoco entra |
| `LABC` | `1` | el Laboratorio de C (`/apps/bin/labc`, D414): solo entra con TCC. `LABC=0` lo quita (y `TCC=0` también) |
| `AWK_DENTRO` | `1` | The One True AWK (`/bin/awk`, D440): solo entra si están las fuentes (`tools/awk_fuentes.sh`, ver 1 y 5). `AWK_DENTRO=0` lo quita; con `SEM_GPL=1` también entra |
| `LUA_DENTRO` | `1` | Lua 5.4 (`/apps/bin/lua` y `luac`, `liblua.a` y `liblua.so.5.4`, D440): solo entra si están las fuentes (`tools/lua_fuentes.sh`, ver 1 y 5). `LUA_DENTRO=0` lo quita; con `SEM_GPL=1` también entra |
| `GO` | `1` | programas en Go con TinyGo (`/apps/go/`, ver 1). Sin TinyGo instalado se saltan con un aviso; `GO=0` los desactiva |
| `OPENTTD_REPO`, `OPENTTD_URL` | `../openttd-saguios` | solo para `make priv_descargas`: dónde está el repositorio de OpenTTD (o de dónde clonarlo). Sin él, la imagen privada no lleva OpenTTD (ver 5) |

**`gerar_versao.sh`** (compila, arma las imágenes y deja la entrega con sumas SHA-256; `./gerar_versao.sh --help`):

| Opción | Por defecto | Qué hace |
| --- | --- | --- |
| `--destino DIR` | `entrega/latest` | carpeta de la entrega |
| `--publicar` | no | copia además la entrega a `entrega/v<versión>` |
| `--iso` | no | añade `saguios.iso` |
| `--fat32 [MB]` | no (2048) | añade la imagen persistente con `/home` FAT32 (`.xz`) |
| `--ext2 [MB]` | no (64) | añade la imagen persistente con `/home` ext2 |
| `--smp` | no | añade `saguios-smp.img` y, con `--ext2`/`--fat32`, las persistentes multinúcleo |
| `--uacpi` | no | añade `saguios-uacpi.img` y, con `--smp` y `--ext2`, `saguios-persistente-smp-uacpi.img` |
| `--particiones` | no | añade el disco con `/home`, `/tmp` y `/logs` en particiones (`.img.xz`) |
| `--sin-gpl` | no | añade la variante sin ports GPL (pasa antes `tools/auditar_licencias.py`) |
| `--todas` | no | todo lo anterior |
| `--variante V` | `gpl` | variante de la imagen principal: `gpl`, `sem-gpl` o `sem-firmware` |
| `--sin-python` | no | sin MicroPython (compila en `build-sem-python/`) |
| `--otimizacao O` | `-O2` | nivel de optimización |
| `--jobs N`, `-j N` | núcleos del PC | trabajos de `make` |
| `--limpiar` | no | borra la carpeta de compilación antes de compilar |
| `--sin-compilar` | no | no compila: usa lo que ya haya en la carpeta de compilación |
| `--con-input-share` | no | mete `input_share/`, los datos shareware bajados, NetSurf y GNUBoy. **Solo uso propio** (ver 6 y 7) |
| `--probar` | no | arranca la imagen en QEMU y comprueba que llega al escritorio |
| `--sin-notas`, `--notas FICHERO` | notas sí | no escribe `VERSION.txt`, o le añade el texto de un fichero |
| `--fijar-version V` | no | cambia la versión en `kernel/inc/versao.h` antes de compilar (`v8.1`...) |

Sin `--con-input-share`, `gerar_versao.sh` compila con `INPUT_SHARE= OPENJAZZ_DATOS=0 QUAKE_DATOS=0 NETSURF=0 GNUBOY=0 PUBLICABLE=1`: es lo **publicable**. Los tamaños se cambian con las variables de la tabla anterior, por ejemplo `HOME_MB=8192 JOGOS_MB=64 ./gerar_versao.sh --particiones --smp`.

**`tools/mkimagem.py`** (lo llama el `Makefile`; `python3 tools/mkimagem.py --help`). Opciones útiles para un usuario; las que empiezan por «prueba» o `--teste` son solo de las pruebas automáticas:

| Opción | Por defecto | Qué hace |
| --- | --- | --- |
| `--vesa` | (la pone el Makefile) | la etapa 2 del arranque cambia al modo gráfico VESA |
| `--smp` | apagado | enciende los procesadores secundarios (sin ella, el sistema usa uno solo) |
| `--ioapic` | apagado | IRQ de los dispositivos por el IOAPIC en vez del PIC 8259 |
| `--uacpi` | apagado | intérprete de AML (uACPI): controlador embebido e indicador de batería |
| `--sem-acpi` | apagado | no usar el ACPI: apagar solo avisa, sin botón de energía |
| `--planificador P` | `rr` | planificador al arrancar: `rr`, `afinidad`, `equilibrio` o `prioridad` |
| `--resolucion LxA`, `--bpp N` | automática (1024x768, 32 bpp) | resolución y profundidad de color preferidas |
| `--sem-wc` | apagado | no activar write-combining en el framebuffer |
| `--sem-pae` | apagado | paginación clásica de 2 niveles aunque la CPU tenga PAE: hasta 4 GB de RAM y sin NX (para probar o para hardware con problemas) |
| `--sem-nx` | apagado | no activar NX (pila y montón no ejecutables) aunque la CPU lo tenga |
| `--sem-avx` | apagado | (D381) el kernel no enciende AVX (XSAVE y los registros YMM) aunque la CPU lo tenga: todo queda como antes (FXSAVE y rutas SSE). Desde el sistema, `kconfig avx` con `si` o `no` |
| `--cache-disco-baja` | apagado | la caché de disco en la memoria baja (hasta 64 MB) aunque haya memoria alta (por si el hardware da problemas) |
| `--sin-swap` | apagado | sin swap comprimida en RAM: cuando se acaba la memoria, el programa falla como antes |
| `--swap-mb N` | 0 (automático) | tope del almacén de la swap comprimida en MB (1-1024); automático = la mitad de la RAM con memoria alta, un cuarto sin ella, 1 GB como mucho |
| `--sgl-motor software` | `auto` | desactiva toda la aceleración 3D por hardware |
| `--acel2d {auto,cpu,intel,nvidia,radeon,tdfx,vmware}`, `--sin-acel2d` | `auto` | acelerador 2D de la GPU; `cpu` o `--sin-acel2d` = solo CPU |
| `--sin-servicios` | apagado | no lanzar el gestor de servicios ni el cron al arrancar |
| `--rescate` | apagado | arrancar en modo rescate: consola con un shell de root, sin escritorio |
| `--sesion-root` / `--sesion-sagui` | `--sesion-sagui` | escritorio como root o como el usuario `sagui` |
| `--sin-frecuencia` | apagado | sin el driver de frecuencia de la CPU |
| `--console` | apagado | sin escritorio: terminal a pantalla completa |
| `--sin-conf` | apagado | sin la partición `SAGUI-CONF` (16 MB, configuración del sistema). La ISO no la lleva |
| `--dados-mb N`, `--dados-fs {ext2,fat32}` | 0 (sin ella) | partición `/home` (`SAGUI-DATOS`) |
| `--tmp-mb N`, `--logs-mb N`, `--tmp-fs`, `--logs-fs` | 0 | particiones `/tmp` y `/logs` |
| `--extra-mb N` | ninguna | partición extra sin carpeta fija; se puede repetir |
| `--esquema {logicas,primarias,mixto}`, `--primarias` | `logicas` | dónde van las particiones de datos |
| `--jogos CARPETA`, `--jogos-mb N` | — | partición de juegos con el contenido de la carpeta (el `Makefile` ya lo hace) |
| `--apps CARPETA`, `--apps-mb N` | — | partición de aplicaciones (idem) |
| `--part-fs {ext2,fat32}` | `ext2` | sistema de las particiones sin `--*-fs` propio |
| `--cd` | apagado | imagen para la ISO (la usa `make iso`) |
| `--raiz CARPETA`, `--raiz-mb N` | — | (D379) la raíz del sistema en la partición `SAGUI-RAIZ` (ext2) con el contenido de la carpeta (`build/rootfs`; con `--apps`, `/apps` va dentro); `--ramdisk` es entonces el initramfs (`tools/mkinitramfs.py`). El `Makefile` ya lo hace |
| `--persist-mb N` | 0 | (D379) partición `SAGUI-PERSIST`: la capa persistente de la live |
| `--modo-raiz {auto,disco,live,toram,ram}` | `auto` | (D379) fuerza cómo se usa la raíz (por defecto, lo que diga el initramfs; con `--cd`, `toram`) |
| `--raiz-sin-comprimir` | apagado | (D379, v8.5.4.2) con `--cd`, la raíz va como ext2 tal cual; por defecto va comprimida por bloques (la ISO pesa la mitad) |

Las opciones de energía de la GPU (D399), `--gpu-energia-simulada` y `--gpu-energia-prueba`, son solo para las pruebas automáticas en QEMU y no aparecen en la tabla.

Un disco con particiones a medida: `python3 tools/crear-disco.py -i` (asistente) o `make imagem-particoes-xz HOME_MB=4096 TMP_MB=512 LOGS_MB=256 PART_FS=fat32`. Como todas las imágenes de disco, arranca por BIOS y por UEFI (D270; `crear-disco.py --sin-uefi`: solo BIOS).

Herramienta del PC: `tools/conversor_video_pc.py` es un conversor de vídeo con ventana (Python 3 + tkinter, con ffmpeg) que prepara vídeos para el Reproductor de SaguiOS; no forma parte de la compilación. Ver `docs/convertir-video.md`.

### 4. Objetivos: qué imagen sale de cada uno

| Orden | Qué genera |
| --- | --- |
| `make` / `make all` | `build/saguios.img` (la normal, un núcleo) |
| `make help` | lista los objetivos y las variables principales |
| `make vdi`, `make iso` | `build/saguios.vdi`, `build/saguios.iso` |
| `make imagem-smp`, `imagem-uacpi` | `build/saguios-smp.img`, `build/saguios-uacpi.img` |
| `make imagem-persistente`, `imagem-persistente-fat32`, `imagem-persistente-smp`, `imagem-persistente-smp-uacpi` | las persistentes (con `/home`) en `build/` |
| `make imagem-particoes`, `imagem-particoes-xz` | `build/saguios-particiones.img` (y `.xz`) |
| `make variantes [DESTINO=entrega/mia]` | **lo publicable**: `./gerar_versao.sh --todas --destino DESTINO`: todas las imágenes de la tabla de abajo, sin `input_share/`, sin NetSurf ni GNUBoy, sin datos shareware |
| `make priv_variantes [PRIV_DESTINO=...]` | **uso propio**: lo mismo con `--con-input-share` (tus juegos, shareware de Quake y Jazz, NetSurf y GNUBoy, que descarga antes). Antes ejecuta `make priv_descargas` (ver 5 y 7) |
| `make entrega-sin-gpl` | `build-sem-gpl/saguios-sin-gpl.img`, tras la auditoría de licencias |
| `./gerar_versao.sh --todas` | lo mismo que `make variantes`, con el destino por defecto `entrega/latest` |

Ficheros que deja `make variantes` en la carpeta de entrega:

| Fichero | Para qué sirve |
| --- | --- |
| `saguios.img` | La normal (un núcleo). **Empieza siempre por esta** |
| `saguios-smp.img` | Multinúcleo (todos los procesadores, sin IOAPIC) |
| `saguios-uacpi.img` | Con el intérprete ACPI (uACPI): indicador de batería y `_PSS`/`_PRT` en portátiles |
| `saguios-completo.img` | Con todo el sistema y las aplicaciones en la memoria (el ramdisk de siempre), para los PC cuya controladora no localiza el disco de arranque (D324); pide unos 64 MB más de RAM y no lleva Freeciv, OpenTTD ni DOOM. Desde la 8.4.3r2 |
| `saguios-persistente-fat32.img.xz` | Con `/home` FAT32 de 2 GB, visible también desde Windows. La mejor para guardar cosas |
| `saguios-persistente-fat32-smp.img.xz` | Lo mismo, multinúcleo |
| `saguios-persistente.img`, `-smp`, `-smp-uacpi` | Con `/home` ext2 (Windows no la ve). Combinan multinúcleo y uACPI |
| `saguios-particiones.img.xz` | `/home`, `/tmp` y `/logs` en particiones (el kernel guarda su registro en `/logs`) |
| `saguios-sin-gpl.img` | Sin ports de terceros GPL |
| `saguios.iso` | CD arrancable (modo en vivo, sin `SAGUI-CONF`): el sistema, con `/apps` y Freeciv, se copia a la RAM al arrancar (toram) y el CD se puede sacar; con poca RAM se usa desde el CD |
| `saguios.vdi` | VirtualBox |
| `SHA256SUMS.txt`, `VERSION.txt` | sumas de comprobación y hoja de datos de lo que se compiló |

Desde la v8.5.3.1 (D379) todas las imágenes de disco llevan el sistema en `SAGUI-RAIZ` (ext2, con `/apps` dentro: ya no hay partición `SAGUI-APPS`), además de `SAGUI-JOGOS` (juegos, `/jogos`) y `SAGUI-CONF` (configuración, 16 MB); las persistentes, también `SAGUI-PERSIST`. Arrancan en live desde el medio (`docs/imagenes.md`). Con `RAIZ_DISCO=0` salen como hasta la v8.5.3 (ramdisk con la base y `SAGUI-APPS`).

Desde la v8.5.4.10r2 (D388) el orden en el disco es: arranque, la ESP, y la extendida la última con `SAGUI-CONF`, `SAGUI-RAIZ`, `SAGUI-PERSIST`, `SAGUI-JOGOS`… y `/home` (`SAGUI-DATOS`) la última lógica: grabada en un disco mayor, «Ampliar /home» la estira hasta el final sin mover nada. `mkimagem.py --orden-antiguo` da la disposición de antes (solo para pruebas).

### 5. Qué se descarga y cuándo

**`make` normal no descarga nada de la red.** Todo lo de abajo lo ejecutas tú, o lo ejecuta `make priv_descargas` / `make priv_variantes`. Todos los guiones comprueban el SHA-256 de lo que bajan y no repiten la descarga si ya está hecha; si no hay red, fallan con un mensaje y el sistema se construye igual sin esa pieza (los datos shareware y las fuentes tipográficas son opcionales).

| Qué | Guion | Origen | Licencia | Dónde queda |
| --- | --- | --- | --- | --- |
| Fuentes de NetSurf 3.11 (unos 90 MB) | `bash tools/netsurf_fuentes.sh [--tarball FICHERO] [--forzar]` | `https://download.netsurf-browser.org/netsurf/releases/source-full/netsurf-all-3.11.tar.gz` | GPL-2.0 solo v2 (las bibliotecas, MIT). Ver 7 | `userland/terceros/netsurf/fuentes/` (ignorada por git). Necesita `perl` |
| Fuentes de GNUBoy (SDL2-GNUBoy, commit `c8642d1`, unos 450 KB) | `bash tools/gnuboy_fuentes.sh [--tarball FICHERO] [--forzar] [--comprobar]` | `https://github.com/AlexOberhofer/SDL2-GNUBoy/archive/c8642d11bd2706333c5255e2b7f2b84445d0a070.tar.gz` | GPL-2.0 (solo v2 en la práctica). Ver 7 | `userland/terceros/gnuboy/jogo/` |
| Shareware de Quake 1.06 (episodio 1) | `bash tools/quake_datos.sh [--zip FICHERO] [--forzar]` | `ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake/quake106.zip` (y la réplica `youfailit.net`) | datos de id Software: gratis para el usuario, **sin derecho a redistribuirlos dentro de otro sistema** | `userland/terceros/quake/datos/` (ignorada por git) |
| Shareware de Jazz Jackrabbit 1.0 (episodio 1) | `bash tools/openjazz_datos.sh [--zip FICHERO] [--forzar]` | `https://slackware.uk/~urchlay/src/jjrabbit.zip` | datos de Epic MegaGames, con condiciones de redistribución: no se publican | `userland/terceros/openjazz/datos/` (ignorada por git) |
| Fuentes tipográficas de Typodermic (28 elegidas de 731) | `python3 tools/fuentes_typodermic.py seleccion DESTINO` (con red, baja el zip); `make priv_descargas` lo hace solo | `https://typodermicfonts.com/wp-content/uploads/2024/12/typodermic-public-domain-2024-12.zip` (**a veces da 404**: ver la nota) | CC0 (dominio público) | `NEW_FONTS/typodermic-public-domain-2024-12.zip` |
| ROM libre de Game Boy (Libbet) | `bash tools/gnuboy_roms_libres.sh` | `github.com/pinobatch/libbet`, versión v0.08 | zlib | `userland/terceros/gnuboy/roms-libres/` (solo útil con GNUBoy, ver 7) |
| Fuentes de Freeciv 3.1.6 y de zlib 1.3.1 (unos 90 MB) | `bash tools/freeciv_fuentes.sh [--tarball FICHERO] [--zlib-tarball FICHERO] [--forzar]` | `https://github.com/freeciv/freeciv/archive/refs/tags/R3_1_6.tar.gz` (zlib: `https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz`) | GPL-2.0 o posterior (Freeciv); licencia zlib (zlib) | `userland/terceros/freeciv/fuentes/` y `userland/terceros/zlib/fuentes/` (ignoradas por git) |
| Fuentes de Chocolate Doom 3.1.1 y Freedoom 0.13.0 (unos 12 y 57 MB) | `bash tools/doom_fuentes.sh [--tarball FICHERO] [--forzar]` y `bash tools/doom_datos.sh [--zip FICHERO] [--forzar]` | `https://github.com/chocolate-doom/chocolate-doom/archive/refs/tags/chocolate-doom-3.1.1.tar.gz` y `https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip` | GPL-2.0 o posterior (Chocolate Doom); BSD-3-Clause (Freedoom) | `userland/terceros/chocolatedoom/fuentes/` y `datos/` (ignoradas por git) |
| Fuentes de libgit2 1.9.1 (la orden `git`, D400) | `bash tools/git_fuentes.sh [--tarball FICHERO]` | `https://github.com/libgit2/libgit2/archive/refs/tags/v1.9.1.tar.gz` | GPL-2.0 con excepción de enlace | `userland/terceros/git/fuentes/` (ignorada por git) |
| Fuentes de GNU make 4.4.1 (`/apps/bin/make`, D413) | `bash tools/make_fuentes.sh [--tarball FICHERO]` | `https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz` | GPL-3.0-or-later | `userland/terceros/make/fuentes/` (ignorada por git) |
| Fuentes de The One True AWK 20261106 (`/bin/awk`, D440) | `bash tools/awk_fuentes.sh [--tarball FICHERO]` | `https://github.com/onetrueawk/awk/archive/refs/tags/20261106.tar.gz` | permisiva de Lucent (tipo HPND) | `userland/terceros/awk/fuentes/` (ignorada por git) |
| Fuentes de Lua 5.4.9 (`/apps/bin/lua` y `luac`, D440) | `bash tools/lua_fuentes.sh [--tarball FICHERO]` | `https://www.lua.org/ftp/lua-5.4.9.tar.gz` | MIT | `userland/terceros/lua/fuentes/` (ignorada por git) |
| Modelos 3D CC0 para la demo de SaguiGL (`sgldemo -p --modelo`, D404) | `bash tools/modelos3d_datos.sh [--forzar]` | `https://opengameart.org/content/low-poly-biplane`, `https://opengameart.org/content/funky-aircraft` y `https://opengameart.org/content/ww1-wwi-airplane-model-obj` | CC0 1.0 (dominio público) | `userland/terceros/modelos3d/datos/` (ignorada por git); no entran en las imágenes |

Notas:

- **Typodermic:** `make priv_descargas` (o `python3 tools/fuentes_typodermic.py seleccion /tmp/fuentes-prueba`) baja el zip a `NEW_FONTS/` si falta. Si la web da 404 (pasó el 2026-11-14), el guion lo dice con claridad y **no rompe nada**: el sistema se construye igual sin esas fuentes. Para tenerlas, baja el zip a mano y ponlo en `NEW_FONTS/typodermic-public-domain-2024-12.zip` (se comprueba el SHA-256). `make` solo mete las fuentes si el zip ya está en `NEW_FONTS/` cuando compilas (esa carpeta no va en el repositorio público).
- **`make priv_descargas`** ejecuta, por este orden: `tools/netsurf_fuentes.sh` (si no hay fuentes preparadas), `tools/gnuboy_fuentes.sh` (si no está `jogo/`), `tools/quake_datos.sh`, `tools/openjazz_datos.sh` y la descarga de las fuentes Typodermic (si falla, avisa y sigue). Funciona igual con el repositorio público: el pegamento de NetSurf y GNUBoy va en él.
- **`make priv_descargas`** también ejecuta `tools/doom_fuentes.sh` y `tools/doom_datos.sh` (D383). Sin `userland/terceros/chocolatedoom/fuentes/.preparado` el `Makefile` salta DOOM con un aviso.
- **Freeciv** no lo baja `make priv_descargas`: hay que ejecutar `bash tools/freeciv_fuentes.sh` a mano. Sin `userland/terceros/freeciv/fuentes/.preparado` el `Makefile` no compila Freeciv, y sin aviso (`FREECIV=0` lo quita). Sus datos de juego van a `/apps/freeciv`. No depende de `--con-input-share`: si las fuentes están preparadas, Freeciv entra también en `make variantes`; bájalas solo si lo quieres en tus imágenes.
- **OpenTTD** (opcional, solo imágenes privadas): el último paso de `make priv_descargas` lo compila como paquete desde su repositorio aparte (por defecto `../openttd-saguios`, o `OPENTTD_URL`) con `tools/openttd_paquete.sh` y lo instala en `input_share/apps`. Sin ese repositorio, avisa y la imagen privada sale sin él. Las imágenes públicas (`INPUT_SHARE=`) nunca lo llevan.
- **Go** (opcional): `tools/tinygo_entorno.sh instalar` descarga TinyGo y Go en versiones fijas, con SHA-256 comprobado, a `~/.local/saguitools` (sin sudo). Sin ello, `make` salta los ejemplos de Go con un aviso.
- **Ya van en el repositorio, no se bajan:** las raíces de certificados (`rootfs/etc/ssl/certs/`; se pueden regenerar desde el almacén `ca-certificates` del PC con `python3 tools/raices_ssl.py`), el firmware del pincho Wi-Fi RTL8188EU (`firmware/realtek/`, de linux-firmware, redistribuible sin cambios; `tools/firmware_rtl8188eu.sh --comprobar` lo verifica y sin `--comprobar` lo vuelve a bajar de `git.kernel.org`; `SEM_FIRMWARE=1` lo quita), el de los pinchos Wi-Fi USB (`firmware/realtek/rtl8192cufw_A.bin`, `firmware/realtek/rtl8192cufw_TMSC.bin` y `firmware/ralink/rt2870.bin`, de linux-firmware, redistribuibles sin cambios; `tools/firmware_wifi_usb.sh --comprobar` los verifica y sin `--comprobar` los vuelve a bajar; `SEM_FIRMWARE=1` los quita), el de los controladores Bluetooth (`firmware/realtek/rtl_bt/` y `firmware/atheros/`, de linux-firmware, redistribuibles sin cambios; `tools/firmware_bt.sh --comprobar` los verifica y sin `--comprobar` los vuelve a bajar; `SEM_FIRMWARE=1` los quita; los parches `.hcd` de Broadcom no van en el repositorio: se ponen a mano en `/lib/firmware/brcm/`), el microcódigo de las Radeon R5xx, R6xx y R7xx para el 3D por la GPU (`firmware/radeon/`, de linux-firmware, redistribuible sin cambios; `tools/firmware_radeon.sh --comprobar` lo verifica y sin `--comprobar` lo vuelve a bajar; `SEM_FIRMWARE=1` lo quita), los datos de OpenTyrian (`userland/terceros/opentyrian/dados.img.xz`), las fuentes de OpenJazz, Quake y Vanilla Conquer (se comprueban con `tools/openjazz_fuentes.sh --comprobar`, `quake_fuentes.sh`, `conquer_fuentes.sh`) y las ROM de CHIP-8, que se ensamblan desde `rootfs/jogos/chip8/fuentes/*.asm`.

### 6. Juegos y datos que pones tú (`input_share/`)

`input_share/` es una carpeta en la raíz del repositorio (créala tú; git la ignora y **nunca se sube**). Lo que pongas entra en las imágenes que compiles con `make` normal, con `make priv_variantes` o con `./gerar_versao.sh --con-input-share`. En `make variantes` no entra, a propósito (así una entrega publicada no lleva tus juegos por descuido).

```
input_share/
  games/
    Quake/id1/PAK0.PAK       Quake completo (también PAK1.PAK, versión registrada)
    Jazz/                    Jazz Jackrabbit completo (p. ej. el de GOG), tal cual
    CnC/CnC_GDI95.iso        Command & Conquer Gold: GDI (volumen GDI95)
    CnC/CnC_NOD95.iso        ... y NOD (opcional: sin ella, solo la campaña GDI)
    gb/juego.gb              ROM de Game Boy / Game Boy Color (.gb, .gbc)
    <otra>/                  cualquier otro juego: acaba en /jogos/<otra>
  Tzar Anthology/            Tzar: Packs, Maps, Campaign, intro.MPG, Tzar.ini
  apps/
    bin/                     ejecutables de SaguiOS que quieras en /apps/bin (en el PATH)
    <app>/                   los datos de cada app, en /apps/<app>
```

| Juego | Qué poner y dónde | Notas |
| --- | --- | --- |
| **Quake** | `input_share/games/Quake/id1/pak0.pak` y `pak1.pak` | Acaba en `/jogos/Quake/id1/`. Vale también el `pak0.pak` suelto en `games/Quake/`, y los nombres en mayúsculas (`ID1/PAK0.PAK`, como en el CD de DOS). Las expansiones, en `games/Quake/hipnotic/` y `games/Quake/rogue/` (se arrancan con `quake -hipnotic` o `-rogue`). Usa tu copia legal (Steam, GOG, el CD). El shareware lo baja solo `tools/quake_datos.sh`; Quake prefiere la versión completa |
| **Jazz Jackrabbit** | `input_share/games/Jazz/` con los ficheros del juego, sin cambiarlos | La versión de GOG u otra completa. OpenJazz la encuentra en cualquier carpeta de `/jogos` y la prefiere al shareware |
| **Command & Conquer** | `input_share/games/CnC/CnC_GDI95.iso` y `CnC_NOD95.iso` | Son las ISO de **Command & Conquer Gold para Windows 95**, el regalo (freeware) de EA de 2007, una por disco; se reconocen por su volumen (GDI95, NOD95), no por el nombre. `tools/cnc_extraer.py` saca de ellas solo lo que usa Vanilla Conquer (63 MB en `/jogos/CnC`; con `CNC_VIDEOS=1`, 840 MB más). Sin las ISO, vale copiar a `/jogos/CnC` la carpeta de un C&C95 ya instalado |
| **Game Boy** | `input_share/games/gb/*.gb` o `*.gbc` | Solo ROM que tengas derecho a usar. Las busca GNUBoy, que no va en las imágenes publicadas: solo en las tuyas de uso propio (ver 7) |
| **Tzar** (Tzar Anthology, D417) | `input_share/Tzar Anthology/` con sus carpetas `Packs/`, `Maps/` y `Campaign/`, y `intro.MPG` y `Tzar.ini`; no se copian `Tzar.exe` ni las DLL | Acaba en `/jogos/TZAR/`. Solo con `INPUT_SHARE` (las imágenes públicas no lo llevan). Necesita unos 1 500 MB de `JOGOS_MB`: `JOGOS_MB=1600` es lo cómodo |
| Otros | `input_share/games/<carpeta>/` | Se copia tal cual a `/jogos/<carpeta>`. Si la carpeta se llama como una del sistema (`opentyrian`, `openjazz`), su contenido se pone encima |

Detalles: el `Makefile` vigila `input_share/games` y `input_share/apps` y rehace las particiones cuando cambian los nombres, tamaños o fechas; las marcas `*:Zone.Identifier` de Windows no se copian. Otra carpeta: `make INPUT_SHARE=/ruta/a/mi_input_share`; ninguna: `make INPUT_SHARE=`. La referencia completa es `docs/input_share.md`.

**Tamaños.** `SAGUI-JOGOS` es ext2 de lectura y escritura, montada en `/jogos`. En `saguios.img`, `-smp` y `-uacpi` mide lo justo para lo que contiene (`JOGOS_IMG_MB=0`): con Quake completo, Jazz y C&C salen imágenes de unos 250 MB. En las persistentes, la de particiones y el instalador mide **800 MB** (`JOGOS_MB=800`): sube el valor si tus juegos pesan más.

**Copiarlos después a un SaguiOS ya instalado.** Desde dentro de SaguiOS, copia la carpeta del juego a `/jogos/<carpeta>` (con **Archivos** o `cp`) desde un pendrive FAT32 o desde `/home`. Los juegos también buscan en `jogos/<carpeta>` de cada disco montado (`/usb`...). En Windows, la unidad `SAGUI-DATOS` de una imagen persistente FAT32 es `/home`: sirve para pasar ficheros.

> **Aviso legal.** Los datos de los juegos comerciales (Quake completo, Jazz Jackrabbit, Command & Conquer, las ROM de Game Boy, Tzar Anthology) **no se redistribuyen con SaguiOS ni están en este repositorio**, y no se pueden compartir. Cada persona usa sus propias copias legales. Una imagen compilada con `input_share/` es **para uso propio**: no la publiques ni la compartas.

### 7. Lo que no está en el repositorio público, y por qué

SaguiOS-Extended es **GPL v3** (`LICENSE`, `docs/licencia.md`). Cuatro piezas son **GPL v2 solamente** (sin «o posterior»), y la v2 sola es incompatible con la v3: no pueden enlazarse en un mismo binario con la libsagui y la libc comunes, así que **su código no se publica** ni va en las imágenes publicadas. Sí se publica lo nuestro: el pegamento con SaguiOS (`netsurf.mk`, `gnuboy.mk`, `plataforma/`, `libc/`, con su SPDX `GPL-2.0-or-later`) y los guiones que descargan el código original; así cada uno puede compilárselos **para uso propio** (usar en privado no es distribuir):

| Pieza | Licencia | Qué pasa en el repositorio público |
| --- | --- | --- |
| **NetSurf** (navegador, `userland/terceros/netsurf/`) | GPL-2.0 solo v2 | su código (`fuentes/`) no está: lo baja `tools/netsurf_fuentes.sh`; el pegamento propio sí |
| **GNUBoy** (Game Boy, `userland/terceros/gnuboy/`) | GPL-2.0 (casi sin «o posterior»: se trata como solo v2) | su código (`jogo/`) no está: lo baja `tools/gnuboy_fuentes.sh` (commit fijo, SHA-256 comprobado); el pegamento propio sí |
| **r8169 derivado de Linux** (`kernel/terceros/linux/r8169/`) | GPL-2.0-only | no está; el driver de la tarjeta Realtek RTL8168/8111 usa su inicio clásico propio (`kernel/rede/r8169_sem_gpl.h`): funciona, pero sin las secuencias de inicio de los chips más nuevos (RTL8111G/H) |
| **Freeciv** (juego de estrategia, `userland/terceros/freeciv/`) | GPL-2.0 o posterior | su código (`fuentes/`, unos 90 MB) no está en git: lo baja `tools/freeciv_fuentes.sh`; el pegamento propio (`freeciv.mk`, `plataforma/`, `parches/`, `traducciones/`) sí |
| **Chocolate Doom** (DOOM, Heretic, Hexen y Strife, `userland/terceros/chocolatedoom/`) | GPL-2.0 o posterior | su código (`fuentes/`) no está en git: lo baja `tools/doom_fuentes.sh`; el pegamento propio (`chocolatedoom.mk`, `plataforma/`, `parches/`) sí. Freedoom (BSD-3-Clause) lo baja `tools/doom_datos.sh` |
| **libgit2** (la orden `git`, `userland/terceros/git/`) | GPL-2.0 con excepción de enlace | su código (`fuentes/`) no está en git: lo baja `tools/git_fuentes.sh` con SHA-256 fijo; el pegamento propio (`git.c`, `git.mk`, `plataforma/`) sí |
| **OpenTTD** (juego, repositorio aparte `../openttd-saguios`) | GPL-2.0 solo | no está en este repositorio ni en las imágenes públicas: se compila como paquete aparte y solo entra en las imágenes privadas (D375) |

**Usarlos en privado está permitido; redistribuir esas imágenes, no.** Compilarte una imagen con ellos y usarla tú no es distribuir (`docs/licencia.md`).

**Cómo compilarlos para ti con solo el repositorio público.** Comprobado de verdad desde un árbol exportado sin git (`tools/publicar.sh preparar` y después `make` y `make priv_variantes INPUT_SHARE=`):

- `make` (sin más) funciona sin NetSurf ni GNUBoy: el menú no los muestra.
- `make priv_variantes INPUT_SHARE=` (o `make priv_descargas` y después `make`) **baja el código original** de NetSurf 3.11 y de SDL2-GNUBoy, comprueba sus SHA-256, los compila con nuestro pegamento y los mete en las imágenes **privadas**. Hace falta red y `perl` (NetSurf). Esas imágenes son solo para ti: **no las redistribuyas** (NetSurf y GNUBoy son GPL v2; enlazados con la libsagui, que es GPL v3, no se pueden distribuir).
- **NetSurf:** `bash tools/netsurf_fuentes.sh` deja `userland/terceros/netsurf/fuentes/.preparado`; el `Makefile` incluye `netsurf.mk` solo si existen las fuentes y el pegamento.
- **GNUBoy:** `bash tools/gnuboy_fuentes.sh` deja `userland/terceros/gnuboy/jogo/` (`--comprobar` compara con el original; si algún día hay parches propios, van en `userland/terceros/gnuboy/parches/*.patch` y el guion los aplica).
- **r8169:** los ficheros son extractos adaptados de `drivers/net/ethernet/realtek/r8169_main.c` de Linux 6.6; no hay guion que los regenere. Sin ellos el driver sigue funcionando con el inicio clásico.

Lo demás de `kernel/terceros/linux/` (`hda`, `hwmon`, GPL-2.0 o posterior) sí está en el repositorio. Tampoco están, por tamaño o privacidad, `entrega/` (las imágenes de cada versión van a la página de «Releases» del proyecto), `input_share/`, `NEW_FONTS/`, `testes_hw/` ni las notas internas (`tools/publicar-excluir.txt`).

### 8. Grabar y probar

**Primero, prueba en QEMU** (nada se graba en ningún disco):

```bash
./run_in_qemu.sh --imagen entrega/mia/saguios.img
./run_in_qemu.sh --imagen entrega/mia/saguios-persistente-fat32.img.xz --guardar
```

Después, la imagen a otro sitio (el paso a paso está en `docs/como-gravar.md`):

| Destino | Cómo |
| --- | --- |
| **Pendrive o disco USB** | En Windows, **Rufus** en modo **Imagen DD** (acepta los `.xz` tal cual) o balenaEtcher. En Linux: `xz -dk imagen.img.xz` si hace falta y `sudo dd if=saguios.img of=/dev/sdX bs=4M status=progress conv=fsync` (cambia `/dev/sdX` por tu pendrive: **se borra todo lo que tenga**). El PC debe arrancar por **BIOS tradicional** (modo Legacy/CSM) |
| **CD** | Graba `saguios.iso` (`make iso`) en un CD |
| **VirtualBox** | Usa `saguios.vdi`: máquina nueva tipo **Other**, versión **Other/Unknown** (32 bits), memoria 512 MB o más, disco existente `saguios.vdi` en el controlador **IDE**, **EFI desmarcado** |
| **QEMU** | `./run_in_qemu.sh --imagen ...` como arriba |

Comprueba las descargas con `sha256sum -c SHA256SUMS.txt`. En una máquina real, empieza por `saguios.img`; si funciona, prueba `-smp` y, en portátiles, `-uacpi`.

Para cambiar las opciones de una imagen ya grabada sin volver a grabarla, desde el propio SaguiOS: `arranque opciones [smp=si|no] [ioapic=si|no] [uacpi=si|no]` (sin opciones, muestra las actuales). Valen desde el siguiente arranque (D392).

### Generador con ventana (opcional)

Si prefieres una ventana en vez de las órdenes de arriba, `tools/generador` (Tkinter) monta la orden de `gerar_versao.sh` o de `tools/mkimagem.py`, deja elegir la carpeta de destino y ejecuta con la salida en vivo. También funciona sin ventana: `tools/generador.py --orden --perfil publica`. Tiene tres perfiles: *Pública* (lo de `make variantes`), *Privada completa* (lo de `make priv_variantes`; muestra «USO PROPIO: NO PUBLICAR» y nunca añade `--publicar`) y *Personalizada* (las opciones de `tools/mkimagem.py`). Ver `docs/generador.md`.

### 9. Problemas frecuentes

| Síntoma | Causa y solución |
| --- | --- |
| `CPU you selected does not support x86-64 instruction set` o `gcc: ... no acepta -m32` | Falta `gcc-multilib` (o el soporte de 32 bits en `~/.local/saguitools/root`). Instálalo; no quites `-m32` |
| Una cabecera «no existe» en el kernel o en un programa | `env.sh` exporta `CPATH` con la glibc del PC y GCC lo añade incluso con `-nostdinc`; el `Makefile` ya lo anula para todo lo que va dentro de SaguiOS. **No lo arregles** volviendo a pasar `CPATH` o `-I` a la glibc del PC: falta una cabecera nuestra o un `-I` |
| `env.sh: line 5: LD_LIBRARY_PATH: unbound variable` | `run_in_qemu.sh` usa `set -u`. Dentro de `env.sh`, las variables que pueden no existir se amplían con `${VAR:+:$VAR}` o `${VAR:-}`, nunca con `$VAR` a secas (ya está así en `tools/entorno-local.sh`; si lo editas, mantenlo). Prueba: `bash -c 'set -u; unset LD_LIBRARY_PATH LIBRARY_PATH CPATH; . ~/.local/saguitools/env.sh && echo OK'` |
| Los programas de C++ (OpenJazz, Command & Conquer, DOSBox) no salen | No hay `g++` de 32 bits (`cc1plus`): instala `g++` o ejecuta `tools/entorno-local.sh`. El resto se construye igual |
| Sin NetSurf, GNUBoy, fuentes Typodermic o datos de Quake/Jazz | Son opcionales y solo entran si descargaste antes sus datos con `make priv_descargas` (ver 5 y 7) |
| `falta el mcopy` / `mke2fs` / `mkfs.vfat` | Instala `mtools`, `e2fsprogs`, `dosfstools` (o ejecuta `tools/entorno-local.sh`) |
| QEMU muy lento | Sin KVM usa emulación por software. Comprueba que existe `/dev/kvm` y que estás en el grupo: `sudo usermod -aG kvm $USER` (abre una sesión nueva). Para forzar sin KVM: `--sin-kvm`. |
| Con KVM falla algo de Intel, o sin KVM va muy lento | Con KVM la CPU es la de tu PC (puede ser AMD): lo que espera una CPU Intel puede fallar. El emulador sin KVM no comprueba límites de segmento ni algunos bits de CPU: probar siempre en hardware real o con KVM antes de fiarse |
| El PC se cuelga o se reinicia al compilar y probar a la vez | En WSL2, `make -jN` con muchos núcleos más varios QEMU en TCG saturan CPU y memoria. Usa `-j4`, una prueba cada vez, y mejor con KVM. La batería completa (`python3 tests/rodar_todos.py`) es larguísima: no la lances con otras cosas pesadas |
| Una prueba (h34) falla al preparar su disco | Pasa si existe `input_share/`: la compilación mete tus juegos. Para probar, `make all INPUT_SHARE= OPENJAZZ_DATOS=0 QUAKE_DATOS=0` |
| Pruebas sobre una imagen vieja sin darte cuenta | Encadena con `&&`: `make -j4 && python3 tests/hNNN_xxx.py`; si el enlace falla, la imagen no se regenera y la prueba arranca la anterior |
| `make` falla con ficheros raros de nombre `*:Zone.Identifier` | Son restos de copiar desde Windows; bórralos |
| El PC no arranca la imagen | Las imágenes de disco y la ISO arrancan por BIOS (Legacy/CSM) y por UEFI (con Secure Boot en «Other OS», `docs/uefi.md`). Graba en modo **DD**. Prueba antes `saguios.img` (un núcleo) |

---

## English

A guide to building SaguiOS-Extended from the public repository and generating your own images. Commands (`make`, `./gerar_versao.sh`...) and file names are not translated. Everything here comes from reading the `Makefile`, `gerar_versao.sh`, `run_in_qemu.sh`, `tools/mkimagem.py` and the scripts in `tools/`.

### 1. PC requirements

- **System:** 64-bit Linux with `apt` (Ubuntu 24.04 or similar; `gcc-13` is what the `tools/entorno-local.sh` wrapper uses) or **WSL2** with Ubuntu, which is where the project is developed. No cross-compiler is needed: the PC's normal `gcc` with `-m32` and `-ffreestanding` does the job (decision D001), which is why `gcc-multilib` is required.
- **Packages** (Ubuntu/Debian):

```bash
sudo apt install build-essential gcc-multilib g++ nasm binutils make python3 \
    mtools dosfstools e2fsprogs xz-utils curl perl \
    qemu-system-x86 qemu-utils
# optional, only for the automated tests: imagemagick ffmpeg zip sqlite3 exfatprogs
# optional, to test UEFI boot (D264: h264, run_in_qemu.sh --uefi): ovmf ovmf-ia32
```

**Optional: Go programs.** `tools/tinygo_entorno.sh instalar` downloads TinyGo and Go into `~/.local/saguitools` (no sudo); with that, `make` also builds the examples in `userland/go/ejemplos/` into `/apps/go/`. Without TinyGo they are skipped with a notice (`GO=0` disables them). See `docs/go.md`.

**Optional: Java programs.** `tools/java_entorno.sh instalar` downloads a JDK 17 and Janino into `~/.local/saguitools` (no sudo) and `tools/minijvm_fuentes.sh` downloads miniJVM (pinned commit, SHA-256) into `userland/terceros/minijvm/fuentes/`; with those, `make` also builds `/apps/bin/java`, `/apps/bin/javac`, the class library in `/apps/java/lib/` and the examples in `userland/java/ejemplos/`. Without the sources or the JDK they are skipped with a notice (`JAVA=0` disables them). See `docs/java.md`.

**Optional: C compiler inside (TCC).** `tools/tcc_fuentes.sh` downloads TCC 0.9.28rc (pinned commit, SHA-256; LGPL-2.1 or later) into `userland/terceros/tcc/fuentes/` and applies its patches; with them `make` also builds `/apps/bin/tcc`, `/apps/tcc/` (headers, `libc.a`, `libSDL2.a`, `libtcc.a`, examples and the TCC sources so it can compile itself) and `/apps/aoen/fuentes` (AoEN, buildable inside with `sh compilar.sh`). Without the sources it is skipped with a notice (`TCC=0` disables it). See `docs/tcc.md`.

**Optional: GNU make inside (D413).** `tools/make_fuentes.sh` downloads GNU make 4.4.1 (fixed SHA-256; GPL-3.0-or-later) into `userland/terceros/make/fuentes/`; with it, `make` also builds `/apps/bin/make` and `/apps/make` (readme, licence and `ejemplos/proyecto`). Without the sources it is skipped with a notice (`MAKE_DENTRO=0` disables it). It is outside `SEM_GPL=1`. See `docs/make.md`.

**Optional: C Lab (D414).** With the TCC sources, `make` also builds `/apps/bin/labc`, the C Lab (a highlighting editor, code run from memory with libtcc, errors with their line, and ten examples in `/apps/labc/ejemplos`). It needs nothing else: it is included from `tcc.mk`, so `TCC=0` and `SEM_GPL=1` also remove it, and `LABC=0` removes only it. See `rootfs/ayuda/laboratorio-c.md`.

**Optional: awk and Lua inside (D440).** `tools/awk_fuentes.sh` downloads The One True AWK 20261106 (fixed SHA-256; permissive Lucent licence) into `userland/terceros/awk/fuentes/`; with it, `make` installs `/bin/awk`. No bison is needed: the already generated grammar is in git. `tools/lua_fuentes.sh` downloads Lua 5.4.9 (fixed SHA-256; MIT) into `userland/terceros/lua/fuentes/`; with it, `make` installs `lua` and `luac` in `/apps/bin`, `liblua.a` for TCC and `liblua.so.5.4`. Without the sources each one is skipped with a warning (`AWK_DENTRO=0` and `LUA_DENTRO=0` turn them off). Both are included with `SEM_GPL=1` too. See `docs/awk-lua.md`.

**Optional: git inside (D400).** `tools/git_fuentes.sh` downloads libgit2 1.9.1 (fixed tag, SHA-256 checked) into `userland/terceros/git/fuentes/`; with it, `make` also builds `/apps/bin/git` (init, clone, fetch, pull, push, status, add, commit, log, diff, branch, checkout, remote and config; HTTP(S) with TLS, and SSH). Without the sources it is skipped with a notice (`GIT=0` disables it). It is left out of `SEM_GPL=1`. See `docs/git.md`.

| Package | What for |
| --- | --- |
| `gcc`, `gcc-multilib`, `binutils`, `make` | building the kernel and the programs in 32 bits |
| `g++` (with `-m32` support) | **optional**: the C++ programs (OpenJazz, Command & Conquer, DOSBox). Without it the system is built anyway, minus those |
| `nasm` | the boot loader and parts of the kernel |
| `python3` | every tool in `tools/` (`mkimagem.py`, `mksgfs.py`...) |
| `mtools`, `dosfstools` | FAT32 partitions (`mcopy`, `mkfs.vfat`) |
| `e2fsprogs` | ext2 partitions (`mke2fs`) |
| `exfatprogs` (optional) | exFAT tests (`tests/h325_exfat.py`: `mkfs.exfat`, `fsck.exfat`); without it that test is not run. `tools/entorno-local.sh` installs it without sudo |
| `xz-utils` | `.xz` images and the compressed OpenTyrian data |
| `qemu-system-x86`, `qemu-utils` | running the system (`qemu-system-i386`) and creating the `.vdi` (`qemu-img`) |
| `ovmf`, `ovmf-ia32` (optional) | 64- and 32-bit UEFI firmware for QEMU, to test UEFI boot (`run_in_qemu.sh --uefi`, test h264; D264). `tools/entorno-local.sh` fetches them without sudo |
| `curl`, `perl` | only the download scripts (`curl`) and the NetSurf preparation (`perl`, see 7) |

- **Without administrator rights (`sudo`):** `bash tools/entorno-local.sh` downloads the missing packages with `apt-get download` (nasm, gcc's 32-bit support, QEMU, mtools, dosfstools, e2fsprogs, g++...), extracts them into `~/.local/saguitools/root` and creates `~/.local/saguitools/env.sh`. Use it like this, in every new terminal:

```bash
bash tools/entorno-local.sh               # only once
. ~/.local/saguitools/env.sh && make      # every time you open a terminal
```

  It requires the system `gcc-13` to be installed already (the wrapper calls `/usr/bin/gcc-13`). With `sudo apt install` as above none of this is needed; `env.sh` is optional and `make`, `run_in_qemu.sh` and `gerar_versao.sh` load it only if it exists.
- **Disk space:** the build (`build/`) takes around 2 GB (1.8 GB on the maintainer's machine). `./gerar_versao.sh --todas` leaves roughly another 2 GB in the delivery folder (the ext2 persistent images are about 900 MB each). Keep **5 GB free**.
- **Time:** not measured for this guide. It depends on your CPU cores: the first build is the slow one (the kernel, libsagui, MicroPython, the Cipó browser and the third-party ports); later builds only redo what changed. Use `make -j8` (or as many cores as you have).

### 2. Build and boot

```bash
make -j8                 # builds everything and creates build/saguios.img
./run_in_qemu.sh         # boots build/saguios.img in QEMU with a window
```

`make` with no target builds `build/saguios.img` and the PC-side tools. Other useful targets: `make vdi` (`build/saguios.vdi`, for VirtualBox), `make iso` (`build/saguios.iso`, bootable CD), `make rodar` (plain QEMU with the image), `make limpar` (deletes `build/`), `make test` (the full test suite in QEMU, very long).

`./run_in_qemu.sh` options (`--help` lists them all):

| Option | What it does |
| --- | --- |
| `--imagen ARCHIVO` | boots that image instead of `build/saguios.img` (an `.img.xz` is decompressed into `build/` first) |
| `--compilar` | runs `make` before booting |
| `--smp`, `--nucleos N` | multicore: needs an image made with `mkimagem.py --smp`; with `--smp` and no `--nucleos`, the machine has 4 cores (or as many as there are) |
| `--ioapic`, `--planificador P`, `--uacpi` | interrupts through the IOAPIC; scheduler (`rr`, `afinidad`, `equilibrio`, `prioridad`); ACPI interpreter and battery indicator. With any of these, `--smp` or `--uacpi`, `build/saguios-opciones.img` is created on the fly and yours is left untouched |
| `--sonido`, `--ac97` | sound through PulseAudio (works on WSLg): PC speaker and an Intel HDA card (or AC'97 with `--ac97`) |
| `--usb`, `--cd`, `--pendrive ARCHIVO`, `--xhci`, `--hid-usb` | boot as a USB stick or from the CD (`build/saguios.iso`); attach a FAT32 data stick; USB 3.0 controller; USB keyboard and mouse |
| `--mem MB`, `--cpu MODELO`, `--gpu MODELO`, `--vram MB` | memory (2048 by default), CPU model (`pentium3`...), graphics card (`std`, `virtio`, `vmware`) and its memory |
| `--usb-real VID:PID`, `--wifi-usb` | pass a real USB device from the PC through (on WSL, with `usbipd`; see `docs/usb-real.md`) |
| `--raton-relativo`, `--serie`, `--vnc`, `--guardar` | mouse as on a real laptop; kernel log in this terminal; no window (VNC on `localhost:5900`); keep changes to the boot disk (discarded by default) |
| `--sin-kvm` | no KVM acceleration. KVM is used by default if `/dev/kvm` exists; otherwise software emulation (slower) |

### 3. Build options

They go on the `make` command line: `make NETSURF=0 B=build-mia -j8`. If you change an option, use a different `B=` (or `make limpar`) so objects are not mixed.

**`Makefile` variables:**

| Variable | Default | What it does |
| --- | --- | --- |
| `B` | `build` (with `SEM_GPL=1`, `build-sem-gpl`; with `SEM_FIRMWARE=1`, `build-sem-firmware`) | build output directory |
| `-j N` | (one) | parallel `make` jobs. `gerar_versao.sh` uses all cores by default |
| `OTIMIZACAO` | `-O2` | optimization level (`-O0`, `-O1`, `-O2`, `-O3`, `-Os`) |
| `PUBLICABLE` | `0` | `1` leaves out what cannot go into a GPL v3 binary: NetSurf, GNUBoy and the Linux-derived r8169 (the driver uses its own classic init). `gerar_versao.sh` sets it for you unless you use `--con-input-share` |
| `SEM_GPL` | `0` | `1` leaves out every GPL-licensed third-party port (OpenTyrian, NetSurf, GNU Prolog, OpenJazz, Vanilla Conquer, Quake, GNUBoy, DOSBox, Linux), and also Tzar, TCC, GNU make and the C Lab. Builds in `build-sem-gpl/` |
| `SEM_FIRMWARE` | `0` | `1` leaves out the binary firmware (`firmware/`: the RTL8188EU Wi-Fi dongle's, the USB Wi-Fi and Bluetooth firmware, and the Radeon R5xx, R6xx and R7xx microcode for GPU 3D). Builds in `build-sem-firmware/` |
| `MICROPYTHON` | `1` | `0` builds the image without MicroPython (considerably smaller) |
| `NETSURF` | `1` | the NetSurf browser; only included if the sources are prepared and the glue exists (see 7). `NETSURF=0` removes it |
| `GNUBOY` | `1` | the Game Boy emulator; only included if its code has been downloaded (`make priv_descargas` or `bash tools/gnuboy_fuentes.sh`, see 5 and 7). `GNUBOY=0` removes it |
| `MGBA` | `1` | the Game Boy Advance, Game Boy and Game Boy Color emulator, mGBA (MPL-2.0; its sources are in git). Also included with `PUBLICABLE=1`. `MGBA=0` removes it |
| `LAINES`, `GEARSYSTEM` | `1` | the NES emulator (LaiNES) and the Master System, Game Gear and SG-1000 emulator (Gearsystem), in C++ with SDL 2; without `g++` they are not included. `0` removes them; with `SEM_GPL=1` they are not included either |
| `SHINE` | `1` | MP3 encoder shine (LGPL-2.0+, D339) of the Audio converter (`convertir-audio`). `SHINE=0` removes it (the program works the same, without MP3 output); with `SEM_GPL=1` it is not included either |
| `QUAKE` | `1` | the Quake engine (`0` removes it) |
| `OPENJAZZ`, `CONQUER`, `DOSBOX`, `GPROLOG` | `1` | OpenJazz, Command & Conquer (Vanilla Conquer), DOSBox (with the Recreativa launcher) and GNU Prolog (`0` removes them) |
| `QUAKE_DATOS`, `OPENJAZZ_DATOS` | `1` | put the shareware data downloaded by `tools/quake_datos.sh` and `tools/openjazz_datos.sh` into `/jogos`. If they were not downloaded, nothing is added; `0` ignores them even if downloaded |
| `CNC_VIDEOS` | `0` | `1` also adds the C&C movies (840 MB more: raise `JOGOS_MB`, for example `JOGOS_MB=2000`) |
| `INPUT_SHARE` | `input_share` | folder with your games and apps (see 6). `INPUT_SHARE=` (empty) ignores it; another path also works |
| `JOGOS` | `1` | `0` removes the `SAGUI-JOGOS` games partition from every image |
| `JOGOS_MB` | `800` | size of `SAGUI-JOGOS` in the persistent images, the partitioned one and what the installer creates (`0` = just enough) |
| `JOGOS_IMG_MB` | `0` | the same for `saguios.img`, `-smp` and `-uacpi` (`0` = just enough for what it holds) |
| `APPS`, `APPS_MB` | `1`, `0` | `APPS=0` puts the applications inside the ramdisk (`/apps`, single-block image); `APPS_MB` is the size of the `SAGUI-APPS` partition (`0` = just enough plus a margin). `convertir-audio`, `traductor` and the database explorers go in the applications partition (`/apps`); the explorers run `/apps/bin/python`. |
| `RAIZ_DISCO` | `1` | (D379, since v8.5.3.1) the images boot with a minimal initramfs (`build/initramfs.sgfs`, < 1 MB) and the whole system, `/apps` included, in the `SAGUI-RAIZ` partition (ext2): live from the medium (read-only root with the changes in RAM, or in `SAGUI-PERSIST` on the persistent images); the ISO copies it to RAM (toram). `0`: all of them with the old ramdisk (as up to v8.5.3) |
| `PERSIST_MB` | `256` | size of `SAGUI-PERSIST` (the live persistent layer) on the persistent images |
| `RAMDISK` | `base` | `completo`: the old ramdisk with the whole system and the applications inside (like `APPS=0` and `RAIZ_DISCO=0`; it is `saguios-completo.img`). It is passed the same way with `gerar_versao.sh`, for example `RAMDISK=completo ./gerar_versao.sh` |
| `DATOS_MB` | `64` | `/home` of the ext2 persistent image |
| `DATOS_FAT32_MB` | `2048` | `/home` of the FAT32 persistent image |
| `HOME_MB`, `TMP_MB`, `LOGS_MB` | `2048`, `256`, `128` | partitions of the partitioned disk (`imagem-particoes`) |
| `PART_FS` | `fat32` | file system of those partitions (`fat32` or `ext2`) |
| `PART_EXT` | (empty) | `--primarias`: the pre-v7.3 layout (by default, all logical inside an extended partition) |
| `DESTINO`, `PRIV_DESTINO` | `entrega/latest` | output folder of `make variantes` and `make priv_variantes` |
| `GERAR_OPC` | (empty) | extra options for `gerar_versao.sh` from those two targets (for example `GERAR_OPC="--probar"`) |
| `FUENTES_RED` | (unset) | `1`: if the Typodermic zip is missing, download it (see 5) |
| `FREECIV` | `1` | Freeciv 3.1.6 (SDL2 client and local server, in Spanish, D374): only included if the sources are prepared (see 5). `FREECIV=0` removes it; with `SEM_GPL=1` it is not included either |
| `DOOM` | `1` | Chocolate Doom 3.1.1 with Heretic, Hexen and Strife (D383): only included if the sources are prepared (`tools/doom_fuentes.sh`). `DOOM=0` removes it; with `SEM_GPL=1` it is not included either. Freedoom (BSD-3-Clause, `tools/doom_datos.sh`) goes to `/apps/doom`, also in the public images; the original WADs only through `input_share/games/DOOM` |
| `GIT` | `1` | SaguiOS git (`/apps/bin/git`, D400): only included if the libgit2 sources are there (`tools/git_fuentes.sh`, see 1 and 5). `GIT=0` removes it; with `SEM_GPL=1` it is not included either |
| `MAKE_DENTRO` | `1` | SaguiOS GNU make (`/apps/bin/make`, D413): only included if its sources are there (`tools/make_fuentes.sh`, see 1 and 5). `MAKE_DENTRO=0` removes it; with `SEM_GPL=1` it is not included either |
| `LABC` | `1` | the C Lab (`/apps/bin/labc`, D414): only included with TCC. `LABC=0` removes it (and `TCC=0` does too) |
| `AWK_DENTRO` | `1` | The One True AWK (`/bin/awk`, D440): only included if its sources are there (`tools/awk_fuentes.sh`, see 1 and 5). `AWK_DENTRO=0` removes it; with `SEM_GPL=1` it is included too |
| `LUA_DENTRO` | `1` | Lua 5.4 (`/apps/bin/lua` and `luac`, `liblua.a` and `liblua.so.5.4`, D440): only included if its sources are there (`tools/lua_fuentes.sh`, see 1 and 5). `LUA_DENTRO=0` removes it; with `SEM_GPL=1` it is included too |
| `GO` | `1` | programs in Go with TinyGo (`/apps/go/`, see 1). Without TinyGo installed they are skipped with a notice; `GO=0` disables them |
| `OPENTTD_REPO`, `OPENTTD_URL` | `../openttd-saguios` | only for `make priv_descargas`: where the OpenTTD repository is (or where to clone it from). Without it the private image does not carry OpenTTD (see 5) |

**`gerar_versao.sh`** (builds, assembles the images and leaves the delivery with SHA-256 sums; `./gerar_versao.sh --help`):

| Option | Default | What it does |
| --- | --- | --- |
| `--destino DIR` | `entrega/latest` | delivery folder |
| `--publicar` | no | also copies the delivery to `entrega/v<version>` |
| `--iso` | no | adds `saguios.iso` |
| `--fat32 [MB]` | no (2048) | adds the persistent image with FAT32 `/home` (`.xz`) |
| `--ext2 [MB]` | no (64) | adds the persistent image with ext2 `/home` |
| `--smp` | no | adds `saguios-smp.img` and, with `--ext2`/`--fat32`, the multicore persistent ones |
| `--uacpi` | no | adds `saguios-uacpi.img` and, with `--smp` and `--ext2`, `saguios-persistente-smp-uacpi.img` |
| `--particiones` | no | adds the disk with `/home`, `/tmp` and `/logs` as partitions (`.img.xz`) |
| `--sin-gpl` | no | adds the variant without GPL ports (it first runs `tools/auditar_licencias.py`) |
| `--todas` | no | everything above |
| `--variante V` | `gpl` | variant of the main image: `gpl`, `sem-gpl` or `sem-firmware` |
| `--sin-python` | no | without MicroPython (builds in `build-sem-python/`) |
| `--otimizacao O` | `-O2` | optimization level |
| `--jobs N`, `-j N` | PC cores | `make` jobs |
| `--limpiar` | no | deletes the build folder before building |
| `--sin-compilar` | no | does not build: uses what is already in the build folder |
| `--con-input-share` | no | adds `input_share/`, the downloaded shareware data, NetSurf and GNUBoy. **Personal use only** (see 6 and 7) |
| `--probar` | no | boots the image in QEMU and checks it reaches the desktop |
| `--sin-notas`, `--notas FICHERO` | notes on | does not write `VERSION.txt`, or appends the text of a file to it |
| `--fijar-version V` | no | sets the version in `kernel/inc/versao.h` before building (`v8.1`...) |

Without `--con-input-share`, `gerar_versao.sh` builds with `INPUT_SHARE= OPENJAZZ_DATOS=0 QUAKE_DATOS=0 NETSURF=0 GNUBOY=0 PUBLICABLE=1`: that is the **publishable** build. Sizes are changed with the variables in the table above, for example `HOME_MB=8192 JOGOS_MB=64 ./gerar_versao.sh --particiones --smp`.

**`tools/mkimagem.py`** (called by the `Makefile`; `python3 tools/mkimagem.py --help`). Options useful to a user; the ones named «prueba» or `--teste` exist only for the automated tests:

| Option | Default | What it does |
| --- | --- | --- |
| `--vesa` | (set by the Makefile) | boot stage 2 switches to VESA graphics mode |
| `--smp` | off | starts the secondary processors (without it, the system uses one) |
| `--ioapic` | off | device IRQs through the IOAPIC instead of the 8259 PIC |
| `--uacpi` | off | AML interpreter (uACPI): embedded controller and battery indicator |
| `--sem-acpi` | off | do not use ACPI: shutting down only warns, no power button |
| `--planificador P` | `rr` | scheduler at boot: `rr`, `afinidad`, `equilibrio` or `prioridad` |
| `--resolucion LxA`, `--bpp N` | automatic (1024x768, 32 bpp) | preferred resolution and colour depth |
| `--sem-wc` | off | do not enable write-combining on the framebuffer |
| `--sem-pae` | off | classic 2-level paging even if the CPU has PAE: up to 4 GB of RAM and no NX (for testing or for troublesome hardware) |
| `--sem-nx` | off | do not enable NX (non-executable stack and heap) even if the CPU has it |
| `--sem-avx` | off | (D381) the kernel does not turn on AVX (XSAVE and the YMM registers) even if the CPU has it: everything stays as before (FXSAVE and SSE paths). From the system, `kconfig avx` with `si` or `no` |
| `--cache-disco-baja` | off | keep the disk cache in low memory (up to 64 MB) even if there is high memory (in case the hardware has problems) |
| `--sin-swap` | off | no compressed swap in RAM: when memory runs out, the program fails as before |
| `--swap-mb N` | 0 (automatic) | cap of the compressed swap store in MB (1-1024); automatic = half the RAM with high memory, a quarter without it, at most 1 GB |
| `--sgl-motor software` | `auto` | disables all hardware 3D acceleration |
| `--acel2d {auto,cpu,intel,nvidia,radeon,tdfx,vmware}`, `--sin-acel2d` | `auto` | the GPU's 2D accelerator; `cpu` or `--sin-acel2d` = CPU only |
| `--sin-servicios` | off | do not start the service manager or cron at boot |
| `--rescate` | off | boot in rescue mode: a console with a root shell, no desktop |
| `--sesion-root` / `--sesion-sagui` | `--sesion-sagui` | desktop as root or as the `sagui` user |
| `--sin-frecuencia` | off | without the CPU frequency driver |
| `--console` | off | no desktop: full-screen terminal |
| `--sin-conf` | off | without the `SAGUI-CONF` partition (16 MB, system configuration). The ISO does not have it |
| `--dados-mb N`, `--dados-fs {ext2,fat32}` | 0 (none) | the `/home` partition (`SAGUI-DATOS`) |
| `--tmp-mb N`, `--logs-mb N`, `--tmp-fs`, `--logs-fs` | 0 | the `/tmp` and `/logs` partitions |
| `--extra-mb N` | none | extra partition with no fixed folder; can be repeated |
| `--esquema {logicas,primarias,mixto}`, `--primarias` | `logicas` | where the data partitions go |
| `--jogos CARPETA`, `--jogos-mb N` | — | games partition with the folder's contents (the `Makefile` already does this) |
| `--apps CARPETA`, `--apps-mb N` | — | applications partition (ditto) |
| `--part-fs {ext2,fat32}` | `ext2` | file system of the partitions without their own `--*-fs` |
| `--cd` | off | image for the ISO (used by `make iso`) |
| `--raiz CARPETA`, `--raiz-mb N` | — | (D379) the system root in the `SAGUI-RAIZ` partition (ext2) with the folder's contents (`build/rootfs`; with `--apps`, `/apps` goes inside); `--ramdisk` is then the initramfs (`tools/mkinitramfs.py`). The `Makefile` already does it |
| `--persist-mb N` | 0 | (D379) `SAGUI-PERSIST` partition: the live persistent layer |
| `--modo-raiz {auto,disco,live,toram,ram}` | `auto` | (D379) forces how the root is used (by default, what the initramfs says; with `--cd`, `toram`) |
| `--raiz-sin-comprimir` | off | (D379, v8.5.4.2) with `--cd`, the root goes as a plain ext2; by default it is compressed in blocks (the ISO is half the size) |

The GPU power options (D399), `--gpu-energia-simulada` and `--gpu-energia-prueba`, are only for the automatic tests in QEMU and are not in the table.

A disk with custom partitions: `python3 tools/crear-disco.py -i` (wizard) or `make imagem-particoes-xz HOME_MB=4096 TMP_MB=512 LOGS_MB=256 PART_FS=fat32`. Like every disk image, it boots by BIOS and by UEFI (D270; `crear-disco.py --sin-uefi`: BIOS only).

PC-side tool: `tools/conversor_video_pc.py` is a windowed video converter (Python 3 + tkinter, with ffmpeg) that prepares videos for the SaguiOS Player; it is not part of the build. See `docs/convertir-video.md`.

### 4. Targets: which image each one produces

| Command | What it produces |
| --- | --- |
| `make` / `make all` | `build/saguios.img` (the normal one, single core) |
| `make help` | lists the main targets and variables |
| `make vdi`, `make iso` | `build/saguios.vdi`, `build/saguios.iso` |
| `make imagem-smp`, `imagem-uacpi` | `build/saguios-smp.img`, `build/saguios-uacpi.img` |
| `make imagem-persistente`, `imagem-persistente-fat32`, `imagem-persistente-smp`, `imagem-persistente-smp-uacpi` | the persistent images (with `/home`) in `build/` |
| `make imagem-particoes`, `imagem-particoes-xz` | `build/saguios-particiones.img` (and `.xz`) |
| `make variantes [DESTINO=entrega/mia]` | **the publishable build**: `./gerar_versao.sh --todas --destino DESTINO`: every image in the table below, without `input_share/`, NetSurf, GNUBoy or shareware data |
| `make priv_variantes [PRIV_DESTINO=...]` | **personal use**: the same with `--con-input-share` (your games, Quake and Jazz shareware, NetSurf and GNUBoy, which it downloads first). It first runs `make priv_descargas` (see 5 and 7) |
| `make entrega-sin-gpl` | `build-sem-gpl/saguios-sin-gpl.img`, after the license audit |
| `./gerar_versao.sh --todas` | the same as `make variantes`, with the default destination `entrega/latest` |

Files that `make variantes` leaves in the delivery folder:

| File | What it is for |
| --- | --- |
| `saguios.img` | The normal one (single core). **Always start with this one** |
| `saguios-smp.img` | Multicore (all processors, no IOAPIC) |
| `saguios-uacpi.img` | With the ACPI interpreter (uACPI): battery indicator and `_PSS`/`_PRT` on laptops |
| `saguios-completo.img` | With the whole system and the applications in memory (the old ramdisk), for PCs whose disk controller does not find the boot disk (D324); it needs about 64 MB more RAM and has no Freeciv, OpenTTD or DOOM. Since 8.4.3r2 |
| `saguios-persistente-fat32.img.xz` | With a 2 GB FAT32 `/home`, also readable from Windows. The best one for keeping files |
| `saguios-persistente-fat32-smp.img.xz` | The same, multicore |
| `saguios-persistente.img`, `-smp`, `-smp-uacpi` | With an ext2 `/home` (Windows cannot see it). They combine multicore and uACPI |
| `saguios-particiones.img.xz` | `/home`, `/tmp` and `/logs` as partitions (the kernel keeps its log in `/logs`) |
| `saguios-sin-gpl.img` | Without GPL third-party ports |
| `saguios.iso` | Bootable CD (live mode, no `SAGUI-CONF`): the system, with `/apps` and Freeciv, is copied to RAM at boot (toram) and the CD can be removed; with little RAM it runs from the CD |
| `saguios.vdi` | VirtualBox |
| `SHA256SUMS.txt`, `VERSION.txt` | checksums and a data sheet of what was built |

Since v8.5.3.1 (D379) every disk image carries the system in `SAGUI-RAIZ` (ext2, with `/apps` inside: there is no `SAGUI-APPS` partition any more), plus `SAGUI-JOGOS` (games, `/jogos`) and `SAGUI-CONF` (configuration, 16 MB); the persistent ones also `SAGUI-PERSIST`. They boot live from the medium (`docs/imagenes.md`). With `RAIZ_DISCO=0` they come out as up to v8.5.3 (ramdisk with the base system and `SAGUI-APPS`).

Since v8.5.4.10r2 (D388) the order on the disk is: boot, the ESP, and the extended partition last, with `SAGUI-CONF`, `SAGUI-RAIZ`, `SAGUI-PERSIST`, `SAGUI-JOGOS`… and `/home` (`SAGUI-DATOS`) last of all, as a logical partition: written on a larger disk, “Expand /home” stretches it to the end without moving anything. `mkimagem.py --orden-antiguo` gives the previous layout (for tests only).

### 5. What is downloaded, and when

**A plain `make` downloads nothing from the network.** Everything below is run by you, or by `make priv_descargas` / `make priv_variantes`. Every script checks the SHA-256 of what it downloads and does not download again if it is already done; with no network they fail with a message and the system is built anyway without that piece (the shareware data and the typefaces are optional).

| What | Script | Source | License | Where it ends up |
| --- | --- | --- | --- | --- |
| NetSurf 3.11 sources (about 90 MB) | `bash tools/netsurf_fuentes.sh [--tarball FILE] [--forzar]` | `https://download.netsurf-browser.org/netsurf/releases/source-full/netsurf-all-3.11.tar.gz` | GPL-2.0 v2 only (the libraries, MIT). See 7 | `userland/terceros/netsurf/fuentes/` (ignored by git). Needs `perl` |
| GNUBoy sources (SDL2-GNUBoy, commit `c8642d1`, about 450 KB) | `bash tools/gnuboy_fuentes.sh [--tarball FILE] [--forzar] [--comprobar]` | `https://github.com/AlexOberhofer/SDL2-GNUBoy/archive/c8642d11bd2706333c5255e2b7f2b84445d0a070.tar.gz` | GPL-2.0 (v2 only in practice). See 7 | `userland/terceros/gnuboy/jogo/` |
| Quake 1.06 shareware (episode 1) | `bash tools/quake_datos.sh [--zip FILE] [--forzar]` | `ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake/quake106.zip` (and the `youfailit.net` mirror) | id Software data: free for the user, **no right to redistribute it inside another system** | `userland/terceros/quake/datos/` (ignored by git) |
| Jazz Jackrabbit 1.0 shareware (episode 1) | `bash tools/openjazz_datos.sh [--zip FILE] [--forzar]` | `https://slackware.uk/~urchlay/src/jjrabbit.zip` | Epic MegaGames data, with redistribution conditions: not published | `userland/terceros/openjazz/datos/` (ignored by git) |
| Typodermic typefaces (28 chosen out of 731) | `python3 tools/fuentes_typodermic.py seleccion DESTINO` (with network, it downloads the zip); `make priv_descargas` does it for you | `https://typodermicfonts.com/wp-content/uploads/2024/12/typodermic-public-domain-2024-12.zip` (**sometimes gives 404**: see the note) | CC0 (public domain) | `NEW_FONTS/typodermic-public-domain-2024-12.zip` |
| Free Game Boy ROM (Libbet) | `bash tools/gnuboy_roms_libres.sh` | `github.com/pinobatch/libbet`, version v0.08 | zlib | `userland/terceros/gnuboy/roms-libres/` (only useful with GNUBoy, see 7) |
| Freeciv 3.1.6 and zlib 1.3.1 sources (about 90 MB) | `bash tools/freeciv_fuentes.sh [--tarball FILE] [--zlib-tarball FILE] [--forzar]` | `https://github.com/freeciv/freeciv/archive/refs/tags/R3_1_6.tar.gz` (zlib: `https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz`) | GPL-2.0 or later (Freeciv); zlib license (zlib) | `userland/terceros/freeciv/fuentes/` and `userland/terceros/zlib/fuentes/` (ignored by git) |
| Chocolate Doom 3.1.1 and Freedoom 0.13.0 sources (about 12 and 57 MB) | `bash tools/doom_fuentes.sh [--tarball FILE] [--forzar]` and `bash tools/doom_datos.sh [--zip FILE] [--forzar]` | `https://github.com/chocolate-doom/chocolate-doom/archive/refs/tags/chocolate-doom-3.1.1.tar.gz` and `https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip` | GPL-2.0 or later (Chocolate Doom); BSD-3-Clause (Freedoom) | `userland/terceros/chocolatedoom/fuentes/` and `datos/` (ignored by git) |
| libgit2 1.9.1 sources (the `git` command, D400) | `bash tools/git_fuentes.sh [--tarball FILE]` | `https://github.com/libgit2/libgit2/archive/refs/tags/v1.9.1.tar.gz` | GPL-2.0 with linking exception | `userland/terceros/git/fuentes/` (ignored by git) |
| GNU make 4.4.1 sources (`/apps/bin/make`, D413) | `bash tools/make_fuentes.sh [--tarball FILE]` | `https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz` | GPL-3.0-or-later | `userland/terceros/make/fuentes/` (ignored by git) |
| The One True AWK 20261106 sources (`/bin/awk`, D440) | `bash tools/awk_fuentes.sh [--tarball FILE]` | `https://github.com/onetrueawk/awk/archive/refs/tags/20261106.tar.gz` | Lucent permissive (HPND-style) | `userland/terceros/awk/fuentes/` (ignored by git) |
| Lua 5.4.9 sources (`/apps/bin/lua` and `luac`, D440) | `bash tools/lua_fuentes.sh [--tarball FILE]` | `https://www.lua.org/ftp/lua-5.4.9.tar.gz` | MIT | `userland/terceros/lua/fuentes/` (ignored by git) |
| CC0 3D models for the SaguiGL demo (`sgldemo -p --modelo`, D404) | `bash tools/modelos3d_datos.sh [--forzar]` | `https://opengameart.org/content/low-poly-biplane`, `https://opengameart.org/content/funky-aircraft` and `https://opengameart.org/content/ww1-wwi-airplane-model-obj` | CC0 1.0 (public domain) | `userland/terceros/modelos3d/datos/` (ignored by git); not in the images |

Notes:

- **Typodermic:** `make priv_descargas` (or `python3 tools/fuentes_typodermic.py seleccion /tmp/fuentes-prueba`) downloads the zip into `NEW_FONTS/` if it is missing. If the website gives 404 (it did on 2026-11-14), the script says so clearly and **breaks nothing**: the system is built anyway without those fonts. To get them, download the zip by hand and put it at `NEW_FONTS/typodermic-public-domain-2024-12.zip` (the SHA-256 is checked). `make` only includes the fonts if the zip is already in `NEW_FONTS/` when you build (that folder is not in the public repository).
- **`make priv_descargas`** runs, in this order: `tools/netsurf_fuentes.sh` (if there are no prepared sources), `tools/gnuboy_fuentes.sh` (if `jogo/` is missing), `tools/quake_datos.sh`, `tools/openjazz_datos.sh` and the download of the Typodermic fonts (if it fails, it warns and goes on). It works the same with the public repository: the NetSurf and GNUBoy glue is in it.
- **`make priv_descargas`** also runs `tools/doom_fuentes.sh` and `tools/doom_datos.sh` (D383). Without `userland/terceros/chocolatedoom/fuentes/.preparado` the `Makefile` skips DOOM with a warning.
- **Freeciv** is not downloaded by `make priv_descargas`: run `bash tools/freeciv_fuentes.sh` yourself. Without `userland/terceros/freeciv/fuentes/.preparado` the `Makefile` does not build Freeciv, and it does not say so (`FREECIV=0` removes it). Its game data goes to `/apps/freeciv`. It does not depend on `--con-input-share`: if the sources are prepared, Freeciv also goes into `make variantes`; download them only if you want it in your images.
- **OpenTTD** (optional, private images only): the final step of `make priv_descargas` builds it as a package from its own repository (by default `../openttd-saguios`, or `OPENTTD_URL`) with `tools/openttd_paquete.sh` and installs it in `input_share/apps`. Without that repository it warns, and the private image comes out without it. The public images (`INPUT_SHARE=`) never carry it.
- **Go** (optional): `tools/tinygo_entorno.sh instalar` downloads TinyGo and Go, at fixed versions with checked SHA-256, into `~/.local/saguitools` (no sudo). Without it, `make` skips the Go examples with a notice.
- **Already in the repository, not downloaded:** the root certificates (`rootfs/etc/ssl/certs/`; they can be regenerated from the PC's `ca-certificates` store with `python3 tools/raices_ssl.py`), the RTL8188EU Wi-Fi dongle firmware (`firmware/realtek/`, from linux-firmware, redistributable unchanged; `tools/firmware_rtl8188eu.sh --comprobar` verifies it and without `--comprobar` it downloads it again from `git.kernel.org`; `SEM_FIRMWARE=1` removes it), the USB Wi-Fi dongles' firmware (`firmware/realtek/rtl8192cufw_A.bin`, `firmware/realtek/rtl8192cufw_TMSC.bin` and `firmware/ralink/rt2870.bin`, from linux-firmware, redistributable unchanged; `tools/firmware_wifi_usb.sh --comprobar` verifies them and without `--comprobar` it downloads them again; `SEM_FIRMWARE=1` removes them), the Bluetooth controller firmware (`firmware/realtek/rtl_bt/` and `firmware/atheros/`, from linux-firmware, redistributable unchanged; `tools/firmware_bt.sh --comprobar` verifies them and without `--comprobar` it downloads them again; `SEM_FIRMWARE=1` removes them; Broadcom `.hcd` patches are not in the repository and go by hand into `/lib/firmware/brcm/`), the Radeon R5xx, R6xx and R7xx microcode for GPU 3D (`firmware/radeon/`, from linux-firmware, redistributable unchanged; `tools/firmware_radeon.sh --comprobar` verifies it and without `--comprobar` it downloads it again; `SEM_FIRMWARE=1` removes it), the OpenTyrian data (`userland/terceros/opentyrian/dados.img.xz`), the OpenJazz, Quake and Vanilla Conquer sources (checked with `tools/openjazz_fuentes.sh --comprobar`, `quake_fuentes.sh`, `conquer_fuentes.sh`) and the CHIP-8 ROMs, which are assembled from `rootfs/jogos/chip8/fuentes/*.asm`.

### 6. Games and data you provide (`input_share/`)

`input_share/` is a folder in the repository root (create it yourself; git ignores it and it is **never uploaded**). What you put there goes into the images you build with a plain `make`, with `make priv_variantes` or with `./gerar_versao.sh --con-input-share`. It does not go into `make variantes`, on purpose (so a published delivery never carries your games by accident).

```
input_share/
  games/
    Quake/id1/PAK0.PAK       full Quake (also PAK1.PAK, registered version)
    Jazz/                    full Jazz Jackrabbit (e.g. the GOG one), as it is
    CnC/CnC_GDI95.iso        Command & Conquer Gold: GDI (volume GDI95)
    CnC/CnC_NOD95.iso        ... and NOD (optional: without it, only the GDI campaign)
    gb/juego.gb              Game Boy / Game Boy Color ROM (.gb, .gbc)
    <other>/                 any other game: it ends up in /jogos/<other>
  Tzar Anthology/            Tzar: Packs, Maps, Campaign, intro.MPG, Tzar.ini
  apps/
    bin/                     SaguiOS executables you want in /apps/bin (on the PATH)
    <app>/                   each app's data, in /apps/<app>
```

| Game | What to put and where | Notes |
| --- | --- | --- |
| **Quake** | `input_share/games/Quake/id1/pak0.pak` and `pak1.pak` | Ends up in `/jogos/Quake/id1/`. A loose `pak0.pak` in `games/Quake/` also works, and upper-case names (`ID1/PAK0.PAK`, as on the DOS CD). Expansions go in `games/Quake/hipnotic/` and `games/Quake/rogue/` (started with `quake -hipnotic` or `-rogue`). Use your legal copy (Steam, GOG, the CD). The shareware is downloaded by `tools/quake_datos.sh`; Quake prefers the full version |
| **Jazz Jackrabbit** | `input_share/games/Jazz/` with the game's files, unchanged | The GOG or another full version. OpenJazz finds it in any folder under `/jogos` and prefers it to the shareware |
| **Command & Conquer** | `input_share/games/CnC/CnC_GDI95.iso` and `CnC_NOD95.iso` | These are the ISOs of **Command & Conquer Gold for Windows 95**, EA's 2007 freeware giveaway, one per disc; they are recognised by their volume (GDI95, NOD95), not by file name. `tools/cnc_extraer.py` pulls out of them only what Vanilla Conquer uses (63 MB in `/jogos/CnC`; with `CNC_VIDEOS=1`, 840 MB more). Without the ISOs, you can copy the folder of an installed C&C95 to `/jogos/CnC` |
| **Game Boy** | `input_share/games/gb/*.gb` or `*.gbc` | Only ROMs you have the right to use. GNUBoy looks for them; it is not in the published images, only in your own personal-use ones (see 7) |
| **Tzar** (Tzar Anthology, D417) | `input_share/Tzar Anthology/` with its `Packs/`, `Maps/` and `Campaign/` folders, and `intro.MPG` and `Tzar.ini`; `Tzar.exe` and the DLLs are not copied | Ends up in `/jogos/TZAR/`. Only with `INPUT_SHARE` (public images do not include it). It needs about 1,500 MB of `JOGOS_MB`: `JOGOS_MB=1600` is a comfortable value |
| Others | `input_share/games/<folder>/` | Copied as is to `/jogos/<folder>`. If the folder is named like a system one (`opentyrian`, `openjazz`), its contents are laid over it |

Details: the `Makefile` watches `input_share/games` and `input_share/apps` and rebuilds the partitions when names, sizes or dates change; Windows `*:Zone.Identifier` marks are not copied. Another folder: `make INPUT_SHARE=/path/to/my_input_share`; none: `make INPUT_SHARE=`. The full reference is `docs/input_share.md`.

**Sizes.** `SAGUI-JOGOS` is read-write ext2 mounted on `/jogos`. In `saguios.img`, `-smp` and `-uacpi` it is just big enough for its contents (`JOGOS_IMG_MB=0`): with full Quake, Jazz and C&C the images come out around 250 MB. In the persistent ones, the partitioned one and the installer it is **800 MB** (`JOGOS_MB=800`): raise the value if your games are bigger.

**Copying them later to an already installed SaguiOS.** From inside SaguiOS, copy the game's folder to `/jogos/<folder>` (with **Archivos** or `cp`) from a FAT32 stick or from `/home`. The games also look in `jogos/<folder>` on every mounted disk (`/usb`...). On Windows, the `SAGUI-DATOS` drive of a FAT32 persistent image is `/home`: handy for moving files.

> **Legal notice.** The data of commercial games (full Quake, Jazz Jackrabbit, Command & Conquer, Game Boy ROMs, Tzar Anthology) is **not redistributed with SaguiOS and is not in this repository**, and may not be shared. Each person uses their own legal copies. An image built with `input_share/` is **for personal use**: do not publish or share it.

### 7. What is not in the public repository, and why

SaguiOS-Extended is **GPL v3** (`LICENSE`, `docs/licencia.md`). Four pieces are **GPL v2 only** (no «or later»), and v2 alone is incompatible with v3: they cannot be linked into one binary with the common libsagui and libc, so **their code is not published** and is not in the published images. What is ours is published: the glue with SaguiOS (`netsurf.mk`, `gnuboy.mk`, `plataforma/`, `libc/`, with their `GPL-2.0-or-later` SPDX) and the scripts that download the original code; so anyone can build them **for personal use** (using it privately is not distributing):

| Piece | License | What happens in the public repository |
| --- | --- | --- |
| **NetSurf** (browser, `userland/terceros/netsurf/`) | GPL-2.0 v2 only | its code (`fuentes/`) is not there: `tools/netsurf_fuentes.sh` downloads it; our glue is there |
| **GNUBoy** (Game Boy, `userland/terceros/gnuboy/`) | GPL-2.0 (almost no «or later»: treated as v2 only) | its code (`jogo/`) is not there: `tools/gnuboy_fuentes.sh` downloads it (pinned commit, SHA-256 checked); our glue is there |
| **Linux-derived r8169** (`kernel/terceros/linux/r8169/`) | GPL-2.0-only | not there; the driver for the Realtek RTL8168/8111 card uses its own classic init (`kernel/rede/r8169_sem_gpl.h`): it works, but without the init sequences of the newest chips (RTL8111G/H) |
| **Freeciv** (strategy game, `userland/terceros/freeciv/`) | GPL-2.0 or later | its code (`fuentes/`, about 90 MB) is not in git: `tools/freeciv_fuentes.sh` downloads it; our own glue (`freeciv.mk`, `plataforma/`, `parches/`, `traducciones/`) is there |
| **Chocolate Doom** (DOOM, Heretic, Hexen and Strife, `userland/terceros/chocolatedoom/`) | GPL-2.0 or later | its code (`fuentes/`) is not in git: `tools/doom_fuentes.sh` downloads it; our own glue (`chocolatedoom.mk`, `plataforma/`, `parches/`) is. Freedoom (BSD-3-Clause) is downloaded by `tools/doom_datos.sh` |
| **libgit2** (the `git` command, `userland/terceros/git/`) | GPL-2.0 with linking exception | its code (`fuentes/`) is not in git: `tools/git_fuentes.sh` downloads it with a fixed SHA-256; our own glue (`git.c`, `git.mk`, `plataforma/`) is there |
| **OpenTTD** (game, separate repository `../openttd-saguios`) | GPL-2.0 only | not in this repository nor in the public images: it is built as a separate package and only goes into the private images (D375) |

**Using them privately is allowed; redistributing those images is not.** Building an image with them and using it yourself is not distributing (`docs/licencia.md`).

**How to build them for yourself with only the public repository.** Verified for real from an exported tree without git (`tools/publicar.sh preparar`, then `make` and `make priv_variantes INPUT_SHARE=`):

- plain `make` works without NetSurf or GNUBoy: the menu does not show them.
- `make priv_variantes INPUT_SHARE=` (or `make priv_descargas` and then `make`) **downloads the original code** of NetSurf 3.11 and SDL2-GNUBoy, checks their SHA-256, builds them with our glue and puts them in the **private** images. It needs network and `perl` (NetSurf). Those images are for you only: **do not redistribute them** (NetSurf and GNUBoy are GPL v2; linked with the libsagui, which is GPL v3, they cannot be distributed).
- **NetSurf:** `bash tools/netsurf_fuentes.sh` leaves `userland/terceros/netsurf/fuentes/.preparado`; the `Makefile` includes `netsurf.mk` only if both the sources and the glue exist.
- **GNUBoy:** `bash tools/gnuboy_fuentes.sh` leaves `userland/terceros/gnuboy/jogo/` (`--comprobar` compares it with the original; if there are ever own patches, they go in `userland/terceros/gnuboy/parches/*.patch` and the script applies them).
- **r8169:** the files are adapted extracts of Linux 6.6's `drivers/net/ethernet/realtek/r8169_main.c`; no script regenerates them. Without them the driver keeps working with the classic init.

The rest of `kernel/terceros/linux/` (`hda`, `hwmon`, GPL-2.0 or later) is in the repository. Also left out, for size or privacy, are `entrega/` (each version's images go to the project's «Releases» page), `input_share/`, `NEW_FONTS/`, `testes_hw/` and the internal notes (`tools/publicar-excluir.txt`).

### 8. Writing the image and testing it

**First, test it in QEMU** (nothing is written to any disk):

```bash
./run_in_qemu.sh --imagen entrega/mia/saguios.img
./run_in_qemu.sh --imagen entrega/mia/saguios-persistente-fat32.img.xz --guardar
```

Then the image to somewhere else (step by step in `docs/como-gravar.md`):

| Target | How |
| --- | --- |
| **USB stick or USB disk** | On Windows, **Rufus** in **DD Image** mode (it accepts `.xz` as is) or balenaEtcher. On Linux: `xz -dk image.img.xz` if needed, and `sudo dd if=saguios.img of=/dev/sdX bs=4M status=progress conv=fsync` (replace `/dev/sdX` with your stick: **everything on it is erased**). The PC must boot in **traditional BIOS** mode (Legacy/CSM) |
| **CD** | Burn `saguios.iso` (`make iso`) to a CD |
| **VirtualBox** | Use `saguios.vdi`: new machine of type **Other**, version **Other/Unknown** (32-bit), 512 MB of memory or more, existing disk `saguios.vdi` on the **IDE** controller, **EFI unchecked** |
| **QEMU** | `./run_in_qemu.sh --imagen ...` as above |

Check downloads with `sha256sum -c SHA256SUMS.txt`. On a real machine, start with `saguios.img`; if it works, try `-smp` and, on laptops, `-uacpi`.

To change the options of an image already written, without writing it again, from SaguiOS itself: `arranque opciones [smp=si|no] [ioapic=si|no] [uacpi=si|no]` (with no options it shows the current ones). They take effect from the next boot (D392).

### Window-based generator (optional)

If you prefer a window over the commands above, `tools/generador` (Tkinter) builds the `gerar_versao.sh` or `tools/mkimagem.py` command, lets you pick the output folder and runs it with live output. It also works without a window: `tools/generador.py --orden --perfil publica`. It has three profiles: *Pública* (what `make variantes` does), *Privada completa* (what `make priv_variantes` does; it shows «USO PROPIO: NO PUBLICAR» and never adds `--publicar`) and *Personalizada* (the `tools/mkimagem.py` options). See `docs/generador.md` (in Spanish).

### 9. Common problems

| Symptom | Cause and fix |
| --- | --- |
| `CPU you selected does not support x86-64 instruction set` or `gcc: ... does not accept -m32` | `gcc-multilib` is missing (or the 32-bit support in `~/.local/saguitools/root`). Install it; do not remove `-m32` |
| A header «does not exist» in the kernel or in a program | `env.sh` exports `CPATH` with the PC's glibc and GCC adds it even with `-nostdinc`; the `Makefile` already clears it for everything that goes inside SaguiOS. **Do not fix it** by passing `CPATH` or `-I` to the PC's glibc again: one of our own headers or an `-I` is missing |
| `env.sh: line 5: LD_LIBRARY_PATH: unbound variable` | `run_in_qemu.sh` uses `set -u`. Inside `env.sh`, variables that may not exist are extended with `${VAR:+:$VAR}` or `${VAR:-}`, never with a bare `$VAR` (it is already like that in `tools/entorno-local.sh`; keep it if you edit it). Test: `bash -c 'set -u; unset LD_LIBRARY_PATH LIBRARY_PATH CPATH; . ~/.local/saguitools/env.sh && echo OK'` |
| The C++ programs (OpenJazz, Command & Conquer, DOSBox) are missing | There is no 32-bit `g++` (`cc1plus`): install `g++` or run `tools/entorno-local.sh`. Everything else is built anyway |
| No NetSurf, GNUBoy, Typodermic fonts or Quake/Jazz data | They are optional and only included if you downloaded their data beforehand with `make priv_descargas` (see 5 and 7) |
| `falta el mcopy` / `mke2fs` / `mkfs.vfat` | Install `mtools`, `e2fsprogs`, `dosfstools` (or run `tools/entorno-local.sh`) |
| QEMU is very slow | Without KVM it uses software emulation. Check that `/dev/kvm` exists and that you are in the group: `sudo usermod -aG kvm $USER` (open a new session). To force no KVM: `--sin-kvm` |
| With KVM something Intel-specific fails, or without KVM it is very slow | With KVM the CPU is your PC's (it may be AMD): what expects an Intel CPU may fail. Emulation without KVM does not check segment limits or some CPU bits: always try on real hardware or with KVM before trusting it |
| The PC hangs or restarts when building and testing at once | On WSL2, `make -jN` with many cores plus several QEMUs in TCG saturate CPU and memory. Use `-j4`, one test at a time, preferably with KVM. The full suite (`python3 tests/rodar_todos.py`) is very long: do not run it alongside other heavy things |
| A test (h34) fails while preparing its disk | It happens if `input_share/` exists: the build adds your games. To test, `make all INPUT_SHARE= OPENJAZZ_DATOS=0 QUAKE_DATOS=0` |
| Testing an old image without noticing | Chain with `&&`: `make -j4 && python3 tests/hNNN_xxx.py`; if linking fails, the image is not regenerated and the test boots the previous one |
| `make` fails because of odd files named `*:Zone.Identifier` | They are leftovers from copying from Windows; delete them |
| The PC will not boot the image | The disk images and the ISO boot by BIOS (Legacy/CSM) and by UEFI (with Secure Boot set to "Other OS", `docs/uefi.md`). Write it in **DD** mode. Try `saguios.img` (single core) first |

---

## Português (Brasil)

Guia para compilar o SaguiOS-Extended a partir do repositório público e gerar suas próprias imagens. Os comandos (`make`, `./gerar_versao.sh`...) e os nomes de arquivo não são traduzidos. Tudo o que está aqui vem da leitura do `Makefile`, do `gerar_versao.sh`, do `run_in_qemu.sh`, do `tools/mkimagem.py` e dos scripts de `tools/`.

### 1. Requisitos do PC

- **Sistema:** Linux de 64 bits com `apt` (Ubuntu 24.04 ou parecido; o `gcc-13` é o que o invólucro de `tools/entorno-local.sh` usa) ou **WSL2** com Ubuntu, que é onde o projeto é desenvolvido. Não é preciso compilador cruzado: o `gcc` normal do PC com `-m32` e `-ffreestanding` faz esse papel (decisão D001); por isso é necessário o `gcc-multilib`.
- **Pacotes** (Ubuntu/Debian):

```bash
sudo apt install build-essential gcc-multilib g++ nasm binutils make python3 \
    mtools dosfstools e2fsprogs xz-utils curl perl \
    qemu-system-x86 qemu-utils
# opcional, só para os testes automáticos: imagemagick ffmpeg zip sqlite3 exfatprogs
# opcional, para testar o arranque UEFI (D264: h264, run_in_qemu.sh --uefi): ovmf ovmf-ia32
```

**Opcional: programas em Go.** `tools/tinygo_entorno.sh instalar` baixa o TinyGo e o Go para `~/.local/saguitools` (sem sudo); com isso o `make` também compila os exemplos de `userland/go/ejemplos/` para `/apps/go/`. Sem o TinyGo eles são pulados com um aviso (`GO=0` os desativa). Ver `docs/go.md`.

**Opcional: programas em Java.** `tools/java_entorno.sh instalar` baixa um JDK 17 e o Janino para `~/.local/saguitools` (sem sudo) e `tools/minijvm_fuentes.sh` baixa o miniJVM (commit fixo, SHA-256) para `userland/terceros/minijvm/fuentes/`; com isso o `make` também compila `/apps/bin/java`, `/apps/bin/javac`, a biblioteca de classes em `/apps/java/lib/` e os exemplos de `userland/java/ejemplos/`. Sem as fontes ou sem o JDK eles são pulados com um aviso (`JAVA=0` os desativa). Ver `docs/java.md`.

**Opcional: compilador de C dentro (TCC).** `tools/tcc_fuentes.sh` baixa o TCC 0.9.28rc (commit fixo, SHA-256; LGPL-2.1 ou posterior) para `userland/terceros/tcc/fuentes/` e aplica seus patches; com isso o `make` também compila `/apps/bin/tcc`, `/apps/tcc/` (cabeçalhos, `libc.a`, `libSDL2.a`, `libtcc.a`, exemplos e as fontes do TCC para se compilar a si mesmo) e `/apps/aoen/fuentes` (o AoEN, compilável dentro com `sh compilar.sh`). Sem as fontes ele é pulado com um aviso (`TCC=0` o desativa). Ver `docs/tcc.md`.

**Opcional: GNU make dentro (D413).** `tools/make_fuentes.sh` baixa o GNU make 4.4.1 (SHA-256 fixo; GPL-3.0-or-later) para `userland/terceros/make/fuentes/`; com isso o `make` também compila `/apps/bin/make` e `/apps/make` (leia-me, licença e `ejemplos/proyecto`). Sem as fontes, é pulado com um aviso (`MAKE_DENTRO=0` o desativa). Fica fora de `SEM_GPL=1`. Ver `docs/make.md`.

**Opcional: Laboratório de C (D414).** Com as fontes do TCC, o `make` também compila `/apps/bin/labc`, o Laboratório de C (editor com realce, execução com libtcc na memória, erros com a sua linha e dez exemplos em `/apps/labc/ejemplos`). Não precisa de mais nada: é incluído a partir de `tcc.mk`, então `TCC=0` e `SEM_GPL=1` também o removem, e `LABC=0` remove só ele. Ver `rootfs/ayuda/laboratorio-c.md`.

**Opcional: awk e Lua dentro (D440).** `tools/awk_fuentes.sh` baixa o The One True AWK 20261106 (SHA-256 fixo; licença permissiva de Lucent) para `userland/terceros/awk/fuentes/`; com isso o `make` instala `/bin/awk`. Não precisa de bison: a gramática já gerada está no git. `tools/lua_fuentes.sh` baixa o Lua 5.4.9 (SHA-256 fixo; MIT) para `userland/terceros/lua/fuentes/`; com isso o `make` instala `lua` e `luac` em `/apps/bin`, `liblua.a` para o TCC e `liblua.so.5.4`. Sem as fontes, cada um é pulado com um aviso (`AWK_DENTRO=0` e `LUA_DENTRO=0` os desativam). Os dois também entram com `SEM_GPL=1`. Ver `docs/awk-lua.md`.

**Opcional: git dentro (D400).** `tools/git_fuentes.sh` baixa o libgit2 1.9.1 (tag fixa, SHA-256 verificado) para `userland/terceros/git/fuentes/`; com isso o `make` também compila `/apps/bin/git` (init, clone, fetch, pull, push, status, add, commit, log, diff, branch, checkout, remote e config; HTTP(S) com TLS, e SSH). Sem as fontes ele é pulado com um aviso (`GIT=0` o desativa). Fica fora de `SEM_GPL=1`. Ver `docs/git.md`.

| Pacote | Para quê |
| --- | --- |
| `gcc`, `gcc-multilib`, `binutils`, `make` | compilar o kernel e os programas em 32 bits |
| `g++` (com suporte a `-m32`) | **opcional**: os programas em C++ (OpenJazz, Command & Conquer, DOSBox). Sem ele o sistema é construído igualmente, sem eles |
| `nasm` | o carregador de inicialização e partes do kernel |
| `python3` | todas as ferramentas de `tools/` (`mkimagem.py`, `mksgfs.py`...) |
| `mtools`, `dosfstools` | partições FAT32 (`mcopy`, `mkfs.vfat`) |
| `e2fsprogs` | partições ext2 (`mke2fs`) |
| `exfatprogs` (opcional) | testes de exFAT (`tests/h325_exfat.py`: `mkfs.exfat`, `fsck.exfat`); sem ele esse teste não é feito. `tools/entorno-local.sh` o baixa sem sudo |
| `xz-utils` | imagens `.xz` e os dados comprimidos do OpenTyrian |
| `qemu-system-x86`, `qemu-utils` | executar o sistema (`qemu-system-i386`) e criar o `.vdi` (`qemu-img`) |
| `ovmf`, `ovmf-ia32` (opcional) | firmware UEFI do QEMU de 64 e 32 bits, para testar o arranque UEFI (`run_in_qemu.sh --uefi`, teste h264; D264). `tools/entorno-local.sh` baixa-os sem sudo |
| `curl`, `perl` | só os scripts de download (`curl`) e a preparação do NetSurf (`perl`, veja 7) |

- **Sem permissões de administrador (`sudo`):** `bash tools/entorno-local.sh` baixa com `apt-get download` os pacotes que faltam (nasm, o suporte de 32 bits do gcc, QEMU, mtools, dosfstools, e2fsprogs, g++...), extrai-os em `~/.local/saguitools/root` e cria `~/.local/saguitools/env.sh`. Use assim, em cada terminal novo:

```bash
bash tools/entorno-local.sh               # uma única vez
. ~/.local/saguitools/env.sh && make      # toda vez que abrir um terminal
```

  Exige que o `gcc-13` do sistema já esteja instalado (o invólucro chama `/usr/bin/gcc-13`). Com `sudo apt install`, como acima, nada disso é necessário; o `env.sh` é opcional e `make`, `run_in_qemu.sh` e `gerar_versao.sh` só o carregam se ele existir.
- **Espaço em disco:** a compilação (`build/`) ocupa cerca de 2 GB (na máquina do mantenedor, 1,8 GB). O `./gerar_versao.sh --todas` deixa ainda uns 2 GB na pasta de entrega (as imagens persistentes ext2 têm cerca de 900 MB cada). Convém ter **5 GB livres**.
- **Tempo:** não foi medido para este guia. Depende dos núcleos do PC: a primeira compilação é a lenta (o kernel, a libsagui, o MicroPython, o navegador Cipó e os ports de terceiros); as seguintes só refazem o que mudou. Use `make -j8` (ou os núcleos que você tiver).

### 2. Compilar e iniciar

```bash
make -j8                 # compila tudo e gera build/saguios.img
./run_in_qemu.sh         # inicia build/saguios.img no QEMU com janela
```

`make` sem alvo compila `build/saguios.img` e as ferramentas do PC. Outros alvos úteis: `make vdi` (`build/saguios.vdi`, para o VirtualBox), `make iso` (`build/saguios.iso`, CD inicializável), `make rodar` (QEMU simples com a imagem), `make limpar` (apaga `build/`), `make test` (bateria completa de testes no QEMU, muito longa).

Opções do `./run_in_qemu.sh` (`--help` lista todas):

| Opção | O que faz |
| --- | --- |
| `--imagen ARCHIVO` | inicia essa imagem em vez de `build/saguios.img` (um `.img.xz` é descomprimido antes em `build/`) |
| `--compilar` | executa `make` antes de iniciar |
| `--smp`, `--nucleos N` | multinúcleo: precisa de uma imagem criada com `mkimagem.py --smp`; com `--smp` e sem `--nucleos`, a máquina tem 4 núcleos (ou os que houver) |
| `--ioapic`, `--planificador P`, `--uacpi` | interrupções pelo IOAPIC; escalonador (`rr`, `afinidad`, `equilibrio`, `prioridad`); interpretador ACPI e indicador de bateria. Com qualquer uma destas, `--smp` ou `--uacpi`, `build/saguios-opciones.img` é criada na hora e a sua não é tocada |
| `--sonido`, `--ac97` | som pelo PulseAudio (no WSLg funciona): alto-falante do PC e placa Intel HDA (ou AC'97 com `--ac97`) |
| `--usb`, `--cd`, `--pendrive ARCHIVO`, `--xhci`, `--hid-usb` | iniciar como pendrive USB ou pelo CD (`build/saguios.iso`); conectar um pendrive de dados FAT32; controlador USB 3.0; teclado e mouse USB |
| `--mem MB`, `--cpu MODELO`, `--gpu MODELO`, `--vram MB` | memória (2048 por padrão), modelo de CPU (`pentium3`...), placa de vídeo (`std`, `virtio`, `vmware`) e sua memória |
| `--usb-real VID:PID`, `--wifi-usb` | repassar um dispositivo USB real do PC (no WSL, com `usbipd`; veja `docs/usb-real.md`) |
| `--raton-relativo`, `--serie`, `--vnc`, `--guardar` | mouse como em um notebook real; log do kernel neste terminal; sem janela (VNC em `localhost:5900`); manter as mudanças no disco de inicialização (por padrão são descartadas) |
| `--sin-kvm` | sem aceleração KVM. Por padrão o KVM é usado se `/dev/kvm` existir; senão, emulação por software (mais lenta) |

### 3. Opções de compilação

Vão na linha do `make`: `make NETSURF=0 B=build-mia -j8`. Se mudar uma opção, use um `B=` diferente (ou `make limpar`) para não misturar objetos.

**Variáveis do `Makefile`:**

| Variável | Padrão | O que faz |
| --- | --- | --- |
| `B` | `build` (com `SEM_GPL=1`, `build-sem-gpl`; com `SEM_FIRMWARE=1`, `build-sem-firmware`) | diretório de saída da compilação |
| `-j N` | (um) | trabalhos em paralelo do `make`. O `gerar_versao.sh` usa todos os núcleos por padrão |
| `OTIMIZACAO` | `-O2` | nível de otimização (`-O0`, `-O1`, `-O2`, `-O3`, `-Os`) |
| `PUBLICABLE` | `0` | `1` deixa de fora o que não pode ir em um binário GPL v3: NetSurf, GNUBoy e o r8169 derivado do Linux (o driver usa o início clássico próprio). O `gerar_versao.sh` passa isso sozinho, a menos que você use `--con-input-share` |
| `SEM_GPL` | `0` | `1` deixa de fora todos os ports de terceiros com licença GPL (OpenTyrian, NetSurf, GNU Prolog, OpenJazz, Vanilla Conquer, Quake, GNUBoy, DOSBox, Linux), e também Tzar, TCC, make GNU e o Laboratório de C. Compila em `build-sem-gpl/` |
| `SEM_FIRMWARE` | `0` | `1` não inclui o firmware binário (`firmware/`: o do adaptador Wi-Fi RTL8188EU, os dos adaptadores Wi-Fi USB e Bluetooth e o microcódigo das Radeon R5xx, R6xx e R7xx para o 3D pela GPU). Compila em `build-sem-firmware/` |
| `MICROPYTHON` | `1` | `0` gera a imagem sem o MicroPython (bem menor) |
| `NETSURF` | `1` | o navegador NetSurf; só entra se as fontes estiverem preparadas e a cola existir (veja 7). `NETSURF=0` o remove |
| `GNUBOY` | `1` | o emulador de Game Boy; só entra se o seu código foi baixado (`make priv_descargas` ou `bash tools/gnuboy_fuentes.sh`, veja 5 e 7). `GNUBOY=0` o remove |
| `MGBA` | `1` | o emulador de Game Boy Advance, Game Boy e Game Boy Color com mGBA (MPL-2.0; as fontes estão no git). Também entra com `PUBLICABLE=1`. `MGBA=0` o remove |
| `LAINES`, `GEARSYSTEM` | `1` | os emuladores de NES (LaiNES) e de Master System, Game Gear e SG-1000 (Gearsystem), em C++ com SDL 2; sem `g++` não entram. `0` os remove; com `SEM_GPL=1` também não entram |
| `SHINE` | `1` | o codificador MP3 shine (LGPL-2.0+, D339) do Conversor de áudio (`convertir-audio`). `SHINE=0` o remove (o programa funciona igual, sem a saída MP3); com `SEM_GPL=1` também não entra |
| `QUAKE` | `1` | o motor do Quake (`0` o remove) |
| `OPENJAZZ`, `CONQUER`, `DOSBOX`, `GPROLOG` | `1` | OpenJazz, Command & Conquer (Vanilla Conquer), DOSBox (com o lançador Recreativa) e GNU Prolog (`0` os remove) |
| `QUAKE_DATOS`, `OPENJAZZ_DATOS` | `1` | colocar em `/jogos` os dados shareware baixados por `tools/quake_datos.sh` e `tools/openjazz_datos.sh`. Se não foram baixados, nada entra; `0` os ignora mesmo que estejam baixados |
| `CNC_VIDEOS` | `0` | `1` inclui também os filmes do C&C (840 MB a mais: aumente `JOGOS_MB`, por exemplo `JOGOS_MB=2000`) |
| `INPUT_SHARE` | `input_share` | pasta com seus jogos e apps (veja 6). `INPUT_SHARE=` (vazio) a ignora; outro caminho também serve |
| `JOGOS` | `1` | `0` tira a partição de jogos `SAGUI-JOGOS` de todas as imagens |
| `JOGOS_MB` | `800` | tamanho da `SAGUI-JOGOS` nas imagens persistentes, na de partições e no que o instalador cria (`0` = o justo) |
| `JOGOS_IMG_MB` | `0` | o mesmo para `saguios.img`, `-smp` e `-uacpi` (`0` = o justo para o que ela leva) |
| `APPS`, `APPS_MB` | `1`, `0` | `APPS=0` coloca os aplicativos dentro do ramdisk (`/apps`, imagem de um só bloco); `APPS_MB` é o tamanho da partição `SAGUI-APPS` (`0` = o justo mais uma margem). `convertir-audio`, `traductor` e os exploradores de bancos de dados ficam na partição de aplicativos (`/apps`); os exploradores executam `/apps/bin/python`. |
| `RAIZ_DISCO` | `1` | (D379, desde a v8.5.3.1) as imagens iniciam com um initramfs mínimo (`build/initramfs.sgfs`, < 1 MB) e o sistema inteiro, com `/apps`, na partição `SAGUI-RAIZ` (ext2): live a partir da mídia (raiz somente leitura e as mudanças na RAM, ou na `SAGUI-PERSIST` nas persistentes); a ISO a copia para a RAM (toram). `0`: todas com o ramdisk de sempre (como até a v8.5.3) |
| `PERSIST_MB` | `256` | tamanho da `SAGUI-PERSIST` (a camada persistente da live) nas imagens persistentes |
| `RAMDISK` | `base` | `completo`: o ramdisk de sempre com o sistema inteiro e os aplicativos dentro (como `APPS=0` e `RAIZ_DISCO=0`; é a `saguios-completo.img`). Passa-se igual com o `gerar_versao.sh`, por exemplo `RAMDISK=completo ./gerar_versao.sh` |
| `DATOS_MB` | `64` | `/home` da imagem persistente ext2 |
| `DATOS_FAT32_MB` | `2048` | `/home` da imagem persistente FAT32 |
| `HOME_MB`, `TMP_MB`, `LOGS_MB` | `2048`, `256`, `128` | partições do disco com partições (`imagem-particoes`) |
| `PART_FS` | `fat32` | sistema de arquivos dessas partições (`fat32` ou `ext2`) |
| `PART_EXT` | (vazio) | `--primarias`: o formato anterior à v7.3 (por padrão, todas lógicas em uma estendida) |
| `DESTINO`, `PRIV_DESTINO` | `entrega/latest` | pasta de saída de `make variantes` e de `make priv_variantes` |
| `GERAR_OPC` | (vazio) | opções extras para o `gerar_versao.sh` a partir desses dois alvos (por exemplo `GERAR_OPC="--probar"`) |
| `FUENTES_RED` | (não definida) | `1`: se faltar o zip da Typodermic, baixá-lo (veja 5) |
| `FREECIV` | `1` | Freeciv 3.1.6 (cliente SDL2 e servidor local, em espanhol, D374): só entra se as fontes estiverem preparadas (veja 5). `FREECIV=0` o remove; com `SEM_GPL=1` também não entra |
| `DOOM` | `1` | Chocolate Doom 3.1.1 com Heretic, Hexen e Strife (D383): só entra se as fontes estiverem preparadas (`tools/doom_fuentes.sh`). `DOOM=0` o tira; com `SEM_GPL=1` também não entra. O Freedoom (BSD-3-Clause, `tools/doom_datos.sh`) vai para `/apps/doom`, também nas imagens públicas; os WAD originais, só por `input_share/games/DOOM` |
| `GIT` | `1` | o git do SaguiOS (`/apps/bin/git`, D400): só entra se as fontes do libgit2 estiverem (`tools/git_fuentes.sh`, ver 1 e 5). `GIT=0` o remove; com `SEM_GPL=1` também não entra |
| `MAKE_DENTRO` | `1` | o GNU make do SaguiOS (`/apps/bin/make`, D413): só entra se as fontes estiverem (`tools/make_fuentes.sh`, ver 1 e 5). `MAKE_DENTRO=0` o remove; com `SEM_GPL=1` também não entra |
| `LABC` | `1` | o Laboratório de C (`/apps/bin/labc`, D414): só entra com o TCC. `LABC=0` o remove (e `TCC=0` também) |
| `AWK_DENTRO` | `1` | o The One True AWK (`/bin/awk`, D440): só entra se as fontes estiverem (`tools/awk_fuentes.sh`, ver 1 e 5). `AWK_DENTRO=0` o remove; com `SEM_GPL=1` também entra |
| `LUA_DENTRO` | `1` | o Lua 5.4 (`/apps/bin/lua` e `luac`, `liblua.a` e `liblua.so.5.4`, D440): só entra se as fontes estiverem (`tools/lua_fuentes.sh`, ver 1 e 5). `LUA_DENTRO=0` o remove; com `SEM_GPL=1` também entra |
| `GO` | `1` | programas em Go com TinyGo (`/apps/go/`, veja 1). Sem o TinyGo instalado são pulados com um aviso; `GO=0` os desativa |
| `OPENTTD_REPO`, `OPENTTD_URL` | `../openttd-saguios` | só para `make priv_descargas`: onde está o repositório do OpenTTD (ou de onde cloná-lo). Sem ele, a imagem privada não leva o OpenTTD (veja 5) |

**`gerar_versao.sh`** (compila, monta as imagens e deixa a entrega com somas SHA-256; `./gerar_versao.sh --help`):

| Opção | Padrão | O que faz |
| --- | --- | --- |
| `--destino DIR` | `entrega/latest` | pasta da entrega |
| `--publicar` | não | copia também a entrega para `entrega/v<versão>` |
| `--iso` | não | inclui `saguios.iso` |
| `--fat32 [MB]` | não (2048) | inclui a imagem persistente com `/home` FAT32 (`.xz`) |
| `--ext2 [MB]` | não (64) | inclui a imagem persistente com `/home` ext2 |
| `--smp` | não | inclui `saguios-smp.img` e, com `--ext2`/`--fat32`, as persistentes multinúcleo |
| `--uacpi` | não | inclui `saguios-uacpi.img` e, com `--smp` e `--ext2`, `saguios-persistente-smp-uacpi.img` |
| `--particiones` | não | inclui o disco com `/home`, `/tmp` e `/logs` em partições (`.img.xz`) |
| `--sin-gpl` | não | inclui a variante sem ports GPL (antes executa `tools/auditar_licencias.py`) |
| `--todas` | não | tudo o que está acima |
| `--variante V` | `gpl` | variante da imagem principal: `gpl`, `sem-gpl` ou `sem-firmware` |
| `--sin-python` | não | sem MicroPython (compila em `build-sem-python/`) |
| `--otimizacao O` | `-O2` | nível de otimização |
| `--jobs N`, `-j N` | núcleos do PC | trabalhos do `make` |
| `--limpiar` | não | apaga a pasta de compilação antes de compilar |
| `--sin-compilar` | não | não compila: usa o que já houver na pasta de compilação |
| `--con-input-share` | não | inclui `input_share/`, os dados shareware baixados, o NetSurf e o GNUBoy. **Só uso próprio** (veja 6 e 7) |
| `--probar` | não | inicia a imagem no QEMU e verifica que chega à área de trabalho |
| `--sin-notas`, `--notas FICHERO` | notas sim | não escreve `VERSION.txt`, ou acrescenta a ele o texto de um arquivo |
| `--fijar-version V` | não | muda a versão em `kernel/inc/versao.h` antes de compilar (`v8.1`...) |

Sem `--con-input-share`, o `gerar_versao.sh` compila com `INPUT_SHARE= OPENJAZZ_DATOS=0 QUAKE_DATOS=0 NETSURF=0 GNUBOY=0 PUBLICABLE=1`: essa é a compilação **publicável**. Os tamanhos mudam com as variáveis da tabela anterior, por exemplo `HOME_MB=8192 JOGOS_MB=64 ./gerar_versao.sh --particiones --smp`.

**`tools/mkimagem.py`** (chamado pelo `Makefile`; `python3 tools/mkimagem.py --help`). Opções úteis para um usuário; as chamadas «prueba» ou `--teste` são só dos testes automáticos:

| Opção | Padrão | O que faz |
| --- | --- | --- |
| `--vesa` | (o Makefile a passa) | a etapa 2 da inicialização troca para o modo gráfico VESA |
| `--smp` | desligado | liga os processadores secundários (sem ela, o sistema usa um só) |
| `--ioapic` | desligado | IRQ dos dispositivos pelo IOAPIC em vez do PIC 8259 |
| `--uacpi` | desligado | interpretador de AML (uACPI): controlador embarcado e indicador de bateria |
| `--sem-acpi` | desligado | não usar o ACPI: desligar só avisa, sem botão de energia |
| `--planificador P` | `rr` | escalonador na inicialização: `rr`, `afinidad`, `equilibrio` ou `prioridad` |
| `--resolucion LxA`, `--bpp N` | automática (1024x768, 32 bpp) | resolução e profundidade de cor preferidas |
| `--sem-wc` | desligado | não ativar o write-combining no framebuffer |
| `--sem-pae` | desligado | paginação clássica de 2 níveis mesmo que a CPU tenha PAE: até 4 GB de RAM e sem NX (para testar ou para hardware com problemas) |
| `--sem-nx` | desligado | não ativar o NX (pilha e heap não executáveis) mesmo que a CPU o tenha |
| `--sem-avx` | desligado | (D381) o kernel não liga o AVX (XSAVE e os registradores YMM) mesmo que a CPU o tenha: tudo fica como antes (FXSAVE e caminhos SSE). Pelo sistema, `kconfig avx` com `si` ou `no` |
| `--cache-disco-baja` | desligado | manter o cache de disco na memória baixa (até 64 MB) mesmo que haja memória alta (caso o hardware dê problemas) |
| `--sin-swap` | desligado | sem swap comprimida na RAM: quando a memória acaba, o programa falha como antes |
| `--swap-mb N` | 0 (automático) | limite do armazém da swap comprimida em MB (1-1024); automático = metade da RAM com memória alta, um quarto sem ela, no máximo 1 GB |
| `--sgl-motor software` | `auto` | desativa toda a aceleração 3D por hardware |
| `--acel2d {auto,cpu,intel,nvidia,radeon,tdfx,vmware}`, `--sin-acel2d` | `auto` | acelerador 2D da GPU; `cpu` ou `--sin-acel2d` = só CPU |
| `--sin-servicios` | desligado | não iniciar o gerenciador de serviços nem o cron na inicialização |
| `--rescate` | desligado | iniciar em modo de resgate: console com shell de root, sem área de trabalho |
| `--sesion-root` / `--sesion-sagui` | `--sesion-sagui` | área de trabalho como root ou como o usuário `sagui` |
| `--sin-frecuencia` | desligado | sem o driver de frequência da CPU |
| `--console` | desligado | sem área de trabalho: terminal em tela cheia |
| `--sin-conf` | desligado | sem a partição `SAGUI-CONF` (16 MB, configuração do sistema). A ISO não a tem |
| `--dados-mb N`, `--dados-fs {ext2,fat32}` | 0 (nenhuma) | partição `/home` (`SAGUI-DATOS`) |
| `--tmp-mb N`, `--logs-mb N`, `--tmp-fs`, `--logs-fs` | 0 | partições `/tmp` e `/logs` |
| `--extra-mb N` | nenhuma | partição extra sem pasta fixa; pode ser repetida |
| `--esquema {logicas,primarias,mixto}`, `--primarias` | `logicas` | onde ficam as partições de dados |
| `--jogos CARPETA`, `--jogos-mb N` | — | partição de jogos com o conteúdo da pasta (o `Makefile` já faz isso) |
| `--apps CARPETA`, `--apps-mb N` | — | partição de aplicativos (idem) |
| `--part-fs {ext2,fat32}` | `ext2` | sistema das partições sem `--*-fs` próprio |
| `--cd` | desligado | imagem para a ISO (usada por `make iso`) |
| `--raiz CARPETA`, `--raiz-mb N` | — | (D379) a raiz do sistema na partição `SAGUI-RAIZ` (ext2) com o conteúdo da pasta (`build/rootfs`; com `--apps`, `/apps` vai dentro); o `--ramdisk` é então o initramfs (`tools/mkinitramfs.py`). O `Makefile` já faz isso |
| `--persist-mb N` | 0 | (D379) partição `SAGUI-PERSIST`: a camada persistente da live |
| `--modo-raiz {auto,disco,live,toram,ram}` | `auto` | (D379) força como a raiz é usada (por padrão, o que o initramfs diz; com `--cd`, `toram`) |
| `--raiz-sin-comprimir` | desligado | (D379, v8.5.4.2) com `--cd`, a raiz vai como ext2 sem mudanças; por padrão vai comprimida por blocos (a ISO fica com a metade do tamanho) |

As opções de energia da GPU (D399), `--gpu-energia-simulada` e `--gpu-energia-prueba`, são só para os testes automáticos no QEMU e não aparecem na tabela.

Um disco com partições sob medida: `python3 tools/crear-disco.py -i` (assistente) ou `make imagem-particoes-xz HOME_MB=4096 TMP_MB=512 LOGS_MB=256 PART_FS=fat32`. Como todas as imagens de disco, arranca por BIOS e por UEFI (D270; `crear-disco.py --sin-uefi`: só BIOS).

Ferramenta do PC: `tools/conversor_video_pc.py` é um conversor de vídeo com janela (Python 3 + tkinter, com ffmpeg) que prepara vídeos para o Reprodutor do SaguiOS; não faz parte da compilação. Veja `docs/convertir-video.md`.

### 4. Alvos: qual imagem sai de cada um

| Comando | O que gera |
| --- | --- |
| `make` / `make all` | `build/saguios.img` (a normal, um núcleo) |
| `make help` | lista os alvos e as variáveis principais |
| `make vdi`, `make iso` | `build/saguios.vdi`, `build/saguios.iso` |
| `make imagem-smp`, `imagem-uacpi` | `build/saguios-smp.img`, `build/saguios-uacpi.img` |
| `make imagem-persistente`, `imagem-persistente-fat32`, `imagem-persistente-smp`, `imagem-persistente-smp-uacpi` | as persistentes (com `/home`) em `build/` |
| `make imagem-particoes`, `imagem-particoes-xz` | `build/saguios-particiones.img` (e `.xz`) |
| `make variantes [DESTINO=entrega/mia]` | **o publicável**: `./gerar_versao.sh --todas --destino DESTINO`: todas as imagens da tabela abaixo, sem `input_share/`, sem NetSurf nem GNUBoy, sem dados shareware |
| `make priv_variantes [PRIV_DESTINO=...]` | **uso próprio**: o mesmo com `--con-input-share` (seus jogos, shareware de Quake e Jazz, NetSurf e GNUBoy, que ele baixa antes). Antes executa `make priv_descargas` (veja 5 e 7) |
| `make entrega-sin-gpl` | `build-sem-gpl/saguios-sin-gpl.img`, depois da auditoria de licenças |
| `./gerar_versao.sh --todas` | o mesmo que `make variantes`, com o destino padrão `entrega/latest` |

Arquivos que o `make variantes` deixa na pasta de entrega:

| Arquivo | Para que serve |
| --- | --- |
| `saguios.img` | A normal (um núcleo). **Comece sempre por esta** |
| `saguios-smp.img` | Multinúcleo (todos os processadores, sem IOAPIC) |
| `saguios-uacpi.img` | Com o interpretador ACPI (uACPI): indicador de bateria e `_PSS`/`_PRT` em notebooks |
| `saguios-completo.img` | Com o sistema inteiro e os aplicativos na memória (o ramdisk de sempre), para PCs cuja controladora não encontra o disco de boot (D324); precisa de uns 64 MB a mais de RAM e não leva Freeciv, OpenTTD nem DOOM. Desde a 8.4.3r2 |
| `saguios-persistente-fat32.img.xz` | Com `/home` FAT32 de 2 GB, visível também no Windows. A melhor para guardar coisas |
| `saguios-persistente-fat32-smp.img.xz` | O mesmo, multinúcleo |
| `saguios-persistente.img`, `-smp`, `-smp-uacpi` | Com `/home` ext2 (o Windows não a vê). Combinam multinúcleo e uACPI |
| `saguios-particiones.img.xz` | `/home`, `/tmp` e `/logs` em partições (o kernel guarda seu registro em `/logs`) |
| `saguios-sin-gpl.img` | Sem ports de terceiros GPL |
| `saguios.iso` | CD inicializável (modo ao vivo, sem `SAGUI-CONF`): o sistema, com `/apps` e Freeciv, é copiado para a RAM ao iniciar (toram) e o CD pode ser retirado; com pouca RAM, é usado a partir do CD |
| `saguios.vdi` | VirtualBox |
| `SHA256SUMS.txt`, `VERSION.txt` | somas de verificação e ficha técnica do que foi compilado |

Desde a v8.5.3.1 (D379) todas as imagens de disco levam o sistema na `SAGUI-RAIZ` (ext2, com `/apps` dentro: já não há partição `SAGUI-APPS`), além da `SAGUI-JOGOS` (jogos, `/jogos`) e da `SAGUI-CONF` (configuração, 16 MB); as persistentes, também a `SAGUI-PERSIST`. Iniciam em modo live a partir da mídia (`docs/imagenes.md`). Com `RAIZ_DISCO=0` saem como até a v8.5.3 (ramdisk com a base e `SAGUI-APPS`).

Desde a v8.5.4.10r2 (D388) a ordem no disco é: inicialização, a ESP, e a estendida por último, com `SAGUI-CONF`, `SAGUI-RAIZ`, `SAGUI-PERSIST`, `SAGUI-JOGOS`… e `/home` (`SAGUI-DATOS`) por último de tudo, como partição lógica: gravada em um disco maior, “Ampliar /home” a estende até o fim sem mover nada. `mkimagem.py --orden-antiguo` dá a disposição anterior (só para testes).

### 5. O que é baixado e quando

**Um `make` normal não baixa nada da rede.** Tudo o que está abaixo é executado por você, ou por `make priv_descargas` / `make priv_variantes`. Todos os scripts verificam o SHA-256 do que baixam e não repetem o download se já estiver feito; sem rede, falham com uma mensagem e o sistema é construído mesmo assim, sem essa peça (os dados shareware e as fontes tipográficas são opcionais).

| O quê | Script | Origem | Licença | Onde fica |
| --- | --- | --- | --- | --- |
| Fontes do NetSurf 3.11 (uns 90 MB) | `bash tools/netsurf_fuentes.sh [--tarball ARQUIVO] [--forzar]` | `https://download.netsurf-browser.org/netsurf/releases/source-full/netsurf-all-3.11.tar.gz` | GPL-2.0 só v2 (as bibliotecas, MIT). Veja 7 | `userland/terceros/netsurf/fuentes/` (ignorada pelo git). Precisa de `perl` |
| Fontes do GNUBoy (SDL2-GNUBoy, commit `c8642d1`, uns 450 KB) | `bash tools/gnuboy_fuentes.sh [--tarball ARQUIVO] [--forzar] [--comprobar]` | `https://github.com/AlexOberhofer/SDL2-GNUBoy/archive/c8642d11bd2706333c5255e2b7f2b84445d0a070.tar.gz` | GPL-2.0 (só v2 na prática). Veja 7 | `userland/terceros/gnuboy/jogo/` |
| Shareware do Quake 1.06 (episódio 1) | `bash tools/quake_datos.sh [--zip ARQUIVO] [--forzar]` | `ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake/quake106.zip` (e o espelho `youfailit.net`) | dados da id Software: grátis para o usuário, **sem direito de redistribuí-los dentro de outro sistema** | `userland/terceros/quake/datos/` (ignorada pelo git) |
| Shareware do Jazz Jackrabbit 1.0 (episódio 1) | `bash tools/openjazz_datos.sh [--zip ARQUIVO] [--forzar]` | `https://slackware.uk/~urchlay/src/jjrabbit.zip` | dados da Epic MegaGames, com condições de redistribuição: não são publicados | `userland/terceros/openjazz/datos/` (ignorada pelo git) |
| Fontes tipográficas da Typodermic (28 escolhidas de 731) | `python3 tools/fuentes_typodermic.py seleccion DESTINO` (com rede, baixa o zip); o `make priv_descargas` faz isso sozinho | `https://typodermicfonts.com/wp-content/uploads/2024/12/typodermic-public-domain-2024-12.zip` (**às vezes dá 404**: veja a nota) | CC0 (domínio público) | `NEW_FONTS/typodermic-public-domain-2024-12.zip` |
| ROM livre de Game Boy (Libbet) | `bash tools/gnuboy_roms_libres.sh` | `github.com/pinobatch/libbet`, versão v0.08 | zlib | `userland/terceros/gnuboy/roms-libres/` (só útil com o GNUBoy, veja 7) |
| Fontes do Freeciv 3.1.6 e do zlib 1.3.1 (cerca de 90 MB) | `bash tools/freeciv_fuentes.sh [--tarball ARQUIVO] [--zlib-tarball ARQUIVO] [--forzar]` | `https://github.com/freeciv/freeciv/archive/refs/tags/R3_1_6.tar.gz` (zlib: `https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz`) | GPL-2.0 ou posterior (Freeciv); licença zlib (zlib) | `userland/terceros/freeciv/fuentes/` e `userland/terceros/zlib/fuentes/` (ignoradas pelo git) |
| Fontes do Chocolate Doom 3.1.1 e do Freedoom 0.13.0 (cerca de 12 e 57 MB) | `bash tools/doom_fuentes.sh [--tarball ARQUIVO] [--forzar]` e `bash tools/doom_datos.sh [--zip ARQUIVO] [--forzar]` | `https://github.com/chocolate-doom/chocolate-doom/archive/refs/tags/chocolate-doom-3.1.1.tar.gz` e `https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip` | GPL-2.0 ou posterior (Chocolate Doom); BSD-3-Clause (Freedoom) | `userland/terceros/chocolatedoom/fuentes/` e `datos/` (ignoradas pelo git) |
| Fontes do libgit2 1.9.1 (o comando `git`, D400) | `bash tools/git_fuentes.sh [--tarball ARQUIVO]` | `https://github.com/libgit2/libgit2/archive/refs/tags/v1.9.1.tar.gz` | GPL-2.0 com exceção de ligação | `userland/terceros/git/fuentes/` (ignorada pelo git) |
| Fontes do GNU make 4.4.1 (`/apps/bin/make`, D413) | `bash tools/make_fuentes.sh [--tarball ARQUIVO]` | `https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz` | GPL-3.0-or-later | `userland/terceros/make/fuentes/` (ignorada pelo git) |
| Fontes do The One True AWK 20261106 (`/bin/awk`, D440) | `bash tools/awk_fuentes.sh [--tarball ARQUIVO]` | `https://github.com/onetrueawk/awk/archive/refs/tags/20261106.tar.gz` | permissiva de Lucent (tipo HPND) | `userland/terceros/awk/fuentes/` (ignorada pelo git) |
| Fontes do Lua 5.4.9 (`/apps/bin/lua` e `luac`, D440) | `bash tools/lua_fuentes.sh [--tarball ARQUIVO]` | `https://www.lua.org/ftp/lua-5.4.9.tar.gz` | MIT | `userland/terceros/lua/fuentes/` (ignorada pelo git) |
| Modelos 3D CC0 para a demo do SaguiGL (`sgldemo -p --modelo`, D404) | `bash tools/modelos3d_datos.sh [--forzar]` | `https://opengameart.org/content/low-poly-biplane`, `https://opengameart.org/content/funky-aircraft` e `https://opengameart.org/content/ww1-wwi-airplane-model-obj` | CC0 1.0 (domínio público) | `userland/terceros/modelos3d/datos/` (ignorada pelo git); não entram nas imagens |

Notas:

- **Typodermic:** o `make priv_descargas` (ou `python3 tools/fuentes_typodermic.py seleccion /tmp/fuentes-prueba`) baixa o zip para `NEW_FONTS/` se faltar. Se o site der 404 (aconteceu em 2026-11-14), o script avisa com clareza e **não quebra nada**: o sistema é construído mesmo assim, sem essas fontes. Para tê-las, baixe o zip à mão e coloque-o em `NEW_FONTS/typodermic-public-domain-2024-12.zip` (o SHA-256 é verificado). O `make` só inclui as fontes se o zip já estiver em `NEW_FONTS/` quando você compila (essa pasta não está no repositório público).
- **`make priv_descargas`** executa, nesta ordem: `tools/netsurf_fuentes.sh` (se não houver fontes preparadas), `tools/gnuboy_fuentes.sh` (se faltar `jogo/`), `tools/quake_datos.sh`, `tools/openjazz_datos.sh` e o download das fontes Typodermic (se falhar, avisa e continua). Funciona igual com o repositório público: a cola do NetSurf e do GNUBoy está nele.
- **`make priv_descargas`** também executa `tools/doom_fuentes.sh` e `tools/doom_datos.sh` (D383). Sem `userland/terceros/chocolatedoom/fuentes/.preparado` o `Makefile` pula o DOOM com um aviso.
- **Freeciv** não é baixado por `make priv_descargas`: é preciso executar `bash tools/freeciv_fuentes.sh` manualmente. Sem `userland/terceros/freeciv/fuentes/.preparado`, o `Makefile` não compila o Freeciv, e sem aviso (`FREECIV=0` o remove). Os dados do jogo vão para `/apps/freeciv`. Não depende de `--con-input-share`: se as fontes estiverem preparadas, o Freeciv entra também em `make variantes`; baixe-as só se o quiser nas suas imagens.
- **OpenTTD** (opcional, só imagens privadas): o último passo do `make priv_descargas` o compila como pacote a partir do seu repositório separado (por padrão `../openttd-saguios`, ou `OPENTTD_URL`) com `tools/openttd_paquete.sh` e o instala em `input_share/apps`. Sem esse repositório, avisa e a imagem privada sai sem ele. As imagens públicas (`INPUT_SHARE=`) nunca o levam.
- **Go** (opcional): `tools/tinygo_entorno.sh instalar` baixa o TinyGo e o Go em versões fixas, com SHA-256 verificado, para `~/.local/saguitools` (sem sudo). Sem isso, o `make` pula os exemplos de Go com um aviso.
- **Já estão no repositório, não são baixados:** as raízes de certificados (`rootfs/etc/ssl/certs/`; podem ser regeneradas a partir do repositório `ca-certificates` do PC com `python3 tools/raices_ssl.py`), o firmware do adaptador Wi-Fi RTL8188EU (`firmware/realtek/`, do linux-firmware, redistribuível sem alterações; `tools/firmware_rtl8188eu.sh --comprobar` o verifica e, sem `--comprobar`, o baixa de novo de `git.kernel.org`; `SEM_FIRMWARE=1` o remove), o firmware dos adaptadores Wi-Fi USB (`firmware/realtek/rtl8192cufw_A.bin`, `firmware/realtek/rtl8192cufw_TMSC.bin` e `firmware/ralink/rt2870.bin`, do linux-firmware, redistribuíveis sem alterações; `tools/firmware_wifi_usb.sh --comprobar` os verifica e, sem `--comprobar`, os baixa de novo; `SEM_FIRMWARE=1` os remove), o firmware dos controladores Bluetooth (`firmware/realtek/rtl_bt/` e `firmware/atheros/`, do linux-firmware, redistribuíveis sem alterações; `tools/firmware_bt.sh --comprobar` os verifica e, sem `--comprobar`, os baixa de novo; `SEM_FIRMWARE=1` os remove; os patches `.hcd` da Broadcom não estão no repositório e vão à mão em `/lib/firmware/brcm/`), o microcódigo das Radeon R5xx, R6xx e R7xx para o 3D pela GPU (`firmware/radeon/`, do linux-firmware, redistribuível sem alterações; `tools/firmware_radeon.sh --comprobar` verifica-o e, sem `--comprobar`, o baixa de novo; `SEM_FIRMWARE=1` o remove), os dados do OpenTyrian (`userland/terceros/opentyrian/dados.img.xz`), as fontes do OpenJazz, do Quake e do Vanilla Conquer (verificadas com `tools/openjazz_fuentes.sh --comprobar`, `quake_fuentes.sh`, `conquer_fuentes.sh`) e as ROM de CHIP-8, que são montadas a partir de `rootfs/jogos/chip8/fuentes/*.asm`.

### 6. Jogos e dados que você fornece (`input_share/`)

`input_share/` é uma pasta na raiz do repositório (crie-a você; o git a ignora e ela **nunca é enviada**). O que você puser ali entra nas imagens que compilar com `make` normal, com `make priv_variantes` ou com `./gerar_versao.sh --con-input-share`. Em `make variantes` não entra, de propósito (assim uma entrega publicada nunca leva seus jogos por descuido).

```
input_share/
  games/
    Quake/id1/PAK0.PAK       Quake completo (também PAK1.PAK, versão registrada)
    Jazz/                    Jazz Jackrabbit completo (p. ex. o da GOG), como está
    CnC/CnC_GDI95.iso        Command & Conquer Gold: GDI (volume GDI95)
    CnC/CnC_NOD95.iso        ... e NOD (opcional: sem ela, só a campanha GDI)
    gb/juego.gb               ROM de Game Boy / Game Boy Color (.gb, .gbc)
    <outra>/                 qualquer outro jogo: vai para /jogos/<outra>
  Tzar Anthology/            Tzar: Packs, Maps, Campaign, intro.MPG, Tzar.ini
  apps/
    bin/                     executáveis do SaguiOS que você quer em /apps/bin (no PATH)
    <app>/                   os dados de cada app, em /apps/<app>
```

| Jogo | O que colocar e onde | Notas |
| --- | --- | --- |
| **Quake** | `input_share/games/Quake/id1/pak0.pak` e `pak1.pak` | Vai para `/jogos/Quake/id1/`. Vale também o `pak0.pak` solto em `games/Quake/`, e nomes em maiúsculas (`ID1/PAK0.PAK`, como no CD do DOS). As expansões, em `games/Quake/hipnotic/` e `games/Quake/rogue/` (iniciam com `quake -hipnotic` ou `-rogue`). Use sua cópia legal (Steam, GOG, o CD). O shareware é baixado por `tools/quake_datos.sh`; o Quake prefere a versão completa |
| **Jazz Jackrabbit** | `input_share/games/Jazz/` com os arquivos do jogo, sem alterá-los | A versão da GOG ou outra completa. O OpenJazz a encontra em qualquer pasta de `/jogos` e a prefere ao shareware |
| **Command & Conquer** | `input_share/games/CnC/CnC_GDI95.iso` e `CnC_NOD95.iso` | São as ISO do **Command & Conquer Gold para Windows 95**, o presente (freeware) da EA em 2007, uma por disco; são reconhecidas pelo volume (GDI95, NOD95), não pelo nome. O `tools/cnc_extraer.py` tira delas só o que o Vanilla Conquer usa (63 MB em `/jogos/CnC`; com `CNC_VIDEOS=1`, 840 MB a mais). Sem as ISO, vale copiar para `/jogos/CnC` a pasta de um C&C95 já instalado |
| **Game Boy** | `input_share/games/gb/*.gb` ou `*.gbc` | Só ROMs que você tenha direito de usar. O GNUBoy as procura; ele não vai nas imagens publicadas, só nas suas de uso próprio (veja 7) |
| **Tzar** (Tzar Anthology, D417) | `input_share/Tzar Anthology/` com as suas pastas `Packs/`, `Maps/` e `Campaign/`, e `intro.MPG` e `Tzar.ini`; `Tzar.exe` e as DLL não são copiados | Vai para `/jogos/TZAR/`. Só com `INPUT_SHARE` (as imagens públicas não o levam). Precisa de uns 1 500 MB de `JOGOS_MB`: `JOGOS_MB=1600` é o confortável |
| Outros | `input_share/games/<pasta>/` | Copiada como está para `/jogos/<pasta>`. Se a pasta tem o nome de uma do sistema (`opentyrian`, `openjazz`), seu conteúdo é posto por cima |

Detalhes: o `Makefile` vigia `input_share/games` e `input_share/apps` e refaz as partições quando mudam nomes, tamanhos ou datas; as marcas `*:Zone.Identifier` do Windows não são copiadas. Outra pasta: `make INPUT_SHARE=/caminho/para/meu_input_share`; nenhuma: `make INPUT_SHARE=`. A referência completa é `docs/input_share.md`.

**Tamanhos.** A `SAGUI-JOGOS` é ext2 de leitura e escrita, montada em `/jogos`. Em `saguios.img`, `-smp` e `-uacpi` tem o justo para o que contém (`JOGOS_IMG_MB=0`): com Quake completo, Jazz e C&C saem imagens de uns 250 MB. Nas persistentes, na de partições e no instalador tem **800 MB** (`JOGOS_MB=800`): aumente o valor se seus jogos forem maiores.

**Copiá-los depois para um SaguiOS já instalado.** De dentro do SaguiOS, copie a pasta do jogo para `/jogos/<pasta>` (com o **Archivos** ou `cp`) a partir de um pendrive FAT32 ou de `/home`. Os jogos também procuram em `jogos/<pasta>` de cada disco montado (`/usb`...). No Windows, a unidade `SAGUI-DATOS` de uma imagem persistente FAT32 é o `/home`: serve para passar arquivos.

> **Aviso legal.** Os dados dos jogos comerciais (Quake completo, Jazz Jackrabbit, Command & Conquer, as ROM de Game Boy, Tzar Anthology) **não são redistribuídos com o SaguiOS nem estão neste repositório**, e não podem ser compartilhados. Cada pessoa usa suas próprias cópias legais. Uma imagem compilada com `input_share/` é **para uso próprio**: não a publique nem a compartilhe.

### 7. O que não está no repositório público, e por quê

O SaguiOS-Extended é **GPL v3** (`LICENSE`, `docs/licencia.md`). Quatro peças são **GPL v2 somente** (sem «ou posterior»), e a v2 sozinha é incompatível com a v3: não podem ser ligadas no mesmo binário com a libsagui e a libc comuns, então **o código delas não é publicado** nem vai nas imagens publicadas. O que é nosso é publicado: a cola com o SaguiOS (`netsurf.mk`, `gnuboy.mk`, `plataforma/`, `libc/`, com o SPDX `GPL-2.0-or-later`) e os scripts que baixam o código original; assim cada um pode compilá-los **para uso próprio** (usar em privado não é distribuir):

| Peça | Licença | O que acontece no repositório público |
| --- | --- | --- |
| **NetSurf** (navegador, `userland/terceros/netsurf/`) | GPL-2.0 só v2 | o código dele (`fuentes/`) não está: o `tools/netsurf_fuentes.sh` o baixa; a cola própria está |
| **GNUBoy** (Game Boy, `userland/terceros/gnuboy/`) | GPL-2.0 (quase sem «ou posterior»: tratado como só v2) | o código dele (`jogo/`) não está: o `tools/gnuboy_fuentes.sh` o baixa (commit fixo, SHA-256 verificado); a cola própria está |
| **r8169 derivado do Linux** (`kernel/terceros/linux/r8169/`) | GPL-2.0-only | não está; o driver da placa Realtek RTL8168/8111 usa seu início clássico próprio (`kernel/rede/r8169_sem_gpl.h`): funciona, mas sem as sequências de início dos chips mais novos (RTL8111G/H) |
| **Freeciv** (jogo de estratégia, `userland/terceros/freeciv/`) | GPL-2.0 ou posterior | o código dele (`fuentes/`, cerca de 90 MB) não está no git: o `tools/freeciv_fuentes.sh` o baixa; a cola própria (`freeciv.mk`, `plataforma/`, `parches/`, `traducciones/`) está |
| **Chocolate Doom** (DOOM, Heretic, Hexen e Strife, `userland/terceros/chocolatedoom/`) | GPL-2.0 ou posterior | o código dele (`fuentes/`) não está no git: `tools/doom_fuentes.sh` o baixa; a cola própria (`chocolatedoom.mk`, `plataforma/`, `parches/`) está. O Freedoom (BSD-3-Clause) é baixado por `tools/doom_datos.sh` |
| **libgit2** (o comando `git`, `userland/terceros/git/`) | GPL-2.0 com exceção de ligação | o código dele (`fuentes/`) não está no git: o `tools/git_fuentes.sh` o baixa, com SHA-256 fixo; a cola própria (`git.c`, `git.mk`, `plataforma/`) está |
| **OpenTTD** (jogo, repositório separado `../openttd-saguios`) | GPL-2.0 só | não está neste repositório nem nas imagens públicas: é compilado como pacote à parte e só entra nas imagens privadas (D375) |

**Usá-los em privado é permitido; redistribuir essas imagens, não.** Compilar para si uma imagem com eles e usá-la não é distribuir (`docs/licencia.md`).

**Como compilá-los para você só com o repositório público.** Verificado de verdade a partir de uma árvore exportada sem git (`tools/publicar.sh preparar`, depois `make` e `make priv_variantes INPUT_SHARE=`):

- o `make` simples funciona sem NetSurf nem GNUBoy: o menu não os mostra.
- `make priv_variantes INPUT_SHARE=` (ou `make priv_descargas` e depois `make`) **baixa o código original** do NetSurf 3.11 e do SDL2-GNUBoy, confere os SHA-256, os compila com a nossa cola e os põe nas imagens **privadas**. Precisa de rede e `perl` (NetSurf). Essas imagens são só suas: **não as redistribua** (NetSurf e GNUBoy são GPL v2; ligados com a libsagui, que é GPL v3, não podem ser distribuídos).
- **NetSurf:** `bash tools/netsurf_fuentes.sh` deixa `userland/terceros/netsurf/fuentes/.preparado`; o `Makefile` inclui o `netsurf.mk` só se existirem as fontes e a cola.
- **GNUBoy:** `bash tools/gnuboy_fuentes.sh` deixa `userland/terceros/gnuboy/jogo/` (`--comprobar` compara com o original; se um dia houver patches próprios, ficam em `userland/terceros/gnuboy/parches/*.patch` e o script os aplica).
- **r8169:** os arquivos são extratos adaptados de `drivers/net/ethernet/realtek/r8169_main.c` do Linux 6.6; não há script que os regenere. Sem eles o driver continua funcionando com o início clássico.

O resto de `kernel/terceros/linux/` (`hda`, `hwmon`, GPL-2.0 ou posterior) está no repositório. Também ficam de fora, por tamanho ou privacidade, `entrega/` (as imagens de cada versão vão para a página de «Releases» do projeto), `input_share/`, `NEW_FONTS/`, `testes_hw/` e as notas internas (`tools/publicar-excluir.txt`).

### 8. Gravar e testar

**Primeiro, teste no QEMU** (nada é gravado em nenhum disco):

```bash
./run_in_qemu.sh --imagen entrega/mia/saguios.img
./run_in_qemu.sh --imagen entrega/mia/saguios-persistente-fat32.img.xz --guardar
```

Depois, a imagem para outro lugar (o passo a passo está em `docs/como-gravar.md`):

| Destino | Como |
| --- | --- |
| **Pendrive ou disco USB** | No Windows, o **Rufus** em modo **Imagem DD** (aceita os `.xz` como estão) ou o balenaEtcher. No Linux: `xz -dk imagem.img.xz` se for preciso, e `sudo dd if=saguios.img of=/dev/sdX bs=4M status=progress conv=fsync` (troque `/dev/sdX` pelo seu pendrive: **tudo o que ele tiver é apagado**). O PC deve iniciar por **BIOS tradicional** (modo Legacy/CSM) |
| **CD** | Grave `saguios.iso` (`make iso`) em um CD |
| **VirtualBox** | Use `saguios.vdi`: máquina nova do tipo **Other**, versão **Other/Unknown** (32 bits), memória de 512 MB ou mais, disco existente `saguios.vdi` no controlador **IDE**, **EFI desmarcado** |
| **QEMU** | `./run_in_qemu.sh --imagen ...` como acima |

Verifique os downloads com `sha256sum -c SHA256SUMS.txt`. Em uma máquina real, comece por `saguios.img`; se funcionar, teste `-smp` e, em notebooks, `-uacpi`.

Para mudar as opções de uma imagem já gravada, sem gravá-la de novo, pelo próprio SaguiOS: `arranque opciones [smp=si|no] [ioapic=si|no] [uacpi=si|no]` (sem opções, mostra as atuais). Valem a partir do próximo arranque (D392).

### Gerador com janela (opcional)

Se preferir uma janela em vez dos comandos acima, `tools/generador` (Tkinter) monta o comando do `gerar_versao.sh` ou do `tools/mkimagem.py`, deixa escolher a pasta de destino e executa com a saída ao vivo. Também funciona sem janela: `tools/generador.py --orden --perfil publica`. Tem três perfis: *Pública* (o que faz o `make variantes`), *Privada completa* (o que faz o `make priv_variantes`; mostra «USO PROPIO: NO PUBLICAR» e nunca acrescenta `--publicar`) e *Personalizada* (as opções do `tools/mkimagem.py`). Veja `docs/generador.md` (em espanhol).

### 9. Problemas frequentes

| Sintoma | Causa e solução |
| --- | --- |
| `CPU you selected does not support x86-64 instruction set` ou `gcc: ... não aceita -m32` | Falta o `gcc-multilib` (ou o suporte de 32 bits em `~/.local/saguitools/root`). Instale-o; não tire o `-m32` |
| Um cabeçalho «não existe» no kernel ou em um programa | O `env.sh` exporta `CPATH` com a glibc do PC e o GCC o acrescenta mesmo com `-nostdinc`; o `Makefile` já o anula para tudo o que vai dentro do SaguiOS. **Não conserte** voltando a passar `CPATH` ou `-I` para a glibc do PC: falta um cabeçalho nosso ou um `-I` |
| `env.sh: line 5: LD_LIBRARY_PATH: unbound variable` | O `run_in_qemu.sh` usa `set -u`. Dentro do `env.sh`, as variáveis que podem não existir são ampliadas com `${VAR:+:$VAR}` ou `${VAR:-}`, nunca com `$VAR` solto (já está assim em `tools/entorno-local.sh`; mantenha se editar). Teste: `bash -c 'set -u; unset LD_LIBRARY_PATH LIBRARY_PATH CPATH; . ~/.local/saguitools/env.sh && echo OK'` |
| Os programas em C++ (OpenJazz, Command & Conquer, DOSBox) não aparecem | Não há `g++` de 32 bits (`cc1plus`): instale `g++` ou execute `tools/entorno-local.sh`. O resto é construído normalmente |
| Sem NetSurf, GNUBoy, fontes Typodermic ou dados de Quake/Jazz | São opcionais e só entram se você baixou antes os dados com `make priv_descargas` (veja 5 e 7) |
| `falta el mcopy` / `mke2fs` / `mkfs.vfat` | Instale `mtools`, `e2fsprogs`, `dosfstools` (ou execute `tools/entorno-local.sh`) |
| QEMU muito lento | Sem KVM usa emulação por software. Verifique se `/dev/kvm` existe e se você está no grupo: `sudo usermod -aG kvm $USER` (abra uma sessão nova). Para forçar sem KVM: `--sin-kvm` |
| Com KVM algo específico de Intel falha, ou sem KVM fica muito lento | Com KVM a CPU é a do seu PC (pode ser AMD): o que espera uma CPU Intel pode falhar. A emulação sem KVM não verifica limites de segmento nem alguns bits da CPU: teste sempre em hardware real ou com KVM antes de confiar |
| O PC trava ou reinicia ao compilar e testar ao mesmo tempo | No WSL2, `make -jN` com muitos núcleos mais vários QEMU em TCG saturam CPU e memória. Use `-j4`, um teste de cada vez, de preferência com KVM. A bateria completa (`python3 tests/rodar_todos.py`) é longuíssima: não a execute junto com outras coisas pesadas |
| Um teste (h34) falha ao preparar seu disco | Acontece se existir `input_share/`: a compilação inclui seus jogos. Para testar, `make all INPUT_SHARE= OPENJAZZ_DATOS=0 QUAKE_DATOS=0` |
| Testar uma imagem velha sem perceber | Encadeie com `&&`: `make -j4 && python3 tests/hNNN_xxx.py`; se a ligação falhar, a imagem não é regenerada e o teste inicia a anterior |
| O `make` falha com arquivos estranhos chamados `*:Zone.Identifier` | São restos de copiar do Windows; apague-os |
| O PC não inicia a imagem | As imagens de disco e a ISO arrancam por BIOS (Legacy/CSM) e por UEFI (com o Secure Boot em «Other OS», `docs/uefi.md`). Grave em modo **DD**. Teste antes a `saguios.img` (um núcleo) |
