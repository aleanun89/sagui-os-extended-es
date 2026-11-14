# Mejoras de SaguiOS

Una mejora por fichero: `NNN-nombre-corto.md` (número de 3 cifras, en orden de alta).
Copia [`_plantilla.md`](_plantilla.md) para crear una nueva.

## Estados

| Estado | Significado |
| --- | --- |
| `propuesta` | Idea anotada; sin decidir |
| `aprobada` | Decidido hacerla |
| `en curso` | Alguien está trabajando en ella |
| `hecha` | Terminada y con tests pasando |
| `descartada` | No se hará (explica por qué en el fichero) |

## Índice

| Nº | Mejora | Estado | Prioridad |
| --- | --- | --- | --- |
| 001 | [Interfaz y documentación en español](001-traduccion-espanol.md) | hecha | alta |
| 002 | [Teclado ES y ABNT2 seleccionable](002-teclado-es-abnt2.md) | hecha | alta |
| 003 | [Compilador de C dentro de SaguiOS](003-compilador-c.md) | hecha; `float`/`double` en curso (rama `cc-float`) | alta |
| 004 | [MicroPython dentro de SaguiOS](004-micropython.md) | hecha, con `float` (SSE2), `math`, `cmath` y `time` | alta |
| 005 | [Entorno de compilación sin sudo](005-entorno-sin-sudo.md) | hecha | media |
| 006 | [Control de versiones con git](006-git.md) | hecha | alta |
| 007 | [Recordar la distribución de teclado entre arranques](007-teclado-persistente.md) | hecha (D112) | baja |
| 008 | [FPU x87, MMX y SSE para los programas](008-fpu-mmx-sse.md) | hecha | alta |
| 009 | [Ratón con posición absoluta en máquinas virtuales](009-raton-absoluto-vm.md) | hecha | alta |
| 010 | [Terminal virtual (pty) para el Terminal en ventana](010-terminal-virtual.md) | hecha | alta |
| 011 | [Shell más completo, tipo sh/bash](011-shell-completo.md) | hecha | alta |
| 012 | [Usar la RAM por encima de 896 MB](012-memoria-alta.md) | en curso: hasta ~1,9 GB hecho (012a); `kmap` y caché de bloques pendientes | media |
| 013 | [Sistemas de archivos permanentes: `mount`, ext2 (escritura) y ext4 (lectura), disco ATA](013-sistemas-de-archivos.md) | hecha (ATA sin probar en el G470) | alta |
| 014 | [`infopc`: información del PC estilo CPU-Z](014-infopc.md) | hecha | media |
| 015 | [SSE2 en gráficos y memoria](015-sse2-graficos.md) | hecha (medir en el G470) | media |
| 016 | [Gestor de tareas gráfico](016-gestor-tareas.md) | hecha | media |
| 017 | [Varios núcleos (SMP) e hilos de usuario](017-smp-multinucleo.md) | propuesta (v9-v10) | media |
| 018 | [Ventanas redimensionables, maximizar y pantalla completa](018-ventanas-redimensionables.md) | hecha | alta |
| 019 | [Visor y editor de Markdown](019-markdown.md) | hecha | media |
| 020 | [Red: e1000/RTL8139, IPv4, TCP, `ping`, `curl` y el navegador Cipó](020-red.md) | hecha (sin driver probado para la Ethernet del G470) | alta |
| 021 | [Rueda del ratón y Markdown redimensionable](021-rueda-raton.md) | hecha | alta |
| 022 | Meta de tamaño de imagen a 6 MB (D050) | hecha | media |
| 023 | [Menú contextual con el botón derecho](023-menu-contextual.md) | hecha | media |
| 024 | [Visor de imágenes y biblioteca de imágenes (BMP, PNM, PNG)](024-visor-imagenes.md) | hecha | media |
| 025 | [Paint, programa de dibujo](025-paint.md) | hecha | media |
| 026 | [Vídeo: write-combining, resolución al arrancar y cambio de modo en caliente](026-video-wc-resoluciones.md) | hecha (WC real y modos VESA del G470 sin probar) | media |
| 027 | [HTTPS: TLS 1.2/1.3, certificados X.509, `certificados`, `wget`](027-tls-certificados.md) | hecha (sin probar en el G470) | alta |
| 028 | [Port a ARM (Raspberry Pi)](028-port-arm-raspberry.md) | opción futura, **no hacer por ahora** | baja |
| 029 | [Teclado y ratón USB (HID, OHCI, UHCI, split transactions)](029-usb-teclado-raton.md) | propuesta | media |
| 030 | [ACPI básico: apagar, botón de encendido y tablas](030-acpi-basico.md) | propuesta | media |
| 031 | [Barra de tareas, menú Inicio y submenús](031-barra-tareas-inicio.md) | hecha | media |
| 032 | [MicroPython: biblioteca estándar, archivos, unicode y red (`socket`, `ssl`, `requests`)](032-micropython-stdlib-red.md) | fases 1 y 2 hechas; fase 3 (módulo `sagui` de gráficos y ventanas) pendiente | media |
| 033 | [Python: módulo `sagui` (ventanas y dibujo), tkinter y Python sin SSE2](033-python-sagui-tkinter.md) | hecha | media |
| 034 | [Emulador CHIP-8 / SUPER-CHIP / XO-CHIP, ensamblador `chip8asm` y juegos propios](034-chip8.md) | hecha (sonido real con la 041; pendiente: Game Boy) | media |
| 035 | [Arquitectura de almacenamiento y memoria: raíz en disco, caché, carga bajo demanda, bibliotecas compartidas](035-arquitectura-almacenamiento.md) | propuesta (diseño) | alta |
| 036 | [Compatibilidad con PC x86 antiguos y SaguiGL (3D tipo OpenGL ES 1.1, motores Radeon)](036-compatibilidad-pc-antiguos.md) | en curso: fase A hecha (sin SSE2 en `cc`, ISO, 16/15 bits, `infopc --informe`; D076-D078) salvo MicroPython sin SSE2; fases B-D, hoja de ruta | alta |
| 037 | [Mico, editor de código](037-mico-editor-codigo.md) | aprobada (tras la 031) | media |
| 038 | [Pestaña «Red» en el Administrador de tareas (y loopback)](038-red-administrador-tareas.md) | hecha | media |
| 039 | [Zona horaria, hora UTC en el kernel y «Fecha y hora»](039-zona-horaria.md) | hecha (RTC local sin probar en el G470) | media |
| 040 | [Compilador de Pascal (`pascal`): estilo Turbo Pascal, unidades `crt` y `sagui`](040-compilador-pascal.md) | hecha (D085-D087); pendiente: colores y `keypressed` en el terminal | media |
| 041 | [Sonido: AC'97 e Intel HDA, mezclador y WAV](041-sonido-ac97-hda.md) | hecha en QEMU (D088-D090); falta probar en un portátil real | alta |
| 042 | [Más componentes en la libsagui (lienzo con imágenes, barra de progreso...) y tkinter sobre ellos](042-componentes-libsagui.md) | en curso (v6.1): lienzo, barra de progreso y casilla hechos (D126); biblioteca de iconos hecha (D135); la barra de progreso del fondo, en la v6.1 | media |
| 043 | [Cliente de chat con IA y asistente en Mico (OpenRouter u otro endpoint configurable)](043-mico-asistente-ia.md) | en curso (v6.2): el comando `ia` del terminal, con streaming, «Detener» y URL configurable, está hecho y probado (D134); falta el panel «Asistente» del Mico | media |
| 044 | [Red por USB: driver genérico CDC-NCM/ECM (RTL8156 y similares)](044-red-usb-cdc-ncm.md) | aprobada (en cola, delante de la 042) | alta |
| 045 | [Rendimiento de la red: ventana grande, control de congestión, ACK retardado, SACK](045-rendimiento-red.md) | propuesta (en cola) | media |
| 046 | [USB 3 (xHCI): pendrives, teclado y ratón en PC modernos](046-xhci-usb3.md) | propuesta | alta |
| 047 | [Personalización del escritorio (fondo, colores) y JPEG en el visor](047-personalizacion-escritorio-jpeg.md) | parcial: fondo y acento (D112) y **decodificador de JPEG baseline** hechos (v6.1); faltan los temas y la vista previa | media |
| 048 | [Vídeo: códec abierto a 240p/30 fps (MJPEG, MPEG-1) y reproductor](048-video-reproductor.md) | en curso (MJPEG en AVI hecho; el MPEG-1 pendiente, D129) | media |
| 049 | [HMKD: Markdown con imágenes y vídeo, la «web» de SaguiOS](049-formato-hmkd.md) | propuesta | media |
| 050 | [Monitor de hardware: temperatura, frecuencias reales, P-states, cachés...](050-temperatura-cpu.md) | hecha en QEMU (D092); falta PC real y la placa | baja-media |
| 051 | [SaguiMark: benchmark de CPU, FPU, SIMD, memoria y gráficos 2D con puntuación en infopc](051-benchmark-saguimark.md) | hecha en QEMU (D093); referencia provisional, falta el HP dv7 | media |
| 052 | [SSH: cliente y servidor de *jails* (cuentas encerradas en una carpeta)](052-ssh-jails.md) | en curso: servidor de administración hecho (D250, v8.0b5); faltan cliente y jails | media |
| 053 | [Formatos de audio libres: IMA ADPCM, MP3, FLAC y Ogg Vorbis](053-formatos-audio-libres.md) | propuesta | media |
| 054 | [Motor 3D con aceleración por software (SaguiGL) y plan de drivers 3D de AMD](054-motor-3d-y-drivers-amd.md) | propuesta (v7) | media |
| 055 | [Gráficas integradas y chipsets Intel: blitter 2D, AHCI, e1000e, 3D Gen3 y resolución nativa](055-intel-igpu-y-chipsets.md) | propuesta (AHCI v6; resto v7-v8) | media-alta |
| 056 | [Compresor y descompresor multiformato: tar, gzip (.tar.gz) y zip](056-compresor-tar-gzip-zip.md) | propuesta (v5) | media |
| 057 | [Compilación con GPL y sin GPL (dos variantes de la imagen)](057-compilacion-con-y-sin-gpl.md) | hecha en `licencias-build` (D109; v5.1) | alta |
| 056 | [Compresor y descompresor multiformato: tar, gzip (.tar.gz) y zip](056-compresor-tar-gzip-zip.md) | hecha (D103; sin visor de contenido ni barra de progreso) | media |
| 057 | [Compilación con GPL y sin GPL (dos variantes de la imagen)](057-compilacion-con-y-sin-gpl.md) | propuesta (v5.1) | alta |
| 058 | [Mejoras de interfaz y del explorador de archivos (ocultos, vistas, copiar/pegar, ajuste de ventanas...)](058-interfaz-y-explorador.md) | propuesta (v6) | media-alta |
| 059 | [Bluetooth por adaptador USB (HID, audio A2DP, OBEX/PAN)](059-bluetooth-usb.md) | propuesta (v9-v11) | media |
| 060 | [Varias pantallas (extender y duplicar)](060-multiples-pantallas.md) | propuesta (v8-v10) | media |
| 061 | [Control de la frecuencia de la CPU (P-states) con límite de 79 °C](061-frecuencia-cpu.md) | hecha en `licencias-build` (D109; v5.1) | alta |
| 061 | [Control de la frecuencia de la CPU (P-states) con límite de 79 °C](061-frecuencia-cpu.md) | hecha en QEMU (simulada, D106); falta el dv7 | alta |
| 062 | [Arranque UEFI](062-arranque-uefi.md) | propuesta (v10) | media |
| 063 | [Mejoras del navegador Cipó](063-navegador.md) | hecha (D110; quedan JPEG, pestañas, caché) | media |
| 012a | Memoria del kernel hasta ~1,9 GB, frontera en 2 GB (D049, primera parte de [012](012-memoria-alta.md)) | hecha | alta |

Al crear o cerrar una mejora, actualiza esta tabla.
| 064 | [SQLite 3 para C y Python](064-sqlite3.md) | hecha (v5.3; rama `sqlite`) | media |
| 065 | [Framework web para Python (Microdot) y capa compatible con Bottle](065-bottle-web-python.md) | propuesta (v8) | media |
| 066 | [Clientes de MySQL/MariaDB y MongoDB para Python](066-clientes-mysql-mongodb.md) | propuesta (mucho más adelante) | media |
| 067 | [lite-monkey: motor NoSQL propio en C (puerto 7979) y cliente Python](067-lite-monkey.md) | en curso (v5.4) | media-alta |
| 069 | [Que el sistema no muera sin RAM: OOM, swap comprimida en RAM y partición de swap](069-memoria-swap.md) | idea (mucho más adelante) | media |
| 067 | [lite-monkey: motor NoSQL propio en C (puerto 7979) y cliente Python](067-lite-monkey.md) | hecha (v5.4, D117) | media-alta |
| 068 | [Imagen de disco con varias particiones (/home, /tmp, /logs) y extendidas](068-imagen-varias-particiones.md) | hecha (v5.4; rama `imagen-particiones`) | media |
| 070 | [Aplicaciones extra en /app, repositorio en red y SDK para crear apps (también con IA)](070-apps-repositorio-sdk.md) | idea (futuro) | media-alta |
| 071 | [Emulador de NES](071-emulador-nes.md) | propuesta (v6.2) | media |
| 072 | [Wi-Fi: Realtek RTL8188EU (USB), Atheros AR9485 (PCIe) y pila 802.11 con WPA2](072-wifi-rtl8188eu.md) | propuesta (v8) | media-alta |
| 073 | [Cortafuegos (firewall)](073-cortafuegos.md) | propuesta (v8) | media |
| 074 | [Gestor de servicios (programas en segundo plano) y cron](074-servicios-y-cron.md) | hecha, pendiente de compilar (v7.3.1, D216) | media-alta |
| 075 | [Mejoras del sonido configurables desde el sistema](075-mejoras-audio.md) | propuesta (v6.1, v6.2, v8) | media |
| 076 | [Sistemas de archivos: exFAT y escritura en ext3/ext4](076-sistemas-archivos-exfat-ext34.md) | propuesta (v7) | media-alta |
| 094 | [Carencias del sistema que reveló el port de NetSurf: libc, memoria, red, tiempo y teclado (reparto v7.1 y v7.2)](094-carencias-libc-sistema.md) | aprobada (v7.1: 4 puntos; v7.2: bloques A-E) | alta |
| 095 | [Arrancar del CD/DVD e instalar en el disco interno (ATAPI, ISO 9660, El Torito)](095-instalar-desde-cd.md) | hecha en `v7.2-cd-instalar` (D207); falta probarla en el Amilo | alta |
| 096 | [OpenLoco (Chris Sawyer's Locomotion) como port](096-openloco.md) | propuesta (v9.0) | media |
| 097 | [Escritorio remoto VNC (servidor y cliente)](097-vnc.md) | hecha (v8.0b6, D251-D254) | alta |
| 098 | [Usuarios y permisos](098-usuarios-permisos.md) | aprobada (fase 1 v8.0b7; 2 y 3 v8.5) | alta |
| 099 | [r8169 de sala limpia (sustituir el código de Linux GPL-2.0-only)](099-r8169-sala-limpia.md) | propuesta (v8.4 o cuando toque la red; la opción A, D259, ya hecha) | media |
| 100 | [Vídeo H.264/AAC en el Cipó (reproductor de robson-apk)](100-cipo-video-h264.md) | aprobada (v8.0.1) | media |
| 101 | [Ordenar los iconos del escritorio](101-ordenar-iconos-escritorio.md) | aprobada (v8.0) | media |
| 102 | [Diálogo gráfico de permisos de administrador (y avisos que se leen enteros)](102-dialogo-permisos-admin.md) | aprobada (v8.0) | alta |
