<!-- Traducción de README.md en el commit 13bedffe (sincronía: tools/readme_sincronia.py) -->
<div align="center">

# 🐒 SaguiOS-Extended

[Español](README.md) · [Português](README_PT.md) · [English](README_EN.md)

*Based on [SaguiOS](https://github.com/erigobeli/sagui-os) by Eli Rigobeli.*

**An operating system written from scratch by AI**
for 32-bit x86 PCs, running for real on a 2011 laptop.

Made by Claude (Anthropic) for the project of **Eli Rigobeli**,
and maintained by **Alejandro Anuncibay**, with improvements written with Claude
(Anthropic) and with an AI agent (MiniMax Code).

`562 KB` · `no GRUB` · `no libc` · `no code from other systems (except drivers, D073)` · `more than 130 automated tests`

<img src="docs/capturas/B4-3-fase1-corredor.png" width="49%" alt="Sagui 3D, the first-person game of SaguiOS">
<img src="docs/capturas/H28-tres-programas.png" width="49%" alt="SaguiOS desktop with three programs open">

</div>

> Detailed documentation (`docs/`, `mejoras/`) is in Spanish.

---

## What it is

> **Note about this version:** this is a Spanish translation of the original project, which was
> written in Brazilian Portuguese. The system interface is in Spanish («Archivos», «Configuración», «Terminal»…)
> and the keyboard supports the **ES** (default) and **ABNT2** layouts, which are chosen with the `teclado`
> program (`teclado es` / `teclado abnt2`). Code identifiers, comments, file names and commands
> are kept in Portuguese.

SaguiOS is a complete operating system, from the boot loader to the 3D game, written
entirely in this project, following the specification in [`prd.md`](prd.md). It boots from a USB stick on a
**Lenovo G470** (2011), in VirtualBox and in QEMU, and fits in **28,959,232 bytes**: 12.2 MB
of boot (kernel and ramdisk) and 16 MB of the system partitions, the games partition and the applications
partition (the image target has been 38 MB since v7.0b, D181; it used to be 16 MB; the original version, without the improvements, took 575,488 bytes, which is
39% of a 1.44 MB floppy disk, the PRD target until D050).

Everything inside the image was written here, except for third-party code: two-stage boot loader, kernel,
drivers (ES and ABNT2 keyboard, touchpad, VESA video, USB 2.0, USB stick), FAT32 file system,
window manager, the programs library (`libsagui`), the shell, the programs, the 8x16 typeface
and the drawings, except for the third-party items listed in
[`docs/terceros.md`](docs/terceros.md) (the DejaVu faces of the system and the Fluent icons, MIT). Every design decision is explained in
[`docs/decisoes.md`](docs/decisoes.md) and every bug found, with its cause and its fix, in
[`docs/erros.md`](docs/erros.md).

## Running on real hardware

Photos by Eli at each phase, on the Lenovo G470:

| Phase 1: boot | Phase 2: kernel and terminal | Phase 3: processes and shell |
| :---: | :---: | :---: |
| <img src="testes_eli/fase_1_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_2_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_3_no_notebook.jpg" width="260"> |
| **Phase 4: graphics and mouse** | **Phase 5: windows and programs** | **Extra: Sagui 3D (VM)** |
| <img src="testes_eli/fase_4_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_5_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_bonus_vm.png" width="260"> |

## The phases

| Phase | What was built | On screen |
| --- | --- | :---: |
| **1. Boot** | Own two-stage loader (MBR + stage 2), A20 line, E820 memory map, copy above 1 MB in *unreal mode*, checksum, protected mode, kernel in C | <img src="docs/capturas/H04-boot-vdi.png" width="200"> |
| **2. Kernel and terminal** | GDT, IDT, TSS, error screen, PIC, 100 Hz clock, physical memory, `kmalloc`, ABNT2 keyboard with accents (and ES in this version), SGFS file system | <img src="docs/capturas/H09-ls-cd.png" width="200"> |
| **3. Processes and programs** | Paging with one address space per process, preemptive scheduler, ring 3, system calls, ELF loader, pipes, `sagsh` shell | <img src="docs/capturas/H18-edicao-e-segundo-plano.png" width="200"> |
| **4. Graphics and mouse** | VESA 1024x768x32 with double buffering, own 8x16 font with accents, graphical terminal, PS/2 touchpad | <img src="docs/capturas/H18-fonte-acentos.png" width="200"> |
| **5. Windows and programs** | Desktop, window manager, Archivos, Terminal, Editor, Calculator, Sagui Atrapa Bananas | <img src="docs/capturas/H28-programas-abertos.png" width="200"> |
| **6. Data USB stick** | PCI, USB 2.0 driver (EHCI) with hubs and hot-plug, USB storage, FAT32 with read and write | <img src="docs/capturas/H34-2-arquivos-no-pendrive.png" width="200"> |
| **Extra** | B3: beep through the PC speaker. B4: **Sagui 3D**. (B1 and B2 cancelled by Eli.) | <img src="docs/capturas/B4-6-macaco-tonto.png" width="200"> |

## Sagui 3D

The sagui (a small marmoset) is trapped in a stone labyrinth in the middle of the Atlantic Forest (Mata Atlântica). It has to collect bananas
to open the gate of each phase, while guardian capuchin monkeys try to take the bananas
out of its pocket. The slingshot fires jabuticaba seeds (with three hits, the monkey is stunned and runs away),
every banana eaten leaves a peel that makes the monkey slip, and each phase hides a golden
bunch behind a wall of lianas.

<div align="center">
<img src="docs/capturas/B4-1-tela-de-inicio.png" width="32%">
<img src="docs/capturas/B4-7-macaco-escorregou.png" width="32%">
<img src="docs/capturas/B4-5-vitoria.png" width="32%">
</div>

The 2.5D engine was designed from geometry, without consulting any code, tutorial or
game repository: convex rooms with their own floor and ceiling heights, column-by-column drawing with a "slit" that narrows at each step,
perspective without fisheye, fixed point (the programs do not use the FPU), textured floor and ceiling, and per-room lighting. There are three phases in
text files, with textures and characters drawn in the code itself. Full explanation in the
"Sagui 3D" section of [`docs/decisoes.md`](docs/decisoes.md).

## Extras: GNU Prolog (third-party code)

- **GNU Prolog 1.5.0** (D189, v7.2): `gprolog` (or `prolog`) opens the interpreter with the ISO library;
  `['/ejemplos/familia.pl'].` loads the kinship and lists example. LGPL-3/GPL-2, isolated in
  `userland/terceros/gprolog/` and outside the no-GPL variant. Test: `tests/h156_gprolog.py`.

## Extras: OpenTyrian (third-party code)

- **OpenTyrian** (D156, v6.6): the 1995 vertical shoot 'em up with the engine of the original project (GPL-2.0),
  isolated in `userland/terceros/opentyrian/` and with its own platform that acts as SDL 1.2 over the
  SaguiOS windows. It is not in the no-GPL variant, neither the game nor its data.
- Since v7.0b it is in **Start > Games > OpenTyrian** (D180) and its data (Tyrian 2.1) goes in an
  ext2 partition of the image itself, `SAGUI-JOGOS`, which the system mounts at boot on `/jogos` (read and write
  since v7.4, D228), with the data in `/jogos/opentyrian` (D181): it does not use RAM
  (the ramdisk does not grow).
- The game **finds its data by itself**: in `/jogos/opentyrian` and in `jogos/opentyrian` of every
  mounted disk (`/usb`, `/datos`…). On the CD, or with an image without the partition, the data is put on a USB stick
  (`tools/imagem_tyrian.sh` builds it from the host). If it does not find it, a window explains how to
  get it and offers «Buscar otra vez» («Search again»). Tests: `tests/h136_opentyrian.py`, `tests/h150_opentyrian_menu.py`.

## Extras: OpenJazz (third-party code, C++)

- **OpenJazz** (D228, v7.4): the free engine of *Jazz Jackrabbit* (1994), version 20260301 of the
  original project (GPL-2.0), in `userland/terceros/openjazz/` without changing a single line. It is **the first C++ program**
  of the system: it carries its own C++ runtime (`userland/libcxx/`: `new`/`delete`, exceptions with our own DWARF unwinder, some minimal standard library headers) and the common
  SDL 1.2 (`userland/libsdl12/`). It goes in `/apps/bin/openjazz` and in **Start > Games > OpenJazz** (also full screen);
  it is not in the no-GPL variant.
- **The game data is not free** and does not come with SaguiOS. OpenJazz looks for it in any folder of
  `/jogos` (or in `jogos/` of a USB stick) and prefers the full version over the shareware. To get it:
  `tools/openjazz_datos.sh` downloads the shareware (episode 1) before building, or the full game is placed in
  `input_share/games/` ([docs/input_share.md](docs/input_share.md)); or the files are copied to `/jogos` from SaguiOS.
  Without the data, a window explains how to get it. The configuration and saved games go to `/home/openjazz`.
  Test: `tests/h228_openjazz.py`.

## Extras: Quake (third-party code)

- **Quake** (D229, v7.4.1): the engine of *Quake* (1996) that id Software published under the GPL-2.0 license, the
  WinQuake with its **software renderer**, in `userland/terceros/quake/` without changing a single line (pure C,
  without the assembly). On top of the common SDL 1.2: 320x200 in a window at double size (or 640x480), full screen in
  4:3, sound at 22050 Hz. It goes in `/apps/bin/quake` and in **Start > Games > Quake** (also full screen);
  it is not in the no-GPL variant. No network and no CD music.
- **The game data is not free** and does not come with SaguiOS. Quake looks for it in any folder of
  `/jogos` (or in `jogos/` of a USB stick): `id1/pak0.pak`, or a loose `pak0.pak`; it prefers the
  registered version. To get it: `tools/quake_datos.sh` downloads the 1.06 shareware (episode 1) before building, or
  the game is placed in `input_share/games/Quake/` ([docs/input_share.md](docs/input_share.md)). Without the data,
  a window explains how to get it. The configuration and saved games go to `/home/quake/id1`.
  Test: `tests/h229_quake.py`.

## Extras: Game Boy (GNUBoy, third-party code, C)

- **SDL2-GNUBoy** (D235, v7.4.2): the emulator of the **Game Boy and Game Boy Color** (GPL-2.0, commit `c8642d1`),
  in `userland/terceros/gnuboy/` without changing a single line, on top of the own SDL 2 (`userland/libsdl2/`). It goes in
  `/apps/bin/gnuboy` and in **Start > Games > Game Boy (GNUBoy)**; it is not in the no-GPL variant. Window
  of 480x432 (160x144 times 3), full screen with aspect ratio and bands (F11), sound, arrow keys, Z/X
  (A/B), Intro (Start), Shift (Select), F5/F7 to save and load the state; the cartridge battery
  (`.sav`) and the states go to `/home/gnuboy`.
- **The ROMs do not come with SaguiOS.** They are searched in `/jogos/gb`, `/home/gnuboy/roms` and the mounted disks, and
  a window lets you choose one (name from the cartridge header); without a ROM, another window explains where to put them.
  `input_share/games/gb/` puts them in the image ([docs/input_share.md](docs/input_share.md));
  `tools/gnuboy_roms_libres.sh` downloads a free ROM (Libbet, zlib) for testing. Test:
  `tests/h235_gnuboy.py` (the "unit" part runs on the PC; our own test ROM is in
  `tools/gnuboy_rom_teste.py`).

## Extras: programming inside SaguiOS

- **`cc`, a C compiler written in this project** (B5): it compiles inside
  SaguiOS a broad subset of C (int/char/short/unsigned, pointers, arrays, struct,
  union, enum, typedef, function pointers, `switch`, macros with parameters,
  `#include "..."`, several source files (D133), `long long` (D128), `float`/`double` with SSE2 or, on a CPU without SSE2, with the x87) directly to an ELF executable,
  without assembler or linker, with `printf`, `malloc`, `<math.h>` and friends included.
  `cc /ejemplos/hola.c -o /tmp/hola` and then `/tmp/hola`; also try
  `/ejemplos/mandelbrot.c`. How it was designed: D033 and D039 in [`docs/decisoes.md`](docs/decisoes.md).
- **`pascal`, a Pascal compiler also of our own** (improvement 040): Pascal in the style of Turbo Pascal (integers, `real` with SSE2 or x87, string, sets, records with variants,
  pointers, files, nested procedures, `{$R+}`) straight to an ELF executable, with the
  same code generator as `cc`. `crt` units (`clrscr`, `gotoxy`, `readkey`...) and
  `sagui` (graphics in a window). `pascal /ejemplos/pascal/hola.pas -o /tmp/hola`; more
  examples in `/ejemplos/pascal` and the manual in `/ayuda/pascal.md`. Details: D085-D087.
- **libsagui** (v6.1, D126): new components, canvas with images, progress bar and checkbox;
  and the icon library (`icones`, D135), with 112 icons of 16x16.
- **`qjs`, JavaScript in the terminal** (D188, v7.2): the browser's QuickJS (D149) as an interpreter:
  `qjs file.js [args]`, `qjs -e 'code'` and a REPL with history. `console`, `print`, timers,
  local ES modules and the `std`/`os` modules (files, environment, `sleep`). Errors with a trace and exit code 1.
- **`python`, MicroPython 1.24** (B6): REPL in the terminal, `python file.py` and
  `python -c`. **It is third-party code** (MIT license), the only exception to the authorship rule,
  isolated in `userland/micropython/` and optional: `make MICROPYTHON=0` builds the
  image without it (D034). It has `float` as double through SSE2 or x87 (D038, D082) and a standard library
  (improvement 032, D067-D069):
  - files with `open()` (text in Latin-1 like the Editor, or binary), `io.StringIO`/`BytesIO`,
    `sys.stdin`/`stdout`/`stderr`; unicode `str` (`len("ñ") == 1`) with accents in the terminal;
  - `import` of `.py` files and packages: the script's folder and `/lib/python` are in `sys.path`;
  - modules `os` and `os.path`, `sys`, `io`, `json`, `re`, `random`, `time` (with `localtime`,
    `strftime`), `struct`, `collections` (`deque`, `namedtuple`, `OrderedDict`,
    `defaultdict`), `heapq`, `binascii`, `hashlib` (md5, sha1, sha256, sha384, sha512),
    `errno`, `array`, `select`, `math`, `cmath`, `gc`, `micropython`, and `sqlite3` (SQLite 3.54.1, a subset of
    DB-API 2.0; improvement 064, D116, [`docs/sqlite.md`](docs/sqlite.md));
  - network: `socket` (TCP client and server, UDP, DNS), `ssl` (the libtls, with the certificates from
    `/etc/ssl/certs`) and, in `/lib/python`, `requests`, `urllib.parse` and `litemonkey`;
  - the heap starts with 4 MB and grows by itself (`-X heap=16M` fixes it). Examples:
    `/ejemplos/archivos.py`, `/ejemplos/red.py` and `/ejemplos/json_http.py`;
  - **windows** (improvement 033, D083-D084): the `sagui` module (in C: `sagui.Ventana` with
    rectangles, ellipses, polygons, text, BMP/PNG images, mouse and keyboard events,
    the context menu and a loop that sleeps until the next event) and, on top, a
    **`tkinter`** written in Python with the look of SaguiOS: `Tk`, `Button`, `Entry`, `Text`,
    `Listbox`, `Canvas`, `Scale`, `Menu`, `pack`/`grid`/`place`, `StringVar`, `bind`, `after`,
    `messagebox`, `simpledialog`, `filedialog` and a minimal `ttk`. Try
    `python /ejemplos/tk/calculadora.py` or `/ejemplos/tk/juego.py`; what is available, in
    `/ayuda/python-tk.md`;
  - without SSE2 (Pentium III, Athlon XP), `python` uses only `/bin/python-x87`, with the same
    results (D082).
- **`simd`**: programs can use the x87 FPU, MMX and SSE; the kernel saves their state
  per process (D032).
- **`pantalla`** (display): lists the video modes and changes the resolution; in QEMU, Bochs and
  VirtualBox without rebooting (`pantalla 1280x1024`), on a real PC at the next boot
  (`--guardar` keeps it on the boot disk). The framebuffer uses *write-combining* when
  the CPU allows it (D057-D059).

## Extras: old PCs (improvement 036, phase A)

- **Without SSE2 automatically** (Pentium III, Athlon XP): the kernel and libsagui copy
  with `rep movsd`, `cc` generates x87 code (`-sse2`/`-x87` force it) and `simd` only tests
  what is there (D076). Python with real numbers still needs SSE2.
- **Bootable CD:** `make iso` → `build/saguios.iso` (El Torito, made by
  `tools/mkiso.py`); `./run_in_qemu.sh --cd`. Nothing is saved to the boot disk from the CD (D077).
- **16- and 15-bit video** (RGB565/RGB555) for 2-8 MB cards: the desktop stays at 32
  bits and is converted when copied to the card (in C or with SSE2); `mkimagem.py --bpp 16` prefers it (D078).
- **`infopc --informe`**: a `.txt` with the CPU, the memory, each PCI device with its driver
  (or the improvement that foresees it), the VESA modes, the disks, the ACPI tables and the kernel log,
  to send. Tested machines: [`docs/compatibilidad.md`](docs/compatibilidad.md).
- **Hardware monitor** (D092, improvement 050): `sensores` and `temp` show the CPU temperature
  (Intel through MSR; AMD K8, 10h-16h and Ryzen through PCI/SMN), the real frequency (APERF/MPERF),
  the multiplier, the P-states, the requested voltage, turbo and microcode; also in the
  CPU tab of `infopc`, in the report and in the Performance tab of the Task Manager (60 s graphs, in yellow near the maximum and in red if the CPU is throttling). It is
  **read-only**: an MSR that does not exist does not bring the system down (the #GP is absorbed), and the board
  (fans, SPD) is not touched. In a virtual machine, «no disponible» («not available»).
- **SaguiMark** (D093, improvement 051): `saguimark [--rapido]` («fast») or the Benchmark tab of `infopc`
  measure integers, x87 FPU, SIMD (SSE2) and memory with verified tests (a CPU that computes
  wrong gives «ERROR») and give a score; 1000 = reference, provisional for now (QEMU/KVM; the
  definitive one will be the HP dv7). The result is saved in `/home/saguimark-AAAAMMDD-HHMM.txt` and
  compared with `/lib/saguimark/referencias.txt`.
- **`benchmark`** (D176): `benchmark [--mono | --multi [N]] [--rapido]` measures eight workloads with one core and with all of them at once (processes and threads), with a score (1000 = QEMU with KVM and one CPU), a scale factor and efficiency; also in the Performance tab of `infopc`. The history is kept in `/home/.benchmark`.
- **SaguiGL** (D104, improvement 054 v5): `libsgl`, a software OpenGL ES 1.1 API (matrices with
  stack, `glFrustumf`/`glOrthof`, `glDrawArrays`/`glDrawElements`/`glBegin`, 16- or
  32-bit Z buffer, Gouraud and flat shading, clipping against the frustum, back-face culling; since v6, also textures,
  blending, fog and lights, with a SIMD path (D130)). `sgldemo` shows a spinning cube with the
  fps (`-t N` measures without a window). The test `tests/h77_sgl.py` compares the result, pixel
  by pixel (sum), with the same engine compiled on the host. `infopc` (Video tab)
  identifies the GPU family from its PCI ID (AMD R300-Evergreen, Intel Gen2-Gen6) and says whether
  SaguiGL would have a hardware engine.
- **Skeleton of the AMD 3D drivers** (D158, improvement 054 B2, `/bin/amd`): the family table
  (Evergreen, R5xx and R300/R420, and R6xx/R7xx), the identification registers and the reading of the
  MMIO registers of the BAR. It is the skeleton and the kernel minimum to read them, not the GPU 3D engine.
- **SATA disks in AHCI mode** (D155, improvement 087): they join the same disk layer as the IDE ones,
  with the same read-only rules. USB 3.0 (xHCI) remains a skeleton, as described in
  [`docs/usb3.md`](docs/usb3.md).

## Extras: lite-monkey, the document database

- **`litemonkey`** (improvement 067, D117): our own **document** database, in C, inspired by
  MongoDB but more basic, on **port 7979**. The same code works on the PC
  (`make -C litemonkey`) and in SaguiOS (`litemonkey &`; data in `/home/litemonkey`). BSON documents,
  CRUD with query and update operators, indexes (with `unique` and multikey), basic
  aggregation (`$match`, `$group`, `$sort`...), cursors, an append-only file with a CRC
  that recovers after a power cut and is compacted by renaming, and optional HMAC-SHA256
  authentication.
- **Python client** `litemonkey` with an API similar to pymongo, the same in CPython and in the
  SaguiOS MicroPython: `c = litemonkey.Cliente("127.0.0.1"); c.tienda.productos.find({...})`.
  Console **`lmk`** (`show dbs`, `use`, `db.col.find(...)`, `it`). From the PC to SaguiOS with the QEMU port
  forwarding; inside SaguiOS through 127.0.0.1. Manual:
  [`docs/litemonkey.md`](docs/litemonkey.md).

## Extras: Mico, the code editor

- **Mico** (`mico folder`, or Start > Accessories): folder explorer, tabs, colors for C, Python, sh, Markdown and CHIP-8 assembly, unlimited undo,
  automatic indentation, comment with Ctrl+/, move lines with Alt+↑/↓, find and replace,
  search across the whole folder, go to line, command palette (Ctrl+Shift+P) and light and
  dark themes. **F5** compiles and runs in the terminal below (C, Pascal, Python, sh); the errors of `cc` and `pascal` and Python tracebacks
  appear in **Problems** and a click takes you to the line. It saves atomically and makes
  recovery copies every 5 s. Try it with `mico /ejemplos/mico`; manual in
  `/ayuda/mico.md`. Details: D075.
- **System clipboard:** copy and paste text between Mico, the Editor, Markdown and
  the Terminal (in the Terminal, Ctrl+Shift+C/V or the right button). Details: D074.

## Extras: spreadsheet and mail

- **Macaco** (D142, v6.3): spreadsheet with a grid, typical formulas (sum, average, minimum,
  maximum, count, if, round…), bar and pie charts, and its own `.mxs` files
  (Macaco eXcel Sheet).
- **Pluma** (D291, v8.3): word processor with the look of a simplified Word 97 (menus, toolbars with icons, ruler, page view, fonts, paragraphs, find and replace). Reads and writes **RTF**
  and plain text, and **exports to PDF**. No macros.
- **Mail client** (D146, v6.3 and v6.6): receives through POP3 or IMAP and sends through SMTP, with a window,
  message list, attachments and body in plain text or HTML (converted to text).

## Extras: taskbar and Start menu

- **Start button** (or the Windows key, or Ctrl+Esc): the most used programs at the top, the
  others grouped in submenus (Accessories, Graphics, Internet, Games, System), and at the bottom
  «Ejecutar...» («Run...»), «Configuración de pantalla» («Display settings»), «Acerca de SaguiOS» («About SaguiOS») and «Reiniciar...» («Restart...»)
  («Apagar» («Shut down») is greyed out: there is no ACPI yet).
- **One button per window** in the top bar: click to bring it to the front or minimize it,
  right button for the window menu; if they do not fit, they shrink and the extra ones go to
  «»». Click on the clock: the full date; right button: «Ajustar fecha y hora...» («Set date and time...»).
- **Right button on the desktop:** «Configuración de pantalla...» («Display settings...») and the submenu
  «Resolución» («Resolution»), which changes the mode immediately. Details: D064-D066.
- **Tray** to the left of the clock: volume (click: slider and «Silenciar» («Mute»); the wheel over the icon raises and lowers it),
  network (the IP when hovering the mouse; click: IP, gateway,
  DNS and «Renovar DHCP» («Renew DHCP»)) and CPU temperature (only if it can be read: not in QEMU; orange
  from 70 °C and red from 79 °C). Details: D114, test `tests/h85_bandeja.py`.
- **Desktop shortcuts** (v6.0 beta): Alt+F4 closes the active window (the program receives the notice,
  as with the X), Alt+Tab switches window (Alt+Shift+Tab, to the previous one), the Windows key opens the
  menu, Windows+D shows the desktop and Ctrl+Alt+Del opens the Task Manager.
- **Screenshots** (beta 3, D125): Print Screen, Alt+Print Screen, Windows+Shift+S, the Snipping program
  and the `captura` command.
- **«Discos» utility** (v6.0 beta): each disk and partition with a bar of used and free space,
  file system, label and mount point; mount, unmount and open in Archivos. The `discos` command
  in the terminal also shows usage.
- **File explorer** (v6.3, D136): show hidden files (Ctrl+H), details and thumbnail views,
  path bar and side panel, multiple selection, copy, cut, paste and drag, rename,
  search, «Abrir con» («Open with»), properties and trash. Alt+Tab shows thumbnails.

## Extras: images and drawing

- **Image viewer** (`visor`, «Visor» icon): opens BMP, PPM/PGM/PBM, **PNG** (with a *deflate* decompressor
  written here) and baseline **JPEG** (photos from cameras and phones; progressive and 12-bit JPEG
  are not read; improvement 047), fitted to the window or at real size, with zoom
  (wheel, `+`/`-`), dragging, previous/next image in the folder (PgDn/PgUp) and
  full screen (F11). Archivos opens images with it. There are examples in `/imagenes`,
  drawn by `tools/mkimagens.py`. Details: D054 and D055.
- **Paint** (`paint`, «Paint» icon): pencil, brush, eraser, airbrush, lines, rectangles and
  ellipses (outline or filled), paint bucket, color picker and text; main color (left click)
  and secondary (right click); multi-level undo (Ctrl+Z); saves to BMP
  (Ctrl+S). Details: D056.
- **MJPEG video in AVI** («Reproductor de vídeo» («Video player») icon, v6.2, D129), with the JPEG decoder and
  `tools/convertir-video.sh` to prepare videos.
- **PDF viewer** (`visorpdf`, D138): own engine in `userland/pdf/`, with fonts, filters and
  RC4 and AES-256 encryption.

## Extras: vector fonts (v7.2, D196)

- **libfontes** (`userland/libfontes/`, `userland/inc/fontes.h`): TrueType, OpenType with CFF outlines,
  WOFF and WOFF2, with antialiasing by exact coverage. It is the engine of the Cipó browser, now shared:
  `fonte_abrir_archivo`, `fonte_medir`, `fonte_texto` (over a window or a buffer, with color and size in
  pixels), `fonte_nombre` and the catalog of `/apps/fuentes` and `/home/fuentes`.
- **Typodermic fonts** (Raymond Larabie, **CC0**): 28 in `/apps/fuentes` (partition `SAGUI-APPS`) and
  `tools/fuentes_typodermic.py` to extract them from the zip (`NEW_FONTS/`, outside git) or to package the 731 into
  a folder or a `.tar.gz` to copy to an installed disk.
- **Settings > Fonts:** list, preview and default font for documents and titles. **Paint**
  uses it in its text tool, and the **Editor** has a «Vista» («View») button that shows the document with it.
  The desktop still uses the 8x16 bitmap font (next step).

## Extras: JClic educational activities

- **Start > Educación > Actividades JClic** («Start > Education > JClic activities») (D185, v7.1) opens the projects from the XTEC Clic repository
  (https://projectes.xtec.cat/clic/es/repo/): `.jclic.zip` or `.jclic`, also by double-clicking in Archivos.
  Puzzles are played (double, swap and hole), simple and complex associations, memory, explore,
  identify, word searches and crosswords, with counters for hits, attempts and time. Text activities
  and the old Clic 3.0 (`.pac`) are not read yet: they are skipped with a warning.
- **Start > Educación > Actividades JClic (bajar)** («Start > Education > JClic activities (download)») lists the repository (about 2,600 projects, language filter and
  search), downloads the ones you want to `/home/jclic/` and opens them. Without network, the saved index is used.
  **No project from the repository ships with SaguiOS** (each one has its own license, usually Creative
  Commons); only one example of our own in `/jogos/jclic`. Test: `tests/h152_jclic.py`.

## Extras: Spider Solitaire

- **Spider Solitaire** (`spider`, D173; Start > Games): the Spider from Windows Me with 104 cards, 10 columns,
  levels of 1, 2 and 4 suits, drag and drop, click to move to the best column, hint (H), undo
  (Ctrl+Z), new game (F2) and Windows scoring; cards, suits and figures drawn in code, with
  animation when dealing, when removing a sequence and when winning. `spider --autoprueba` checks the rules without a window.

<div align="center">
<img src="docs/capturas/H143-2-tapete.png" width="32%">
<img src="docs/capturas/H143-4-arrastre.png" width="32%">
<img src="docs/capturas/H143-9b-pantalla-completa.png" width="32%">
</div>

## Extras: CHIP-8

- **`chip8` emulator** («CHIP-8» icon, D070): CHIP-8, **SUPER-CHIP 1.1** and **XO-CHIP**
  (128x64, scrolling, 16x16 sprites, 64 KB, four colors and the real sound pattern
  through the sound card; without a card, approximated through the speaker), written from the public documentation. The *quirks* of each machine are chosen by
  the extension (`.ch8`, `.sc8`, `.xo8`) or one by one (`-q`); resizable window with
  full screen, palettes (one with the SaguiOS colors), pause, step by step,
  debugger (F10) and `chip8 -t` to run without a window.
- **`chip8asm` assembler** (D071): labels, constants, `db`/`dw`, `include` and errors with
  file and line; `-d` disassembles. The same code assembles the included ROMs during the build.
- **Four games of our own** in `/jogos/chip8`, with their source in assembly (D072): RAMAS (the sagui collects bananas
  and dodges sticks), SUMAS (a 4x4 puzzle with the hexadecimal keypad), CANOA (going down the river in SUPER-CHIP) and AURORA (XO-CHIP demo with 4 colors and a melody).
  Manual: `/ayuda/chip8.md` (`markdown /ayuda/chip8.md`).

<div align="center">
<img src="docs/capturas/H63-chip8-ramas-jogo.png" width="32%">
<img src="docs/capturas/H63-chip8-depurador.png" width="32%">
<img src="docs/capturas/H63-chip8-aurora.png" width="32%">
</div>

## Extras: AI assistant

- **`ia` client** (D134, v6.2.1): terminal command with an API compatible with OpenAI's and
  a *streaming* response. Providers: **OpenRouter** or any OpenAI-compatible endpoint
  (llama.cpp, Ollama, LM Studio…).
- **Shared configuration** (D145, v6.6): in Settings, the «IA» tab, you choose the provider, the address, the key and the models (general, terminal, chat and Mico). Each program uses its own model; the
  key is stored separately and is not shown.
- **Chat application** with a window, context and in Spanish (D143), and the **Asistente panel** («Assistant») of Mico
  (D140). According to the ROADMAP, the side list of conversations, Markdown in
  responses and the provider and model selector inside the chat window are still pending.

## Extras: sound

- **AC'97 and Intel HDA sound cards** (D088-D090, improvement 041): Intel AC'97 (ICH),
  SiS, NVIDIA and AMD, and any HDA controller, with the path through the codec's *widgets*
  to the speaker, the headphones and the line output. 16-bit stereo PCM at 48 kHz, with
  interrupts and, if they do not arrive, by polling. The PC speaker remains as a fallback.
- **Mixer in the kernel**: several programs play at the same time, each at its own frequency
  (4-96 kHz, 8 or 16 bits, mono or stereo), with per-stream and master volume.
  - `reproducir archivo.wav|mp3|flac` («play») and `volumen [0-100|silencio|activar]` («volume»); `infopc` shows the
    card and the codec.
  - **Audio formats with own decoders** (D098-D100, improvement 053): MP3 (MPEG-1 and 2
    Layer III, VBR, ID3 tags), lossless FLAC and WAV IMA ADPCM, recognized by their content and played in *streaming*; `reproducir` shows format, frequency, channels,
    duration and tags; double-clicking in Archivos opens `.wav`, `.mp3` and `.flac`. A stereo MP3
    costs ~18 Mcycles per second of audio (real time with room to spare on a Pentium III). Ogg
    Vorbis, pending. To prepare audio: `tools/convertir-audio.sh`.
  - **Graphical music player** (`musica`, Start > Accessories > «Música» («Music»), D115): playlist
    (files or a whole folder), previous/next, play/pause/stop,
    progress bar with time and jump with a click, volume, format and tags
    (ID3, FLAC comments); space, arrows ±5 s, N, P. It does not block the interface. Double-clicking
    `.wav`, `.mp3`, `.flac` or `.ogg` in Archivos opens it. Test `h86_musica.py`.
  - **MIDI player** (`midi`, Start > Accessories > «MIDI», D190): plays `.mid`, `.midi` and `.kar` with an own
    synthesizer of libsagui (`audio_midi.c`, without data files); view of the 16 channels with the GM instrument, 88-key keyboard and meter, karaoke lyrics and folder list. `musica` and
    `reproducir` also play MIDI. Examples in `/ejemplos/musica`. Test `h157_midi.py`.
  - CHIP-8 plays the real XO-CHIP audio pattern; Banana and Sagui 3D, their effects; in
    Python, `sagui.Sonido(22050).escribir(datos)`.
  - In QEMU: `./run_in_qemu.sh --sonido` («--sonido») (HDA through PulseAudio; `--ac97` for the other one).

## Extras: date, time and time zone

- **The kernel stores the time in UTC** and the board clock can be in UTC or in local time,
  as on PCs with Windows (by default, local on a PC and UTC on a virtual machine). Time zones use
  **POSIX TZ** rules with an interpreter written here and a table of
  84 zones («España (Madrid, península)» («Spain (Madrid, mainland)»), «Brasil (São Paulo, Brasilia, Río)»...): the bar
  changes by itself to daylight saving time. Details: D080 and D081, improvement 039.
  - `zona` (the current one, `zona lista`, `zona Europe/Madrid`, `zona --rtc local`), the
    **«Fecha y hora»** («Date and time») window (Start → System, or right button on the clock), `date` with the
    abbreviation (CET/CEST) and `-u`, the `TZ` variable per program, `ls -l` with dates and
    `ntp` (SNTP) to set the time. With a persistent `/home` the zone is saved in
    `/home/.zona`.

## Network and HTTPS

- **Wired network** (D046, D047): Intel e1000 and Realtek RTL8139 drivers, own IPv4 stack
  (ARP, ICMP, UDP, DHCP, DNS, TCP), loopback `127.0.0.1` and sockets as descriptors.
  Commands `ifconfig`, `dhcp`, `ping`, `nslookup`, `nc` and the text browser **Cipó**
  (`cipo`).
- **«Red» («Network») tab of the Task Manager** (D079, improvement 038): interfaces with status,
  addresses, counters and speed, last-minute graph, connections with their process,
  and «Renovar DHCP» («Renew DHCP»), «Desactivar interfaz» («Disable interface») and «Cerrar conexión» («Close connection»).
- **HTTPS with cryptography written here** (D060-D063, improvement 027): TLS 1.3 and TLS 1.2 (ECDHE with
  AES-GCM or ChaCha20-Poly1305), X25519, P-256/P-384, RSA and ECDSA certificates, full X.509
  validation (chain, dates, name, usages) with clear messages («certificado caducado» («certificate expired»), «nombre no coincide» («name does not match»), «emisor desconocido» («unknown issuer»)). Random numbers come from a kernel generator
  (RDRAND if available; otherwise, the interrupt clock), also as `/dev/urandom`.
  - `curl https://...` (`-v` shows the connection and the certificate, `-k` does not check it,
    `--cacert ca.pem`), `wget` with a progress bar, `sha256sum`, and Cipó opens HTTPS pages.
  - `certificados` manages the trusted roots of `/etc/ssl/certs` (32 factory ones:
    Let's Encrypt, DigiCert, GlobalSign, Google, Amazon...): `lista`, `ver`, `añadir` («add»),
    `quitar` («remove») and `probar host` (connects and explains the validation). With a persistent `/home`
    the changes survive a reboot.
  - No auditing and no revocation (OCSP/CRL): it is an educational system; the limits are in
    D063.

## Extras: the Cipó browser

- **Own web engine** (D141, v6.3): CSS, JavaScript, Canvas, SVG and WebGL in software, with band-by-band
  drawing; downloads one at a time and right button to save.
- **Tabs, bookmarks and search** (D148, D152): each tab with its own cache and reading position,
  bookmarks, copy, accent-insensitive search and reduced JPEG images. Ctrl+click opens the link in another
  tab, and the reading position of each site is remembered.
- **JavaScript** (D149, D160): the QuickJS engine, ported to the OS. Each `<script>` sees a DOM that it can
  query (`getElementById`, `getElementsByTagName`, `querySelector`), change attributes and
  `textContent`, and set click handlers.
- **Redirects** (D159): `window`, `location` and `meta refresh`, with the navigation deferred until the
  script finishes.
- **CSS transitions** (D153) and **pseudo-elements** without breaking the DOM (D161): `https://www.amd.com/`
  loads in full.

## Extras: compression (improvement 056)

- **`tar`, `gzip`/`gunzip`/`zcat`, `zip` and `unzip`** with our own DEFLATE (LZ77 + dynamic/fixed
  Huffman, levels 0-9), CRC-32, multi-member gzip, tar ustar/GNU/pax and zip stored and
  deflate (no encryption and no ZIP64). Extraction never leaves the destination folder. Tested in both directions with the host's `tar`, `gzip`, `zip`, `unzip`, `tarfile` and `zipfile`
  (`tests/h76_compresor.py`). Details and limits: D103.

## Disks: partitions and installer (v7.0b, D179)

- **Partitions:** `particiones` in the terminal and the **Particiones** button of the Disks window
  (`discos --particiones [disco]`): MBR table with primaries, **extended and logical**, aligned to
  1 MB. Create, delete, resize (a FAT32 grows with its partition if its FAT allows it;
  shrinking a file system is not done), format as **FAT32 or ext2**, mark as active,
  **TRIM** (on SATA/AHCI disks that support it) and **undo** the last change. The GPT is shown
  but not touched.
- **Installer:** Start > System > **Instalar SaguiOS** («Install SaguiOS») (or `instalar <disco>`). It copies the running system
  to a hard disk or to another USB stick (the whole disk, a free gap or in place of
  a partition) and leaves it bootable by BIOS, with `/home`, the games (in `/jogos/opentyrian`) and the
  applications (in `/apps`, with whatever is in the system's `/apps`) in their own ext2 partitions that are
  mounted by label, as in the image. Optionally copies the game data from `/jogos`.
- **Security:** the kernel only allows writing to a **reserved** disk, and only reserves it if nothing is mounted from it; the boot disk asks for **two confirmations**; each dialog says what is erased; the table is validated before being written (EBR first, MBR last) and re-read; the installer
  leaves the disk **non-bootable** until everything has been verified. Details in
  [`docs/decisoes.md`](docs/decisoes.md#d179-gestión-de-particiones-trim-y-un-instalador-de-saguios-a-disco-duro-o-pendrive-v70b).

## System partitions: games and applications (v7.0b and v7.1, D181 and D194)

The ramdisk, which is copied entirely into RAM at boot, carries only **the base** of the system (the shell, the desktop, Archivos, Disks, the installer, Settings and the libraries). What is not essential goes in
partitions of the image itself, mounted by their label, which do not use RAM:

| Partition | Format | Mounted on | What it carries | Variables |
| --- | --- | --- | --- | --- |
| `SAGUI-JOGOS` | ext2, read and write | `/jogos` | one game per folder: `/jogos/opentyrian` (D181), `/jogos/openjazz`, `/jogos/quake` (D229) and whatever is added, such as the full version of a game (D228); also the games from the ramdisk base (chip8, jclic, sagui3d), which the mount covers | `JOGOS_MB` (800 in the persistent ones, the partitions one and the installer), `JOGOS_IMG_MB` (0 = just enough in `saguios.img`), `JOGOS=0` |
| `SAGUI-APPS` | ext2, read and write | `/apps` | the non-essential applications: `/apps/bin` (on the `PATH`: NetSurf, `sqlite3`) and each one's data in `/apps/<app>` (D194) | `APPS_MB` (0 = just enough plus margin), `APPS=0` |
| `SAGUI-CONF` | ext2, 16 MB, root only | mounted by the kernel (improvement 098) | the system configuration (improvement 098, D256): users, passwords, network; for now just a `LEEME`. It goes last, in every disk or USB-stick image and in installations; **the ISO does not carry it** (live mode). The installer keeps the one already there (`--formatear-conf` erases it); `mkimagem.py --sin-conf` removes it | (fixed) |

- If a partition is missing, the system **boots anyway**: without `SAGUI-APPS`, NetSurf does not appear in the Start
  menu and `netsurf` is not found.
- `make APPS=0` leaves the applications **inside the ramdisk** (in `/apps`, with the same paths): a
  single-block image. The **CD** does not read partitions, so the ISO always carries them in its ramdisk.
- To install another application, just copy its executable to `/apps/bin` and its data to `/apps/<app>`;
  the installer copies all of `/apps` to the installed disk. Test: `tests/h159_sagui_apps.py`.

## Multicore and threads

SaguiOS boots with one processor, but it can already use several. Each piece has its option when the image is built.

- **Optional SMP** (D164, v6.7; general kernel lock in D165, v6.8): with `--smp` the image turns on the
  secondary processors. A general kernel lock (`kernel/nucleo/trava_geral.c`) lets each CPU
  handle its system calls, its faults and its exceptions, so several programs run at the same time.
  Kernel tasks stay on CPU 0.
- **Optional IOAPIC** (D166, v6.8): with `--ioapic` the device IRQs arrive through the IOAPIC and not through
  the 8259, with a fallback to the PIC if the tick does not arrive.
- **Distributed interrupts** (D168, v6.8): with `--ioapic --smp` IRQs 1-15 are shared in turns among
  the secondary CPUs; the PIT (IRQ 0) stays on CPU 0. In the tests, the keyboard goes to CPU 1 and the network
  or the sound (IRQ 11), to another CPU.
- **User threads** (D167, v6.8): a thread shares the address space, files, folder and windows of its
  process. libsagui provides `thread_create`, `thread_join`, `thread_yield`, `thread_sair`, `mutex_t` and
  `semaforo_t` (which sleep in the kernel, through futex), and `malloc` is thread-safe. The same API is in the
  `cc` library; example: `/ejemplos/hilos.c`. Python and Pascal do not have threads yet
  ([`docs/multihilo.md`](docs/multihilo.md)).

**By default the image boots with a single CPU.** To build the image with several processors, after `make`:

```bash
python3 tools/mkimagem.py --mbr build/boot/mbr.bin --estagio2 build/boot/estagio2.bin \
    --kernel build/kernel.bin --kernel-elf build/kernel.elf --ramdisk build/ramdisk.sgfs \
    --saida build/saguios.img --vesa --smp --ioapic
```

The two options are independent: `--smp` turns on the secondaries, `--ioapic` changes the interrupt controller, and with both the IRQs are shared. The command replaces `build/saguios.img`, which is the one
that `./run_in_qemu.sh --nucleos 4` boots (4 CPUs in the machine); the next `make` that changes something rebuilds it
without these options. Without `--smp` in the image, the extra CPUs are not used.

**Measurements** (KVM, 4 CPUs, [`docs/multihilo.md`](docs/multihilo.md)): the computation with `bench 4 c` gains between 2.7x
and 3.2x, and four threads (`bench 4 h`) between 3.8x and 4.1x.

**Limits:** there is only one general lock, so an IRQ handled on a secondary CPU waits for the CPU
that is in the kernel to release it; the IRQ sharing is fixed in turns; and the IOAPIC and PCI lines without `_PRT`
have not been tested on real hardware (D166, D168).

## Figures

| | |
| --- | --- |
| Boot image | **28,959,232 bytes** used: 12,182,016 of boot (11,740,144 of ramdisk) and 16 MB of the SAGUI-JOGOS and SAGUI-APPS partitions (target of 38 MB since v7.0b, D181 and D194; the original, 575,488) |
| Lines of code | boot 903, kernel 9,614, programs 6,317 (plus 3,591 of tools and tests) |
| Programs | dozens in `/bin`: shell, Unix-like commands (`ls`, `grep`, `find`, `sort`, `head`, `tail`... see `docs/comandos.md`, D097) and graphical programs |
| Automated tests | more than 130 (`tests/h*.py`, one per story), in QEMU |
| Recorded decisions | 163 entries (D001 to D168), with the reasoning for each one |
| Recorded bugs | 25, with symptom, cause and fix |
| Commits | 711 on `main` (`git log`) |

## Testing it

### Which images are in the repository and which are generated

Since v7.4 (decision of the maintainer, 2026-11-13), **only the two basic images** of each version are uploaded to GitHub,
in `entrega/vX.Y[.Z]/`: `saguios.img` (one core) and `saguios-smp.img`
(multicore), with `SHA256SUMS.txt` and `VERSION.txt`. This way the repository does not grow by hundreds of MB
per version (GitHub rejects files larger than 100 MB). In these two the games partition has
exactly the size of what it carries.

**All the others are generated on your machine** once the repository is downloaded:

```
make variantes DESTINO=entrega/mia      # all of them: persistent ext2 and FAT32, with and without multicore, with uACPI,
                                        # the partitions one, the no-GPL one, the ISO and the .vdi
make imagem-persistente-smp             # or just one (see «make help»)
make imagem-persistente-smp-uacpi       # the laptop one: persistent, multicore and with
                                        # the battery indicator (uACPI, D192)
```

The persistent ones, the partitions one and the installer create the **800 MB games partition, read and write**
(`/jogos`, one folder per game), where you can later copy the full version of a game from SaguiOS itself. Sizes are changed with variables
(`JOGOS_MB`, `APPS_MB`, `DATOS_MB`…).

**Your own content:** whatever cannot go in the repository nor be downloaded by a script at build time
(for example, the full version of a game you own) is placed in the `input_share/` folder
(ignored by git: never uploaded) and goes into the images you generate. For example,
`input_share/games/Jazz/` ends up in `/jogos/Jazz` (with a normal `make`; with `make variantes` you have to
ask for it with `make variantes GERAR_OPC=--con-input-share`, so that a published release never carries it by mistake). Details in
[`docs/input_share.md`](docs/input_share.md).

### How to use them

The ready images are in [`entrega/final/`](entrega/final/) and in `entrega/vX.Y[.Z]/` (the basic ones):

- **VirtualBox:** `saguios.vdi` (machine "Other/Unknown", 32 bits, EFI disabled).
- **Real PC:** write `saguios.img` to a USB stick in DD mode (Rufus, balenaEtcher or `dd`)
  and boot with the traditional BIOS; on PCs that do not boot from USB, write `saguios.iso`
  (`make iso`) to a CD.
- **QEMU with a window:** `./run_in_qemu.sh` (builds if needed and boots `build/saguios.img`;
  `--usb` boots it as a USB stick, `--cd` from the ISO, `--nucleos N` with N processors (see «Multicore and threads»), `--mem MB`
  (2048 by default), `--cpu pentium3` without SSE2, `--sonido` («--sonido») turns on the speaker and an Intel HDA card and `--help` shows the rest).
- **Generating a release:** `./gerar_versao.sh` (the sibling of `run_in_qemu.sh`): builds, assembles the boot images and leaves the release in `entrega/latest/` with its SHA-256 sums and a `VERSION.txt` sheet; `--help` shows the options, which include the license variant, whether it includes MicroPython, the optimization level and `--probar` (boot the image before publishing it).

The step-by-step guide is in [`docs/como-gravar.md`](docs/como-gravar.md).

## Building and testing

Requirements (Ubuntu): `nasm gcc gcc-multilib binutils make python3 qemu-system-x86 qemu-utils imagemagick mtools dosfstools`.

**Complete guide to building your own images** (requirements, all options, downloads, your own games and common problems), in Spanish, English and Portuguese: [`compilar_versiones.md`](compilar_versiones.md).

```bash
make            # build/saguios.img
make vdi        # build/saguios.vdi (VirtualBox)
make iso        # build/saguios.iso (bootable CD, D077)
make test       # all tests in QEMU (no window)
make rodar      # opens in QEMU with a window
make versao     # generates the release and leaves the delivery (./gerar_versao.sh)
```

Whatever the person building wants to put in their image and cannot go in the repository (for example the full version
of a game) goes in `input_share/` (ignored by git): `input_share/games/<folder>` ends up in
`/jogos/<folder>` and `input_share/apps/` in `/apps`. See [docs/input_share.md](docs/input_share.md).
The C++ programs (OpenJazz) require the `cc1plus` of g++-13, which `tools/entorno-local.sh` installs.

The automated tests are in `tests/`, one per story. A single one: `python3 tests/h101_sagui_gl_v6.py`.
The full battery: `python3 tests/rodar_todos.py` (in series) or `python3 tests/rodar_paralelo.py --help`
(several at once, with `-j N` and `--kvm` if KVM is available). In parallel, **`-j 2` or `-j 3` at most**: each QEMU
uses CPU and memory, and with more the computer may freeze or reboot.

## Structure

```
boot/        stage 1 (MBR) and stage 2 of the loader, in NASM
kernel/      freestanding C kernel + NASM: core, memory, drivers, USB, files, windows
userland/    libsagui and the programs; userland/sagui3d/ is the game of bonus B4;
             userland/micropython/ is MicroPython (third-party, MIT, optional)
             userland/web/ (Cipó engine and QuickJS), userland/pdf/ (PDF viewer), userland/ia/,
             userland/mail/ (mail client) and userland/terceros/ (OpenTyrian, D156)
rootfs/      files that go to the disk in RAM (texts, Sagui 3D phases)
tools/       build (mksgfs.py, mkimagem.py, mkiso.py), panel (status.py), phase checker
tests/       automated tests in QEMU, one per story
docs/        stories, decisions, diary, bugs, test scripts and screenshots
dashboard/   project panel (open dashboard/index.html in the browser)
entrega/     .img and .vdi images of each phase, with SHA-256
testes_eli/  Eli's test photos on the G470
```

## Documentation

> **Version roadmap**: [`docs/ROADMAP.md`](docs/ROADMAP.md) · visual version: [`docs/roadmap.html`](docs/roadmap.html).
>
> **To continue the work on another PC**: [`docs/CONTINUAR.md`](docs/CONTINUAR.md) (status, half-finished branches, pending tests and the improvement queue).

| Document | What it contains |
| --- | --- |
| [`docs/resumo-final.md`](docs/resumo-final.md) | Project summary, time of each phase and the table of concepts of Tanenbaum's book completed |
| [`docs/decisoes.md`](docs/decisoes.md) | The 163 design decision entries (D001 to D168) and the Sagui 3D section |
| [`docs/erros.md`](docs/erros.md) | The 25 bugs in the table, including those found on the G470 |
| [`docs/compatibilidad.md`](docs/compatibilidad.md) | Tested machines (✓, ⚠, ✗) and how to send a report (`infopc --informe`) |
| [`docs/diario.md`](docs/diario.md) | The project diary, phase by phase |
| [`docs/multihilo.md`](docs/multihilo.md) | Multithreading and multicore: what exists, what is missing and how the gain is measured |
| [`docs/licencia.md`](docs/licencia.md) | The GPL v3 license explained: layers, SPDX, compatibilities, what may be included and what may not |
| [`docs/terceros.md`](docs/terceros.md) | Third-party code: what it is, where it comes from, under which license and in which decision it was accepted |
| [`docs/historias/`](docs/historias/) | One page per story |
| [`status.md`](status.md) and [`dashboard/`](dashboard/index.html) | Status of each phase and story |

## License

SaguiOS-Extended is free software under the **GNU General Public License, version 3** (full text
in [`LICENSE`](LICENSE)). It is a fork of **[SaguiOS](https://github.com/erigobeli/sagui-os)**, the
operating system created by **Eli Rigobeli** (GPL v3), who expressly authorized translating it,
testing it and publishing this version as long as it remains GPL v3 and with the source code available
(see [`AUTORES.md`](AUTORES.md)). **If this project is useful to you, visit and support the original:
https://github.com/erigobeli/sagui-os**

The scheme, by layers (explained calmly in [`docs/licencia.md`](docs/licencia.md)):

| Layer | License |
| --- | --- |
| The whole (the repository and the images) | GPL v3 |
| Files that already existed in the original SaguiOS (commit `dba0fffc`) and derivatives of them | `GPL-3.0-only` |
| New files of this fork | `GPL-2.0-or-later` (which can be used under GPL v3) |
| Third parties (MicroPython, uACPI, SQLite, the ported games...) | their own, recorded in [`docs/terceros.md`](docs/terceros.md) |

Each file of our own carries its `SPDX-License-Identifier` line. NetSurf, GNUBoy and the modern start of the Realtek r8169 network driver (GPL-2.0 without «or later») **are not included in the published images or code**: only whoever builds them can include them, for private use. Details and the decision: D258 in [`docs/decisoes.md`](docs/decisoes.md).

## The project's rules

SaguiOS started from a simple idea (see [`prd.md`](prd.md), Eli Rigobeli's original specification):
**an operating system written from scratch**. This fork keeps that spirit with these rules:

- **The essentials are our own:** the boot loader (no GRUB), the kernel, the libc, the graphics library,
  the desktop and the system applications are written in the project.
- **Third-party code is the exception, and it is always isolated and recorded** in
  [`docs/terceros.md`](docs/terceros.md), with its origin and its license: interpreters and libraries
  (MicroPython, QuickJS, SQLite, uACPI), the ported games (Quake, OpenTyrian, OpenJazz, Command &
  Conquer…), hardware tables taken from free drivers (FreeBSD, OpenBSD, Mesa and some from Linux under
  «GPL v2 or later»), icons and fonts.
- **Only licenses compatible with GPL v3** in what is published. What is not (NetSurf, GNUBoy and the
  modern start of the Realtek r8169 network driver, which are «GPL v2 only») stays out of the published
  images and code; each person can build it for their own use
  ([`compilar_versiones.md`](compilar_versiones.md)).
- **One automated test per feature**, which boots the system in QEMU and checks the result
  ([`tests/`](tests/)).
- **Decisions are written down** in [`docs/decisoes.md`](docs/decisoes.md) (D001, D002…) and proposals in
  [`mejoras/`](mejoras/README.md).
- **Tested on real hardware** before a version is considered good, not only in virtual machines.
- Identifiers and file names come from the original, in Portuguese; comments, documentation and the
  interface are in Spanish.

