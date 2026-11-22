# SaguiOS-Extended Kernel Guide

[Español](guia-kernel.md) · [Português](guia-kernel_PT.md) · [English](guia-kernel_EN.md)

> For anyone who wants to learn how an operating system is built, by reading real code.
> This guide explains the **kernel** (the core): what each part does, how it fits with the others
> and which file it lives in. The applications are described in the [applications guide](guia-aplicaciones_EN.md).

**Index**

1. [What SaguiOS is](#1-what-saguios-is)
2. [How it boots](#2-how-it-boots)
3. [Kernel map: the folders](#3-kernel-map-the-folders)
4. [The blocks, one by one](#4-the-blocks-one-by-one)
5. [How a system call is made, step by step](#5-how-a-system-call-is-made-step-by-step)
6. [File tables](#6-file-tables)
7. [Where to read more](#7-where-to-read-more)

---

## 1. What SaguiOS is

**SaguiOS-Extended** is an **educational** operating system, written in Spanish and designed for
learning. Its main features:

| Feature | What it means |
| --- | --- |
| **i686, 32 bits** | It is designed for old 32-bit computers (i686 processors and compatibles). That is why the code uses 32-bit pointers and i386 assembly. The 64-bit modes are not used. |
| **Its own** | The boot loader, the kernel, the C library (the *libc*) and the graphics library (the *libsagui*) were written in this project. It does not use GRUB. Some drivers come from other projects (Linux, FreeBSD, uACPI): they are isolated and marked in `kernel/terceros/`. |
| **GPL v3** | It is free software. Code that comes from other projects is marked and recorded in [`docs/terceros.md`](terceros.md). |

> **What a kernel is.** It is the program that always runs, underneath all the others.
> It shares the CPU among the programs, gives them memory, handles the disks and the network, and
> stops them from overwriting each other. Programs (an editor, a browser) ask it for things through
> **system calls**. See section 5.

The SaguiOS kernel is written in **C** (with a little **assembly** for what C cannot do,
such as loading the interrupt table). It is compiled without the standard library of the PC
(*freestanding*): there is no `printf` or `malloc` coming from outside, everything is in `kernel/`.

---

## 2. How it boots

When the PC is switched on, nothing of SaguiOS is running yet. Booting is a **chain**: each
stage loads the next one.

```
 Power on
    │
    ▼
 BIOS (or UEFI)             the PC firmware; reads the first sector of the disk
    │
    ▼
 Stage 1: MBR               boot/mbr.asm · 512 bytes; looks for the header (LBA 1)
    │
    ▼
 Stage 2                    boot/estagio2.asm · switches to 32-bit protected mode, A20,
    │                       memory map (E820), copies everything above 1 MB
    ▼
 Kernel                     kernel/nucleo/principal.c · switches on the parts in order
    │
    ▼
 initramfs (ramdisk)        the bare minimum in RAM: the SGFS format (kernel/fs/sgfs.c)
    │
    ▼
 SAGUI-RAIZ root            the real file system on a disk (decision D379)
    │
    ▼
 Graphical session          login, desktop and taskbar (kernel/gui/)
```

### 2.1 What each step is

- **BIOS or UEFI.** The computer firmware. It copies the first sector of the disk into memory
  (`0x7C00`) and jumps there. With UEFI, SaguiOS uses its own loader (`boot/uefi/cargador.c`,
  decision D264) that does with the firmware services what stage 2 does with the BIOS.
  See [`docs/uefi.md`](uefi.md).
- **Stage 1 (MBR).** A sector of 512 bytes. It is so small that it can only read the **boot
  header** (LBA 1 of the image), which says where each part is. See `boot/cabecalho.inc`.
- **Stage 2.** It is the large part of the loader. As the processor is in *real mode* of 16 bits
  when the PC is switched on, stage 2 does everything that only the BIOS knows how to do: it
  enables the A20 line (so that more than 1 MB of memory can be used), asks for the **memory map**
  (E820), copies the kernel above 1 MB and, last, switches to 32-bit **protected mode**. It leaves
  all the information in a structure (`boot/bootinfo.inc`) that the kernel receives.
- **Kernel.** `kernel/nucleo/principal.c` is the entry point in C. It checks that the loader did
  its job (the signature and the boot structure), and switches on the pieces in order: interrupts
  and clock, physical memory, keyboard, file system and, last, the terminal.
- **initramfs (in SaguiOS, *ramdisk*).** A small file system that is loaded into RAM together with
  the kernel, with what is essential to mount the disk. In SaguiOS it is in the **SGFS** format
  (read by `kernel/fs/sgfs.c`). See [`docs/almacenamiento.md`](almacenamiento.md).
- **SAGUI-RAIZ root.** Decision **D379** separates "what is needed to boot" from "the system". The
  system lives in an ext2 partition called `SAGUI-RAIZ`, which is mounted as `/`. In this way it
  can be booted from a pendrive in *live* mode, installed on the disk or with everything in RAM.
- **Graphical session.** `kernel/gui/sessao.c` shows the login, and then the desktop
  (`kernel/gui/area_trabalho.c`) with its taskbar (`kernel/gui/barra.c`).

> **To test the boot:** `make` generates `build/saguios.img` and `./run_in_qemu.sh` boots it in a
> virtual machine. See the README, section "Compilar y probar" (Build and test).

---

## 3. Kernel map: the folders

The kernel is in the folder `kernel/`. Each subfolder is a **block** with one responsibility.

| Folder | What it is for | Files that are read first |
| --- | --- | --- |
| `kernel/arch/` | What depends on the processor: GDT and IDT tables, interrupts, APIC, SMP | `idt.c`, `gdt.c`, `pic.c`, `lapic.c`, `ioapic.c` |
| `kernel/mem/` | Memory: physical pages, paging, PAE, compressed swap, copy on write | `fisica.c`, `paginacao.c`, `heap.c`, `copia_escrita.c`, `troca.c` |
| `kernel/nucleo/` | Processes, threads, scheduler, signals, system calls, users and permissions | `processos.c`, `sistema.c`, `bifurcar.c`, `carregador.c`, `principal.c` |
| `kernel/fs/` | File system: VFS, ext2/3/4, FAT32, exFAT, ISO 9660, UDF, cache, root | `vfs.c`, `arquivos.c`, `ext2.c`, `cache_disco.c`, `raiz.c` |
| `kernel/dispositivos/` | Drivers: disks, sound, video, 2D and 3D by GPU, ACPI, laptops, sensors | `ahci.c`, `hda.c`, `video.c`, `gpu3d.c`, `uacpi_sagui.c` |
| `kernel/rede/` | TCP/IP stack, network card drivers, Wi-Fi | `rede.c`, `tcp.c`, `socket.c`, `wifi.c` |
| `kernel/usb/` | USB controllers (EHCI, OHCI, UHCI, xHCI), devices and classes | `usb.c`, `xhci.c`, `hid.c` |
| `kernel/bluetooth/` | Classic Bluetooth stack (HCI, L2CAP, SDP, HID) | `bt_hci.c`, `bt_l2cap.c`, `bt_gestor.c` |
| `kernel/gui/` | Compositor, windows, desktop, taskbar, menus, styles | `janelas.c`, `barra.c`, `inicio.c`, `sessao.c` |
| `kernel/lib/` | Pure kernel functions (without touching hardware): formats, cryptography, fast copies | `formatar.c`, `sha256.c`, `rapido.c`, `wpa.c` |
| `kernel/inc/` | The **headers** (`.h`) shared by the kernel parts and, partly, by the programs | `abi.h`, `processo.h`, `vfs.h` |
| `kernel/terceros/` | **Code from other projects** (uACPI, FreeBSD, Linux, OpenBSD, X.org): what does not belong to this project | `uacpi/` |

> **How they relate.** The folders are not islands. A program asks for a file (`nucleo`), the
> file system (`fs`) looks for the block on the disk (`dispositivos`), which asks the controller
> (`dispositivos/ahci.c`), which talks to the hardware using the memory map and the interrupts
> (`arch`, `mem`). That path is well shown in section 5.

---

## 4. The blocks, one by one

### 4.1 `kernel/arch/`: what depends on the processor

- **GDT** (`gdt.c`): the table of **segments**. It defines which code and which data belong to the
  kernel (ring 0) and which to the programs (ring 3). It also has the **TSS**, the structure that
  keeps the kernel stack for each CPU (decision D157).
- **IDT** (`idt.c`): the table of **interrupts**. It says which function runs for each interrupt
  number: processor faults, the clock, the keyboard, the disks and the `0x80` gate of the system
  calls. `tratar_interrupcao()` is the general dispatcher.
- **Hardware interrupts** (`pic.c`, `ioapic.c`, `lapic.c`, `irq_rotas.c`): the 8259 (old
  controller), the **local APIC** of each CPU and the **IOAPIC**, which distributes the device lines
  (decision D166). `irq_rotas.c` decides on which line each PCI IRQ arrives (with the ACPI `_PRT`
  table, decision D211).
- **Error screen** (`excecoes.c`): when a program fails in a serious way, the state of the processor
  is shown instead of hanging the system.
- **FPU, MMX and SSE** (`fpu.c`): saves and restores the floating-point state of each process.
- **MSR** (`msr.c`): reading of the special registers of the processor that does not bring the
  kernel down if the MSR does not exist (decision D092).
- **SMP**: the start of the secondary processors is in `kernel/nucleo/smp.c`. It is optional
  (decision D164): it is only switched on with the `--smp` image. The kernel code keeps running on
  CPU 0 (master and slave model); the other CPUs run processes and serve system calls
  (decisions D157 and D165).

### 4.2 `kernel/mem/`: memory

- **Physical memory** (`fisica.c`): how many 4 KB pages are free. It keeps a **bitmap** (one bit
  per page: 1 = in use).
- **Kernel allocator** (`heap.c`): `kmalloc` and `kfree`. Small blocks come from size classes;
  large ones, from whole pages.
- **Paging** (`paginacao.c`): each process has its **address space**, with its page directories.
  There are two modes: the classic 2-level one and the 3-level **PAE**, which allows more than 4 GB
  of physical memory and the **NX** bit (the page with data cannot be executed; decisions D010, D049
  and D349).
- **High memory** (`memalta.c`): temporary windows to write into physical pages that are not mapped
  (decision D348).
- **Copy on write** (`copia_escrita.c`): when `fork` duplicates a process, the pages are shared and
  they are only copied when someone writes to them (decision D401).
- **Compressed swap in RAM** (`troca.c`, `compressor.c`): when memory runs short, instead of writing
  to the disk the page is compressed in RAM itself (decision D352).
- **Write-combining** (`wc.c`): the video framebuffer is written in blocks, which is faster.
- **Shared pages of the libraries** (`imagens.c`): the code of a shared library lives in a single
  copy (decision D378, [`docs/enlace-dinamico.md`](enlace-dinamico.md)).

### 4.3 `kernel/nucleo/`: processes, threads and system calls

- **Processes** (`processos.c`): the process table and the preemptive *round-robin* **scheduler**:
  each process receives a time slice and the clock takes the CPU away from it when it ends.
- **Schedulers between CPUs** (`planificadores.c`, `afinidad.c`): on which processor each process
  runs (decision D174).
- **Threads** (`hilos.c`): threads inside a process, with waiting on an address (*futex*, D167).
- **Locks** (`trava.c`, `trava_geral.c`, `spinlock.c`, `atomicas.c`): mutual exclusion. With SMP,
  the kernel has a general lock (D165) and atomic primitives (D150).
- **Program loading** (`carregador.c`): reads an **ELF32** executable and creates the process with
  its code, its data and its stack.
- **`fork` and `exec`** (`bifurcar.c`): `fork` creates a copy of the process (with copy on write) and
  `exec` replaces it with another program. It is the Unix model; the shell uses them for each command.
- **Signals** (`sinais.c`): notices between processes (decision D401).
- **System calls** (`sistema.c`): the entry gate `int 0x80`. See section 5.
- **Users and permissions** (`usuarios.c`, `permisos.c`): who each process is (its uid and gid) and
  which calls each one may make (decisions D255 and D282).
- **Time** (`hora.c`) and **power** (`energia.c`): the system clock, the time zone (D080),
  rebooting and shutting down.
- **Growing stacks** (`pilha.c`): the stack of a program grows when needed, with a guard that detects
  the overflow (decision D186).
- **Service manager** (`supervisor.c`): the background services, seen from the kernel
  (decision D216).
- **Boot and configuration**: `principal.c` (kernel entry), `kconf.c` (options that are saved for the
  next boot, D353), `info_hardware.c` (data for `infopc`, D041).

### 4.4 `kernel/fs/`: the file systems

The key idea is the **VFS** (*Virtual File System*, `vfs.c`): programs always use the same calls
(`abrir`, `ler`, `escrever`), and the VFS sends them to the file system that corresponds.

```
 program → system call → arquivos.c → vfs.c → ext2 / FAT32 / exFAT / ISO 9660 / UDF / SGFS
                                                   → pipe.c (pipes)  → tv.c (terminal)
                                                                       ↓
                                                   cache_disco.c (block cache) → blocos.c → disks
```

- **File descriptors** (`arquivos.c`): the table of open files of each process.
- **Mounts** (`montagem.c`): the mount table and `/etc/fstab` (decision D040).
- **Disks and partitions** (`discos.c`, `blocos.c`): access to the sectors, partition table and disk
  notices (decisions D040 and D179).
- **Block cache** (`cache_disco.c`): read-ahead and delayed write (decision D281).
- **ext2, ext3 and ext4** (`ext2.c`, `jbd2.c`): reading and writing, with the JBD2 journal (D326).
  It is the system of the `SAGUI-RAIZ` root.
- **FAT32** (`fat32.c`) and **exFAT** (`exfat.c`): pendrives and memory cards.
- **CD and DVD**: `iso9660.c` (ISO 9660), `udf.c` (UDF, D233) and `cdda.c` (an audio CD seen as a
  folder of WAV files).
- **Overlay** (`sobreposicao.c`): the root of the *live* version. The system of the medium is
  read-only and a writable layer goes on top, like the *overlayfs* of Linux (decision D379).
- **Root on disk and in RAM** (`raiz.c`, `ramfs.c`, `disco_ram.c`, `disco_comprimido.c`): different
  ways of mounting `/` (decision D379).
- **Pipes** (`pipe.c`): communication between processes through a circular buffer inside the kernel.
- **Virtual terminal** (`tv.c`): the pseudo-terminal used by the Window terminal and `ssh`.
- **Configuration and log** (`conf_sistema.c`, `logs_disco.c`): the `SAGUI-CONF` partition and the
  kernel log in `SAGUI-LOGS`.

### 4.5 `kernel/dispositivos/`: the drivers

It is the largest block (83 files). It is grouped like this:

- **Disks**: `ata.c` (IDE over PIO), `ahci.c` (SATA), `nvme.c` (NVMe), `atapi.c` (CD/DVD readers).
- **Bus**: `pci.c` (the PCI bus), `bar.c` (the memory windows of each device),
  `pci_realoc.c` (moving a 64-bit BAR below 4 GB), `isa.c` and `isapnp.c` (the ISA bus and the
  *Plug and Play* ISA cards).
- **ACPI**: `uacpi_sagui.c` is the kernel layer for the **uACPI** library, which interprets the
  AML language of the firmware. On top of it are `acpi.c` (shutdown), `bateria.c`, `ec.c`, `prt.c`
  and `wmi.c`.
- **Laptops**: `portatil.c`, `toshiba.c`, `fujitsu.c`, `hp_wmi.c`, `video_acpi.c` (brightness and keys).
- **Sensors and power**: `sensores.c` (temperature and frequency), `frequencia.c` (temperature
  limit), `eist_antiguo.c` (SpeedStep), `zonas_termicas.c`.
- **Keyboard, mouse and gamepads**: `teclado.c`, `i8042.c`, `mouse.c`, `painel.c` (PS/2 touchpads),
  `mandos.c` and `portajogos.c`.
- **Video**: `video.c` (VESA with double buffer), `vga_texto.c` (text mode), `bga.c` (Bochs/QEMU),
  `modos_video.c` and `telas.c` (several screens).
- **Sound**: `audio.c` (mixer), `hda.c` and `ac97.c` (cards), `sb16.c`, `captura.c` (microphone),
  `som.c` (the PC speaker).
- **GPU**: the common layers `acel2d.h`, `gpu3d.h` and `motor_gpu.c`, and one driver per family
  (`acel_intel.c`, `acel_radeon.c`, `acel_r600.c`, `gpu3d_r600.c`, `gpu3d_evergreen.c`...).
  Which cards are supported and with which version is in [`docs/gpus.md`](gpus.md); GPU 3D is
  explained in [`docs/gpu3d.md`](gpu3d.md).
- **Time and log**: `relogio.c` (PIT and CMOS), `serial.c` (the kernel "log" over COM1),
  `terminal_es.c` and `terminal_grafico.c`.

### 4.6 `kernel/rede/`: the network and Wi-Fi

- **Network stack** (`rede.c`): Ethernet, ARP, IPv4, ICMP (`ping`), UDP, DHCP and DNS.
- **TCP** (`tcp.c`) and **sockets** (`socket.c`): the reliable connection and the interface that
  programs see.
- **Card drivers**: `e1000.c`, `r8169.c`, `rtl8139.c`, `ne2000.c`, `alc.c` (Atheros AR813x).
- **Wi-Fi**: `wifi.c` (802.11 client with the RTL8188EU dongle), `rtl8188eu.c`, `rt2870.c` (Ralink),
  `ar9485.c` (integrated Atheros) and `wifi_pcie.c`. The part that does not touch the hardware
  (WPA2 and WPA3 cryptography) is in `kernel/lib/wpa.c` and `kernel/lib/sae.c`. See
  [`docs/wifi_usb.md`](wifi_usb.md) and [`docs/wifi-ar9485.md`](wifi-ar9485.md).
- **Tunnel and VPN**: `tunel.c` (the `wg0` interface of WireGuard, [`docs/wireguard.md`](wireguard.md)).
- **Advertising blocking by DNS** (`bloqueo.c`, decision D218).

The files with the suffix `_logica.h` are the **pure part** of a driver: calculations and decoding
that do not touch the hardware. In this way they can be tested on the PC, without a card. It is a
pattern that is repeated throughout the kernel.

### 4.7 `kernel/usb/` and `kernel/bluetooth/`

- **USB** (`usb.c`): enumeration of the devices, hubs and hot connection.
  The controllers are `xhci.c` (USB 3.0), `ehci.c` (2.0), `ohci.c` and `uhci.c` (1.1).
  The device classes: `armazenamento.c` (pendrives, with SCSI), `hid.c` (keyboards and mice),
  `usbaudio.c` (sound), `uvc.c` (cameras). See [`docs/usb3.md`](usb3.md).
- **Bluetooth** (`kernel/bluetooth/`): the classic stack is divided into layers, as in the textbook:
  `bt_hci.c` (commands and events of the controller), `bt_l2cap.c` (channels), `bt_sdp.c` (finding
  services), `bt_hid.c` (wireless keyboards and mice) and `bt_gestor.c` (devices and pairing).
  `bt_fw.c` loads the firmware of some chips. See [`docs/bluetooth.md`](bluetooth.md).

### 4.8 `kernel/gui/`: the desktop

- **Compositor and windows** (`janelas.c`): the window manager. It decides which window is on top,
  drags it, minimises it and closes it.
- **Drawing** (`desenho.c`, `escala.c`, `rapido.c` from `kernel/lib`): drawing with clipping, scaling
  and fast copies.
- **Desktop** (`area_trabalho.c`): background, mascot, icons and the right-button menu (`menu.c`).
- **Taskbar** (`barra.c`): Start button, window buttons, tray and clock.
- **Start menu** (`inicio.c`): the column menus. The entries of each category are in the table
  `programas[]` of `barra.c`. The full list is in the
  [applications guide](guia-aplicaciones_EN.md).
- **Styles and colours** (`aparencia.c`, `arte.c`, `idioma.c`): highlight colour, pixel-art drawings
  and the interface language (decision D313).
- **Notices, power and session**: `avisos.c` (notifications), `ocio.c` (screensaver and lock),
  `sessao.c` (login and logout), `permisos_dialogo.c` (administrator password).

### 4.9 `kernel/lib/`: pure functions

These are functions that **do not touch the hardware**: they can be understood and tested without a
real machine.

- Formats and text: `formatar.c` (like `printf`), `texto.c`, `caminho.c` (paths `/`, `.`, `..`),
  `catalogo.c` (translations).
- Cryptography: `sha256.c`, `p256.c`, `wpa.c`, `wpa_cifra.c`, `sae.c`.
- Fast copies: `rapido.c`, `sse1.c`, `sse2.c`, `sse3.c` and `ssse3.c`. Each one uses the instruction
  that the CPU has.
- Sensor and power conversions: `sensores_conv.c`, `bateria_conv.c`, `brilho_conv.c`,
  `frequencia_pol.c`.
- Time: `fuso.c` (time zones with POSIX TZ rules).
- Decompression: `inflar.c` (DEFLATE, for the compressed root of the CD).

### 4.10 `kernel/terceros/`: code from other projects

Here is what was **not** written in this project. Each origin has its folder and its licence:

- `uacpi/`: the **uACPI** library (MIT), which interprets the tables and the AML language of ACPI.
- `bsd/` (FreeBSD and OpenBSD, BSD licence) and `linux/` (Linux, GPL-2.0), with register data and
  card initialisation tables.
- `mit/` (X.org and the ATOM interpreter of the Radeons, MIT).

What comes from outside is explained in [`docs/terceros.md`](terceros.md), and the authorship rule
(decisions D034 and D073) in [`docs/decisoes.md`](decisoes.md). The GPL pieces are isolated in
`kernel/terceros/linux/` so that the variant without GPL (`make SEM_GPL=1`) can be compiled without them.

---

## 5. How a system call is made, step by step

A program cannot touch the disk or the screen by itself: the processor prevents it from using
certain instructions (it is in **ring 3**). To ask the kernel for something, it uses a **system
call**. As an example we follow **`write`**, the call that writes data into a file, a pipe or the
terminal.

```
 programa (anillo 3)
   write(fd, buf, n)                      userland/libc/posix.c
     └─ escrever(fd, buf, n)              userland/libsagui/chamadas.c
          └─ EAX=12, EBX=fd, ECX=buf, EDX=n;  int $0x80
 ───────────────────────────── cambio de anillo (la CPU entra al kernel) ─────────
 kernel (anillo 0)
   IDT, puerta 0x80                       kernel/arch/idt.c  → tratar_interrupcao()
     └─ tratar_sistema(cuadro)            kernel/nucleo/sistema.c
          ├─ ¿número válido? ¿punteros del usuario correctos? (usuario_ok)
          ├─ ¿permitido para este usuario? (kernel/nucleo/permisos.c)
          └─ tabela[SIS_ESCREVER]  →  sis_escrever()      kernel/fs/arquivos.c
               └─ escrever_em()  →  vfs_escrever()        kernel/fs/vfs.c
                    └─ fichero: driver del sistema (ext2, FAT32...) + caché de bloques
                       tubería: kernel/fs/pipe.c
                       terminal: kernel/fs/tv.c
 ───────────────────────────── vuelta al programa ───────────────────────────────
   EAX = bytes escritos (o negativo si hay error)
   write() devuelve ese valor
```

### 5.1 The steps, one by one

1. **The program calls `write`.** It is the function of the *libc* (`userland/libc/posix.c`). The
   POSIX-style functions come from the libc; the ones specific to SaguiOS, from the libsagui.
2. **`escrever` puts the numbers in the registers.** `userland/libsagui/chamadas.c` places the
   number of the call in `EAX` (`SIS_ESCREVER` is worth **12**, according to `kernel/inc/abi.h`), the
   arguments in `EBX`, `ECX` and `EDX`, and executes `int $0x80`. That is the contract described by
   the `abi.h` header: "int 0x80, number in EAX, arguments in EBX, ECX, EDX, result in EAX".
3. **The CPU passes to the kernel.** The `int 0x80` instruction looks up entry 0x80 of the **IDT**
   (`kernel/arch/idt.c`). As that gate allows it to be called from ring 3, the CPU switches to the
   kernel stack (the TSS of `gdt.c`) and jumps into the kernel.
4. **The call dispatcher.** `sistema_iniciar()` (`kernel/nucleo/sistema.c`) registers
   `tratar_sistema` for that vector. The function reads `EAX` and consults the **table** `tabela[]`,
   which points each number to its function (`[SIS_ESCREVER] = sis_escrever`).
5. **Checks.** Before running anything, the kernel checks that the number exists, that the pointers
   (such as `buf`) are inside the program's memory (`usuario_ok`) and that the user may make that
   call (the class table of `permisos.c`).
6. **The real work.** `sis_escrever` (`kernel/fs/arquivos.c`) looks for the descriptor `fd` in the
   table of the process. Then `vfs_escrever` (`kernel/fs/vfs.c`) decides which file system or
   device that descriptor corresponds to.
7. **If it has to wait.** If the call has to wait (for example, reading an empty pipe), the kernel
   puts the process to sleep and **leaves the instruction pointer pointing at the `int 0x80`**.
   When the process wakes up, the call is repeated (`sistema.c`, header and line `eip -= 2`).
8. **The return.** The result is stored in `EAX`: bytes written, or a negative number if it fails.
   The kernel restores the registers of the program and returns (`iret`). The libc receives the value
   and `write` returns it.

> **To practise:** search for `SIS_ESCREVER` in the code. You will see the constant in `abi.h`, its
> use in `chamadas.c`, the table in `sistema.c` and the permission in `permisos.c`. It is the complete
> route of any other call, changing only the name.

---

## 6. File tables

Each table has **all** the `.c` and `.h` files of its folder, with a line about what they do.
The line comes from the header comment of each file (almost all of them start with
`/* nombre.c - qué hace ... */`). The references `(D…)` are decisions from
[`docs/decisoes.md`](decisoes.md), and the `H…` and "mejora" entries are the historical numbering of
the project. The path is relative to the folder of the table.

### 6.1 kernel/arch/ (GDT, IDT, interrupts, APIC, SMP)

| File | What it does |
| --- | --- |
| `excecoes.c` | the SaguiOS error screen |
| `fpu.c` | x87 FPU, MMX and SSE for the programs |
| `gdt.c` | kernel segment table and the TSS (D157: one per CPU) |
| `idt.c` | interrupt table and the dispatcher |
| `ioapic.c` | the IOAPIC: the device IRQs without the 8259 (D166) |
| `irq_rotas.c` | the path of each PCI IRQ: self-test at boot, _PRT with uACPI and fallback to the 8259 by line (D211) |
| `lapic.c` | the local APIC of each processor (D157) |
| `msr.c` | MSR reading that does not bring the kernel down (D092) |
| `pic.c` | the two 8259 interrupt controllers (master and slave) |


### 6.2 kernel/mem/ (physical memory, paging, PAE/NX, compressed swap, copy on write)

| File | What it does |
| --- | --- |
| `compressor.c` | page compression for the compressed swap (D352) |
| `copia_escrita.c` | the reference count of the pages shared by fork (D401) |
| `fisica.c` | physical page manager (bitmaps) |
| `heap.c` | kmalloc/kfree: size classes for small blocks and whole pages for large ones |
| `imagens.c` | read-only pages of the shared libraries among processes (D378, phase 3; design in docs/enlace-dinamico.md §15 and in D378) |
| `memalta.c` | temporary windows to physical pages (D348) |
| `paginacao.c` | paging in two modes: classic 2-level or 3-level PAE (D010, D049, D349) |
| `troca.c` | compressed swap in RAM (D352) |
| `wc.c` | write-combining (WC) for the framebuffer |


### 6.3 kernel/nucleo/ (processes, threads, scheduler, signals, calls, users and permissions)

| File | What it does |
| --- | --- |
| `acoes.c` | common system actions (volume, mute, brightness, radio) and their on-screen notice (D403) |
| `aleatorio.c` | unpredictable numbers for the TLS, the TCP and the DNS (D061) |
| `atomicas.c` | the 64-bit part of the atomic primitives (D150) |
| `autoteste.c` | tests that the kernel runs on itself |
| `bifurcar.c` | fork with copy on write and waitpid (D401) |
| `carregador.c` | creates processes from ELF32 executables (H13) |
| `chamadas_processos.c` | calls of the "processes" and "miscellaneous" groups |
| `console.c` | the text output of the kernel: kprintf, klog and the console |
| `cpu_dados.c` | the per-CPU data area (D150, D157) |
| `energia.c` | rebooting and shutting down the machine |
| `hilos.c` | user threads and waiting on an address (futex) (D167) |
| `hora.c` | the system time and the time zone (D080) |
| `info_hardware.c` | what infopc (D041) needs and only the kernel can reach |
| `kconf.c` | kernel options that are saved from SaguiOS and take effect on the next boot (D353) |
| `permisos.c` | who can do what (D255 and D282, improvement 098, phases 1 and 2) |
| `pilha.c` | user stacks that grow on demand, with a guard (D186) |
| `planificadores.c` | the scheduler policies between the CPUs (D174) |
| `portapapeis.c` | the system clipboard (D074) |
| `principal.c` | the C entry point of the SaguiOS kernel |
| `processos.c` | process table and preemptive round-robin scheduler |
| `rastro.c` | marks that survive a warm reboot (see rastro.h) |
| `sinais.c` | POSIX signals (D401) |
| `sistema.c` | entry gate of the system calls (int 0x80) |
| `smp.c` | the start of the secondary processors (D157, D164) |
| `spinlock.c` | kernel busy-wait locks (D150) |
| `supervisor.c` | the service manager seen from the kernel (D216) |
| `trava.c` | mutual exclusion between processes and kernel tasks (D095) |
| `trava_geral.c` | the general kernel lock (D165) |
| `usuarios.c` | SIS_USUARIO: the identity of the processes (D255, improvement 098, phase 1) |


### 6.4 kernel/fs/ (VFS, ext2/3/4, FAT32, exFAT, ISO 9660, UDF, disk cache, overlay and root)

| File | What it does |
| --- | --- |
| `arquivos.c` | open files and the file and folder calls (H14) |
| `blocos.c` | raw access to the sectors of a disk (D179) |
| `cache_disco.c` | block cache with read-ahead and delayed write (D281) |
| `cdda.c` | the "cdda" file system: an audio CD as a folder of WAV files (D233) |
| `cdda_logica.c` | what does not touch the hardware of the audio CD (D233): the table of contents, CD-TEXT, file names, the WAV header and the list |
| `conf_sistema.c` | the persistent system configuration (D255, option A of the maintainer) |
| `disco_comprimido.c` | the CD root compressed by blocks, seen as a disk ("rz0"; v8.5.4.2, D379 P5) |
| `disco_ram.c` | a disk in RAM with the copy of a volume ("ram0"; D379, phase T1: toram) |
| `discos.c` | disks, partitions, formats and notices (H32, H34, D040) |
| `exfat.c` | the exFAT file system of pendrives and large memory cards (D325) |
| `ext2.c` | ext2, ext3 and ext4: reading and writing (D040, D326) |
| `fat32.c` | the FAT32 file system of the data pendrive (H32, H33) |
| `iso9660.c` | reading of CD/DVD with ISO 9660 (D207) |
| `jbd2.c` | playback and writing of the JBD2 journal (D326) |
| `logs_disco.c` | the kernel log in the SAGUI-LOGS partition (D120) |
| `montagem.c` | mount table and /etc/fstab (D040) |
| `pipe.c` | communication between processes through a circular buffer in the kernel |
| `raiz.c` | the root of the system on a disk (D379, prototype of improvement 035 F2) |
| `ramfs.c` | a branch of the tree in RAM mounted as one more volume (D379, phase R3) |
| `sgfs.c` | reading of the SGFS ramdisk |
| `sobreposicao.c` | the root of the live: the system of the medium, read-only, with a writable layer on top (D379, phase L1; in the style of the Linux overlayfs) |
| `tv.c` | virtual terminal (pseudo-terminal), D036 |
| `udf.c` | reading of CD/DVD with UDF (D233), read-only |
| `vfs.c` | virtual file layer with the writable layer in RAM (H14) |


### 6.5 kernel/dispositivos/ (disks, sound, video, 2D and 3D by GPU, ACPI/uACPI, laptops, sensors)

| File | What it does |
| --- | --- |
| `ac97.c` | AC'97 controllers from Intel (ICH) and compatible ones (D089) |
| `acel_gcn.c` | copying and filling rectangles with the DMA engine of the GCN 1.0 Radeons (Southern Islands: Tahiti, Pitcairn, Cape Verde, Oland; HD 7000, R7/R9 200 and 300 of those families) (D360) |
| `acel_intel.c` | the blitter of the Intel GPUs, from Gen3 to Gen8 (D195, D359) |
| `acel_intel_logica.h` | the pure part of the Intel blitter (D195, D359): the models of Gen7 (Ivy Bridge), Gen7.5 (Haswell) and Gen8 (Broadwell) with exact identifier, and the families that are recognised but have no 2D |
| `acel_mach64.c` | the 2D engine of the ATI Mach64 and Rage (GX, CT, VT, Rage II, Rage Pro, Rage LT/XL/Mobility) (D308) |
| `acel_nv.c` | the 2D engine of the NVIDIA GeForce 6/7 and Go 6xxx/7xxx (NV4x/G7x) and of the integrated ones in the nForce chipsets (GeForce 6100/6150 and 7025/7050/7100/7150: C51, C61, C67, C68 and C73) (D224) |
| `acel_nv_logica.h` | the pure part of the 2D accelerator of the GeForce 6/7 (D224): the models (exact PCI identifiers and the chip that has to say PMC_BOOT_0), the format of the buffer commands (headers, points, sizes, jumps), the PRAMIN hash table and the pitch of the surface |
| `acel_r600.c` | copying and filling rectangles with the DMA engine of the R6xx/R7xx Radeons (D223; the common layer is the one of D195) |
| `acel_radeon.c` | the classic 2D engine of the R100 to R500 Radeons (D195) |
| `acel_s3.c` | the 2D of the S3 Trio64 and ViRGE (D308) |
| `acel_tdfx.c` | the 2D engine of the 3dfx Banshee, Voodoo3 and Voodoo4/5 (VSA-100) (D305) |
| `acel_vmware.c` | 2D accelerator of the VMware SVGA II (PCI 15ad:0405, D195) |
| `acpi.c` | ACPI shutdown (state S5) and the power button (D113, improvement 030) |
| `ahci.c` | driver of the AHCI SATA controller (D155) |
| `ata.c` | internal ATA (IDE) disks over PIO, without DMA or interrupts (D040) |
| `atapi.c` | CD/DVD readers (ATAPI), read-only (D207) |
| `atom_sintetica.h` | a toy ATOM BIOS, built by hand, to test without a card (D399) |
| `audio.c` | the kernel sound mixer and the audio calls (D088-D090) |
| `bar.c` | the BARs (the address windows) of a PCI device (D158) |
| `bateria.c` | the laptop battery through ACPI (D192) |
| `bga.c` | video card of Bochs/QEMU ("dispi", BGA) |
| `bga_extra.c` | second (and third) Bochs/QEMU cards as extra video outputs (D290) |
| `captura.c` | kernel sound input (microphone) (D292) |
| `ec.c` | the embedded controller (EC, ACPI PNP0C09) for the AML (D192) |
| `eist_antiguo.c` | Enhanced SpeedStep of old Intel processors (D177, phases A and B) |
| `energia_radeon.c` | clocks and power states of the R5xx Radeons through the ATOM BIOS (D399) |
| `energia_sim.c` | fake power driver for the tests in QEMU (D399, mkimagem.py --gpu-energia-simulada) |
| `evergreen_3d_pacotes.c` | PM4 packets, shaders and start values of the 3D engine of the Evergreen and Northern Islands Radeons without Cayman (D427) |
| `evergreen_dma.c` | model list, copy packet and screen check of the Evergreen and Northern Islands Radeons (D286) |
| `frequencia.c` | CPU frequency driver with temperature limit (D106, improvement 061) |
| `fujitsu.c` | the ACPI devices specific to Fujitsu (and Fujitsu Siemens) (D403) |
| `g3d_v2_provas.c` | tests of the gpu3d v2 capabilities of a driver (D426) |
| `gart_r700_logica.h` | the GART (virtual memory, context 0) of the R7xx Radeons (RV710, RV730, RV740, RV770), D406 |
| `gcn_dma.c` | list of GCN models, DMA packets of the Southern Islands, screen check (DCE 6) and filling with CONSTANT_FILL (D360) |
| `gen3_gtt.h` | system RAM in the GTT of Intel Gen3 (915, 945, G33, Pineview), D358 |
| `gen3_pacotes.h` | the 3D commands of the Intel Gen3 GPUs (915, 945, G33, Pineview), D285 |
| `gpu3d_evergreen.c` | 3D by the GPU in the Evergreen Radeons (HD 5000: Cedar, Redwood, Juniper, Cypress, Hemlock) and Northern Islands without Cayman (HD 6000 and HD 7500M/7600M: Barts, Turks, Caicos), under the common gpu3d layer (D427) |
| `gpu3d_intel.c` | 3D by the GPU in Intel Gen3: 915, 945, G33 and Pineview (D285, D358) |
| `gpu3d_r500.c` | 3D by the GPU in the R5xx Radeons (X1300 to X1950: RV515, R520, RV530, RV560, RV570, R580), under the common gpu3d layer (D285, D287) |
| `gpu3d_r600.c` | 3D by the GPU in the R7xx Radeons (RV710, RV730, RV740, RV770) and, since D357, R6xx (R600, RV610, RV620, RV630, RV635, RV670): the command processor (CP) with its microcode, the state of the 3D engine and the drawings, under the common gpu3d layer |
| `gpu3d_sim.c` | a fake 3D driver, on the CPU, to test the common layer (D285) |
| `gpu_energia_logica.h` | pure logic of the GPU power (D399): what can be tested on the PC |
| `hda.c` | Intel High Definition Audio (D089) |
| `hda_ajustes.c` | choice of the setting of an HDA codec (D107) |
| `hp_wmi.c` | radios and panel keys of HP laptops through WMI (D405) |
| `i8042.c` | the keyboard and touchpad controller (H19-fix) |
| `injecao.c` | injected input for the remote desktop (D251, improvement 097) |
| `isa.c` | the ISA bus (D302): the 8237 DMA controller, the low memory for it and the list of detected ISA devices (for infopc and lspci) |
| `isapnp.c` | ISA Plug and Play (D302): finding the ISA PnP cards and configuring a Creative Sound Blaster (Vibra16, SB16 PnP, AWE32/64 PnP) that the BIOS left inactive |
| `mandos.c` | the common layer of gamepads and joysticks (D307) |
| `modos_video.c` | list of modes, hot change and preferred resolution |
| `motor_gpu.c` | what the kernel knows about the 3D engine through the GPU (D158) |
| `mouse.c` | PS/2 mouse driver (the touchpad of the G470 speaks the same protocol) |
| `nvme.c` | NVMe controller driver (D265), written from the public NVM Express 1.4 / 2.0 specification (without code from other systems) |
| `painel.c` | PS/2 touch panels (touchpads) in absolute mode, with gestures (D407) |
| `painel_logica.h` | the pure part of the PS/2 touch panels (D407): recognising the model from its replies, decoding the absolute packets of Synaptics, ALPS and Elantech and converting the touches into movement, clicks and wheel (the gestures) |
| `pci.c` | the PCI bus (H29) |
| `pci_realoc.c` | moves below 4 GB a 64-bit BAR that the firmware left high up (D283) |
| `portajogos.c` | the analogue joystick of the game port (0x201) (D307) |
| `portatil.c` | what is specific to laptops through ACPI: brightness, firmware notices, SIS_PORTATIL and the dump of the tables (D403) |
| `prt.c` | the routing of the PCI interrupts of the firmware (_PRT) with uACPI (D211) |
| `r500_pacotes.h` | the 3D commands of the R5xx Radeons (RV515, R520, RV530, RV560, RV570, R580) for the command processor (CP): pipeline state (VAP, RS, US, FG, RB3D, ZB), the vertex program and the two fragment programs |
| `r600_3d_pacotes.c` | PM4 packets, shaders and start values of the 3D engine of the R6xx/R7xx Radeons (D284) |
| `r600_dma_pacotes.c` | packets of the DMA engine of the R6xx/R7xx Radeons and the splitting of rectangles into linear copies (D223) |
| `radeon2d_ordens.h` | the register writes of a command of the 2D engine of the R100-R500 Radeons (D195, D387, D391), without effects: the same ones, in the same order, go through MMIO (acel_radeon.c) or, with the CP running, through the ring as PACKET0 (gpu3d_r500.c) |
| `relogio.c` | the PIT (programmable timer) and the CMOS clock |
| `sb16.c` | the Creative Sound Blaster 16, on the ISA bus (D302) |
| `sensores.c` | hardware monitor: CPU temperature and frequency (D092, improvement 050) |
| `serial.c` | serial port COM1 (0x3F8), the kernel "log" |
| `som.c` | beep through the PC speaker (bonus B3) |
| `teclado.c` | PS/2 keyboard driver (i8042 controller, IRQ 1), Spanish layouts (ES, the default) and ABNT2 |
| `telas.c` | several screens: output register, desktop layout and configuration (D290) |
| `terminal_es.c` | the terminal as seen by the programs |
| `terminal_grafico.c` | the terminal drawn in graphic mode with its own font (H18) |
| `toshiba.c` | the HCI interface of Toshiba laptops (D403) |
| `uacpi_sagui.c` | the kernel layer for uACPI and the "acpi-aml" task (D192) |
| `vga_texto.c` | VGA text-mode console (80x25), used in phases 1 to 3 |
| `video.c` | VESA framebuffer with double buffer |
| `video_acpi.c` | the standard ACPI Video: brightness of the laptop screen and notices of the brightness and screen keys (D403; ACPI specification, appendix B «Video Extensions») |
| `volcado_logica.h` | the pure part of dumping the back buffer to the screen through the GPU (D387): entries of the PCIE GART of the R300-R5xx Radeons, where to put its window in the map of the memory controller (MC) and the splitting of a rectangle into strips for the 2D engine |
| `vram_zona_logica.h` | where to put a work area of the GPU (command queue, 3D buffers) in video memory without overwriting anything that the card shows (D386) |
| `wmi.c` | the WMI bridge of ACPI (PNP0C14 devices) over uACPI (D405) |
| `zonas_termicas.c` | the ACPI thermal zones (ThermalZone, _TMP), to show them next to the GPU (D399) |


### 6.6 kernel/rede/ (TCP/IP stack, network drivers, Wi-Fi)

| File | What it does |
| --- | --- |
| `alc.c` | EXPERIMENTAL driver of the Atheros AR813x/AR815x Ethernet (L1C/L2C family, Linux's "atl1c"), D200 |
| `alc_logica.h` | the pure part of the alc driver (Atheros AR813x/AR815x, D200): models, MAC, descriptors, PHY state and values of the configuration registers |
| `ar9485.c` | integrated Atheros AR9485 Wi-Fi (PCIe 168c:0032, 802.11b/g/n 1x1, 2.4 GHz), D280 |
| `ar9485_logica.h` | the pure part of the Atheros AR9485 driver (D280): reading and restoring the EEPROM/OTP image (blocks with checksum and templates), channel tables and the interpolation of the powers |
| `bloqueo.c` | advertising and tracking blocking by DNS (D218) |
| `e1000.c` | driver of the Intel PRO/1000 (82540EM and similar) |
| `ne2000.c` | driver of NE2000 cards (D329): the DP8390 with 16 KB of its own RAM, on PCI (Realtek RTL8029 and clones, 10ec:8029) and on ISA (ports 0x300/0x280/0x320/0x340) |
| `r8169.c` | EXPERIMENTAL driver of the Realtek RTL8101E/8102E/8105E and RTL8168/8169 family |
| `r8169_ajustes.h` | hot settings of the r8169 driver (D124): RxConfig, RxMaxSize, buffer size in the descriptor and "multi" (bit 14 of RxConfig), requested with "ifconfig eth0 r8169 ..." to test variants on real hardware without recompiling |
| `r8169_logica.h` | part of the r8169 driver that does not touch the hardware (D091): chip revision, start profile and decoding |
| `r8169_sem_gpl.h` | the minimum of the r8169 driver for the build without GPL (`make SEM_GPL=1`, improvement 057, D109) |
| `rede.c` | the network stack: Ethernet, ARP, IPv4, ICMP (ping), UDP, DHCP and DNS |
| `rt2870.c` | the Ralink RT2870 / RT3070 / RT3071 / RT3072 / RT5370 Wi-Fi dongles (D409) |
| `rt2870_logica.h` | the pure part of the support for the Ralink RT2870 / RT3070 / RT3071 / RT3072 / RT5370 Wi-Fi dongles (D409, experimental, untested on hardware) |
| `rtl8139.c` | driver of the Realtek RTL8139 (QEMU: -nic user,model=rtl8139) |
| `rtl8188eu.c` | the chip of the Realtek RTL8188EU Wi-Fi dongle (USB 0bda:8179), D215 |
| `rtl8192cu_logica.h` | the pure part of the support for the Realtek RTL8188CU / RTL8192CU / RTL8188RU Wi-Fi dongles (D409, experimental, untested on hardware) |
| `socket.c` | sockets: the network as seen by the programs |
| `tcp.c` | TCP: simple client and server |
| `tunel.c` | the tunnel interface, wg0 (D363) |
| `wifi.c` | Wi-Fi client (802.11 b/g) with the Realtek RTL8188EU dongle (D215) |
| `wifi_pcie.c` | identification of more integrated PCIe Wi-Fi cards (D410), experimental |
| `wifi_pcie_logica.h` | the pure part of the support for more integrated PCIe Wi-Fi (D410): card catalogue by PCI identifier, Atheros families (ath9k/athn), decoding of the SREV, geometry and reading of the ROM of the AR5008 family |
| `wifi_usb_ids.h` | VID:PID of the USB Wi-Fi dongles (D409) |
| `wifi_usb_logica.h` | which USB Wi-Fi dongle family a VID:PID is (D409) |


### 6.7 kernel/usb/ (USB controllers and device classes)

| File | What it does |
| --- | --- |
| `armazenamento.c` | pendrives: Bulk-Only Transport + SCSI (H31) |
| `ehci.c` | driver of the USB 2.0 controller (EHCI) (H29) |
| `hid.c` | USB keyboards and mice (HID class) through xHCI, EHCI, UHCI and OHCI (D214, D267) |
| `isoent.h` | queue of packets received from an input isochronous stream (D277) |
| `ohci.c` | driver of the USB 1.1 OHCI controller (AMD, NVIDIA, SiS, ALi, Apple...) (D267) |
| `uhci.c` | driver of the USB 1.1 UHCI controller (Intel PIIX/ICH, VIA...) (D267) |
| `usb.c` | USB devices: enumeration, hubs and hot connection (H30, D214) |
| `usbaudio.c` | USB audio, Audio 1.0 class (UAC 1), sound output (D275) |
| `usbbt.c` | USB transport of the Bluetooth stack and the SIS_BLUETOOTH call (D279, improvement 059) |
| `uvc.c` | UVC 1.1 cameras (USB video class) and the test source (D277) |
| `uvc_nucleo.c` | the pure part of the UVC 1.1 driver (D277) |
| `uvc_nucleo.h` | the pure part of the UVC 1.1 driver (USB video class, D277) |
| `xhci.c` | driver of the USB 3.0 controller (xHCI) (D155, D214) |


### 6.8 kernel/bluetooth/ (classic Bluetooth stack)

| File | What it does |
| --- | --- |
| `bt.h` | SaguiOS classic Bluetooth stack (BR/EDR) (D279, improvement 059, v8.2) |
| `bt_fw.c` | firmware loading for the Broadcom, Realtek and Atheros Bluetooth controllers (D411) |
| `bt_fw.h` | firmware loading and patches for the Bluetooth controllers that do not start ready (D411) |
| `bt_gestor.c` | the Bluetooth manager (D279): device table, connection and pairing flow, saved link keys and the operations that use the SIS_BLUETOOTH call and the terminal |
| `bt_hci.c` | HCI layer of the Bluetooth stack (D279): commands, events, ACL traffic with credits, controller start and the adapter register |
| `bt_hid.c` | HIDP (D279): the Bluetooth HID profile over two L2CAP channels, control (PSM 0x11) and interrupt (PSM 0x13) |
| `bt_l2cap.c` | basic L2CAP (D279): signalling over channel 1, dynamic channels in basic mode, reassembly of fragmented ACL frames |
| `bt_sdp.c` | SDP client (D279): finds the HID service of a device with a single ServiceSearchAttributeRequest and reads from the reply the PSMs of the channels, the subclass, whether it is a boot device and the report descriptor |


### 6.9 kernel/gui/ (compositor, windows, desktop, taskbar, menus, styles)

| File | What it does |
| --- | --- |
| `acel2d.c` | 2D by the GPU for the desktop: the common layer (D195) |
| `aparencia.c` | highlight colour and background chosen in the Settings (D112) |
| `area_trabalho.c` | the background, the mascot, the icons and the desktop menu (H20, H28, D051, D066) |
| `arte.c` | the desktop drawings, in pixel art made in this project |
| `atalhos.c` | desktop keyboard shortcuts (v6.0 beta, D119) |
| `avisos.c` | pop-up notifications with the full text (issue #16, D269) |
| `barra.c` | the taskbar: Start button, task buttons, tray and clock (D065, D361) |
| `desenho.c` | drawing routines with clipping, used by the desktop and by the window manager |
| `escala.c` | enlarged or reduced copy of pixels (full screen, D044) |
| `gpu3d.c` | 3D by the GPU: the common layer (D285) |
| `gpu3d_mem.c` | common parts of the 3D drivers: GPU memory by blocks, buffers and floating point with integers (D285) |
| `gpu_energia.c` | GPU power: the common layer (D399) |
| `idioma.c` | language of the kernel interface and SIS_IDIOMA (D313, improvement 118) |
| `inicio.c` | the column Start menus: "Two columns" and "Modern" (D361) |
| `janelas.c` | the window manager (H21) |
| `menu.c` | context menu of the right button (D051) and submenus (D064) |
| `ocio.c` | power and screen: screensaver, lock and screen switch-off on inactivity, battery notice |
| `permisos_dialogo.c` | the administrator permissions dialogue (D269, improvement 102, issue #16) |
| `sessao.c` | graphical login, logout and user switching (D340, improvement 098, phase 3) |


### 6.10 kernel/lib/ (pure functions of the kernel)

| File | What it does |
| --- | --- |
| `acpi_pss.c` | minimal reader of _PSS, _PSD and _PCT without an AML interpreter (D177) |
| `bateria_conv.c` | battery percentage and remaining time (D192) |
| `brilho_conv.c` | brightness levels of laptops (D403) |
| `caminho.c` | path resolution |
| `catalogo.c` | translation catalogue reader (D313) |
| `crc32c.c` | CRC32c (polynomial 0x1EDC6F41, in reflected order 0x82F63B78) and CRC16 (polynomial 0x8005, reflected 0xA001) |
| `div64.c` | the 64-bit routines that GCC calls in 32-bit mode (D192) |
| `formatar.c` | text formatting in the style of printf, written for SaguiOS |
| `frequencia_pol.c` | policies and temperature limit of the frequency driver (D106, improvement 061) |
| `fuso.c` | interpreter of POSIX TZ strings and calendar (D080) |
| `hid_informe.c` | analyser of HID report descriptors and decoding of gamepads (D307) |
| `inflar.c` | DEFLATE decompression (RFC 1951) in the core (v8.5.4.2, D379: the compressed root of the CD) |
| `ocio_efectos.c` | the screensaver effects (D422) |
| `p256.c` | the NIST P-256 curve for the WPA3 SAE (D220) |
| `portajogos_conv.c` | the counts of the game port (D307) |
| `rapido.c` | large copies and fills of the graphical interface (D042) |
| `sae.c` | WPA3-Personal SAE with P-256 (D220) |
| `sensores_conv.c` | hardware monitor formulas (D092) |
| `sha256.c` | SHA-256 written from FIPS 180-4 (sections 4.1.2, 4.2.2, 5.3.3 and 6.2), D060 |
| `sse1.c` | copies and fills with SSE only, without SSE2 (D208) |
| `sse2.c` | copies and fills with SSE2 (D042) |
| `sse3.c` | copies with SSE3: lddqu for unaligned reads (D208) |
| `ssse3.c` | reordering pixel channels with pshufb (D208) |
| `texto.c` | memory and text routines of the kernel |
| `wifi_n.c` | performance of the Wi-Fi of the RTL8188EU dongle (D238) |
| `wmi_wdg.c` | parsing of _WDG and HP buffers, without hardware (D405) |
| `wpa.c` | cryptography and formats of the client Wi-Fi (D215) |
| `wpa_cifra.c` | Wi-Fi primitives for WEP, TKIP, descriptor 1 of EAPOL-Key, the AKMs with SHA-256 (SAE, PSK-SHA256), software CCMP and BIP (D220, v7.3.2) |


### 6.11 kernel/inc/ (shared headers)

| File | What it does |
| --- | --- |
| `abi.h` | contract between the kernel and the programs: system call numbers and common structures |
| `acel2d.h` | 2D by the GPU for the desktop: copying and filling rectangles (D195) |
| `acel_intel.h` | the command ring of the Intel GPUs, shared by the 2D blitter (acel_intel.c, D195) and the Gen3 3D (gpu3d_intel.c, D285) |
| `acoes.h` | common system actions: volume, mute, brightness, radio (D403) |
| `acpi.h` | ACPI shutdown (S5) and the power button (D113) |
| `acpi_pss.h` | minimal reader of _PSS, _PSD and _PCT of the ACPI tables (D177) |
| `ahci.h` | AHCI SATA controller (D155) |
| `aleatorio.h` | random number generator of the kernel (D061) |
| `aparencia.h` | desktop highlight colour (D112) |
| `ar9485.h` | integrated Atheros AR9485 Wi-Fi (PCIe 168c:0032, AR9003 family, no firmware), D280 |
| `arte.h` | pixel-art drawings (letters = colours of the palette) |
| `ata.h` | internal ATA/IDE disks over PIO (D040) and SATA/AHCI (D155) |
| `atapi.h` | CD/DVD readers (ATAPI), read-only (D207) |
| `atomicas.h` | atomic primitives of the kernel (D150, the basis of SMP) |
| `audio.h` | sound cards (AC'97 and Intel HDA) and the kernel mixer (D088-D090) |
| `autoteste.h` | internal kernel tests, enabled only in the test images |
| `avisos.h` | pop-up notifications (H34, D125, D233; issue #16, D269) |
| `bateria.h` | uACPI, the embedded controller and the battery, seen from the rest of the kernel (D192) |
| `bateria_conv.h` | from the numbers of _BIF/_BIX/_BST to percentage and time (D192) |
| `bga.h` | video card of Bochs/QEMU/VirtualBox ("dispi"): mode change without the BIOS (D059) |
| `bloqueo.h` | advertising and tracking blocking by DNS in the kernel resolver, in the style of a DNS sinkhole (D218) |
| `bloqueo_tabla.h` | the format of the DNS advertising blocking table and the lookup in it (D218) |
| `bluetooth.h` | Bluetooth with a standard USB controller (D279, improvement 059, v8.2) |
| `bootinfo.h` | what stage 2 hands over to the kernel (mirror of boot/bootinfo.inc) |
| `brilho_conv.h` | from the brightness levels the firmware gives to indices and percentages (D403) |
| `cache_disco.h` | block cache common to the internal disks (D281) |
| `caminho.h` | assembling and simplifying paths ("/", ".", "..") |
| `catalogo.h` | translation catalogues (D313, improvement 118) |
| `cdda.h` | the tracks of an audio CD as WAV files, read-only (D233) |
| `compressor.h` | fast compression of a 4 KB page for the swap (D352) |
| `conf_sistema.h` | the SAGUI-CONF partition: persistent system configuration (D255) |
| `console.h` | kernel text output (screen + mirror on the serial port) |
| `copia_escrita.h` | reference count of the physical pages shared by several address spaces (copy on write of fork, D401) |
| `cpu.h` | segments, interrupts and the frame saved on the stack at each interrupt |
| `cpu_dados.h` | per-CPU data: the basis of SMP (D150) and its real use (D157) |
| `crc32c.h` | CRC32c (Castagnoli) and CRC16 (the one of the group sums of ext4 without metadata_csum) |
| `desenho.h` | drawing with clipping on a pixel surface (phase 5) |
| `disco.h` | disks and partitions as seen by the file system (phase 6, D040) |
| `disco_comprimido.h` | the CD root compressed by blocks (v8.5.4.2, D379 P5) |
| `disco_ram.h` | "ram0": the copy in RAM of a volume (D379, toram) |
| `eist_antiguo.h` | Enhanced SpeedStep of old Intel processors (D177) |
| `eq_fixo.h` | the equaliser filter in fixed point (D201) |
| `esquemas.h` | appearance schemes of the whole system (v8.5.5.2, D428) |
| `evergreen_3d.h` | PM4 packets, shaders and start values of the 3D engine of the Evergreen Radeons (HD 5000: Cedar, Redwood, Juniper, Cypress, Hemlock) and Northern Islands without Cayman (HD 6000 and HD 7500M/7600M: Barts, Turks, Caicos) (D427) |
| `evergreen_dma.h` | what is specific to the Evergreen (HD 5000) and Northern Islands (HD 6000) Radeons for the 2D through the DMA engine (D286): the exact list of models, the copy packet and the check of the display controller |
| `exfat.h` | reading and writing exFAT (D325) |
| `fat32.h` | reading and writing FAT32 (H32, H33) |
| `fontes_sistema.h` | system fonts: table of typefaces built into the kernel |
| `fpu.h` | FPU x87, MMX and SSE for the programs (state saved per process) |
| `frequencia.h` | CPU frequency driver (D106, improvement 061) |
| `frequencia_pol.h` | the decision of the frequency driver (D106, improvement 061) |
| `fuso.h` | time zones with POSIX TZ rules (D080) |
| `g3d_v2_provas.h` | tests of the gpu3d v2 capabilities of a driver (D426) |
| `gcn_dma.h` | what is specific to the GCN Radeons for the 2D by the GPU (D360): the exact list of models (GCN 1.0 to 5), the DMA engine packets of the Southern Islands (GCN 1.0) and the check of the display controller |
| `gpu3d.h` | 3D by the GPU: the common layer and what its drivers share (D285) |
| `gpu3d_diag.h` | diagnosis of the 3D common to all the drivers (D390; it generalises what D386 did for the R5xx of the Amilo) |
| `gpu_energia.h` | GPU power: clocks, states, temperature and VRAM (D399) |
| `gui.h` | desktop and window manager (phase 5) |
| `hda_ajustes.h` | settings ("quirks") of HDA codecs by codec and subsystem (D107) |
| `hid_informe.h` | analyser of HID report descriptors and decoding of gamepads (D307) |
| `i8042.h` | keyboard and touchpad controller |
| `idioma.h` | the language of the kernel interface (D313, improvement 118) |
| `imagens.h` | read-only pages of the shared libraries among processes (D378, phase 3; docs/enlace-dinamico.md §4 and §15) |
| `inflar.h` | DEFLATE decompression and CRC-32 in the core (v8.5.4.2, D379) |
| `info_hardware.h` | hardware data for infopc (D041) |
| `ioapic.h` | the IOAPIC: the interrupt controller of the devices that replaces the pair of 8259 (D166) |
| `irq_rotas.h` | which path each IRQ of the PCI devices follows and the self-test that checks it (D211) |
| `isa.h` | the ISA bus: the 8237 DMA controller, the low memory for it and the list of detected ISA devices (D302) |
| `iso9660.h` | file system of the CD/DVD (ISO 9660), read-only (D207) |
| `jbd2.h` | the JBD2 journal of ext3/ext4 (D326): replaying what is pending when mounting and writing transactions |
| `kconf.h` | kernel options that are changed from SaguiOS and take effect on the next boot (D353) |
| `lapic.h` | the local APIC (LAPIC) of each processor (D157) |
| `mandos.h` | the common layer of gamepads and joysticks (D307): USB HID, wired Xbox 360 and the game port |
| `memalta.h` | temporary kernel windows to physical pages (D348) |
| `memoria.h` | physical pages and the kernel allocator |
| `modos_video.h` | list of modes, hot change and preferred resolution (D058, D059) |
| `montagem.h` | mount table and the interface of the file systems (D040) |
| `mouse.h` | PS/2 mouse/touchpad (H19) |
| `msr.h` | protected MSR reading (D092) |
| `nvme.h` | NVMe controller (D265) |
| `ocio.h` | power and screen: screensaver, lock and screen switch-off on inactivity, battery notice and «do not disturb» (D422) |
| `ocio_efectos.h` | the screensaver effects (D422) |
| `p256.h` | the NIST P-256 elliptic curve (FIPS 186-4, D.1.2.3; SEC 1) for the WPA3 SAE (D220, v7.3.2) |
| `paginacao.h` | address spaces (page directories) in classic or PAE mode |
| `painel.h` | PS/2 touch panels: Synaptics, ALPS and Elantech in absolute mode, with gestures (D407) |
| `pci.h` | access to the PCI configuration space (mechanism 1, ports 0xCF8/0xCFC) |
| `pci_realoc.h` | moving below 4 GB a 64-bit BAR that the firmware left high up (D283) |
| `permisos.h` | who can do what: users and privileged calls (D255, improvement 098, phase 1) |
| `permisos_dialogo.h` | administrator permissions dialogue (D269, improvement 102) |
| `pipe.h` | pipes between processes (H15) |
| `planificador.h` | the process schedulers between the CPUs (D174) |
| `portajogos_conv.h` | the counts of the game port (D307), without touching the hardware |
| `portas.h` | access to the x86 I/O ports (in/out instructions) |
| `portatil.h` | what is specific to laptops as seen from the rest of the kernel (D403) |
| `prazo.h` | waits with a real-time limit (D199, D207) |
| `processo.h` | processes and the scheduler |
| `prt.h` | the routing of the PCI interrupts of the firmware (_PRT) with uACPI (D211) |
| `r600_3d.h` | PM4 packets, shaders and start values of the 3D engine of the R6xx/R7xx Radeons (D284) |
| `r600_base.h` | what the 2D by DMA (acel_r600.c, D223) and the 3D by the CP (gpu3d_r600.c, D284) of the R6xx/R7xx Radeons share: the read-only probe of the card (exact model, BAR, MMIO and where the screen is in the controller) |
| `r600_dma.h` | packets of the DMA engine of the R6xx/R7xx Radeons and the splitting of a rectangle into linear copies (D223) |
| `radios.h` | the radios and the panel of the HP laptops (hp_wmi.c) and the dump of ACPI tables, seen from the system calls (D405) |
| `raiz.h` | the root of the system on a disk (D379) |
| `ramfs.h` | a folder of the tree in RAM mounted as a volume (D379) |
| `rapido.h` | large copies and fills of the graphical interface (D042) |
| `rastro.h` | debugging marks that survive a warm reboot (Amilo, dbg5, 2026-11-19) |
| `rede.h` | network cards, IPv4 stack (ARP, IP, ICMP, UDP, DHCP, DNS, TCP) and sockets |
| `relogio.h` | time since boot (PIT) and date/time (CMOS) |
| `sae.h` | SAE (Simultaneous Authentication of Equals) of WPA3-Personal with group 19 (P-256), IEEE 802.11-2020 12.4 (D220, v7.3.2) |
| `sb16.h` | the Sound Blaster 16 (ISA) and the ISA PnP that the Vibra/AWE PnP need (D302) |
| `sensores.h` | what the kernel asks of the hardware monitor (D092) without going through the syscall |
| `sensores_conv.h` | the formulas of the hardware monitor (D092) |
| `sessao.h` | graphical login, logout and user switching (D340, improvement 098, phase 3) |
| `sgfs.h` | reader of the SGFS file system (the format is generated by tools/mksgfs.py) |
| `sha256.h` | SHA-256 (FIPS 180-4), D060 |
| `sistema.h` | system calls through "int 0x80" (D007) |
| `smp.h` | start of the secondary processors (D157) |
| `sobreposicao.h` | the root of the live: read-only ext2 with a writable layer (D379) |
| `som.h` | the PC speaker (bonus B3) |
| `spinlock.h` | busy-wait locks of the kernel (D150) |
| `sse2.h` | copies and fills with SSE2 (D042) |
| `supervisor.h` | the service manager seen from the kernel (D216) |
| `teclado.h` | PS/2 keyboard with ES (default) and ABNT2 layouts |
| `telas.h` | several screens (video outputs) and extended or mirrored desktop (improvement 060, D290) |
| `terminal_es.h` | the terminal as a file (input 0 and output 1/2 of the processes) |
| `texto.h` | memory and text functions written for the kernel |
| `tipos.h` | basic kernel types. No libc: they are defined by the project itself |
| `trava.h` | kernel mutual exclusion lock (D095) |
| `trava_geral.h` | the general kernel lock (D165) |
| `troca.h` | compressed swap in RAM (D352, improvement 125 phase 3b, improvement 069 phase 3) |
| `tunel.h` | the wg0 tunnel interface (kernel/rede/tunel.c, D363) |
| `tv.h` | virtual terminal (pseudo-terminal): a terminal for the Window terminal (D036) |
| `uacpi_sagui.h` | what the uACPI layer (uacpi_sagui.c), the embedded controller (ec.c) and the battery (bateria.c) share |
| `uacpi_types.h` | the uACPI types on top of the kernel ones (D192) |
| `udf.h` | UDF file system of the CD/DVD, read-only (D233) |
| `usb.h` | EHCI and xHCI controllers, USB devices, hubs, storage and HID (phase 6; xHCI and HID in v7.3, D214) |
| `versao.h` | system name and version, in one place (D101) |
| `vfs.h` | virtual file layer: one single path for everything (PRD 4.3) |
| `video.h` | VESA graphic mode with double buffer (H17) |
| `wc.h` | write-combining in the framebuffer (D057) |
| `wifi.h` | Wi-Fi client with the Realtek RTL8188EU USB dongle (D215) |
| `wifi_n.h` | performance of the Wi-Fi of the RTL8188EU dongle (D238): the breakdown of a receive transfer with several frames (chip aggregation), the 802.11n (HT) and WMM elements, the reply to an ADDBA and the rate ladder |
| `wifi_pcie.h` | more integrated PCIe Wi-Fi of laptops (D410): PCI recognition of the Atheros ath9k family (AR9285, AR9287, AR9380...), Broadcom BCM4313 and Ralink RT3090 (experimental: it only identifies and logs) |
| `wmi_wdg.h` | the WMI bridge of ACPI (PNP0C14) without hardware or uACPI: the parsing of _WDG and the formats of the HP buffers (D405) |
| `wpa.h` | cryptography and formats of the client Wi-Fi (D215): SHA-1, HMAC-SHA1, PBKDF2 (the PMK), the PRF of 802.11i, AES-128 with its key wrap |
| `wpa_cifra.h` | the Wi-Fi cryptographic primitives that are not those of WPA2-CCMP (D220, v7.3.2): MD5 and HMAC-MD5 (descriptor 1 of EAPOL-Key), RC4, CRC-32, Michael and the TKIP key mixing, the TKIP and WEP encapsulation |
| `xhci.h` | USB 3.0 controller (xHCI) (D155, D214) |


### 6.12 kernel/terceros/ (code from other projects: uACPI, BSD, Linux, MIT)

| File | What it does |
| --- | --- |
| `bsd/alc/alc_regs.h` | registers and descriptors of the Atheros AR813x/AR815x Ethernet (from FreeBSD) |
| `bsd/athn/ar9003reg.h` | registers of the Atheros AR9003 Wi-Fi family (from BSD) |
| `bsd/athn/ar9485_datos.h` | start tables of the Atheros AR9485 (from BSD) |
| `bsd/athn/athnreg.h` | common registers of the Atheros AR9003 (from BSD) |
| `bsd/coretemp/coretemp_tjmax.h` | TjMax per Intel CPU model (from FreeBSD coretemp) |
| `bsd/rtwn/r92creg_88e.h` | registers and start tables of the Realtek RTL8188E (from OpenBSD) |
| `bsd/rtwn/r92creg_92c.h` | registers and start tables of the Realtek RTL8188CU/8192CU (from OpenBSD) |
| `bsd/run/rt2860reg.h` | registers and descriptors of the Ralink RT2870/RT3070/RT5370 (from OpenBSD) |
| `linux/hda/idt_92hd71bxx.h` | data of the IDT/SigmaTel 92HD71Bxx codec (from Linux) |
| `linux/hwmon/sensores_tabelas.h` | Tctl/Tdie offset table of the Ryzen (from Linux k10temp) |
| `linux/r8169/r8169_inicio.h` | start-up sequences of the Realtek RTL8168/8111 (from Linux) |
| `linux/r8169/r8169_versoes.h` | versions and chip names of the Realtek RTL8168/8169 (from Linux) |
| `mit/atom/atom-bits.h` | bits and constants of the interpreter of the ATOM BIOS of the Radeons (MIT) |
| `mit/atom/atom.c` | interpreter of the command tables of the ATOM BIOS of the Radeons (MIT) |
| `mit/atom/atom.h` | interface of the ATOM interpreter of the Radeons (MIT) |
| `mit/atom/atom_compat.h` | compatibility of the ATOM interpreter with the kernel (MIT) |
| `mit/nv/nv_2d.h` | 2D methods and PGRAPH/PFIFO writes of the NVIDIA GeForce 6/7 (from xf86-video-nv) |
| `uacpi/include/uacpi/acpi.h` | main header of the ACPI structures |
| `uacpi/include/uacpi/context.h` | global context of uACPI |
| `uacpi/include/uacpi/event.h` | public API of ACPI events (GPE and fixed events) |
| `uacpi/include/uacpi/helpers.h` | auxiliary functions of uACPI |
| `uacpi/include/uacpi/internal/compiler.h` | compiler macros for uACPI (internal use) |
| `uacpi/include/uacpi/internal/context.h` | local copy of the FADT, already checked and converted |
| `uacpi/include/uacpi/internal/dynamic_array.h` | dynamic arrays of uACPI (internal use) |
| `uacpi/include/uacpi/internal/event.h` | ACPI events: internal interface (GPE and fixed events) |
| `uacpi/include/uacpi/internal/helpers.h` | internal auxiliary functions of uACPI |
| `uacpi/include/uacpi/internal/interpreter.h` | AML interpreter: internal interface |
| `uacpi/include/uacpi/internal/io.h` | writing of AML fields (internal use) |
| `uacpi/include/uacpi/internal/log.h` | uACPI debugging messages |
| `uacpi/include/uacpi/internal/mutex.h` | uACPI mutex over the kernel API |
| `uacpi/include/uacpi/internal/namespace.h` | ACPI namespace: internal interface |
| `uacpi/include/uacpi/internal/notify.h` | ACPI notifications: internal interface |
| `uacpi/include/uacpi/internal/opcodes.h` | AML operation codes that the interpreter understands |
| `uacpi/include/uacpi/internal/opregion.h` | operation regions (hardware access from AML): internal interface |
| `uacpi/include/uacpi/internal/osi.h` | _OSI support: internal interface |
| `uacpi/include/uacpi/internal/registers.h` | ACPI hardware registers: internal interface |
| `uacpi/include/uacpi/internal/resources.h` | ACPI resources (IRQ, I/O, memory): conversion from AML |
| `uacpi/include/uacpi/internal/shareable.h` | shared objects of the interpreter (internal use) |
| `uacpi/include/uacpi/internal/stdlib.h` | minimal libc functions used by uACPI |
| `uacpi/include/uacpi/internal/tables.h` | ACPI tables: internal interface |
| `uacpi/include/uacpi/internal/types.h` | common AML interpreter objects |
| `uacpi/include/uacpi/internal/utilities.h` | internal utilities (names, EISA ids) |
| `uacpi/include/uacpi/io.h` | public API for access to ports and memory |
| `uacpi/include/uacpi/kernel_api.h` | API that the SaguiOS kernel implements for uACPI |
| `uacpi/include/uacpi/log.h` | public API of the uACPI messages |
| `uacpi/include/uacpi/namespace.h` | public API of the ACPI namespace |
| `uacpi/include/uacpi/notify.h` | public API of notifications |
| `uacpi/include/uacpi/opregion.h` | public API of operation regions |
| `uacpi/include/uacpi/osi.h` | public API of _OSI |
| `uacpi/include/uacpi/platform/arch_helpers.h` | platform thread identifier (adapts to each system) |
| `uacpi/include/uacpi/platform/atomic.h` | atomic operations for uACPI |
| `uacpi/include/uacpi/platform/compiler.h` | compiler attributes and macros |
| `uacpi/include/uacpi/platform/config.h` | uACPI build options (context, sizes) |
| `uacpi/include/uacpi/platform/libc.h` | libc functions used by uACPI (with default implementation) |
| `uacpi/include/uacpi/platform/types.h` | platform-specific types |
| `uacpi/include/uacpi/registers.h` | public API of the ACPI registers |
| `uacpi/include/uacpi/resources.h` | public API of resources (_CRS, _PRS) |
| `uacpi/include/uacpi/sleep.h` | public API of sleep and shutdown states |
| `uacpi/include/uacpi/status.h` | uACPI status codes |
| `uacpi/include/uacpi/tables.h` | public API of the ACPI tables |
| `uacpi/include/uacpi/types.h` | public uACPI types |
| `uacpi/include/uacpi/uacpi.h` | main uACPI header: initialisation and shutdown |
| `uacpi/include/uacpi/utilities.h` | public API of utilities (evaluate methods, names) |
| `uacpi/source/default_handlers.c` | default handlers of the operation regions (memory, I/O, PCI) |
| `uacpi/source/event.c` | ACPI events: GPE (general-purpose events) and fixed events |
| `uacpi/source/interpreter.c` | AML interpreter: the virtual machine that runs the ACPI methods |
| `uacpi/source/io.c` | reading and writing of AML fields over the hardware |
| `uacpi/source/mutex.c` | interpreter mutex over the kernel API |
| `uacpi/source/namespace.c` | ACPI namespace: object tree (the root `\`, `_SB`, etc.) |
| `uacpi/source/notify.c` | notifications (Notify) to the devices |
| `uacpi/source/opcodes.c` | AML code table and its parsing |
| `uacpi/source/opregion.c` | operation regions: hardware access from the AML methods |
| `uacpi/source/osi.c` | response to _OSI (which system the kernel says it is) |
| `uacpi/source/registers.c` | reading and writing of the ACPI hardware registers |
| `uacpi/source/resources.c` | ACPI resources (_CRS, _PRS): IRQ, I/O and memory |
| `uacpi/source/sleep.c` | sleep and shutdown states (S5) |
| `uacpi/source/stdlib.c` | minimal libc functions used by uACPI |
| `uacpi/source/tables.c` | ACPI tables: lookup and installation (DSDT, SSDT, FADT...) |
| `uacpi/source/types.c` | interpreter objects: integers, buffers, strings and packages |
| `uacpi/source/uacpi.c` | initialisation, table loading and shutdown of uACPI |
| `uacpi/source/utilities.c` | utilities: ACPI names, EISA ids and method evaluation |


---

## 7. Where to read more

| Topic | Document |
| --- | --- |
| Architecture by layers and what is in each one | [`docs/arquitecturas.md`](arquitecturas.md) (processors) and the [README](../README_EN.md) |
| Storage: root on disk, live and "everything in RAM" (D379) | [`docs/almacenamiento.md`](almacenamiento.md) |
| Boot with UEFI | [`docs/uefi.md`](uefi.md) |
| Several processors and threads | [`docs/multihilo.md`](multihilo.md) |
| Dynamic linking and shared libraries | [`docs/enlace-dinamico.md`](enlace-dinamico.md) |
| Graphics cards and 3D by GPU | [`docs/gpus.md`](gpus.md), [`docs/gpu3d.md`](gpu3d.md) |
| Third-party code and licences | [`docs/terceros.md`](terceros.md), [`docs/licencia.md`](licencia.md) |
| All the design decisions (D001 to D4xx) | [`docs/decisoes.md`](decisoes.md) |
| How it is compiled and tested | [`docs/make.md`](make.md) and the README |

---

> *This guide was generated from the code on 2026-11-21; if something does not match, trust the code.*
