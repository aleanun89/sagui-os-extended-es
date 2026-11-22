# Guía del kernel de SaguiOS-Extended

[Español](guia-kernel.md) · [Português](guia-kernel_PT.md) · [English](guia-kernel_EN.md)

> Para quien quiere aprender cómo está hecho un sistema operativo, leyendo código real.
> Esta guía explica el **kernel** (el núcleo): qué hace cada parte, cómo encaja con las demás
> y en qué fichero está. Las aplicaciones están en la [guía de aplicaciones](guia-aplicaciones.md).

**Índice**

1. [Qué es SaguiOS](#1-qué-es-saguios)
2. [Cómo arranca](#2-cómo-arranca)
3. [Mapa del kernel: las carpetas](#3-mapa-del-kernel-las-carpetas)
4. [Los bloques, uno por uno](#4-los-bloques-uno-por-uno)
5. [Cómo se hace una llamada al sistema, paso a paso](#5-cómo-se-hace-una-llamada-al-sistema-paso-a-paso)
6. [Tablas de ficheros](#6-tablas-de-ficheros)
7. [Dónde leer más](#7-dónde-leer-más)

---

## 1. Qué es SaguiOS

**SaguiOS-Extended** es un sistema operativo **educativo**, escrito en español y pensado para
aprender. Sus rasgos principales:

| Rasgo | Qué significa |
| --- | --- |
| **i686, 32 bits** | Está pensado para ordenadores antiguos de 32 bits (procesadores i686 y compatibles). Por eso el código usa punteros de 32 bits y ensamblador i386. Los modos de 64 bits no se usan. |
| **Propio** | El cargador de arranque, el kernel, la biblioteca C (la *libc*) y la biblioteca gráfica (la *libsagui*) se escribieron en este proyecto. No usa GRUB. Algún driver viene de otros proyectos (Linux, FreeBSD, uACPI): está aislado y marcado en `kernel/terceros/`. |
| **GPL v3** | Es software libre. El código que viene de otros proyectos está marcado y registrado en [`docs/terceros.md`](terceros.md). |

> **Qué es un kernel.** Es el programa que se ejecuta siempre, por debajo de todos los demás.
> Reparte la CPU entre los programas, les da memoria, maneja los discos y la red, y les impide
> pisarse entre ellos. Los programas (un editor, un navegador) le piden cosas mediante
> **llamadas al sistema**. Ver el apartado 5.

El kernel de SaguiOS está escrito en **C** (con un poco de **ensamblador** para lo que C no
puede hacer, como cargar la tabla de interrupciones). Se compila sin la biblioteca estándar del
PC (*freestanding*): no hay `printf` ni `malloc` que venga de fuera, todo está en `kernel/`.

---

## 2. Cómo arranca

Cuando se enciende el PC, nada de SaguiOS está en marcha. El arranque es una **cadena**: cada
etapa carga la siguiente.

```
 Encendido
    │
    ▼
 BIOS (o UEFI)              el firmware del PC; lee el primer sector del disco
    │
    ▼
 Etapa 1: MBR               boot/mbr.asm · 512 bytes; busca la cabecera (LBA 1)
    │
    ▼
 Etapa 2                    boot/estagio2.asm · pasa a modo protegido de 32 bits,
    │                       A20, mapa de memoria (E820), copia todo por encima de 1 MB
    ▼
 Kernel                     kernel/nucleo/principal.c · enciende las partes en orden
    │
    ▼
 initramfs (ramdisk)       el mínimo imprescindible en RAM: el formato SGFS (kernel/fs/sgfs.c)
    │
    ▼
 Raíz SAGUI-RAIZ            el sistema de archivos real en un disco (decisión D379)
    │
    ▼
 Sesión gráfica             inicio de sesión, escritorio y barra de tareas (kernel/gui/)
```

### 2.1 Qué es cada paso

- **BIOS o UEFI.** El firmware del ordenador. Copia el primer sector del disco a la memoria
  (`0x7C00`) y salta allí. Con UEFI, SaguiOS usa un cargador propio (`boot/uefi/cargador.c`,
  decisión D264) que hace con los servicios del firmware lo mismo que la etapa 2 hace con la BIOS.
  Ver [`docs/uefi.md`](uefi.md).
- **Etapa 1 (MBR).** Un sector de 512 bytes. Es tan pequeño que solo sabe leer la **cabecera de
  arranque** (LBA 1 de la imagen), que dice dónde está cada parte. Ver `boot/cabecalho.inc`.
- **Etapa 2.** Es la parte grande del cargador. Como cuando se enciende el PC el procesador está
  en *modo real* de 16 bits, la etapa 2 hace todo lo que solo la BIOS sabe hacer: activa la línea
  A20 (para poder usar más de 1 MB de memoria), pide el **mapa de memoria** (E820), copia el
  kernel por encima de 1 MB y, por último, pasa al **modo protegido** de 32 bits. Deja toda la
  información en una estructura (`boot/bootinfo.inc`) que el kernel recibe.
- **Kernel.** `kernel/nucleo/principal.c` es el punto de entrada en C. Comprueba que el cargador
  hizo su trabajo (la firma y la estructura de arranque), y enciende las piezas en orden:
  interrupciones y reloj, memoria física, teclado, sistema de archivos y, por último, la terminal.
- **initramfs (en SaguiOS, *ramdisk*).** Un pequeño sistema de archivos que se carga en la RAM
  junto con el kernel, con lo imprescindible para montar el disco. En SaguiOS está en el formato
  **SGFS** (lo lee `kernel/fs/sgfs.c`). Ver [`docs/almacenamiento.md`](almacenamiento.md).
- **Raíz SAGUI-RAIZ.** La decisión **D379** separa «lo que hace falta para arrancar» de «el
  sistema». El sistema vive en una partición ext2 llamada `SAGUI-RAIZ`, que se monta como `/`.
  Así se puede arrancar desde un pendrive en modo *live*, instalado en el disco o con todo en RAM.
- **Sesión gráfica.** `kernel/gui/sessao.c` muestra el inicio de sesión, y después el escritorio
  (`kernel/gui/area_trabalho.c`) con su barra de tareas (`kernel/gui/barra.c`).

> **Para probar el arranque:** `make` genera `build/saguios.img` y `./run_in_qemu.sh` lo arranca en
> una máquina virtual. Ver el README, sección «Compilar y probar».

---

## 3. Mapa del kernel: las carpetas

El kernel está en la carpeta `kernel/`. Cada subcarpeta es un **bloque** con una responsabilidad.

| Carpeta | Para qué sirve | Ficheros que más se leen primero |
| --- | --- | --- |
| `kernel/arch/` | Lo que depende del procesador: tablas GDT e IDT, interrupciones, APIC, SMP | `idt.c`, `gdt.c`, `pic.c`, `lapic.c`, `ioapic.c` |
| `kernel/mem/` | Memoria: páginas físicas, paginación, PAE, swap comprimida, copia al escribir | `fisica.c`, `paginacao.c`, `heap.c`, `copia_escrita.c`, `troca.c` |
| `kernel/nucleo/` | Procesos, hilos, planificador, señales, llamadas al sistema, usuarios y permisos | `processos.c`, `sistema.c`, `bifurcar.c`, `carregador.c`, `principal.c` |
| `kernel/fs/` | Sistema de archivos: VFS, ext2/3/4, FAT32, exFAT, ISO 9660, UDF, caché, raíz | `vfs.c`, `arquivos.c`, `ext2.c`, `cache_disco.c`, `raiz.c` |
| `kernel/dispositivos/` | Drivers: discos, sonido, vídeo, 2D y 3D por GPU, ACPI, portátiles, sensores | `ahci.c`, `hda.c`, `video.c`, `gpu3d.c`, `uacpi_sagui.c` |
| `kernel/rede/` | Pila TCP/IP, drivers de tarjetas de red, Wi-Fi | `rede.c`, `tcp.c`, `socket.c`, `wifi.c` |
| `kernel/usb/` | Controladores USB (EHCI, OHCI, UHCI, xHCI), dispositivos y clases | `usb.c`, `xhci.c`, `hid.c` |
| `kernel/bluetooth/` | Pila Bluetooth clásica (HCI, L2CAP, SDP, HID) | `bt_hci.c`, `bt_l2cap.c`, `bt_gestor.c` |
| `kernel/gui/` | Compositor, ventanas, escritorio, barra de tareas, menús, estilos | `janelas.c`, `barra.c`, `inicio.c`, `sessao.c` |
| `kernel/lib/` | Funciones puras del kernel (sin tocar hardware): formatos, criptografía, copias rápidas | `formatar.c`, `sha256.c`, `rapido.c`, `wpa.c` |
| `kernel/inc/` | Las **cabeceras** (`.h`) que comparten las partes del kernel y, en parte, los programas | `abi.h`, `processo.h`, `vfs.h` |
| `kernel/terceros/` | Código **de otros proyectos** (uACPI, FreeBSD, Linux, OpenBSD, X.org): lo que no es de este proyecto | `uacpi/` |

> **Cómo se relacionan.** Las carpetas no son islas. Un programa pide un archivo (`nucleo`), el
> sistema de archivos (`fs`) busca el bloque en el disco (`dispositivos`), que lo pide al
> controlador (`dispositivos/ahci.c`), que habla con el hardware usando el mapa de memoria y las
> interrupciones (`arch`, `mem`). Ese recorrido se ve bien en el apartado 5.

---

## 4. Los bloques, uno por uno

### 4.1 `kernel/arch/`: lo que depende del procesador

- **GDT** (`gdt.c`): la tabla de **segmentos**. Define qué código y qué datos son del kernel
  (anillo 0) y cuáles de los programas (anillo 3). Tiene también la **TSS**, la estructura que
  guarda la pila del kernel para cada CPU (decisión D157).
- **IDT** (`idt.c`): la tabla de **interrupciones**. Dice qué función se ejecuta para cada número
  de interrupción: fallos del procesador, el reloj, el teclado, los discos y la puerta `0x80` de
  las llamadas al sistema. `tratar_interrupcao()` es el despachador general.
- **Interrupciones de hardware** (`pic.c`, `ioapic.c`, `lapic.c`, `irq_rotas.c`): el 8259
  (controlador antiguo), el **APIC local** de cada CPU y el **IOAPIC**, que reparte las líneas
  de los dispositivos (decisión D166). `irq_rotas.c` decide por qué línea llega cada IRQ de PCI
  (con la tabla `_PRT` de ACPI, decisión D211).
- **Pantalla de error** (`excecoes.c`): cuando un programa falla de forma grave, se muestra
  el estado del procesador en vez de colgar el sistema.
- **FPU, MMX y SSE** (`fpu.c`): guarda y restaura el estado de la coma flotante de cada proceso.
- **MSR** (`msr.c`): lectura de los registros especiales del procesador que no derriba el kernel
  si el MSR no existe (decisión D092).
- **SMP**: el arranque de los procesadores secundarios está en `kernel/nucleo/smp.c`. Es opcional
  (decisión D164): solo se enciende con la imagen `--smp`. El código del kernel sigue corriendo en la
  CPU 0 (modelo maestro y esclavo); las demás CPU ejecutan procesos y atienden llamadas al sistema
  (decisiones D157 y D165).

### 4.2 `kernel/mem/`: la memoria

- **Memoria física** (`fisica.c`): cuántas páginas de 4 KB están libres. Lleva un **mapa de bits**
  (un bit por página: 1 = ocupada).
- **Asignador del kernel** (`heap.c`): `kmalloc` y `kfree`. Los bloques pequeños salen de clases
  de tamaño; los grandes, de páginas enteras.
- **Paginación** (`paginacao.c`): cada proceso tiene su **espacio de direcciones**, con sus
  directorios de páginas. Hay dos modos: el clásico de 2 niveles y el **PAE** de 3 niveles, que
  permite más de 4 GB de memoria física y el bit **NX** (la página con datos no se puede ejecutar;
  decisiones D010, D049 y D349).
- **Memoria alta** (`memalta.c`): ventanas temporales para escribir en páginas físicas que no están
  mapeadas (decisión D348).
- **Copia al escribir** (`copia_escrita.c`): cuando `fork` duplica un proceso, las páginas se
  comparten y solo se copian cuando alguien escribe en ellas (decisión D401).
- **Swap comprimida en RAM** (`troca.c`, `compressor.c`): cuando falta memoria, en vez de escribir
  en el disco se comprime la página en la propia RAM (decisión D352).
- **Write-combining** (`wc.c`): el framebuffer de vídeo se escribe por bloques, más rápido.
- **Páginas compartidas de las bibliotecas** (`imagens.c`): el código de una biblioteca
  compartida vive en una sola copia (decisión D378, [`docs/enlace-dinamico.md`](enlace-dinamico.md)).

### 4.3 `kernel/nucleo/`: procesos, hilos y llamadas al sistema

- **Procesos** (`processos.c`): la tabla de procesos y el **planificador** *round-robin*
  apropiativo: cada proceso recibe un turno de tiempo y el reloj le quita la CPU al terminarlo.
- **Planificadores entre CPU** (`planificadores.c`, `afinidad.c`): en qué procesador corre cada
  proceso (decisión D174).
- **Hilos** (`hilos.c`): hilos dentro de un proceso, con espera por dirección (*futex*, D167).
- **Cerrojos** (`trava.c`, `trava_geral.c`, `spinlock.c`, `atomicas.c`): la exclusión mutua.
  Con SMP, el kernel tiene un cerrojo general (D165) y primitivas atómicas (D150).
- **Carga de programas** (`carregador.c`): lee un ejecutable **ELF32** y crea el proceso con su
  código, sus datos y su pila.
- **`fork` y `exec`** (`bifurcar.c`): `fork` crea una copia del proceso (con copia al escribir) y
  `exec` lo sustituye por otro programa. Es el modelo de Unix; el shell los usa para cada orden.
- **Señales** (`sinais.c`): avisos entre procesos (decisión D401).
- **Llamadas al sistema** (`sistema.c`): la puerta de entrada `int 0x80`. Ver el apartado 5.
- **Usuarios y permisos** (`usuarios.c`, `permisos.c`): quién es cada proceso (su uid y gid) y qué
  llamadas puede hacer cada uno (decisiones D255 y D282).
- **Hora** (`hora.c`) y **energía** (`energia.c`): el reloj del sistema, la zona horaria (D080),
  reiniciar y apagar.
- **Pilas que crecen** (`pilha.c`): la pila de un programa crece cuando hace falta, con una
  guarda que detecta el desbordamiento (decisión D186).
- **Gestor de servicios** (`supervisor.c`): los servicios en segundo plano, vistos desde el kernel
  (decisión D216).
- **Arranque y configuración**: `principal.c` (entrada del kernel), `kconf.c` (opciones que se
  guardan para el siguiente arranque, D353), `info_hardware.c` (datos para `infopc`, D041).

### 4.4 `kernel/fs/`: los sistemas de archivos

La idea clave es el **VFS** (*Virtual File System*, `vfs.c`): los programas usan siempre las mismas
llamadas (`abrir`, `ler`, `escrever`), y el VFS las manda al sistema de archivos que corresponda.

```
 programa → llamada al sistema → arquivos.c → vfs.c → ext2 / FAT32 / exFAT / ISO 9660 / UDF / SGFS
                                                   → pipe.c (tuberías)  → tv.c (terminal)
                                                                       ↓
                                                   cache_disco.c (caché de bloques) → blocos.c → discos
```

- **Descriptores de archivo** (`arquivos.c`): la tabla de archivos abiertos de cada proceso.
- **Montajes** (`montagem.c`): la tabla de montajes y `/etc/fstab` (decisión D040).
- **Discos y particiones** (`discos.c`, `blocos.c`): acceso a los sectores, tabla de particiones
  y avisos de disco (decisiones D040 y D179).
- **Caché de bloques** (`cache_disco.c`): lectura anticipada y escritura diferida (decisión D281).
- **ext2, ext3 y ext4** (`ext2.c`, `jbd2.c`): lectura y escritura, con el diario JBD2 (D326).
  Es el sistema de la raíz `SAGUI-RAIZ`.
- **FAT32** (`fat32.c`) y **exFAT** (`exfat.c`): pendrives y tarjetas.
- **CD y DVD**: `iso9660.c` (ISO 9660), `udf.c` (UDF, D233) y `cdda.c` (un CD de audio visto como
  una carpeta de archivos WAV).
- **Sobreposición** (`sobreposicao.c`): la raíz de la versión *live*. El sistema del medio es de
  solo lectura y una capa escribible va encima, como el *overlayfs* de Linux (decisión D379).
- **Raíz en disco y en RAM** (`raiz.c`, `ramfs.c`, `disco_ram.c`, `disco_comprimido.c`): distintas
  maneras de montar `/` (decisión D379).
- **Pipes** (`pipe.c`): comunicación entre procesos por un buffer circular dentro del kernel.
- **Terminal virtual** (`tv.c`): la pseudoterminal que usan el Terminal de ventana y `ssh`.
- **Configuración y registro** (`conf_sistema.c`, `logs_disco.c`): la partición `SAGUI-CONF` y el
  registro del kernel en `SAGUI-LOGS`.

### 4.5 `kernel/dispositivos/`: los drivers

Es el bloque más grande (83 ficheros). Se agrupa así:

- **Discos**: `ata.c` (IDE por PIO), `ahci.c` (SATA), `nvme.c` (NVMe), `atapi.c` (lectores de CD/DVD).
- **Bus**: `pci.c` (el bus PCI), `bar.c` (las ventanas de memoria de cada dispositivo),
  `pci_realoc.c` (recolocar una BAR de 64 bits por debajo de 4 GB), `isa.c` e `isapnp.c` (el bus
  ISA y las tarjetas ISA *Plug and Play*).
- **ACPI**: `uacpi_sagui.c` es la capa del kernel para la biblioteca **uACPI**, que interpreta el
  lenguaje AML del firmware. Sobre ella están `acpi.c` (apagado), `bateria.c`, `ec.c`, `prt.c` y
  `wmi.c`.
- **Portátiles**: `portatil.c`, `toshiba.c`, `fujitsu.c`, `hp_wmi.c`, `video_acpi.c` (brillo y teclas).
- **Sensores y energía**: `sensores.c` (temperatura y frecuencia), `frequencia.c` (límite de
  temperatura), `eist_antiguo.c` (SpeedStep), `zonas_termicas.c`.
- **Teclado, ratón y mandos**: `teclado.c`, `i8042.c`, `mouse.c`, `painel.c` (touchpads PS/2),
  `mandos.c` y `portajogos.c`.
- **Vídeo**: `video.c` (VESA con doble búfer), `vga_texto.c` (modo texto), `bga.c` (Bochs/QEMU),
  `modos_video.c` y `telas.c` (varias pantallas).
- **Sonido**: `audio.c` (mezclador), `hda.c` y `ac97.c` (tarjetas), `sb16.c`, `captura.c` (micrófono),
  `som.c` (el altavoz del PC).
- **GPU**: las capas comunes `acel2d.h`, `gpu3d.h` y `motor_gpu.c`, y un driver por familia
  (`acel_intel.c`, `acel_radeon.c`, `acel_r600.c`, `gpu3d_r600.c`, `gpu3d_evergreen.c`...).
  Qué tarjetas tienen soporte y con qué versión está en [`docs/gpus.md`](gpus.md); el 3D por GPU
  está explicado en [`docs/gpu3d.md`](gpu3d.md).
- **Tiempo y registro**: `relogio.c` (PIT y CMOS), `serial.c` (el «log» del kernel por COM1),
  `terminal_es.c` y `terminal_grafico.c`.

### 4.6 `kernel/rede/`: la red y el Wi-Fi

- **Pila de red** (`rede.c`): Ethernet, ARP, IPv4, ICMP (`ping`), UDP, DHCP y DNS.
- **TCP** (`tcp.c`) y **sockets** (`socket.c`): la conexión fiable y la interfaz que ven los programas.
- **Drivers de tarjetas**: `e1000.c`, `r8169.c`, `rtl8139.c`, `ne2000.c`, `alc.c` (Atheros AR813x).
- **Wi-Fi**: `wifi.c` (cliente 802.11 con el pincho RTL8188EU), `rtl8188eu.c`, `rt2870.c` (Ralink),
  `ar9485.c` (Atheros integrada) y `wifi_pcie.c`. La parte que no toca el hardware (criptografía
  WPA2 y WPA3) está en `kernel/lib/wpa.c` y `kernel/lib/sae.c`. Ver [`docs/wifi_usb.md`](wifi_usb.md)
  y [`docs/wifi-ar9485.md`](wifi-ar9485.md).
- **Túnel y VPN**: `tunel.c` (la interfaz `wg0` de WireGuard, [`docs/wireguard.md`](wireguard.md)).
- **Bloqueo de publicidad por DNS** (`bloqueo.c`, decisión D218).

Los ficheros con sufijo `_logica.h` son la **parte pura** de un driver: cálculos y decodificación
que no tocan el hardware. Así se pueden probar en el PC, sin tarjeta. Es un patrón que se repite
en todo el kernel.

### 4.7 `kernel/usb/` y `kernel/bluetooth/`

- **USB** (`usb.c`): enumeración de los dispositivos, hubs y conexión en caliente.
  Los controladores son `xhci.c` (USB 3.0), `ehci.c` (2.0), `ohci.c` y `uhci.c` (1.1).
  Las clases de dispositivo: `armazenamento.c` (pendrives, con SCSI), `hid.c` (teclados y ratones),
  `usbaudio.c` (sonido), `uvc.c` (cámaras). Ver [`docs/usb3.md`](usb3.md).
- **Bluetooth** (`kernel/bluetooth/`): la pila clásica se divide en capas, como en el libro:
  `bt_hci.c` (órdenes y eventos del controlador), `bt_l2cap.c` (canales), `bt_sdp.c` (buscar
  servicios), `bt_hid.c` (teclados y ratones inalámbricos) y `bt_gestor.c` (dispositivos y
  emparejamiento). `bt_fw.c` carga el firmware de algunos chips. Ver [`docs/bluetooth.md`](bluetooth.md).

### 4.8 `kernel/gui/`: el escritorio

- **Compositor y ventanas** (`janelas.c`): el gestor de ventanas. Decide qué ventana está encima,
  la arrastra, la minimiza y la cierra.
- **Dibujo** (`desenho.c`, `escala.c`, `rapido.c` de `kernel/lib`): dibujo con recorte, ampliación
  y copias rápidas.
- **Escritorio** (`area_trabalho.c`): fondo, mascota, iconos y menú del botón derecho (`menu.c`).
- **Barra de tareas** (`barra.c`): botón Inicio, botones de las ventanas, bandeja y reloj.
- **Menú Inicio** (`inicio.c`): los menús de columnas. Las entradas de cada categoría están en la
  tabla `programas[]` de `barra.c`. La lista completa está en la
  [guía de aplicaciones](guia-aplicaciones.md).
- **Estilos y colores** (`aparencia.c`, `arte.c`, `idioma.c`): color de resalte, dibujos en pixel
  art y el idioma de la interfaz (decisión D313).
- **Avisos, energía y sesión**: `avisos.c` (notificaciones), `ocio.c` (salvapantallas y bloqueo),
  `sessao.c` (inicio y cierre de sesión), `permisos_dialogo.c` (contraseña de administrador).

### 4.9 `kernel/lib/`: funciones puras

Son funciones que **no tocan el hardware**: se pueden entender y probar sin máquina real.

- Formatos y texto: `formatar.c` (como `printf`), `texto.c`, `caminho.c` (rutas `/`, `.`, `..`),
  `catalogo.c` (traducciones).
- Criptografía: `sha256.c`, `p256.c`, `wpa.c`, `wpa_cifra.c`, `sae.c`.
- Copias rápidas: `rapido.c`, `sse1.c`, `sse2.c`, `sse3.c` y `ssse3.c`. Cada una usa la instrucción
  que tiene la CPU.
- Conversiones de sensores y energía: `sensores_conv.c`, `bateria_conv.c`, `brilho_conv.c`,
  `frequencia_pol.c`.
- Tiempo: `fuso.c` (zonas horarias con reglas POSIX TZ).
- Descompresión: `inflar.c` (DEFLATE, para la raíz comprimida del CD).

### 4.10 `kernel/terceros/`: código de otros proyectos

Aquí está lo que **no** se escribió en este proyecto. Cada origen tiene su carpeta y su licencia:

- `uacpi/`: la biblioteca **uACPI** (MIT), que interpreta las tablas y el lenguaje AML de ACPI.
- `bsd/` (FreeBSD y OpenBSD, licencia BSD) y `linux/` (Linux, GPL-2.0), con datos de registros y
  tablas de inicio de tarjetas.
- `mit/` (X.org y el intérprete ATOM de las Radeon, MIT).

Lo que viene de fuera está explicado en [`docs/terceros.md`](terceros.md) y la regla de autoría
(decisiones D034 y D073) en [`docs/decisoes.md`](decisoes.md). Las piezas GPL se aíslan en
`kernel/terceros/linux/` para que la variante sin GPL (`make SEM_GPL=1`) pueda compilarse sin ellas.

---

## 5. Cómo se hace una llamada al sistema, paso a paso

Un programa no puede tocar el disco ni la pantalla por sí mismo: el procesador le impide usar
ciertas instrucciones (está en el **anillo 3**). Para pedir algo al kernel, usa una **llamada al
sistema**. Como ejemplo sigamos **`write`**, la llamada que escribe datos en un archivo, una
tubería o la terminal.

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

### 5.1 Los pasos, uno por uno

1. **El programa llama a `write`.** Es la función de la *libc* (`userland/libc/posix.c`). Las
   funciones del estilo POSIX las da la libc; las específicas de SaguiOS, la libsagui.
2. **`escrever` pone los números en los registros.** `userland/libsagui/chamadas.c` coloca el
   número de la llamada en `EAX` (`SIS_ESCREVER` vale **12**, según `kernel/inc/abi.h`), los
   argumentos en `EBX`, `ECX` y `EDX`, y ejecuta `int $0x80`. Ese es el contrato que describe la
   cabecera `abi.h`: «int 0x80, número en EAX, argumentos en EBX, ECX, EDX, resultado en EAX».
3. **La CPU pasa al kernel.** La instrucción `int 0x80` busca la entrada 0x80 de la **IDT**
   (`kernel/arch/idt.c`). Como esa puerta permite llamarla desde el anillo 3, la CPU cambia a la
   pila del kernel (la TSS de `gdt.c`) y salta al kernel.
4. **El despachador de llamadas.** `sistema_iniciar()` (`kernel/nucleo/sistema.c`) registra
   `tratar_sistema` para ese vector. La función lee `EAX` y consulta la **tabla** `tabela[]`, que
   apunta cada número a su función (`[SIS_ESCREVER] = sis_escrever`).
5. **Comprobaciones.** Antes de ejecutar nada, el kernel comprueba que el número existe, que los
   punteros (como `buf`) están dentro de la memoria del programa (`usuario_ok`) y que el usuario
   puede hacer esa llamada (la tabla de clases de `permisos.c`).
6. **El trabajo real.** `sis_escrever` (`kernel/fs/arquivos.c`) busca el descriptor `fd` en la tabla
   del proceso. Después `vfs_escrever` (`kernel/fs/vfs.c`) decide a qué sistema de archivos o
   dispositivo corresponde ese descriptor.
7. **Si hay que esperar.** Si la llamada tiene que esperar (por ejemplo, leer una tubería vacía),
   el kernel pone el proceso a dormir y **deja el puntero de instrucción apuntando al `int 0x80`**.
   Cuando el proceso despierta, la llamada se repite (`sistema.c`, cabecera y línea `eip -= 2`).
8. **La vuelta.** El resultado se guarda en `EAX`: bytes escritos, o un número negativo si falla.
   El kernel restaura los registros del programa e interrumpe la vuelta (`iret`). La libc recibe el
   valor y `write` lo devuelve.

> **Para practicar:** busca `SIS_ESCREVER` en el código. Verás la constante en `abi.h`, su uso en
> `chamadas.c`, la tabla en `sistema.c` y el permiso en `permisos.c`. Es el recorrido completo de
> cualquier otra llamada, cambiando solo el nombre.

---

## 6. Tablas de ficheros

Cada tabla tiene **todos** los ficheros `.c` y `.h` de su carpeta, con una línea de lo que hacen.
La línea sale del comentario de cabecera de cada fichero (casi todos empiezan con
`/* nombre.c - qué hace ... */`). Las referencias `(D…)` son decisiones de
[`docs/decisoes.md`](decisoes.md) y las `H…` y «mejora» son la numeración histórica del proyecto.
La ruta es relativa a la carpeta de la tabla.

### 6.1 kernel/arch/ (GDT, IDT, interrupciones, APIC, SMP)

| Fichero | Qué hace |
| --- | --- |
| `excecoes.c` | la pantalla de error de SaguiOS |
| `fpu.c` | FPU x87, MMX y SSE para los programas |
| `gdt.c` | tabla de segmentos del kernel y la TSS (D157: una por CPU) |
| `idt.c` | tabla de interrupciones y el despachador |
| `ioapic.c` | el IOAPIC: las IRQ de los dispositivos sin el 8259 (D166) |
| `irq_rotas.c` | el camino de cada IRQ PCI: autoprueba en el arranque, _PRT con uACPI y vuelta al 8259 por línea (D211) |
| `lapic.c` | el APIC local de cada procesador (D157) |
| `msr.c` | lectura de MSR que no derriba el kernel (D092) |
| `pic.c` | los dos controladores de interrupción 8259 (maestro y esclavo) |


### 6.2 kernel/mem/ (memoria física, paginación, PAE/NX, swap comprimida, copia al escribir)

| Fichero | Qué hace |
| --- | --- |
| `compressor.c` | compresión de páginas para la swap comprimida (D352) |
| `copia_escrita.c` | la cuenta de referencias de las páginas compartidas por el fork (D401) |
| `fisica.c` | gestor de páginas físicas (mapas de bits) |
| `heap.c` | kmalloc/kfree: clases de tamaño para bloques pequeños y páginas enteras para los grandes |
| `imagens.c` | páginas de solo lectura de las bibliotecas compartidas entre procesos (D378, fase 3; diseño en docs/enlace-dinamico.md §15 y en D378) |
| `memalta.c` | ventanas temporales a páginas físicas (D348) |
| `paginacao.c` | paginación en dos modos: clásica de 2 niveles o PAE de 3 (D010, D049, D349) |
| `troca.c` | swap comprimida en RAM (D352) |
| `wc.c` | write-combining (WC) para el framebuffer |


### 6.3 kernel/nucleo/ (procesos, hilos, planificador, señales, llamadas, usuarios y permisos)

| Fichero | Qué hace |
| --- | --- |
| `acoes.c` | acciones comunes del sistema (volumen, silencio, brillo, radio) y su aviso en pantalla (D403) |
| `aleatorio.c` | números impredecibles para el TLS, el TCP y el DNS (D061) |
| `atomicas.c` | la parte de 64 bits de las primitivas atómicas (D150) |
| `autoteste.c` | pruebas que el kernel ejecuta sobre sí mismo |
| `bifurcar.c` | fork con copia al escribir y waitpid (D401) |
| `carregador.c` | crea procesos a partir de ejecutables ELF32 (H13) |
| `chamadas_processos.c` | llamadas de los grupos "procesos" y "diversas" |
| `console.c` | la salida de texto del kernel: kprintf, klog y la consola |
| `cpu_dados.c` | la zona de datos por CPU (D150, D157) |
| `energia.c` | reiniciar y apagar la máquina |
| `hilos.c` | hilos de usuario y espera por dirección (futex) (D167) |
| `hora.c` | la hora del sistema y la zona horaria (D080) |
| `info_hardware.c` | lo que el infopc (D041) necesita y sólo el kernel alcanza |
| `kconf.c` | opciones del kernel que se guardan desde SaguiOS y valen en el siguiente arranque (D353) |
| `permisos.c` | quién puede hacer qué (D255 y D282, mejora 098, fases 1 y 2) |
| `pilha.c` | pilas de usuario que crecen bajo demanda, con guarda (D186) |
| `planificadores.c` | las políticas del planificador entre las CPU (D174) |
| `portapapeis.c` | el portapapeles del sistema (D074) |
| `principal.c` | punto de entrada en C del kernel del SaguiOS |
| `processos.c` | tabla de procesos y planificador round-robin apropiativo |
| `rastro.c` | marcas que sobreviven a un reinicio en caliente (ver rastro.h) |
| `sinais.c` | señales POSIX (D401) |
| `sistema.c` | puerta de entrada de las llamadas al sistema (int 0x80) |
| `smp.c` | el arranque de los procesadores secundarios (D157, D164) |
| `spinlock.c` | cerrojos de espera activa del kernel (D150) |
| `supervisor.c` | el gestor de servicios visto desde el kernel (D216) |
| `trava.c` | exclusión mutua entre procesos y tareas del kernel (D095) |
| `trava_geral.c` | el cerrojo general del kernel (D165) |
| `usuarios.c` | SIS_USUARIO: la identidad de los procesos (D255, mejora 098, fase 1) |


### 6.4 kernel/fs/ (VFS, ext2/3/4, FAT32, exFAT, ISO 9660, UDF, caché de disco, sobreposición y raíz)

| Fichero | Qué hace |
| --- | --- |
| `arquivos.c` | archivos abiertos y las llamadas de archivos y carpetas (H14) |
| `blocos.c` | acceso en bruto a los sectores de un disco (D179) |
| `cache_disco.c` | caché de bloques con lectura anticipada y escritura diferida (D281) |
| `cdda.c` | el sistema de archivos "cdda": un CD de audio como una carpeta de WAV (D233) |
| `cdda_logica.c` | lo que no toca el hardware del CD de audio (D233): la tabla de contenido, el CD-TEXT, los nombres de los archivos, la cabecera WAV y la lista |
| `conf_sistema.c` | la configuración persistente del sistema (D255, opción A del mantenedor) |
| `disco_comprimido.c` | la raíz del CD comprimida por bloques, vista como un disco ("rz0"; v8.5.4.2, D379 P5) |
| `disco_ram.c` | un disco en la RAM con la copia de un volumen ("ram0"; D379, fase T1: toram) |
| `discos.c` | discos, particiones, formatos y avisos (H32, H34, D040) |
| `exfat.c` | el sistema de archivos exFAT de los pendrives y tarjetas grandes (D325) |
| `ext2.c` | ext2, ext3 y ext4: lectura y escritura (D040, D326) |
| `fat32.c` | el sistema de archivos FAT32 del pendrive de datos (H32, H33) |
| `iso9660.c` | lectura de los CD/DVD con ISO 9660 (D207) |
| `jbd2.c` | reproducción y escritura del diario JBD2 (D326) |
| `logs_disco.c` | el registro del kernel en la partición SAGUI-LOGS (D120) |
| `montagem.c` | tabla de montajes y /etc/fstab (D040) |
| `pipe.c` | comunicación entre procesos por un buffer circular en el kernel |
| `raiz.c` | la raíz del sistema en un disco (D379, prototipo de la mejora 035 F2) |
| `ramfs.c` | una rama del árbol en RAM montada como un volumen más (D379, fase R3) |
| `sgfs.c` | lectura del ramdisk SGFS |
| `sobreposicao.c` | la raíz de la live: el sistema del medio, de solo lectura, con una capa escribible encima (D379, fase L1; al estilo del overlayfs de Linux) |
| `tv.c` | terminal virtual (pseudoterminal), D036 |
| `udf.c` | lectura de los CD/DVD con UDF (D233), solo lectura |
| `vfs.c` | capa de archivos virtual con la capa escribible en la RAM (H14) |


### 6.5 kernel/dispositivos/ (discos, sonido, vídeo, 2D y 3D por GPU, ACPI/uACPI, portátiles, sensores)

| Fichero | Qué hace |
| --- | --- |
| `ac97.c` | controladores AC'97 de Intel (ICH) y compatibles (D089) |
| `acel_gcn.c` | copiar y rellenar rectángulos con el motor DMA de las Radeon GCN 1.0 (Southern Islands: Tahiti, Pitcairn, Cape Verde, Oland; HD 7000, R7/R9 200 y 300 de esas familias) (D360) |
| `acel_intel.c` | el blitter de las GPU de Intel, de Gen3 a Gen8 (D195, D359) |
| `acel_intel_logica.h` | la parte pura del blitter de Intel (D195, D359): los modelos de Gen7 (Ivy Bridge), Gen7.5 (Haswell) y Gen8 (Broadwell) con identificador exacto y las familias que se reconocen pero no llevan 2D |
| `acel_mach64.c` | el motor 2D de las ATI Mach64 y Rage (GX, CT, VT, Rage II, Rage Pro, Rage LT/XL/Mobility) (D308) |
| `acel_nv.c` | el motor 2D de las NVIDIA GeForce 6/7 y Go 6xxx/7xxx (NV4x/G7x) y de las integradas en los chipsets nForce (GeForce 6100/6150 y 7025/7050/7100/7150: C51, C61, C67, C68 y C73) (D224) |
| `acel_nv_logica.h` | la parte pura del acelerador 2D de las GeForce 6/7 (D224): los modelos (identificadores PCI exactos y el chip que tiene que decir PMC_BOOT_0), el formato de las órdenes del búfer (cabeceras, puntos, tamaños, saltos), la tabla hash de PRAMIN y el pitch de la superficie |
| `acel_r600.c` | copiar y rellenar rectángulos con el motor DMA de las Radeon R6xx/R7xx (D223; la capa común es la de la D195) |
| `acel_radeon.c` | el motor 2D clásico de las Radeon R100 a R500 (D195) |
| `acel_s3.c` | el 2D de las S3 Trio64 y ViRGE (D308) |
| `acel_tdfx.c` | el motor 2D de las 3dfx Banshee, Voodoo3 y Voodoo4/5 (VSA-100) (D305) |
| `acel_vmware.c` | acelerador 2D de la VMware SVGA II (PCI 15ad:0405, D195) |
| `acpi.c` | apagado por ACPI (estado S5) y botón de encendido (D113, mejora 030) |
| `ahci.c` | driver del controlador SATA AHCI (D155) |
| `ata.c` | discos internos ATA (IDE) por PIO, sin DMA ni interrupciones (D040) |
| `atapi.c` | lectores de CD/DVD (ATAPI), solo lectura (D207) |
| `atom_sintetica.h` | una BIOS ATOM de juguete, construida a mano, para probar sin tarjeta (D399) |
| `audio.c` | el mezclador de sonido del kernel y las llamadas de audio (D088-D090) |
| `bar.c` | los BAR (las ventanas de direcciones) de un dispositivo PCI (D158) |
| `bateria.c` | la batería del portátil por ACPI (D192) |
| `bga.c` | tarjeta de vídeo de Bochs/QEMU ("dispi", BGA) |
| `bga_extra.c` | segundas (y terceras) tarjetas de Bochs/QEMU como salidas de vídeo extra (D290) |
| `captura.c` | entrada de sonido (micrófono) del kernel (D292) |
| `ec.c` | el controlador embebido (EC, ACPI PNP0C09) para el AML (D192) |
| `eist_antiguo.c` | Enhanced SpeedStep de los Intel antiguos (D177, fases A y B) |
| `energia_radeon.c` | relojes y estados de energía de las Radeon R5xx por la BIOS ATOM (D399) |
| `energia_sim.c` | driver de energía de mentira para las pruebas en QEMU (D399, mkimagem.py --gpu-energia-simulada) |
| `evergreen_3d_pacotes.c` | paquetes PM4, sombreadores y valores de inicio del motor 3D de las Radeon Evergreen y Northern Islands sin Cayman (D427) |
| `evergreen_dma.c` | lista de modelos, paquete de copia y comprobación de la pantalla de las Radeon Evergreen y Northern Islands (D286) |
| `frequencia.c` | driver de frecuencia de la CPU con límite de temperatura (D106, mejora 061) |
| `fujitsu.c` | los dispositivos ACPI propios de Fujitsu (y Fujitsu Siemens) (D403) |
| `g3d_v2_provas.c` | pruebas de las capacidades gpu3d v2 de un driver (D426) |
| `gart_r700_logica.h` | la GART (memoria virtual, contexto 0) de las Radeon R7xx (RV710, RV730, RV740, RV770), D406 |
| `gcn_dma.c` | lista de modelos GCN, paquetes del DMA de las Southern Islands, comprobación de la pantalla (DCE 6) y relleno con CONSTANT_FILL (D360) |
| `gen3_gtt.h` | RAM del sistema en el GTT de Intel Gen3 (915, 945, G33, Pineview), D358 |
| `gen3_pacotes.h` | las órdenes 3D de las GPU Intel Gen3 (915, 945, G33, Pineview), D285 |
| `gpu3d_evergreen.c` | 3D por la GPU en las Radeon Evergreen (HD 5000: Cedar, Redwood, Juniper, Cypress, Hemlock) y Northern Islands sin Cayman (HD 6000 y HD 7500M/7600M: Barts, Turks, Caicos), debajo de la capa común gpu3d (D427) |
| `gpu3d_intel.c` | 3D por la GPU en Intel Gen3: 915, 945, G33 y Pineview (D285, D358) |
| `gpu3d_r500.c` | 3D por la GPU en las Radeon R5xx (X1300 a X1950: RV515, R520, RV530, RV560, RV570, R580), debajo de la capa común gpu3d (D285, D287) |
| `gpu3d_r600.c` | 3D por la GPU en las Radeon R7xx (RV710, RV730, RV740, RV770) y, desde la D357, R6xx (R600, RV610, RV620, RV630, RV635, RV670): el procesador de órdenes (CP) con su microcódigo, el estado del motor 3D y los dibujos, debajo de la capa común gpu3d |
| `gpu3d_sim.c` | un driver 3D de mentira, por la CPU, para probar la capa común (D285) |
| `gpu_energia_logica.h` | lógica pura de la energía de la GPU (D399): lo que se puede probar en el PC |
| `hda.c` | Intel High Definition Audio (D089) |
| `hda_ajustes.c` | elección del ajuste de un códec HDA (D107) |
| `hp_wmi.c` | radios y teclas del panel de los portátiles HP por WMI (D405) |
| `i8042.c` | el controlador del teclado y del touchpad (H19-fix) |
| `injecao.c` | entrada inyectada para el escritorio remoto (D251, mejora 097) |
| `isa.c` | el bus ISA (D302): el controlador de DMA 8237, la memoria baja para él y la lista de dispositivos ISA detectados (para infopc y lspci) |
| `isapnp.c` | ISA Plug and Play (D302): encontrar las tarjetas ISA PnP y configurar una Sound Blaster de Creative (Vibra16, SB16 PnP, AWE32/64 PnP) que la BIOS dejó sin activar |
| `mandos.c` | la capa común de mandos y joysticks (D307) |
| `modos_video.c` | lista de modos, cambio en caliente y resolución preferida |
| `motor_gpu.c` | lo que el kernel sabe del motor 3D por la GPU (D158) |
| `mouse.c` | driver del mouse PS/2 (el touchpad del G470 habla el mismo protocolo) |
| `nvme.c` | driver del controlador NVMe (D265), escrito desde la especificación pública NVM Express 1.4 / 2.0 (sin código de otros sistemas) |
| `painel.c` | paneles táctiles PS/2 (touchpads) en modo absoluto, con gestos (D407) |
| `painel_logica.h` | la parte pura de los paneles táctiles PS/2 (D407): reconocer el modelo por sus respuestas, decodificar los paquetes absolutos de Synaptics, ALPS y Elantech y convertir los toques en movimiento, clics y rueda (los gestos) |
| `pci.c` | el bus PCI (H29) |
| `pci_realoc.c` | baja por debajo de 4 GB una BAR de 64 bits que el firmware dejó arriba (D283) |
| `portajogos.c` | el joystick analógico del puerto de juegos (0x201) (D307) |
| `portatil.c` | lo propio de los portátiles por ACPI: el brillo, los avisos del firmware, SIS_PORTATIL y el volcado de las tablas (D403) |
| `prt.c` | el enrutado de las interrupciones PCI del firmware (_PRT) con uACPI (D211) |
| `r500_pacotes.h` | las órdenes 3D de las Radeon R5xx (RV515, R520, RV530, RV560, RV570, R580) para el procesador de órdenes (CP): estado de la tubería (VAP, RS, US, FG, RB3D, ZB), el programa de vértices y los dos programas de fragmentos |
| `r600_3d_pacotes.c` | paquetes PM4, sombreadores y valores de inicio del motor 3D de las Radeon R6xx/R7xx (D284) |
| `r600_dma_pacotes.c` | paquetes del motor DMA de las Radeon R6xx/R7xx y el reparto de los rectángulos en copias lineales (D223) |
| `radeon2d_ordens.h` | las escrituras de registros de una orden del motor 2D de las Radeon R100-R500 (D195, D387, D391), sin efectos: las mismas, en el mismo orden, van por MMIO (acel_radeon.c) o, con el CP en marcha, por el anillo como PACKET0 (gpu3d_r500.c) |
| `relogio.c` | el PIT (temporizador programable) y el reloj CMOS |
| `sb16.c` | la Sound Blaster 16 de Creative, en el bus ISA (D302) |
| `sensores.c` | monitor de hardware: temperatura y frecuencia de la CPU (D092, mejora 050) |
| `serial.c` | puerto serie COM1 (0x3F8), el "log" del kernel |
| `som.c` | pitido por el altavoz del PC (bonus B3) |
| `teclado.c` | driver del teclado PS/2 (controlador i8042, IRQ 1), layouts español (ES, el por defecto) y ABNT2 |
| `telas.c` | varias pantallas: registro de salidas, disposición del escritorio y configuración (D290) |
| `terminal_es.c` | el terminal visto por los programas |
| `terminal_grafico.c` | el terminal dibujado en modo gráfico con la fuente propia (H18) |
| `toshiba.c` | la interfaz HCI de los portátiles Toshiba (D403) |
| `uacpi_sagui.c` | la capa del kernel para uACPI y la tarea "acpi-aml" (D192) |
| `vga_texto.c` | consola en modo texto VGA (80x25), usada en las fases 1 a 3 |
| `video.c` | framebuffer VESA con doble búfer |
| `video_acpi.c` | el ACPI Video estándar: brillo de la pantalla del portátil y avisos de las teclas de brillo y de pantalla (D403; especificación ACPI, apéndice B «Video Extensions») |
| `volcado_logica.h` | la parte pura del volcado del búfer de atrás a la pantalla por la GPU (D387): entradas de la GART PCIE de las Radeon R300-R5xx, dónde poner su ventana en el mapa del controlador de memoria (MC) y el troceo de un rectángulo en franjas para el motor 2D |
| `vram_zona_logica.h` | dónde poner una zona de trabajo de la GPU (cola de órdenes, búferes del 3D) en la memoria de vídeo sin pisar nada de lo que muestra la tarjeta (D386) |
| `wmi.c` | el puente WMI de ACPI (dispositivos PNP0C14) sobre uACPI (D405) |
| `zonas_termicas.c` | las zonas térmicas del ACPI (ThermalZone, _TMP), para enseñarlas junto a la GPU (D399) |


### 6.6 kernel/rede/ (pila TCP/IP, drivers de red, Wi-Fi)

| Fichero | Qué hace |
| --- | --- |
| `alc.c` | driver EXPERIMENTAL de las Ethernet Atheros AR813x/AR815x (familia L1C/L2C, el «atl1c» de Linux), D200 |
| `alc_logica.h` | la parte pura del driver alc (Atheros AR813x/AR815x, D200): modelos, MAC, descriptores, estado del PHY y valores de los registros de configuración |
| `ar9485.c` | Wi-Fi integrada Atheros AR9485 (PCIe 168c:0032, 802.11b/g/n 1x1, 2,4 GHz), D280 |
| `ar9485_logica.h` | la parte pura del driver del Atheros AR9485 (D280): lectura y restauración de la imagen de la EEPROM/OTP (bloques con suma de control y plantillas), tablas de canales y la interpolación de las potencias |
| `bloqueo.c` | bloqueo de publicidad y rastreo por DNS (D218) |
| `e1000.c` | driver de la Intel PRO/1000 (82540EM y parecidas) |
| `ne2000.c` | driver de las tarjetas NE2000 (D329): el DP8390 con 16 KB de RAM propia, en PCI (Realtek RTL8029 y clones, 10ec:8029) y en ISA (puertos 0x300/0x280/0x320/0x340) |
| `r8169.c` | driver EXPERIMENTAL de la familia Realtek RTL8101E/8102E/8105E y RTL8168/8169 |
| `r8169_ajustes.h` | ajustes en caliente del driver r8169 (D124): RxConfig, RxMaxSize, tamaño del búfer en el descriptor y "multi" (bit 14 de RxConfig), pedidos con "ifconfig eth0 r8169 ..." para probar variantes en el hardware real sin recompilar |
| `r8169_logica.h` | parte del driver r8169 que no toca el hardware (D091): revisión del chip, perfil de arranque y decodificación |
| `r8169_sem_gpl.h` | lo mínimo del driver r8169 para la compilación sin GPL (`make SEM_GPL=1`, mejora 057, D109) |
| `rede.c` | la pila de red: Ethernet, ARP, IPv4, ICMP (ping), UDP, DHCP y DNS |
| `rt2870.c` | los pinchos Wi-Fi Ralink RT2870 / RT3070 / RT3071 / RT3072 / RT5370 (D409) |
| `rt2870_logica.h` | la parte pura del soporte de los pinchos Wi-Fi Ralink RT2870 / RT3070 / RT3071 / RT3072 / RT5370 (D409, experimental, sin probar en hardware) |
| `rtl8139.c` | driver de la Realtek RTL8139 (QEMU: -nic user,model=rtl8139) |
| `rtl8188eu.c` | el chip del pincho Wi-Fi Realtek RTL8188EU (USB 0bda:8179), D215 |
| `rtl8192cu_logica.h` | la parte pura del soporte de los pinchos Wi-Fi Realtek RTL8188CU / RTL8192CU / RTL8188RU (D409, experimental, sin probar en hardware) |
| `socket.c` | sockets: a rede vista pelos programas |
| `tcp.c` | TCP: cliente e servidor simples |
| `tunel.c` | la interfaz del túnel, wg0 (D363) |
| `wifi.c` | Wi-Fi cliente (802.11 b/g) con el pincho Realtek RTL8188EU (D215) |
| `wifi_pcie.c` | identificación de más Wi-Fi integradas PCIe (D410), experimental |
| `wifi_pcie_logica.h` | la parte pura del soporte de más Wi-Fi integradas PCIe (D410): catálogo de tarjetas por identificador PCI, familias de las Atheros (ath9k/athn), decodificación del SREV, geometría y lectura de la ROM de la familia AR5008 |
| `wifi_usb_ids.h` | VID:PID de los pinchos Wi-Fi USB (D409) |
| `wifi_usb_logica.h` | qué familia de pincho Wi-Fi USB es un VID:PID (D409) |


### 6.7 kernel/usb/ (controladores USB y clases de dispositivo)

| Fichero | Qué hace |
| --- | --- |
| `armazenamento.c` | pendrives: Bulk-Only Transport + SCSI (H31) |
| `ehci.c` | driver del controlador USB 2.0 (EHCI) (H29) |
| `hid.c` | teclados y ratones USB (clase HID) por xHCI, EHCI, UHCI y OHCI (D214, D267) |
| `isoent.h` | cola de paquetes recibidos de un flujo isócrono de entrada (D277) |
| `ohci.c` | driver del controlador USB 1.1 OHCI (AMD, NVIDIA, SiS, ALi, Apple...) (D267) |
| `uhci.c` | driver del controlador USB 1.1 UHCI (Intel PIIX/ICH, VIA...) (D267) |
| `usb.c` | dispositivos USB: enumeración, hubs y conexión en caliente (H30, D214) |
| `usbaudio.c` | audio USB, clase Audio 1.0 (UAC 1), salida de sonido (D275) |
| `usbbt.c` | transporte USB de la pila Bluetooth y llamada SIS_BLUETOOTH (D279, mejora 059) |
| `uvc.c` | cámaras UVC 1.1 (clase vídeo USB) y la fuente de prueba (D277) |
| `uvc_nucleo.c` | la parte pura del driver UVC 1.1 (D277) |
| `uvc_nucleo.h` | la parte pura del driver UVC 1.1 (clase vídeo USB, D277) |
| `xhci.c` | driver del controlador USB 3.0 (xHCI) (D155, D214) |


### 6.8 kernel/bluetooth/ (pila Bluetooth clásica)

| Fichero | Qué hace |
| --- | --- |
| `bt.h` | pila Bluetooth clásico (BR/EDR) de SaguiOS (D279, mejora 059, v8.2) |
| `bt_fw.c` | carga de firmware de los controladores Bluetooth Broadcom, Realtek y Atheros (D411) |
| `bt_fw.h` | carga de firmware y parches de los controladores Bluetooth que no arrancan listos (D411) |
| `bt_gestor.c` | el gestor Bluetooth (D279): tabla de dispositivos, flujo de conexión y emparejamiento, claves de enlace guardadas y operaciones que usan la llamada SIS_BLUETOOTH y el terminal |
| `bt_hci.c` | capa HCI de la pila Bluetooth (D279): órdenes, eventos, tráfico ACL con créditos, inicio del controlador y el registro del adaptador |
| `bt_hid.c` | HIDP (D279): el perfil HID de Bluetooth sobre dos canales L2CAP, control (PSM 0x11) e interrupción (PSM 0x13) |
| `bt_l2cap.c` | L2CAP básico (D279): señalización por el canal 1, canales dinámicos en modo básico, reensamblado de las tramas ACL fragmentadas |
| `bt_sdp.c` | cliente SDP (D279): busca el servicio HID de un dispositivo con una sola petición ServiceSearchAttributeRequest y lee de la respuesta los PSM de los canales, la subclase, si es dispositivo de arranque y el descriptor de informe |


### 6.9 kernel/gui/ (compositor, ventanas, escritorio, barra, menús, estilos)

| Fichero | Qué hace |
| --- | --- |
| `acel2d.c` | 2D por la GPU para el escritorio: la capa común (D195) |
| `aparencia.c` | cor de destaque e fundo escolhidos na Configuración (D112) |
| `area_trabalho.c` | el fondo, la mascota, los íconos y el menú del escritorio (H20, H28, D051, D066) |
| `arte.c` | los dibujos del escritorio, en pixel art hecho en este proyecto |
| `atalhos.c` | atajos de teclado del escritorio (v6.0 beta, D119) |
| `avisos.c` | notificaciones emergentes con el texto completo (issue #16, D269) |
| `barra.c` | la barra de tareas: botón Inicio, botones de las tareas, bandeja y reloj (D065, D361) |
| `desenho.c` | rutinas de dibujo con recorte, usadas por el escritorio y por el gestor de ventanas |
| `escala.c` | copia ampliada o reducida de píxeles (pantalla completa, D044) |
| `gpu3d.c` | 3D por la GPU: la capa común (D285) |
| `gpu3d_mem.c` | piezas comunes de los drivers 3D: memoria de GPU por bloques, búferes y coma flotante con enteros (D285) |
| `gpu_energia.c` | energía de la GPU: la capa común (D399) |
| `idioma.c` | idioma de la interfaz del kernel y SIS_IDIOMA (D313, mejora 118) |
| `inicio.c` | los menús Inicio de columnas: «Dos columnas» y «Moderno» (D361) |
| `janelas.c` | el gestor de ventanas (H21) |
| `menu.c` | menú contextual del botón derecho (D051) y submenús (D064) |
| `ocio.c` | energía y pantalla: salvapantallas, bloqueo y apagado de la pantalla por inactividad, aviso de batería |
| `permisos_dialogo.c` | el diálogo de permisos de administrador (D269, mejora 102, issue #16) |
| `sessao.c` | inicio de sesión gráfico, cerrar sesión y cambiar de usuario (D340, mejora 098, fase 3) |


### 6.10 kernel/lib/ (funciones puras del kernel)

| Fichero | Qué hace |
| --- | --- |
| `acpi_pss.c` | lector mínimo de _PSS, _PSD y _PCT sin intérprete AML (D177) |
| `bateria_conv.c` | porcentaje y tiempo restante de la batería (D192) |
| `brilho_conv.c` | niveles de brillo de los portátiles (D403) |
| `caminho.c` | resolución de rutas |
| `catalogo.c` | lector de catálogos de traducción (D313) |
| `crc32c.c` | CRC32c (polinomio 0x1EDC6F41, en orden reflejado 0x82F63B78) y CRC16 (polinomio 0x8005, reflejado 0xA001) |
| `div64.c` | las rutinas de 64 bits que el GCC llama en 32 bits (D192) |
| `formatar.c` | formateo de texto al estilo printf, escrito para SaguiOS |
| `frequencia_pol.c` | políticas y límite de temperatura del driver de frecuencia (D106, mejora 061) |
| `fuso.c` | intérprete de cadenas POSIX TZ y calendario (D080) |
| `hid_informe.c` | analizador de descriptores de informe HID y decodificación de mandos (D307) |
| `inflar.c` | descompresión DEFLATE (RFC 1951) en el núcleo (v8.5.4.2, D379: la raíz comprimida del CD) |
| `ocio_efectos.c` | los efectos del salvapantallas (D422) |
| `p256.c` | la curva NIST P-256 para el SAE de WPA3 (D220) |
| `portajogos_conv.c` | las cuentas del puerto de juegos (D307) |
| `rapido.c` | copias y rellenos grandes de la interfaz gráfica (D042) |
| `sae.c` | SAE de WPA3-Personal con P-256 (D220) |
| `sensores_conv.c` | fórmulas del monitor de hardware (D092) |
| `sha256.c` | SHA-256 escrito a partir de FIPS 180-4 (secciones 4.1.2, 4.2.2, 5.3.3 y 6.2), D060 |
| `sse1.c` | copias y rellenos con SSE solo, sin SSE2 (D208) |
| `sse2.c` | copias y rellenos con SSE2 (D042) |
| `sse3.c` | copias con SSE3: lddqu para las lecturas desalineadas (D208) |
| `ssse3.c` | reordenar canales de píxeles con pshufb (D208) |
| `texto.c` | rutinas de memoria y de texto del kernel |
| `wifi_n.c` | rendimiento del Wi-Fi del pincho RTL8188EU (D238) |
| `wmi_wdg.c` | el análisis de _WDG y los búferes de HP, sin hardware (D405) |
| `wpa.c` | criptografía y formatos del Wi-Fi cliente (D215) |
| `wpa_cifra.c` | primitivas del Wi-Fi para WEP, TKIP, el descriptor 1 de EAPOL-Key, las AKM con SHA-256 (SAE, PSK-SHA256), el CCMP por software y BIP (D220, v7.3.2) |


### 6.11 kernel/inc/ (cabeceras compartidas)

| Fichero | Qué hace |
| --- | --- |
| `abi.h` | contrato entre el kernel y los programas: números de las llamadas al sistema y estructuras comunes |
| `acel2d.h` | 2D por la GPU para el escritorio: copiar y rellenar rectángulos (D195) |
| `acel_intel.h` | el ring de órdenes de las GPU de Intel, compartido por el blitter 2D (acel_intel.c, D195) y el 3D de Gen3 (gpu3d_intel.c, D285) |
| `acoes.h` | acciones comunes del sistema: volumen, silencio, brillo, radio (D403) |
| `acpi.h` | apagado por ACPI (S5) y botón de encendido (D113) |
| `acpi_pss.h` | lector mínimo de _PSS, _PSD y _PCT de las tablas ACPI (D177) |
| `ahci.h` | controlador SATA AHCI (D155) |
| `aleatorio.h` | generador de números aleatorios del kernel (D061) |
| `aparencia.h` | color de resalte del escritorio (D112) |
| `ar9485.h` | Wi-Fi integrada Atheros AR9485 (PCIe 168c:0032, familia AR9003, sin firmware), D280 |
| `arte.h` | dibujos en pixel art (letras = colores de la paleta) |
| `ata.h` | discos internos ATA/IDE por PIO (D040) y SATA/AHCI (D155) |
| `atapi.h` | lectores de CD/DVD (ATAPI), solo lectura (D207) |
| `atomicas.h` | primitivas atómicas del kernel (D150, base del SMP) |
| `audio.h` | tarjetas de sonido (AC'97 e Intel HDA) y el mezclador del kernel (D088-D090) |
| `autoteste.h` | pruebas internas del kernel, activadas solo en las imágenes de prueba |
| `avisos.h` | notificaciones emergentes (H34, D125, D233; issue #16, D269) |
| `bateria.h` | uACPI, el controlador embebido y la batería, vistos desde el resto del kernel (D192) |
| `bateria_conv.h` | de los números de _BIF/_BIX/_BST a porcentaje y tiempo (D192) |
| `bga.h` | tarjeta de vídeo de Bochs/QEMU/VirtualBox ("dispi"): cambio de modo sin la BIOS (D059) |
| `bloqueo.h` | bloqueo de publicidad y rastreo por DNS en el resolvedor del kernel, al estilo de un sumidero DNS (D218) |
| `bloqueo_tabla.h` | el formato de la tabla del bloqueo de publicidad por DNS y la búsqueda en ella (D218) |
| `bluetooth.h` | Bluetooth con un controlador USB estándar (D279, mejora 059, v8.2) |
| `bootinfo.h` | lo que la etapa 2 entrega al kernel (espejo de boot/bootinfo.inc) |
| `brilho_conv.h` | de los niveles de brillo que da el firmware a índices y porcentajes (D403) |
| `cache_disco.h` | caché de bloques común a los discos internos (D281) |
| `caminho.h` | montar y simplificar rutas ("/", ".", "..") |
| `catalogo.h` | catálogos de traducción (D313, mejora 118) |
| `cdda.h` | las pistas de un CD de audio como archivos WAV, solo lectura (D233) |
| `compressor.h` | compresión rápida de una página de 4 KB para la swap (D352) |
| `conf_sistema.h` | la partición SAGUI-CONF: configuración persistente del sistema (D255) |
| `console.h` | salida de texto del kernel (pantalla + espejo en el serial) |
| `copia_escrita.h` | cuenta de referencias de las páginas físicas que comparten varios espacios de direcciones (copia al escribir del fork, D401) |
| `cpu.h` | segmentos, interrupciones y el marco guardado en la pila en cada interrupción |
| `cpu_dados.h` | datos por CPU: la base del SMP (D150) y su uso real (D157) |
| `crc32c.h` | CRC32c (Castagnoli) y CRC16 (el de las sumas de grupo del ext4 sin metadata_csum) |
| `desenho.h` | dibujo con recorte sobre una superficie de píxeles (fase 5) |
| `disco.h` | discos y particiones vistos por el sistema de archivos (fase 6, D040) |
| `disco_comprimido.h` | la raíz del CD comprimida por bloques (v8.5.4.2, D379 P5) |
| `disco_ram.h` | "ram0": la copia en la RAM de un volumen (D379, toram) |
| `eist_antiguo.h` | Enhanced SpeedStep de los Intel antiguos (D177) |
| `eq_fixo.h` | el filtro del ecualizador en punto fijo (D201) |
| `esquemas.h` | esquemas de apariencia de todo el sistema (v8.5.5.2, D428) |
| `evergreen_3d.h` | paquetes PM4, sombreadores y valores de inicio del motor 3D de las Radeon Evergreen (HD 5000: Cedar, Redwood, Juniper, Cypress, Hemlock) y Northern Islands sin Cayman (HD 6000 y HD 7500M/7600M: Barts, Turks, Caicos) (D427) |
| `evergreen_dma.h` | lo propio de las Radeon Evergreen (HD 5000) y Northern Islands (HD 6000) para el 2D por el motor DMA (D286): la lista exacta de modelos, el paquete de copia y la comprobación del controlador de pantalla |
| `exfat.h` | lectura y escritura de exFAT (D325) |
| `fat32.h` | lectura y escritura de FAT32 (H32, H33) |
| `fontes_sistema.h` | fuentes del sistema: tabla de tipografías integradas en el kernel |
| `fpu.h` | FPU x87, MMX y SSE para los programas (estado guardado por proceso) |
| `frequencia.h` | driver de frecuencia de la CPU (D106, mejora 061) |
| `frequencia_pol.h` | la decisión del driver de frecuencia (D106, mejora 061) |
| `fuso.h` | husos horarios con reglas POSIX TZ (D080) |
| `g3d_v2_provas.h` | pruebas de las capacidades gpu3d v2 de un driver (D426) |
| `gcn_dma.h` | lo propio de las Radeon GCN para el 2D por la GPU (D360): la lista exacta de modelos (GCN 1.0 a 5), los paquetes del motor DMA de las Southern Islands (GCN 1.0) y la comprobación del controlador de pantalla |
| `gpu3d.h` | 3D por la GPU: la capa común y lo que comparten sus drivers (D285) |
| `gpu3d_diag.h` | diagnóstico del 3D común a todos los drivers (D390; generaliza lo que la D386 hizo para la R5xx del Amilo) |
| `gpu_energia.h` | energía de la GPU: relojes, estados, temperatura y VRAM (D399) |
| `gui.h` | área de trabalho e gestor de ventanas (fase 5) |
| `hda_ajustes.h` | ajustes («quirks») de códecs HDA por códec y subsistema (D107) |
| `hid_informe.h` | analizador de descriptores de informe HID y decodificación de mandos (D307) |
| `i8042.h` | controlador del teclado y del touchpad |
| `idioma.h` | el idioma de la interfaz del kernel (D313, mejora 118) |
| `imagens.h` | páginas de solo lectura de las bibliotecas compartidas entre procesos (D378, fase 3; docs/enlace-dinamico.md §4 y §15) |
| `inflar.h` | descompresión DEFLATE y CRC-32 en el núcleo (v8.5.4.2, D379) |
| `info_hardware.h` | datos del hardware para el infopc (D041) |
| `ioapic.h` | el IOAPIC: el controlador de interrupciones de los dispositivos que sustituye al par de 8259 (D166) |
| `irq_rotas.h` | qué camino sigue cada IRQ de los dispositivos PCI y la autoprueba que lo comprueba (D211) |
| `isa.h` | el bus ISA: el controlador de DMA 8237, la memoria baja para él y la lista de dispositivos ISA detectados (D302) |
| `iso9660.h` | sistema de archivos de los CD/DVD (ISO 9660), solo lectura (D207) |
| `jbd2.h` | el diario JBD2 del ext3/ext4 (D326): reproducir lo pendiente al montar y escribir transacciones |
| `kconf.h` | opciones del kernel que se cambian desde SaguiOS y valen en el siguiente arranque (D353) |
| `lapic.h` | el APIC local (LAPIC) de cada procesador (D157) |
| `mandos.h` | la capa común de mandos y joysticks (D307): USB HID, Xbox 360 con cable y el puerto de juegos |
| `memalta.h` | ventanas temporales del kernel a páginas físicas (D348) |
| `memoria.h` | páginas físicas y el asignador del kernel |
| `modos_video.h` | lista de modos, cambio en caliente y resolución preferida (D058, D059) |
| `montagem.h` | tabla de montajes y la interfaz de los sistemas de archivos (D040) |
| `mouse.h` | mouse/touchpad PS/2 (H19) |
| `msr.h` | lectura protegida de MSR (D092) |
| `nvme.h` | controlador NVMe (D265) |
| `ocio.h` | energía y pantalla: salvapantallas, bloqueo y apagado de la pantalla por inactividad, aviso de batería y «no molestar» (D422) |
| `ocio_efectos.h` | los efectos del salvapantallas (D422) |
| `p256.h` | la curva elíptica NIST P-256 (FIPS 186-4, D.1.2.3; SEC 1) para el SAE de WPA3 (D220, v7.3.2) |
| `paginacao.h` | espacios de direcciones (directorios de páginas) en modo clásico o PAE |
| `painel.h` | paneles táctiles PS/2: Synaptics, ALPS y Elantech en modo absoluto, con gestos (D407) |
| `pci.h` | acceso al espacio de configuración PCI (mecanismo 1, puertos 0xCF8/0xCFC) |
| `pci_realoc.h` | recolocar por debajo de 4 GB una BAR de 64 bits que el firmware dejó arriba (D283) |
| `permisos.h` | quién puede hacer qué: usuarios y llamadas privilegiadas (D255, mejora 098, fase 1) |
| `permisos_dialogo.h` | diálogo de permisos de administrador (D269, mejora 102) |
| `pipe.h` | tuberías entre procesos (H15) |
| `planificador.h` | los planificadores de procesos entre las CPU (D174) |
| `portajogos_conv.h` | las cuentas del puerto de juegos (D307), sin tocar el hardware |
| `portas.h` | acceso a los puertos de E/S del x86 (instrucciones in/out) |
| `portatil.h` | lo propio de los portátiles visto desde el resto del kernel (D403) |
| `prazo.h` | esperas con límite de tiempo real (D199, D207) |
| `processo.h` | procesos y planificador |
| `prt.h` | el enrutado de las interrupciones PCI del firmware (_PRT) con uACPI (D211) |
| `r600_3d.h` | paquetes PM4, sombreadores y valores de inicio del motor 3D de las Radeon R6xx/R7xx (D284) |
| `r600_base.h` | lo que comparten el 2D por DMA (acel_r600.c, D223) y el 3D por el CP (gpu3d_r600.c, D284) de las Radeon R6xx/R7xx: la sonda de solo lectura de la tarjeta (modelo exacto, BAR, MMIO y dónde está la pantalla en el controlador) |
| `r600_dma.h` | paquetes del motor DMA de las Radeon R6xx/R7xx y el reparto de un rectángulo en copias lineales (D223) |
| `radios.h` | las radios y el panel de los HP (hp_wmi.c) y el volcado de tablas ACPI, vistos desde las llamadas al sistema (D405) |
| `raiz.h` | la raíz del sistema en un disco (D379) |
| `ramfs.h` | una carpeta del árbol en RAM montada como volumen (D379) |
| `rapido.h` | copias y rellenos grandes de la interfaz gráfica (D042) |
| `rastro.h` | marcas de depuración que sobreviven a un reinicio en caliente (Amilo, dbg5, 2026-11-19) |
| `rede.h` | tarjetas de red, pila IPv4 (ARP, IP, ICMP, UDP, DHCP, DNS, TCP) y sockets |
| `relogio.h` | tiempo desde el arranque (PIT) y fecha/hora (CMOS) |
| `sae.h` | SAE (Simultaneous Authentication of Equals) de WPA3-Personal con el grupo 19 (P-256), IEEE 802.11-2020 12.4 (D220, v7.3.2) |
| `sb16.h` | la Sound Blaster 16 (ISA) y el ISA PnP que necesitan las Vibra/AWE PnP (D302) |
| `sensores.h` | lo que el kernel pregunta al monitor de hardware (D092) sin pasar por la syscall |
| `sensores_conv.h` | las fórmulas del monitor de hardware (D092) |
| `sessao.h` | inicio de sesión gráfico, cerrar sesión y cambiar de usuario (D340, mejora 098, fase 3) |
| `sgfs.h` | lector del sistema de archivos SGFS (el formato lo genera tools/mksgfs.py) |
| `sha256.h` | SHA-256 (FIPS 180-4), D060 |
| `sistema.h` | llamadas al sistema por «int 0x80» (D007) |
| `smp.h` | arranque de los procesadores secundarios (D157) |
| `sobreposicao.h` | la raíz de la live: ext2 de solo lectura con una capa escribible (D379) |
| `som.h` | el altavoz del PC (bonus B3) |
| `spinlock.h` | cerrojos de espera activa del kernel (D150) |
| `sse2.h` | copias y rellenos con SSE2 (D042) |
| `supervisor.h` | el gestor de servicios visto desde el kernel (D216) |
| `teclado.h` | teclado PS/2 con diseños ES (por defecto) y ABNT2 |
| `telas.h` | varias pantallas (salidas de vídeo) y escritorio extendido o en espejo (mejora 060, D290) |
| `terminal_es.h` | el terminal como archivo (entrada 0 y salida 1/2 de los procesos) |
| `texto.h` | funciones de memoria y de texto escritas para el kernel |
| `tipos.h` | tipos básicos del kernel. Sin libc: se definen los propios |
| `trava.h` | cerrojo de exclusión mutua del kernel (D095) |
| `trava_geral.h` | el cerrojo general del kernel (D165) |
| `troca.h` | swap comprimida en RAM (D352, mejora 125 fase 3b, mejora 069 fase 3) |
| `tunel.h` | la interfaz del túnel wg0 (kernel/rede/tunel.c, D363) |
| `tv.h` | terminal virtual (pseudoterminal): un terminal para el Terminal en ventana (D036) |
| `uacpi_sagui.h` | lo que comparten la capa de uACPI (uacpi_sagui.c), el controlador embebido (ec.c) y la batería (bateria.c) |
| `uacpi_types.h` | los tipos de uACPI sobre los del kernel (D192) |
| `udf.h` | sistema de archivos UDF de los CD/DVD, solo lectura (D233) |
| `usb.h` | controladores EHCI y xHCI, dispositivos USB, hubs, almacenamiento y HID (fase 6; xHCI y HID en la v7.3, D214) |
| `versao.h` | nombre y versión del sistema, en un solo lugar (D101) |
| `vfs.h` | capa de archivos virtual: una ruta sola para todo (PRD 4.3) |
| `video.h` | modo gráfico VESA con doble búfer (H17) |
| `wc.h` | write-combining en el framebuffer (D057) |
| `wifi.h` | Wi-Fi cliente con el pincho USB Realtek RTL8188EU (D215) |
| `wifi_n.h` | rendimiento del Wi-Fi del pincho RTL8188EU (D238): el desglose de una transferencia de recepción con varias tramas (agregación del chip), los elementos de la 802.11n (HT) y de WMM, la respuesta a un ADDBA y la escalera de tasas |
| `wifi_pcie.h` | más Wi-Fi integradas PCIe de portátiles (D410): reconocimiento por PCI de la familia Atheros ath9k (AR9285, AR9287, AR9380...), Broadcom BCM4313 y Ralink RT3090 (experimental: solo identifica y registra) |
| `wmi_wdg.h` | el puente WMI de ACPI (PNP0C14) sin hardware ni uACPI: el análisis de _WDG y los formatos de los búferes de HP (D405) |
| `wpa.h` | criptografía y formatos del Wi-Fi cliente (D215): SHA-1, HMAC-SHA1, PBKDF2 (la PMK), el PRF de la 802.11i, AES-128 con su envoltura de claves |
| `wpa_cifra.h` | las primitivas criptográficas del Wi-Fi que no son las de WPA2-CCMP (D220, v7.3.2): MD5 y HMAC-MD5 (descriptor 1 de EAPOL-Key), RC4, CRC-32, Michael y la mezcla de clave de TKIP, el encapsulado TKIP y WEP |
| `xhci.h` | controlador USB 3.0 (xHCI) (D155, D214) |


### 6.12 kernel/terceros/ (código de otros proyectos: uACPI, BSD, Linux, MIT)

| Fichero | Qué hace |
| --- | --- |
| `bsd/alc/alc_regs.h` | registros y descriptores de las Ethernet Atheros AR813x/AR815x (de FreeBSD) |
| `bsd/athn/ar9003reg.h` | registros de la familia Wi-Fi Atheros AR9003 (de BSD) |
| `bsd/athn/ar9485_datos.h` | tablas de inicio del Atheros AR9485 (de BSD) |
| `bsd/athn/athnreg.h` | registros comunes de los Atheros AR9003 (de BSD) |
| `bsd/coretemp/coretemp_tjmax.h` | TjMax por modelo de CPU Intel (de FreeBSD coretemp) |
| `bsd/rtwn/r92creg_88e.h` | registros y tablas de inicio de los Realtek RTL8188E (de OpenBSD) |
| `bsd/rtwn/r92creg_92c.h` | registros y tablas de inicio de los Realtek RTL8188CU/8192CU (de OpenBSD) |
| `bsd/run/rt2860reg.h` | registros y descriptores de los Ralink RT2870/RT3070/RT5370 (de OpenBSD) |
| `linux/hda/idt_92hd71bxx.h` | datos del códec IDT/SigmaTel 92HD71Bxx (de Linux) |
| `linux/hwmon/sensores_tabelas.h` | tabla de desvío Tctl/Tdie de los Ryzen (de Linux k10temp) |
| `linux/r8169/r8169_inicio.h` | secuencias de arranque de las Realtek RTL8168/8111 (de Linux) |
| `linux/r8169/r8169_versoes.h` | versiones y nombres de chip de las Realtek RTL8168/8169 (de Linux) |
| `mit/atom/atom-bits.h` | bits y constantes del intérprete de la BIOS ATOM de las Radeon (MIT) |
| `mit/atom/atom.c` | intérprete de las tablas de órdenes de la BIOS ATOM de las Radeon (MIT) |
| `mit/atom/atom.h` | interfaz del intérprete ATOM de las Radeon (MIT) |
| `mit/atom/atom_compat.h` | compatibilidad del intérprete ATOM con el kernel (MIT) |
| `mit/nv/nv_2d.h` | métodos 2D y escrituras de PGRAPH/PFIFO de las NVIDIA GeForce 6/7 (de xf86-video-nv) |
| `uacpi/include/uacpi/acpi.h` | cabecera principal de las estructuras ACPI |
| `uacpi/include/uacpi/context.h` | contexto global de uACPI |
| `uacpi/include/uacpi/event.h` | API pública de eventos ACPI (GPE y eventos fijos) |
| `uacpi/include/uacpi/helpers.h` | funciones auxiliares de uACPI |
| `uacpi/include/uacpi/internal/compiler.h` | macros del compilador para uACPI (uso interno) |
| `uacpi/include/uacpi/internal/context.h` | copia local de la FADT ya comprobada y convertida |
| `uacpi/include/uacpi/internal/dynamic_array.h` | arrays dinámicos de uACPI (uso interno) |
| `uacpi/include/uacpi/internal/event.h` | eventos ACPI: interfaz interna (GPE y eventos fijos) |
| `uacpi/include/uacpi/internal/helpers.h` | funciones auxiliares internas de uACPI |
| `uacpi/include/uacpi/internal/interpreter.h` | intérprete de AML: interfaz interna |
| `uacpi/include/uacpi/internal/io.h` | escritura de campos AML (uso interno) |
| `uacpi/include/uacpi/internal/log.h` | mensajes de depuración de uACPI |
| `uacpi/include/uacpi/internal/mutex.h` | mutex de uACPI sobre la API del kernel |
| `uacpi/include/uacpi/internal/namespace.h` | espacio de nombres ACPI: interfaz interna |
| `uacpi/include/uacpi/internal/notify.h` | notificaciones de ACPI: interfaz interna |
| `uacpi/include/uacpi/internal/opcodes.h` | códigos de operación AML que entiende el intérprete |
| `uacpi/include/uacpi/internal/opregion.h` | regiones de operación (acceso a hardware desde AML): interfaz interna |
| `uacpi/include/uacpi/internal/osi.h` | soporte de _OSI: interfaz interna |
| `uacpi/include/uacpi/internal/registers.h` | registros de hardware de ACPI: interfaz interna |
| `uacpi/include/uacpi/internal/resources.h` | recursos ACPI (IRQ, E/S, memoria): conversión desde AML |
| `uacpi/include/uacpi/internal/shareable.h` | objetos compartidos del intérprete (uso interno) |
| `uacpi/include/uacpi/internal/stdlib.h` | funciones de libc mínimas que usa uACPI |
| `uacpi/include/uacpi/internal/tables.h` | tablas ACPI: interfaz interna |
| `uacpi/include/uacpi/internal/types.h` | objetos comunes del intérprete AML |
| `uacpi/include/uacpi/internal/utilities.h` | utilidades internas (nombres, ids EISA) |
| `uacpi/include/uacpi/io.h` | API pública de acceso a los puertos y la memoria |
| `uacpi/include/uacpi/kernel_api.h` | API que el kernel de SaguiOS implementa para uACPI |
| `uacpi/include/uacpi/log.h` | API pública de los mensajes de uACPI |
| `uacpi/include/uacpi/namespace.h` | API pública del espacio de nombres ACPI |
| `uacpi/include/uacpi/notify.h` | API pública de notificaciones |
| `uacpi/include/uacpi/opregion.h` | API pública de regiones de operación |
| `uacpi/include/uacpi/osi.h` | API pública de _OSI |
| `uacpi/include/uacpi/platform/arch_helpers.h` | identificador de hilo de la plataforma (se adapta a cada sistema) |
| `uacpi/include/uacpi/platform/atomic.h` | operaciones atómicas para uACPI |
| `uacpi/include/uacpi/platform/compiler.h` | atributos y macros del compilador |
| `uacpi/include/uacpi/platform/config.h` | opciones de compilación de uACPI (contexto, tamaños) |
| `uacpi/include/uacpi/platform/libc.h` | funciones de libc que usa uACPI (con implementación por defecto) |
| `uacpi/include/uacpi/platform/types.h` | tipos específicos de la plataforma |
| `uacpi/include/uacpi/registers.h` | API pública de los registros ACPI |
| `uacpi/include/uacpi/resources.h` | API pública de recursos (_CRS, _PRS) |
| `uacpi/include/uacpi/sleep.h` | API pública de estados de sueño y apagado |
| `uacpi/include/uacpi/status.h` | códigos de estado de uACPI |
| `uacpi/include/uacpi/tables.h` | API pública de las tablas ACPI |
| `uacpi/include/uacpi/types.h` | tipos públicos de uACPI |
| `uacpi/include/uacpi/uacpi.h` | cabecera principal de uACPI: inicialización y apagado |
| `uacpi/include/uacpi/utilities.h` | API pública de utilidades (evaluar métodos, nombres) |
| `uacpi/source/default_handlers.c` | manejadores por defecto de las regiones de operación (memoria, E/S, PCI) |
| `uacpi/source/event.c` | eventos ACPI: GPE (eventos de propósito general) y eventos fijos |
| `uacpi/source/interpreter.c` | intérprete de AML: la máquina virtual que ejecuta los métodos ACPI |
| `uacpi/source/io.c` | lectura y escritura de los campos AML (fields) sobre el hardware |
| `uacpi/source/mutex.c` | mutex del intérprete sobre la API del kernel |
| `uacpi/source/namespace.c` | espacio de nombres ACPI: árbol de objetos (la raíz `\`, `_SB`, etc.) |
| `uacpi/source/notify.c` | notificaciones (Notify) a los dispositivos |
| `uacpi/source/opcodes.c` | tabla de códigos AML y su análisis |
| `uacpi/source/opregion.c` | regiones de operación: acceso a hardware desde los métodos AML |
| `uacpi/source/osi.c` | respuesta a _OSI (qué sistema dice ser el kernel) |
| `uacpi/source/registers.c` | lectura y escritura de los registros de hardware de ACPI |
| `uacpi/source/resources.c` | recursos ACPI (_CRS, _PRS): IRQ, E/S y memoria |
| `uacpi/source/sleep.c` | estados de sueño y apagado (S5) |
| `uacpi/source/stdlib.c` | funciones de libc mínimas que usa uACPI |
| `uacpi/source/tables.c` | tablas ACPI: búsqueda e instalación (DSDT, SSDT, FADT...) |
| `uacpi/source/types.c` | objetos del intérprete: enteros, búferes, cadenas y paquetes |
| `uacpi/source/uacpi.c` | inicialización, carga de tablas y apagado de uACPI |
| `uacpi/source/utilities.c` | utilidades: nombres ACPI, ids EISA y evaluación de métodos |


---

## 7. Dónde leer más

| Tema | Documento |
| --- | --- |
| Arquitectura por capas y lo que hay en cada una | [`docs/arquitecturas.md`](arquitecturas.md) (procesadores) y el [README](../README.md) |
| Almacenamiento: raíz en disco, live y «todo a RAM» (D379) | [`docs/almacenamiento.md`](almacenamiento.md) |
| Arranque con UEFI | [`docs/uefi.md`](uefi.md) |
| Varios procesadores y hilos | [`docs/multihilo.md`](multihilo.md) |
| Enlace dinámico y bibliotecas compartidas | [`docs/enlace-dinamico.md`](enlace-dinamico.md) |
| Tarjetas gráficas y 3D por GPU | [`docs/gpus.md`](gpus.md), [`docs/gpu3d.md`](gpu3d.md) |
| Código de terceros y licencias | [`docs/terceros.md`](terceros.md), [`docs/licencia.md`](licencia.md) |
| Todas las decisiones de diseño (D001 a D4xx) | [`docs/decisoes.md`](decisoes.md) |
| Cómo se compila y se prueba | [`docs/make.md`](make.md) y el README |

---

> *Esta guía se generó a partir del código el 2026-11-21; si algo no cuadra, manda el código.*
