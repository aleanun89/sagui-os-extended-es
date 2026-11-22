# SaguiOS-Extended Applications Guide

[Español](guia-aplicaciones.md) · [Português](guia-aplicaciones_PT.md) · [English](guia-aplicaciones_EN.md)

> For anyone who wants to know **which programs SaguiOS ships** and **where the code** of each one is.
> The applications are organised like the **Start** menu. For each one the guide gives the name that
> is seen, the command or the file in the image, one line about what it does, the path of its code and
> the most important design decision (`D…`), which is in [`decisoes.md`](decisoes.md).
> The kernel is explained in the [kernel guide](guia-kernel_EN.md).

**Index**

1. [How the programs are split: `/bin` and `/apps`](#1-how-the-programs-are-split-bin-and-apps)
2. [Start menu, by categories](#2-start-menu-by-categories)
3. [Development: compilers and tools](#3-development-compilers-and-tools)
4. [Console commands: `sagsh` and `/bin`](#4-console-commands-sagsh-and-bin)
5. [Libraries](#5-libraries)
6. [Third-party ports](#6-third-party-ports)
7. [Where to read more](#7-where-to-read-more)

---

## 1. How the programs are split: `/bin` and `/apps`

SaguiOS has two places for the executables:

| Folder | What is there | How it gets there |
| --- | --- | --- |
| `/bin` | The basic commands and the small applications | In the base of the system, the one that is loaded at boot |
| `/apps/bin` | The large applications: the browser, the games, the compilers | In the **SAGUI-APPS** partition, which is mounted at `/apps` (decisions D194 and D324) |

The list of what goes to `/apps` is in the `Makefile` (variable `APPS_MOVER_FIJO` and
`APPS_PROGRAMAS`). In this way the boot in RAM weighs less. The other build rules of each program
are in its folder, in a `.mk` file (for example, `userland/terceros/netsurf/netsurf.mk`).

> **Menu rule:** an entry of `/apps` only appears in the Start menu if its executable exists
> (function `programa_disponivel()` of `kernel/gui/barra.c`, decision D194). That is why the entries
> of the large games do not appear in an image that does not carry them.

**Symbol `†`:** the programs with this mark are not in the **without GPL** variant (`make SEM_GPL=1`,
decision D109): their code is third-party GPL and it is isolated. See [`terceros.md`](terceros.md).

---

## 2. Start menu, by categories

The Start menu is defined in the table `programas[]` of `kernel/gui/barra.c`. There are seven groups.
The tables of this section come from that table: each row is an entry of the menu, in the same order.

### 2.1 Accessories

| Name in the menu | Command (path) | What it does | Code | Decision |
| --- | --- | --- | --- | --- |
| Archivos (Files) | `/bin/arquivos` | file and folder explorer | `userland/programas/arquivos.c` | — |
| Calculadora (Calculator) | `/bin/calculadora` | calculator | `userland/programas/calculadora.c` | — |
| Editor de texto (Text editor) | `/bin/editor` | simple text editor | `userland/programas/editor.c` | — |
| Recortes (Snipping) | `/bin/recortes` | screen snippets | `userland/programas/recortes.c` | D125 |
| Iconos (Icons) | `/bin/icones` | icon library at a glance | `userland/programas/icones.c` | D135 |
| Música (Music) | `/apps/bin/musica` | simple music player | `userland/programas/musica.c` | — |
| MIDI | `/bin/midi` | player for MIDI files | `userland/programas/midi.c` | D190 |
| Markdown | `/bin/markdown` | Markdown viewer and editor | `userland/programas/markdown.c` | — |
| Mico (code editor) | `/apps/bin/mico` | code editor (Mico) | `userland/programas/mico/` | — |
| VisualPythonS (designer) | `/apps/bin/python /lib/python/vps/disenador.py` | visual designer with Python (it runs with MicroPython) | `rootfs/lib/python/vps/disenador.py` | D297 |
| Explorador SQLite (SQLite explorer) | `/apps/bin/python /lib/python/basedatos/explorador_sqlite.py` | explorer of SQLite databases (with MicroPython) | `rootfs/lib/python/basedatos/explorador_sqlite.py` | D333 |
| Explorador lite-monkey (lite-monkey explorer) | `/apps/bin/python /lib/python/basedatos/explorador_litemonkey.py` | explorer of the lite-monkey database (with MicroPython) | `rootfs/lib/python/basedatos/explorador_litemonkey.py` | D333 |
| Pluma (word processor) | `/apps/bin/pluma` | word processor | `userland/programas/pluma.c` | D291 |
| Traductor (Translator) | `/apps/bin/traductor` | translation editor (in the style of Poedit) | `userland/programas/traductor.c` | D335 |
| Macaco (spreadsheet) | `/apps/bin/macaco` | spreadsheet | `userland/programas/macaco.c` | D142 |
| Tucán (presentations) | `/apps/bin/tucan` | presentation editor | `userland/programas/tucan.c` | D369 |
| Visor de PDF (PDF viewer) | `/apps/bin/visorpdf` | PDF viewer (engine in userland/pdf/) | `userland/programas/visorpdf.c` | D139 |
| IA (chat) (AI chat) | `/apps/bin/ia` | AI chat on the command line (library in userland/ia/) | `userland/programas/ia.c` | D139 |
| Chat IA (AI chat) | `/apps/bin/chatia` | AI chat in a window (library in userland/ia/) | `userland/programas/chatia.c` | D143 |

### 2.2 Graphics

| Name in the menu | Command (path) | What it does | Code | Decision |
| --- | --- | --- | --- | --- |
| Paint | `/apps/bin/paint` | drawing program | `userland/programas/paint.c` | — |
| Visor de imágenes (Image viewer) | `/bin/visor` | image viewer | `userland/programas/visor.c` | — |
| PlanoMini (2D CAD) | `/apps/bin/planomini` | mini 2D CAD (plans) | `userland/planomini/` | D312 |
| Reproductor de vídeo (Video player) | `/apps/bin/reproductor` | video player | `userland/programas/reproductor.c` | D139 |
| Cámara (Camera) | `/apps/bin/camara` | webcam (UVC) | `userland/programas/camara.c` | D277 |

### 2.3 Internet

| Name in the menu | Command (path) | What it does | Code | Decision |
| --- | --- | --- | --- | --- |
| Cipó (browser) | `/apps/bin/cipo` | Cipó web browser (engine in userland/web/) | `userland/programas/cipo.c` | — |
| NetSurf (browser) | `/apps/bin/netsurf` | NetSurf browser (third-party code) | `userland/terceros/netsurf/` | D184 † |
| Correo (Mail) | `/apps/bin/correo` | mail client (library in userland/mail/) | `userland/programas/correo.c` | D146 |
| Escritorio remoto (VNC) (Remote desktop) | `/bin/vnc` | VNC remote desktop client | `userland/programas/vnc.c` | D254 |

### 2.4 Multimedia

| Name in the menu | Command (path) | What it does | Code | Decision |
| --- | --- | --- | --- | --- |
| Sintonía (player) (Tuning player) | `/apps/bin/sintonia` | music player with playlists (m3u, pls) and radio stations over HTTPS | `userland/programas/sintonia.c` | D225 |
| Grabadora de sonido (Sound recorder) | `/apps/bin/grabadora` | sound recorder from the microphone | `userland/programas/grabadora.c` | D292 |
| Conversor de audio (Audio converter) | `/apps/bin/convertir-audio` | converter between MP3, FLAC and WAV (the MP3 encoder is Shine, in userland/terceros/shine/) | `userland/programas/convertir-audio.c` | D339 |

### 2.5 Education

| Name in the menu | Command (path) | What it does | Code | Decision |
| --- | --- | --- | --- | --- |
| GebraMini (dynamic geometry) | `/apps/bin/gebramini` | dynamic geometry | `userland/gebramini/` | D309 |
| Jabuti (programming by playing) | `/apps/bin/jabuti` | block programming for children | `userland/jabuti/` | D377 |
| Laboratorio de C (C laboratory) | `/apps/bin/labc` | C laboratory: editor, compiler (libtcc) and examples | `userland/programas/labc/` | D414 |
| Actividades JClic (JClic activities) | `/apps/bin/jclic` | viewer of JClic educational activities | `userland/programas/jclic.c` | D185 |
| Actividades JClic (bajar) (JClic activities, download) | `/apps/bin/jclicbajar` | downloader of JClic activities from the repository | `userland/programas/jclicbajar.c` | D185 |

### 2.6 Games

| Name in the menu | Command (path) | What it does | Code | Decision |
| --- | --- | --- | --- | --- |
| Sagui Atrapa Bananas (Sagui catches bananas) | `/apps/bin/banana` | Sagui Pega Banana game | `userland/programas/banana.c` | — |
| Solitario Spider (Spider solitaire) | `/apps/bin/spider` | Spider solitaire | `userland/programas/spider.c` | D173 |
| Buscaminas (Minesweeper) | `/apps/bin/buscaminas` | Minesweeper game | `userland/programas/buscaminas.c` | D248 |
| AoEN | `/apps/bin/aoen` | AoEN, real-time strategy game | `userland/juegos/aoen/` | D382 |
| OpenTyrian | `/apps/bin/opentyrian` | OpenTyrian (shoot-em-up, third-party code) | `userland/terceros/opentyrian/` | D180 † |
| OpenTyrian (pantalla completa) (full screen) | `/apps/bin/opentyrian --pantalla-completa` | OpenTyrian (shoot-em-up, third-party code) (full screen) | `userland/terceros/opentyrian/` | D204 † |
| Freeciv | `/apps/bin/freeciv-sdl2` | Freeciv, turn-based strategy (SDL 2 client) | `userland/terceros/freeciv/` | D374 † |
| OpenTTD | `/apps/bin/openttd` | OpenTTD. Its code is not in this branch: the entry only appears if `/apps/bin/openttd` exists | `—` | D375 † |
| OpenJazz | `/apps/bin/openjazz` | OpenJazz, platformer (Jazz Jackrabbit) | `userland/terceros/openjazz/` | D228 † |
| OpenJazz (pantalla completa) (full screen) | `/apps/bin/openjazz --pantalla-completa` | OpenJazz, platformer (Jazz Jackrabbit) (full screen) | `userland/terceros/openjazz/` | D228 † |
| Command & Conquer | `/apps/bin/conquer` | Command & Conquer (Vanilla Conquer) | `userland/terceros/conquer/` | D230 † |
| Quake | `/apps/bin/quake` | Quake (free engine by id Software) | `userland/terceros/quake/` | D229 † |
| Quake (pantalla completa) (full screen) | `/apps/bin/quake --pantalla-completa` | Quake (free engine by id Software) (full screen) | `userland/terceros/quake/` | D229 † |
| DOOM | `/apps/bin/chocolate-doom` | DOOM (Chocolate Doom) | `userland/terceros/chocolatedoom/` | D383 † |
| Freedoom | `/apps/bin/chocolate-doom --freedoom` | DOOM (Chocolate Doom) | `userland/terceros/chocolatedoom/` | D383 † |
| Tzar | `/apps/bin/tzar` | Tzar, strategy game (C port of ReTzar) | `userland/juegos/tzar/` | D417 † |
| Game Boy (GNUBoy) | `/apps/bin/gnuboy` | Game Boy and Game Boy Color emulator (GNUBoy) | `userland/terceros/gnuboy/` | D235 † |
| Recreativa (juegos y consolas) (Arcade: games and consoles) | `/apps/bin/recreativa` | launcher of DOS games | `userland/terceros/recreativa/` | D294 † |
| DOSBox | `/apps/bin/dosbox` | MS-DOS emulator (DOSBox 0.74) | `userland/terceros/dosbox/` | D294 † |
| DOSBox (pantalla completa) (full screen) | `/apps/bin/dosbox --pantalla-completa` | MS-DOS emulator (DOSBox 0.74) (full screen) | `userland/terceros/dosbox/` | D294 † |
| Sagui 3D | `/apps/bin/sagui3d` | own 3D game | `userland/sagui3d/` | — |
| CHIP-8 | `/apps/bin/chip8` | CHIP-8 emulator | `userland/chip8/` | — |
| ROM de CHIP-8 (bajar) (CHIP-8 ROM, download) | `/apps/bin/python /apps/bin/chip8-roms-gui` | CHIP-8 ROM (download) | `userland/apps/bin/chip8-roms-gui` | D123 |

### 2.7 System

| Name in the menu | Command (path) | What it does | Code | Decision |
| --- | --- | --- | --- | --- |
| Terminal | `/bin/terminal` | terminal in a window | `userland/programas/terminal.c` | — |
| Administrador de tareas (Task manager) | `/bin/tareas` | task manager | `userland/programas/tareas.c` | — |
| Información del PC (PC information) | `/bin/infopc` | PC information | `userland/programas/infopc.c` | — |
| Pantalla (Display) | `/bin/pantalla --ventana` | resolution and screens | `userland/programas/pantalla.c` | — |
| Fecha y hora (Date and time) | `/bin/zona --ventana` | date, time and time zone | `userland/programas/zona.c` | — |
| Configuración (Settings) | `/bin/configuracion` | system settings | `userland/programas/configuracion.c` | — |
| Discos (Disks) | `/bin/discos --ventana` | disks and partitions | `userland/programas/discos.c` | D121 |
| Bluetooth | `/bin/bluetooth --ventana` | Bluetooth | `userland/programas/bluetooth.c` | D279 |
| Instalar SaguiOS (Install SaguiOS) | `/bin/instalar --ventana` | SaguiOS installer on a disk or pendrive | `userland/programas/instalar.c` | D179 |
| DEBUG-APP (remote debugging) | `/bin/debug-app` | remote debugging over SSH | `userland/programas/debug-app.c` | D362 |

---

## 3. Development: compilers and tools

The menu has no "Development" group. These tools are in `/apps/bin` (or in `/bin`) and they are used
from the terminal. The **C laboratory** (in Education) and **Mico** (in Accessories)
also serve to programme.

| Tool | Command | What it does | Code | Decision |
| --- | --- | --- | --- | --- |
| SaguiOS C compiler | `/apps/bin/cc` | compiles C programs into SaguiOS executables (it generates x86 code) | `userland/programas/cc.c`, `userland/programas/cc_biblioteca.h` | — |
| Pascal compiler | `/apps/bin/pascal` | compiles Pascal programs | `userland/programas/pascal.c`, `userland/programas/pascal_biblioteca.h` | D085-D087 |
| TCC (Tiny C Compiler) | `/apps/bin/tcc` | fast C compiler, in the system itself | `userland/terceros/tcc/` | — |
| GNU make | `/apps/bin/make` | builds projects according to a `Makefile` | `userland/terceros/make/` | — |
| git | `/apps/bin/git` | version control, over libgit2 | `userland/terceros/git/` | D400 |
| MicroPython | `/apps/bin/python` | reduced Python 3 (the `python` command of SaguiOS) | `userland/micropython/` | D034 |
| miniJVM (`java`, `javac`) | `/apps/bin/java`, `/apps/bin/javac` | Java interpreter and its compiler | `userland/terceros/minijvm/` | D376 |
| GNU Prolog | `/apps/bin/gprolog`, `/apps/bin/pl2wam` | Prolog interpreter and its compiler | `userland/terceros/gprolog/` | D189 |
| TinyGo (examples) | — | examples in Go; they are compiled with TinyGo | `userland/go/` (see `docs/go.md`) | D364 |
| JavaScript (`qjs`) | `/apps/bin/qjs` | JavaScript interpreter in the terminal | `userland/programas/qjs.c` | D188 |
| SQLite (`sqlite3`) | `/apps/bin/sqlite3` | interpreter of SQL commands over SQLite | `userland/programas/sqlite3.c`, `userland/terceros/sqlite/` | D116 |
| Shell `sagsh` | `/bin/sagsh` (copy: `/bin/sh`) | the command interpreter; see section 4 | `userland/sagsh/` | D037 |
| `debug-app` | `/bin/debug-app` | remote debugging over SSH (another computer on the network gets in) | `userland/programas/debug-app.c` | D362 |

---

## 4. Console commands: `sagsh` and `/bin`

**`sagsh`** is the command interpreter of SaguiOS (the *shell*). It reads what you type, looks for the
program (in `/bin`, `/apps/bin` and the `PATH`) and runs it. `/bin/sh` is a copy of `sagsh`. Its code
is in `userland/sagsh/` (parser in `analise.c`, execution in `executar.c`, the help in
`ayuda.c`). Decision D037.

Each `.c` file of `userland/programas/` is compiled as a command. The name of the file is the
name of the command. The following table comes from the header of each file, so the description is
the one the code itself gives. The **Command** column shows where the executable is in the default image
(`/apps/bin` for those in the list `APPS_MOVER_FIJO` of the `Makefile`, `/bin` for the rest).

| Command | What it does | Code |
| --- | --- | --- |
| `/bin/acel2d` | the 2D by the GPU of the desktop (D195) | `userland/programas/acel2d.c` |
| `/bin/admin` | runs a command as administrator (root) (D255, improvement 098) | `userland/programas/admin.c` |
| `/bin/afinidad` | the CPUs where a process can run (D174) | `userland/programas/afinidad.c` |
| `/bin/amd` | the state of the 3D engine and the identification of the AMD GPU (improvement 054, part B2; D158) | `userland/programas/amd.c` |
| `/bin/ampliar-home` | "Extend /home to use all the free space" (D318, v8.4.3) | `userland/programas/ampliar-home.c` |
| `/bin/arquivos` | the file explorer of SaguiOS (H23; improvement 058, D136; renewed in v8.5.5.2, D420) | `userland/programas/arquivos.c` |
| `/bin/arranque` | test a kernel by booting it ONLY once, and change the stage 2 of the disk (D392) | `userland/programas/arranque.c` |
| `/apps/bin/banana` | Sagui Pega Banana, the game of SaguiOS (H27) | `userland/programas/banana.c` |
| `/bin/base64` | encodes or decodes Base64 (D232) | `userland/programas/base64.c` |
| `/bin/basename` | removes the folder (and a suffix) from a path (D097) | `userland/programas/basename.c` |
| `/bin/bc` | command-line calculator, simple version (D232) | `userland/programas/bc.c` |
| `/bin/bench` | measures the speed of the memory and of the graphical interface (D042) | `userland/programas/bench.c` |
| `/bin/benchmark` | benchmark of monoNúcleo and multiNúcleo with a score (D176) | `userland/programas/benchmark.c` |
| `/bin/bip` | bip [frequency [duration]] - plays a note on the PC speaker (B3) | `userland/programas/bip.c` |
| `/bin/bloqueo` | advertising and tracking blocking by DNS, in the style of Pi-hole (D218) | `userland/programas/bloqueo.c` |
| `/bin/bluetooth` | Bluetooth with a standard USB controller, e.g. the CSR8510 | `userland/programas/bluetooth.c` |
| `/bin/brillo` | the brightness of the laptop screen (D403) | `userland/programas/brillo.c` |
| `/apps/bin/buscaminas` | the Minesweeper of SaguiOS (D248) | `userland/programas/buscaminas.c` |
| `/bin/cal` | calendar (D232): the current month, any month or a whole year | `userland/programas/cal.c` |
| `/bin/calculadora` | the Calculator of SaguiOS (H26) | `userland/programas/calculadora.c` |
| `/apps/bin/camara` | camara [-p] [--modo N] [--info] [--foto file [--cuadros N]] - the Camera application (D277) | `userland/programas/camara.c` |
| `/bin/captura` | captura [file] [-v/--ventana] [-r x,y,width,height] [-t seconds] (D125) Saves the screen already composed (without cursor) as PNG | `userland/programas/captura.c` |
| `/bin/cat` | shows files; without arguments (or with "-") copies the input to the output (D097) | `userland/programas/cat.c` |
| `/apps/bin/cc` | the C compiler of SaguiOS | `userland/programas/cc.c` |
| `/bin/certificados` | manages the TLS root store (D063) | `userland/programas/certificados.c` |
| `/apps/bin/chatia` | the AI chat with a window (D143, D239) | `userland/programas/chatia.c` |
| `/bin/chgrp` | changes the group of files and folders (D282, improvement 098): root, or the owner to its own group | `userland/programas/chgrp.c` |
| `/bin/chmod` | changes the permissions of files and folders (D282, improvement 098) | `userland/programas/chmod.c` |
| `/bin/chown` | changes the owner (and the group) of files and folders (D282, improvement 098) | `userland/programas/chown.c` |
| `/apps/bin/cipo` | cipo [address] (browser alias) - the Cipó, browser of SaguiOS (D047) | `userland/programas/cipo.c` |
| `/bin/cmp` | compares two files byte by byte (D232) | `userland/programas/cmp.c` |
| `/bin/column` | formats a text into columns (D232) | `userland/programas/column.c` |
| `/bin/comm` | compares two sorted files line by line (D232) | `userland/programas/comm.c` |
| `/bin/componentes` | demonstration of the graphical kit of the libsagui (H22): label, button, text box, text area with scrolling, list and dialogues | `userland/programas/componentes.c` |
| `/bin/configuracion` | the "Configuración" (Settings) application of SaguiOS (D112, improvements 007 and 047) | `userland/programas/configuracion.c` |
| `/bin/conky` | own clone (not a port) of Conky for the SaguiOS desktop (D365) | `userland/programas/conky.c` |
| `/bin/contador` | counts seconds, to show multitasking | `userland/programas/contador.c` |
| `/apps/bin/convertir-audio` | the Audio converter (D339) | `userland/programas/convertir-audio.c` |
| `/apps/bin/correo` | mail client with a window (082) and the look of classic Outlook (113, D289) | `userland/programas/correo.c` |
| `/bin/cp` | copies files and folders (D097) | `userland/programas/cp.c` |
| `/bin/cron` | runs tasks at fixed times (D216, improvement 074) | `userland/programas/cron.c` |
| `/bin/crontab` | the cron table of the user, /home/.config/crontab (D216) | `userland/programas/crontab.c` |
| `/bin/curl` | download over HTTP/1.1 or HTTPS (D046, D062) | `userland/programas/curl.c` |
| `/bin/cut` | extracts fields or columns from each line (D097) | `userland/programas/cut.c` |
| `/bin/data` | date and time (D080) | `userland/programas/data.c` |
| `/bin/debug-app` | DEBUG-APP: lets another computer on the local network in, by SSH and for a while, to debug this one (D362) | `userland/programas/debug-app.c` |
| `/bin/desmontar` | writes everything that is pending and frees the CD or pendrive | `userland/programas/desmontar.c` |
| `/bin/df` | free space on the mounted file systems (D097) | `userland/programas/df.c` |
| `/bin/dhcp` | asks for an IP address by DHCP and shows the result (D046) | `userland/programas/dhcp.c` |
| `/bin/diff` | compares two files line by line (D097) | `userland/programas/diff.c` |
| `/bin/dirname` | removes the last component of a path (D097) | `userland/programas/dirname.c` |
| `/bin/discos` | the disks and the space each one takes up (H34, D040, D121) | `userland/programas/discos.c` |
| `/bin/dmesg` | the kernel log (D232): what it wrote at boot and afterwards | `userland/programas/dmesg.c` |
| `/bin/dnsbloqueo` | minimal DNS server with the advertising blocking filter (D218): the house "Pi-hole" | `userland/programas/dnsbloqueo.c` |
| `/bin/du` | space taken up by files and folders (D097) | `userland/programas/du.c` |
| `/bin/echo` | writes the arguments separated by spaces | `userland/programas/echo.c` |
| `/bin/ecualizador` | 5-band equaliser for all the sound of the system (D201) | `userland/programas/ecualizador.c` |
| `/bin/editor` | the Text editor of SaguiOS (H25) | `userland/programas/editor.c` |
| `/bin/eject` | eject [-t] [reader] - unmounts the CD/DVD and opens the tray (D233) | `userland/programas/eject.c` |
| `/bin/ejecutar` | the dialogues of the Start menu (D065) | `userland/programas/ejecutar.c` |
| `/bin/energia` | screensaver, lock and screen switch-off on inactivity, battery and "do not disturb" (D422) | `userland/programas/energia.c` |
| `/bin/env` | shows the environment or runs a command with a modified environment (D097) | `userland/programas/env.c` |
| `/apps/bin/es11demo` | OpenGL ES 1.1 demonstration with EGL (D425) | `userland/programas/es11demo.c` |
| `/bin/escaner` | SaguiOS's own network scanner (D398) | `userland/programas/escaner.c` |
| `/bin/expand` | changes tabs into spaces (D232) | `userland/programas/expand.c` |
| `/bin/expr` | evaluates an expression from its arguments (D232), like the Linux one | `userland/programas/expr.c` |
| `/bin/falha` | causes an error on purpose to show the protection: only this process is ended by the kernel; the shell and the system carry on | `userland/programas/falha.c` |
| `/bin/file` | says what type each file is, by looking at its content (D232) | `userland/programas/file.c` |
| `/bin/find` | searches files and folders by name, type, size | `userland/programas/find.c` |
| `/bin/fold` | breaks long lines (D232) | `userland/programas/fold.c` |
| `/bin/frecuencia` | control of the CPU frequency (D106, improvement 061) | `userland/programas/frecuencia.c` |
| `/bin/free` | total, used and free memory (D097) | `userland/programas/free.c` |
| `/bin/fuentes` | install, remove and see the system fonts (D356) | `userland/programas/fuentes.c` |
| `/apps/bin/gebramini` | dynamic geometry and functions of SaguiOS (D309, improvement 128) | `userland/programas/gebramini.c` |
| `/bin/gpu3d` | 3D by the GPU (D285) | `userland/programas/gpu3d.c` |
| `/apps/bin/grabadora` | the Sound recorder (D292) | `userland/programas/grabadora.c` |
| `/bin/grabar` | grabar file.wav [seconds] [options] - records the microphone to a WAV (D292) | `userland/programas/grabar.c` |
| `/bin/grep` | searches for lines that match a pattern (D097) | `userland/programas/grep.c` |
| `/bin/gunzip` | gunzip = gzip -d (improvement 056): see gzip.c | `userland/programas/gunzip.c` |
| `/bin/gzip` | gzip, gunzip and zcat (improvement 056, D103): a single source; gunzip.c and zcat.c include it with a different MODO_GZ | `userland/programas/gzip.c` |
| `/bin/head` | first lines (or bytes) of files or of the input (D097) | `userland/programas/head.c` |
| `/bin/hostname` | name of the machine (D097) | `userland/programas/hostname.c` |
| `/bin/htop` | interactive process monitor for the terminal (D366) | `userland/programas/htop.c` |
| `/apps/bin/ia` | AI chat client on the command line (D134) | `userland/programas/ia.c` |
| `/bin/icones` | the icon library of SaguiOS, at a glance (D135) | `userland/programas/icones.c` |
| `/bin/id` | identity of the process (D232; real since D255): the uid and gid that the kernel gives it, with the names from /etc/usuarios | `userland/programas/id.c` |
| `/bin/ifconfig` | shows and configures the network card (D046) | `userland/programas/ifconfig.c` |
| `/bin/infopc` | PC information, in the style of CPU-Z (D041) | `userland/programas/infopc.c` |
| `/bin/instalar` | copies SaguiOS to a hard disk or to another pendrive (D179) | `userland/programas/instalar.c` |
| `/apps/bin/jabuti` | programming with blocks for children (D377, improvement 137) | `userland/programas/jabuti.c` |
| `/apps/bin/jclic` | viewer of JClic educational activities (D185) | `userland/programas/jclic.c` |
| `/apps/bin/jclicbajar` | "JClic activities (download)": the downloader of the XTEC Clic repository (D185) | `userland/programas/jclicbajar.c` |
| `/bin/kconfig` | kernel options that are saved from the system and take effect on reboot (D353) | `userland/programas/kconfig.c` |
| `/bin/kill` | ends a process by its PID | `userland/programas/kill.c` |
| `/bin/less` | less / more (pager) - shows a file or the screen input on screen (D097) | `userland/programas/less.c` |
| `/bin/login` | login on the text console (D371, phase 6, P12) | `userland/programas/login.c` |
| `/bin/ls` | lists files and folders (D097; D385 puts them in columns) | `userland/programas/ls.c` |
| `/bin/lscpu` | CPU information in the Linux format (D232), based on CPUID and what the kernel gives | `userland/programas/lscpu.c` |
| `/bin/lspci` | lists the PCI devices in the Linux format (D232), based on what the kernel gives | `userland/programas/lspci.c` |
| `/apps/bin/macaco` | the SaguiOS spreadsheet (improvement 078, D140) | `userland/programas/macaco.c` |
| `/bin/mandos` | gamepads and joysticks from the terminal (D307; docs/mandos.md) | `userland/programas/mandos.c` |
| `/bin/markdown` | markdown [-t] [file.md] - SaguiOS Markdown viewer and editor (D045) | `userland/programas/markdown.c` |
| `/bin/md5sum` | MD5 digest of files, as in Linux (D232) | `userland/programas/md5sum.c` |
| `/bin/mem` | total, used and free memory | `userland/programas/mem.c` |
| `/bin/midi` | midi [file / folder] | `userland/programas/midi.c` |
| `/bin/mkdir` | creates folders (D097) | `userland/programas/mkdir.c` |
| `/bin/mktemp` | creates a temporary file or folder with a unique name and writes its path (D232) | `userland/programas/mktemp.c` |
| `/bin/montar` | mounts the pendrive on /usb (no name: the first one available to mount) | `userland/programas/montar.c` |
| `/bin/more` | simple pager: the same program as less (D097) | `userland/programas/more.c` |
| `/bin/mount` | mounts file systems (D040) | `userland/programas/mount.c` |
| `/apps/bin/musica` | musica [file / folder] | `userland/programas/musica.c` |
| `/bin/mv` | moves or renames files and folders (D097) | `userland/programas/mv.c` |
| `/bin/nc` | basic nc (netcat) - TCP connections from the command line (D046) | `userland/programas/nc.c` |
| `/bin/nl` | numbers the lines (D232) | `userland/programas/nl.c` |
| `/bin/nproc` | how many CPUs there are (D232) | `userland/programas/nproc.c` |
| `/bin/nslookup` | nslookup (alias host) name [server[:port]] - asks the DNS (D046) | `userland/programas/nslookup.c` |
| `/bin/ntp` | sets the clock right by SNTP (RFC 4330), over UDP (D080) | `userland/programas/ntp.c` |
| `/bin/od` | dumps a file in octal, hexadecimal or characters (D232); with -C it gives the format of "hexdump -C" | `userland/programas/od.c` |
| `/bin/ola` | prints the arguments received (loader test, H13) | `userland/programas/ola.c` |
| `/apps/bin/paint` | the SaguiOS drawing program (D056) | `userland/programas/paint.c` |
| `/bin/panel` | the touch panel (touchpad) from the console (D407) | `userland/programas/panel.c` |
| `/bin/pantalla` | screen resolution (D058, D059) | `userland/programas/pantalla.c` |
| `/bin/particiones` | the partition table of the disks, from the terminal (D179) | `userland/programas/particiones.c` |
| `/apps/bin/pascal` | the SaguiOS Pascal compiler (D085-D087) | `userland/programas/pascal.c` |
| `/bin/passwd` | changes the password of an account (D255, improvement 098) | `userland/programas/passwd.c` |
| `/bin/paste` | joins the lines of several files side by side (D232) | `userland/programas/paste.c` |
| `/bin/ping` | ping [-c N] [-s bytes] [-W ms] host - ICMP echo (D046) | `userland/programas/ping.c` |
| `/bin/planificador` | the process scheduler between the CPUs (D174) | `userland/programas/planificador.c` |
| `/apps/bin/planomini` | SaguiOS mini 2D CAD (D312, improvement 129) | `userland/programas/planomini.c` |
| `/apps/bin/pluma` | the SaguiOS word processor (improvement 116, D291), with the look of a simplified Word 97: menu bar, Standard and Format toolbars with icons, ruler with margins and indents, page view on a grey background and status bar | `userland/programas/pluma.c` |
| `/bin/portatil` | what is specific to laptops through ACPI (D403) | `userland/programas/portatil.c` |
| `/bin/ps` | lists the processes (D097) | `userland/programas/ps.c` |
| `/apps/bin/qjs` | the JavaScript interpreter in the terminal (D188) | `userland/programas/qjs.c` |
| `/bin/radios` | state of the radios (Wi-Fi, Bluetooth, WWAN) of HP laptops through WMI, and turning them on or off (D405) | `userland/programas/radios.c` |
| `/bin/realpath` | clean absolute path of a file or folder (D232) | `userland/programas/realpath.c` |
| `/bin/recortes` | recortes [--recorte] [-t seconds] - screen snippets (D125) | `userland/programas/recortes.c` |
| `/bin/reparar` | repairs an ext2 that was left damaged after a cut or a freeze (D397) | `userland/programas/reparar.c` |
| `/bin/reproducir` | reproducir [-v volume] file | `userland/programas/reproducir.c` |
| `/apps/bin/reproductor` | reproductor [file / folder / list.m3u] | `userland/programas/reproductor.c` |
| `/bin/rev` | writes each line backwards (D232) | `userland/programas/rev.c` |
| `/bin/rm` | deletes files and, with -r, folders (D097) | `userland/programas/rm.c` |
| `/bin/rmdir` | deletes empty folders (D097) | `userland/programas/rmdir.c` |
| `/bin/saguimark` | the SaguiOS benchmark in text mode (D093, improvement 051) | `userland/programas/saguimark.c` |
| `/bin/scp` | copies a file to another machine, or fetches one, over SSH (D317) | `userland/programas/scp.c` |
| `/bin/sed` | stream editor, the basics (D232): s///, d, p, q, =, a, i, c with addresses | `userland/programas/sed.c` |
| `/bin/sensores` | the hardware monitor in text mode (D092, improvement 050) | `userland/programas/sensores.c` |
| `/bin/seq` | writes a sequence of whole numbers (D097) | `userland/programas/seq.c` |
| `/bin/servicio` | manages the background services from the terminal (D216) | `userland/programas/servicio.c` |
| `/bin/servicios` | the supervisor of the service manager (D216, improvement 074) | `userland/programas/servicios.c` |
| `/apps/bin/sgldemo` | rotating colour cube with SaguiGL (improvement 054, v5; D104) | `userland/programas/sgldemo.c` |
| `/bin/sha1sum` | SHA-1 digest of files, as in Linux (D232) | `userland/programas/sha1sum.c` |
| `/bin/sha256sum` | SHA-256 digest of files (D060), as in Linux | `userland/programas/sha256sum.c` |
| `/bin/simd` | shows and tests the x87 FPU, MMX, SSE and SSE2 (D032) | `userland/programas/simd.c` |
| `/apps/bin/sintonia` | sintonia [file / folder / list.m3u / list.pls / http://station] | `userland/programas/sintonia.c` |
| `/bin/sleep` | waits a few seconds (D097) | `userland/programas/sleep.c` |
| `/bin/sobre` | version and credits of SaguiOS-Extended (D101) | `userland/programas/sobre.c` |
| `/bin/sobre-o-sistema` | the "About SaguiOS-Extended" window (H28, D101): version, image size, active processes, memory and credits | `userland/programas/sobre-o-sistema.c` |
| `/bin/sonido` | diagnostics and tests of the HDA sound codec (D107) | `userland/programas/sonido.c` |
| `/bin/sonidos` | the system sounds (D421): plays the one of an event and watches USB and battery | `userland/programas/sonidos.c` |
| `/bin/sort` | sorts the lines of files or of the input (D097) | `userland/programas/sort.c` |
| `/apps/bin/spider` | SaguiOS Spider solitaire (D173) | `userland/programas/spider.c` |
| `/bin/split` | splits a file into pieces (D232) | `userland/programas/split.c` |
| `/apps/bin/sqlite3` | interpreter of SQL commands over the SQLite of userland/terceros/sqlite (D116) | `userland/programas/sqlite3.c` |
| `/bin/ssh` | SSH client (D317): log in from SaguiOS to another machine | `userland/programas/ssh.c` |
| `/bin/ssh-keygen` | ed25519 keys for the ssh client (D317) | `userland/programas/ssh-keygen.c` |
| `/bin/sshd` | SSH administration server (D250, improvement 052) | `userland/programas/sshd.c` |
| `/bin/stat` | data of a file or folder (D097) | `userland/programas/stat.c` |
| `/bin/su` | opens a shell with another account, root by default (D255, improvement 098) | `userland/programas/su.c` |
| `/bin/sync` | writes the block cache of all the disks (D281) | `userland/programas/sync.c` |
| `/bin/tablasacpi` | dumps the ACPI tables (DSDT, SSDT, FACP...) of the machine to .dat files, to study their AML on another computer (iasl -d) or with tools/uacpi_volcado (D405). It only reads: it does not change anything in the system | `userland/programas/tablasacpi.c` |
| `/bin/tac` | writes the lines in reverse order: the last one first (D232) | `userland/programas/tac.c` |
| `/bin/tail` | last lines (or bytes) of files or of the input (D097) | `userland/programas/tail.c` |
| `/bin/tar` | packs and unpacks files (improvement 056, D103) | `userland/programas/tar.c` |
| `/bin/tareas` | the Task manager (D043, D079, D216) | `userland/programas/tareas.c` |
| `/bin/teclado` | shows or changes the keyboard layout (ES or ABNT2) | `userland/programas/teclado.c` |
| `/bin/tee` | copies the input to the output and to files (D097) | `userland/programas/tee.c` |
| `/bin/temp` | the CPU temperature in one line (D092, improvement 050) | `userland/programas/temp.c` |
| `/bin/terminal` | the Window terminal (H24), with tabs and preferences (D231) | `userland/programas/terminal.c` |
| `/bin/time` | runs a command and measures how long it takes (D232) | `userland/programas/time.c` |
| `/bin/timeout` | runs a command and ends it if it goes over a time (D232) | `userland/programas/timeout.c` |
| `/bin/top` | the live processes, in the terminal (D232) | `userland/programas/top.c` |
| `/bin/touch` | creates empty files if they do not exist and sets their dates (D097, D341) | `userland/programas/touch.c` |
| `/bin/touchpad` | touchpad = panel (D407): see panel.c | `userland/programas/touchpad.c` |
| `/bin/tr` | translates, deletes or squeezes characters of the input (D232) | `userland/programas/tr.c` |
| `/apps/bin/traductor` | SaguiOS translation editor, in the style of Poedit (D335, improvement 133) | `userland/programas/traductor.c` |
| `/bin/tree` | shows folders and files as a tree (D097) | `userland/programas/tree.c` |
| `/bin/truncate` | changes the size of a file (D232) | `userland/programas/truncate.c` |
| `/apps/bin/tucan` | the SaguiOS presentation editor (D369), with the ribbon of the suite (Pluma, Macaco) | `userland/programas/tucan.c` |
| `/bin/umount` | umount <folder/device> - writes what is pending and unmounts (D040) | `userland/programas/umount.c` |
| `/bin/uname` | name and version of the system (D097) | `userland/programas/uname.c` |
| `/bin/uniq` | removes (or counts) consecutive repeated lines (D097) | `userland/programas/uniq.c` |
| `/bin/unzip` | lists, checks and extracts .zip files (improvement 056, D103) | `userland/programas/unzip.c` |
| `/bin/uptime` | current time, how long the system has been on and how many processes there are (D097) | `userland/programas/uptime.c` |
| `/bin/usb` | diagnostics: USB 2.0 controllers, ports with a device and recognised devices (H29, H30) | `userland/programas/usb.c` |
| `/bin/visor` | the SaguiOS Image viewer (D055) | `userland/programas/visor.c` |
| `/apps/bin/visorpdf` | the SaguiOS PDF viewer (improvement 059, D138; full desktop interface in improvement 105, D274) | `userland/programas/visorpdf.c` |
| `/bin/vnc` | VNC remote desktop client (improvement 097 phase 2, D254) | `userland/programas/vnc.c` |
| `/bin/vncd` | VNC remote desktop server (improvement 097, D252) | `userland/programas/vncd.c` |
| `/bin/volumen` | volumen [0-100 / +N / -N / silencio / activar / salida [N]] - general sound volume (D088) | `userland/programas/volumen.c` |
| `/bin/watch` | repeats a command every certain time and shows its output (D232) | `userland/programas/watch.c` |
| `/bin/wc` | counts lines, words and letters (bytes) of files or of the input (D097) | `userland/programas/wc.c` |
| `/bin/wg` | the SaguiOS WireGuard VPN (D363) | `userland/programas/wg.c` |
| `/bin/wg-quick` | turns the WireGuard VPN on and off (D363): "wg-quick up NAME", "wg-quick down" | `userland/programas/wg-quick.c` |
| `/bin/wget` | downloads a file by HTTP or HTTPS and saves it with the name of the URL (D062) | `userland/programas/wget.c` |
| `/bin/which` | says which program would run for a name (D097) | `userland/programas/which.c` |
| `/bin/whoami` | name of the user (D097; real since D255): the account of the uid of the process | `userland/programas/whoami.c` |
| `/bin/widget-calendario` | the current month, with today marked (D365) | `userland/programas/widget-calendario.c` |
| `/bin/widget-notas` | a sticky note on the desktop (D365) | `userland/programas/widget-notas.c` |
| `/bin/widget-relogio` | the desktop clock (D365): analogue or digital (one click changes the mode; the choice is saved in relogio.modo of widgets.conf). It only repaints when the second changes | `userland/programas/widget-relogio.c` |
| `/bin/widget-sistema` | small system monitor (D365): CPU with its graph, memory and network (download and upload with the graph of the last minute). One reading per second; it does not repaint if the numbers have not changed | `userland/programas/widget-sistema.c` |
| `/bin/widgets` | the desktop widget manager (D365): lists the available ones, turns each one on or off and changes its options; what stays active is saved in /home/.config/widgets.conf and starts by itself when the session starts | `userland/programas/widgets.c` |
| `/bin/wifi` | the Wi-Fi with the Realtek RTL8188EU dongle (D215, D220) | `userland/programas/wifi.c` |
| `/bin/xargs` | builds and runs commands with what arrives on the input (D097) | `userland/programas/xargs.c` |
| `/bin/xxd` | hexadecimal dump of a file, or its reverse with -r (D232) | `userland/programas/xxd.c` |
| `/bin/yes` | repeats a text (by default "y") until the output is closed (D097) | `userland/programas/yes.c` |
| `/bin/zcat` | zcat = gzip -dc (improvement 056): see gzip.c | `userland/programas/zcat.c` |
| `/bin/zip` | creates .zip files (improvement 056, D103) | `userland/programas/zip.c` |
| `/bin/zona` | time zone, board clock and the "Date and time" window (D080) | `userland/programas/zona.c` |

> **Commands that the Makefile names and whose code is not in this branch:** `sglv6` and `es11conf`.
> They are not documented here until their sources exist.

---

## 5. Libraries

A **library** is code that several programs reuse. In SaguiOS there are two groups: the system's own
ones (the *libc* and the *libsagui*) and those of each type of application.

| Library | Folder | What it is for |
| --- | --- | --- |
| **libsagui** | `userland/libsagui/` | SaguiOS's own library. Its file `chamadas.c` wraps each system call (`int 0x80`, see the kernel guide). It also brings windows and the graphical kit (`gui_*.c`), audio (`audio_*.c`), images (JPEG, PNG, GIF), MPEG, compression, network (`http.c`) and documents (`pdf_*.c`, `presentacion_*.c`). |
| **libc** | `userland/libc/` | The common C library: input and output (`es.c`), memory (`memoria.c`), text (`texto.c`) and the POSIX part that SaguiOS has (`posix.c`: `write`, `read`, `open`...). In this way the third-party ports compile almost without changes. |
| **libcxx** | `userland/libcxx/` | The C++ support (`rt/`) for the programs written in C++ (OpenJazz, DOSBox, Gearsystem...). |
| **SDL 2** | `userland/libsdl2/` | An SDL 2 written for SaguiOS: its headers are its own and the code calls the windows and the audio of SaguiOS (D230). It is used to port games. |
| **SDL 1.2** | `userland/libsdl12/` | The same for SDL 1.2, which some games use (D228). |
| **OpenAL** | `userland/libopenal/` | An own subset of OpenAL 1.1 for the 3D sound of the games (D230). |
| **SaguiGL** (`libsgl`) | `userland/libsgl/` | SaguiOS's own 3D. It draws in software and, since v8.3, it can send the triangles to the GPU (see [`gpu3d.md`](gpu3d.md)). It includes fixed scenes (`cenas.c`). |
| **libamd** | `userland/libamd/` | The framework of the 3D driver of the AMD GPUs (D158). |
| **libtls** | `userland/libtls/` | Cryptography and TLS: AES, SHA, GCM, big numbers and HTTPS connections. They are used, among others, by `curl`, `wget` and the browser. |
| **libdl** | `userland/libdl/` | `dlopen`, `dlsym` and `dlclose` inside `libsagui.so` (D378, [`enlace-dinamico.md`](enlace-dinamico.md)). |
| **libfontes** | `userland/libfontes/` | The font engine: families, font loading, subsets and text. Pluma uses it as the document font provider (D319). |
| **web** | `userland/web/` | The web engine of the Cipó: CSS, JavaScript with DOM, Canvas 2D, SVG, software WebGL and the Brotli decompressor for WOFF2 fonts (D141). |
| **pdf** | `userland/pdf/` | The PDF engine of the viewer (D138). |
| **llm** | `userland/llm/` | An own engine for local language models (GGUF format), with the `llm` command in `/apps/bin` (D380). |
| **IA and mail** | `userland/ia/`, `userland/mail/` | The libraries of the AI client (`userland/ia/`) and of the mail client (`userland/mail/`). They are shared, among others, by `ia`, `chatia`, `pluma`, `macaco`, `tucan` and `correo`. |
| **MicroPython** | `userland/micropython/` | Python, with the SaguiOS "glue" (`arquivos_sagui.c`, `janela_sagui.c`). The MicroPython part is third-party (D034). |
| **litemonkey** | `litemonkey/` | Own document database, in C (D117). It is outside `userland/`. |
| **Linkers** | `userland/programa.ld`, `userland/programa_cxx.ld` | They say how the code of a C or C++ program is placed. |
| **API headers** | `userland/inc/` | The OpenGL ES and EGL headers, in addition to those of the libc. |

---

## 6. Third-party ports

A **port** is a free program by another author that has been adapted to run on SaguiOS. Each
folder has a file `leeme.txt` with the changes. The register of everything foreign, with licence and
origin, is in [`terceros.md`](terceros.md).

| Port | Folder | What it is | Command | Decision |
| --- | --- | --- | --- | --- |
| NetSurf | `userland/terceros/netsurf/` | lightweight web browser | `/apps/bin/netsurf` | D184 † |
| Freeciv 3.1.6 | `userland/terceros/freeciv/` | turn-based strategy; server and SDL 2 client | `/apps/bin/freeciv-server`, `/apps/bin/freeciv-sdl2` | D374 † |
| OpenTTD | — (not in this branch) | transport simulator | `/apps/bin/openttd` (if it exists) | D375 † |
| Chocolate Doom 3.1.1 | `userland/terceros/chocolatedoom/` | DOOM, Heretic, Hexen and Strife; with the free content Freedoom | `/apps/bin/chocolate-doom` | D383 † |
| Quake | `userland/terceros/quake/` | the Quake engine, with its software renderer | `/apps/bin/quake` | D229 † |
| Vanilla Conquer | `userland/terceros/conquer/` | Command & Conquer (Tiberian Dawn and Red Alert) | `/apps/bin/conquer` | D230 † |
| OpenJazz | `userland/terceros/openjazz/` | Jazz Jackrabbit | `/apps/bin/openjazz` | D228 † |
| OpenTyrian | `userland/terceros/opentyrian/` | Tyrian 2.1 (the data goes in the SAGUI-JOGOS partition) | `/apps/bin/opentyrian` | D180, D181 † |
| Tzar | `userland/juegos/tzar/` | C port of ReTzar | `/apps/bin/tzar` | D417 † |
| AoEN | `userland/juegos/aoen/` | real-time strategy game of this project | `/apps/bin/aoen` | D382 |
| DOSBox 0.74-3 | `userland/terceros/dosbox/` | MS-DOS emulator | `/apps/bin/dosbox` | D294 † |
| Recreativa | `userland/terceros/recreativa/` | launcher of DOS games (own code) | `/apps/bin/recreativa` | D294 † |
| GNUBoy | `userland/terceros/gnuboy/` | Game Boy and Game Boy Color | `/apps/bin/gnuboy` | D235 † |
| mGBA 0.10.5 | `userland/terceros/mgba/` | Game Boy Advance | `/apps/bin/mgba` | D322 |
| Gearsystem | `userland/terceros/gearsystem/` | Master System, Game Gear and SG-1000 | `/apps/bin/gearsystem` | D321 |
| LaiNES | `userland/terceros/laines/` | NES (Famicom) | `/apps/bin/laines` | D321 |
| Common emulator entry | `userland/terceros/emucomun/` | the entry point shared by the console emulators (own code) | — | D321 |
| TCC 0.9.28rc | `userland/terceros/tcc/` | C compiler; it is also the libtcc used by the C laboratory | `/apps/bin/tcc` | — |
| GNU make 4.4.1 | `userland/terceros/make/` | project building | `/apps/bin/make` | — |
| git (libgit2) | `userland/terceros/git/` | version control | `/apps/bin/git` | D400 |
| MicroPython | `userland/micropython/` | Python | `/apps/bin/python` | D034 |
| GNU Prolog 1.5.0 | `userland/terceros/gprolog/` | Prolog | `/apps/bin/gprolog`, `/apps/bin/pl2wam` | D189 |
| miniJVM | `userland/terceros/minijvm/` | Java | `/apps/bin/java`, `/apps/bin/javac` | D376 |
| SQLite | `userland/terceros/sqlite/` | database | `/apps/bin/sqlite3` | D116 |
| Shine | `userland/terceros/shine/` | fixed-point MP3 encoder (used by the Audio converter) | — (library) | — |
| openrsync | `userland/terceros/openrsync/` | `rsync` as an SSH client | `/apps/bin/rsync` | D314 |
| zlib 1.3.1 | `userland/terceros/zlib/` | data compression library | — (library) | — |
| "Kenney" sounds | `userland/terceros/kenney-sonidos/` | interface sounds (data, with their licence) | — (data) | D421 |
| "Fluent UI" icons | `userland/terceros/fluentui/` | icons of the system library (data, with their licence) | — (data) | D135 |

The games of this project (Sagui Atrapa Bananas, Solitario Spider, Buscaminas, Sagui 3D and CHIP-8)
are not ports: they are in `userland/programas/`, `userland/sagui3d/` and `userland/chip8/`.

---

## 7. Where to read more

| Topic | Document |
| --- | --- |
| The kernel and the system calls | [`guia-kernel_EN.md`](guia-kernel_EN.md) |
| All the design decisions | [`decisoes.md`](decisoes.md) |
| Third-party code, licences and origin | [`terceros.md`](terceros.md), [`licencia.md`](licencia.md) |
| The game ports, one by one | [`doom.md`](doom.md), [`freeciv.md`](freeciv.md), [`tzar.md`](tzar.md), [`aoen.md`](aoen.md), [`dosbox.md`](dosbox.md), [`mgba.md`](mgba.md), [`openttd.md`](openttd.md), [`carencias-netsurf.md`](carencias-netsurf.md) |
| Compiling tools and programs | [`make.md`](make.md), [`tcc.md`](tcc.md), [`git.md`](git.md), [`go.md`](go.md) |
| Desktop applications | [`pluma.md`](pluma.md), [`cipo.md`](cipo.md), [`macaco.md`](macaco.md), [`visualpythons.md`](visualpythons.md), [`litemonkey.md`](litemonkey.md), [`camara.md`](camara.md), [`microfono.md`](microfono.md) |
| Games and DOS, Game Boy and more emulators | [`recreativa.md`](recreativa.md), [`mandos.md`](mandos.md) |

---

> *This guide was generated from the code on 2026-11-21; if something does not match, trust the code.*
