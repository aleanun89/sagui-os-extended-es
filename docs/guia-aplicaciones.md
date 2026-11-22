# Guía de aplicaciones de SaguiOS-Extended

[Español](guia-aplicaciones.md) · [Português](guia-aplicaciones_PT.md) · [English](guia-aplicaciones_EN.md)

> Para quien quiere saber **qué programas trae SaguiOS** y **dónde está el código** de cada uno.
> Las aplicaciones se organizan como el menú **Inicio**. Para cada una se indica el nombre que
> se ve, la orden o el fichero en la imagen, una línea de lo que hace, la ruta de su código y la
> decisión de diseño (`D…`) más importante, que está en [`decisoes.md`](decisoes.md).
> El kernel está explicado en la [guía del kernel](guia-kernel.md).

**Índice**

1. [Cómo están repartidos los programas: `/bin` y `/apps`](#1-cómo-están-repartidos-los-programas-bin-y-apps)
2. [Menú Inicio, por categorías](#2-menú-inicio-por-categorías)
3. [Desarrollo: compiladores y herramientas](#3-desarrollo-compiladores-y-herramientas)
4. [Órdenes de consola: `sagsh` y `/bin`](#4-órdenes-de-consola-sagsh-y-bin)
5. [Bibliotecas](#5-bibliotecas)
6. [Ports de terceros](#6-ports-de-terceros)
7. [Dónde leer más](#7-dónde-leer-más)

---

## 1. Cómo están repartidos los programas: `/bin` y `/apps`

SaguiOS tiene dos sitios para los ejecutables:

| Carpeta | Qué hay | Cómo llega ahí |
| --- | --- | --- |
| `/bin` | Las órdenes básicas y las aplicaciones pequeñas | En la base del sistema, la que se carga al arrancar |
| `/apps/bin` | Las aplicaciones grandes: el navegador, los juegos, los compiladores | En la partición **SAGUI-APPS**, que se monta en `/apps` (decisiones D194 y D324) |

La lista de qué va a `/apps` está en el `Makefile` (variable `APPS_MOVER_FIJO` y
`APPS_PROGRAMAS`). Así el arranque en RAM pesa menos. Las demás reglas de compilación de cada
programa están en su carpeta, en un fichero `.mk` (por ejemplo, `userland/terceros/netsurf/netsurf.mk`).

> **Regla del menú:** una entrada de `/apps` solo aparece en el menú Inicio si su ejecutable existe
> (función `programa_disponivel()` de `kernel/gui/barra.c`, decisión D194). Por eso las entradas de los
> juegos grandes no salen en una imagen que no los trae.

**Símbolo `†`:** los programas con esta marca no están en la variante **sin GPL** (`make SEM_GPL=1`,
decisión D109): su código es GPL de terceros y se aísla. Ver [`terceros.md`](terceros.md).

---

## 2. Menú Inicio, por categorías

El menú Inicio se define en la tabla `programas[]` de `kernel/gui/barra.c`. Hay siete grupos.
Las tablas de esta sección salen de esa tabla: cada fila es una entrada del menú, en el mismo orden.

### 2.1 Accesorios

| Nombre en el menú | Orden (ruta) | Qué hace | Código | Decisión |
| --- | --- | --- | --- | --- |
| Archivos | `/bin/arquivos` | explorador de archivos y carpetas | `userland/programas/arquivos.c` | — |
| Calculadora | `/bin/calculadora` | calculadora | `userland/programas/calculadora.c` | — |
| Editor de texto | `/bin/editor` | editor de texto sencillo | `userland/programas/editor.c` | — |
| Recortes | `/bin/recortes` | recortes de la pantalla | `userland/programas/recortes.c` | D125 |
| Iconos | `/bin/icones` | biblioteca de iconos a la vista | `userland/programas/icones.c` | D135 |
| Música | `/apps/bin/musica` | reproductor de música sencillo | `userland/programas/musica.c` | — |
| MIDI | `/bin/midi` | reproductor de archivos MIDI | `userland/programas/midi.c` | D190 |
| Markdown | `/bin/markdown` | visor y editor de Markdown | `userland/programas/markdown.c` | — |
| Mico (editor de código) | `/apps/bin/mico` | editor de código (Mico) | `userland/programas/mico/` | — |
| VisualPythonS (diseñador) | `/apps/bin/python /lib/python/vps/disenador.py` | diseñador visual con Python (se ejecuta con MicroPython) | `rootfs/lib/python/vps/disenador.py` | D297 |
| Explorador SQLite | `/apps/bin/python /lib/python/basedatos/explorador_sqlite.py` | explorador de bases de datos SQLite (con MicroPython) | `rootfs/lib/python/basedatos/explorador_sqlite.py` | D333 |
| Explorador lite-monkey | `/apps/bin/python /lib/python/basedatos/explorador_litemonkey.py` | explorador de la base de datos lite-monkey (con MicroPython) | `rootfs/lib/python/basedatos/explorador_litemonkey.py` | D333 |
| Pluma (procesador de textos) | `/apps/bin/pluma` | procesador de textos | `userland/programas/pluma.c` | D291 |
| Traductor | `/apps/bin/traductor` | editor de traducciones (al estilo de Poedit) | `userland/programas/traductor.c` | D335 |
| Macaco (hoja de cálculo) | `/apps/bin/macaco` | hoja de cálculo | `userland/programas/macaco.c` | D142 |
| Tucán (presentaciones) | `/apps/bin/tucan` | editor de presentaciones | `userland/programas/tucan.c` | D369 |
| Visor de PDF | `/apps/bin/visorpdf` | visor de PDF (motor en userland/pdf/) | `userland/programas/visorpdf.c` | D139 |
| IA (chat) | `/apps/bin/ia` | chat con IA en la línea de órdenes (biblioteca en userland/ia/) | `userland/programas/ia.c` | D139 |
| Chat IA | `/apps/bin/chatia` | chat con IA en ventana (biblioteca en userland/ia/) | `userland/programas/chatia.c` | D143 |

### 2.2 Gráficos

| Nombre en el menú | Orden (ruta) | Qué hace | Código | Decisión |
| --- | --- | --- | --- | --- |
| Paint | `/apps/bin/paint` | programa de dibujo | `userland/programas/paint.c` | — |
| Visor de imágenes | `/bin/visor` | visor de imágenes | `userland/programas/visor.c` | — |
| PlanoMini (CAD 2D) | `/apps/bin/planomini` | mini CAD 2D (planos) | `userland/planomini/` | D312 |
| Reproductor de vídeo | `/apps/bin/reproductor` | reproductor de vídeo | `userland/programas/reproductor.c` | D139 |
| Cámara | `/apps/bin/camara` | cámara web (UVC) | `userland/programas/camara.c` | D277 |

### 2.3 Internet

| Nombre en el menú | Orden (ruta) | Qué hace | Código | Decisión |
| --- | --- | --- | --- | --- |
| Cipó (navegador) | `/apps/bin/cipo` | navegador web Cipó (motor en userland/web/) | `userland/programas/cipo.c` | — |
| NetSurf (navegador) | `/apps/bin/netsurf` | navegador NetSurf (código de terceros) | `userland/terceros/netsurf/` | D184 † |
| Correo | `/apps/bin/correo` | cliente de correo (biblioteca en userland/mail/) | `userland/programas/correo.c` | D146 |
| Escritorio remoto (VNC) | `/bin/vnc` | cliente de escritorio remoto VNC | `userland/programas/vnc.c` | D254 |

### 2.4 Multimedia

| Nombre en el menú | Orden (ruta) | Qué hace | Código | Decisión |
| --- | --- | --- | --- | --- |
| Sintonía (reproductor) | `/apps/bin/sintonia` | reproductor de música con listas (m3u, pls) y emisoras por HTTPS | `userland/programas/sintonia.c` | D225 |
| Grabadora de sonido | `/apps/bin/grabadora` | grabadora de sonido desde el micrófono | `userland/programas/grabadora.c` | D292 |
| Conversor de audio | `/apps/bin/convertir-audio` | conversor entre MP3, FLAC y WAV (el codificador MP3 es Shine, en userland/terceros/shine/) | `userland/programas/convertir-audio.c` | D339 |

### 2.5 Educación

| Nombre en el menú | Orden (ruta) | Qué hace | Código | Decisión |
| --- | --- | --- | --- | --- |
| GebraMini (geometría dinámica) | `/apps/bin/gebramini` | geometría dinámica | `userland/gebramini/` | D309 |
| Jabuti (programar jugando) | `/apps/bin/jabuti` | programación con bloques para niños | `userland/jabuti/` | D377 |
| Laboratorio de C | `/apps/bin/labc` | laboratorio de C: editor, compilador (libtcc) y ejemplos | `userland/programas/labc/` | D414 |
| Actividades JClic | `/apps/bin/jclic` | visor de actividades educativas JClic | `userland/programas/jclic.c` | D185 |
| Actividades JClic (bajar) | `/apps/bin/jclicbajar` | descargador de actividades JClic del repositorio | `userland/programas/jclicbajar.c` | D185 |

### 2.6 Juegos

| Nombre en el menú | Orden (ruta) | Qué hace | Código | Decisión |
| --- | --- | --- | --- | --- |
| Sagui Atrapa Bananas | `/apps/bin/banana` | juego Sagui Pega Banana | `userland/programas/banana.c` | — |
| Solitario Spider | `/apps/bin/spider` | solitario Spider | `userland/programas/spider.c` | D173 |
| Buscaminas | `/apps/bin/buscaminas` | juego Buscaminas | `userland/programas/buscaminas.c` | D248 |
| AoEN | `/apps/bin/aoen` | AoEN, juego de estrategia en tiempo real | `userland/juegos/aoen/` | D382 |
| OpenTyrian | `/apps/bin/opentyrian` | OpenTyrian (shoot-em-up, código de terceros) | `userland/terceros/opentyrian/` | D180 † |
| OpenTyrian (pantalla completa) | `/apps/bin/opentyrian --pantalla-completa` | OpenTyrian (shoot-em-up, código de terceros) (en pantalla completa) | `userland/terceros/opentyrian/` | D204 † |
| Freeciv | `/apps/bin/freeciv-sdl2` | Freeciv, estrategia por turnos (cliente SDL 2) | `userland/terceros/freeciv/` | D374 † |
| OpenTTD | `/apps/bin/openttd` | OpenTTD. Su código no está en esta rama: la entrada solo sale si existe `/apps/bin/openttd` | `—` | D375 † |
| OpenJazz | `/apps/bin/openjazz` | OpenJazz, plataformas (Jazz Jackrabbit) | `userland/terceros/openjazz/` | D228 † |
| OpenJazz (pantalla completa) | `/apps/bin/openjazz --pantalla-completa` | OpenJazz, plataformas (Jazz Jackrabbit) (en pantalla completa) | `userland/terceros/openjazz/` | D228 † |
| Command & Conquer | `/apps/bin/conquer` | Command & Conquer (Vanilla Conquer) | `userland/terceros/conquer/` | D230 † |
| Quake | `/apps/bin/quake` | Quake (motor libre de id Software) | `userland/terceros/quake/` | D229 † |
| Quake (pantalla completa) | `/apps/bin/quake --pantalla-completa` | Quake (motor libre de id Software) (en pantalla completa) | `userland/terceros/quake/` | D229 † |
| DOOM | `/apps/bin/chocolate-doom` | DOOM (Chocolate Doom) | `userland/terceros/chocolatedoom/` | D383 † |
| Freedoom | `/apps/bin/chocolate-doom --freedoom` | DOOM (Chocolate Doom) | `userland/terceros/chocolatedoom/` | D383 † |
| Tzar | `/apps/bin/tzar` | Tzar, juego de estrategia (port en C de ReTzar) | `userland/juegos/tzar/` | D417 † |
| Game Boy (GNUBoy) | `/apps/bin/gnuboy` | emulador de Game Boy y Game Boy Color (GNUBoy) | `userland/terceros/gnuboy/` | D235 † |
| Recreativa (juegos y consolas) | `/apps/bin/recreativa` | lanzador de juegos de DOS | `userland/terceros/recreativa/` | D294 † |
| DOSBox | `/apps/bin/dosbox` | emulador de MS-DOS (DOSBox 0.74) | `userland/terceros/dosbox/` | D294 † |
| DOSBox (pantalla completa) | `/apps/bin/dosbox --pantalla-completa` | emulador de MS-DOS (DOSBox 0.74) (en pantalla completa) | `userland/terceros/dosbox/` | D294 † |
| Sagui 3D | `/apps/bin/sagui3d` | juego en 3D propio | `userland/sagui3d/` | — |
| CHIP-8 | `/apps/bin/chip8` | emulador de CHIP-8 | `userland/chip8/` | — |
| ROM de CHIP-8 (bajar) | `/apps/bin/python /apps/bin/chip8-roms-gui` | ROM de CHIP-8 (descarga) | `userland/apps/bin/chip8-roms-gui` | D123 |

### 2.7 Sistema

| Nombre en el menú | Orden (ruta) | Qué hace | Código | Decisión |
| --- | --- | --- | --- | --- |
| Terminal | `/bin/terminal` | terminal en ventana | `userland/programas/terminal.c` | — |
| Administrador de tareas | `/bin/tareas` | administrador de tareas | `userland/programas/tareas.c` | — |
| Información del PC | `/bin/infopc` | información del PC | `userland/programas/infopc.c` | — |
| Pantalla | `/bin/pantalla --ventana` | resolución y pantallas | `userland/programas/pantalla.c` | — |
| Fecha y hora | `/bin/zona --ventana` | fecha, hora y zona horaria | `userland/programas/zona.c` | — |
| Configuración | `/bin/configuracion` | configuración del sistema | `userland/programas/configuracion.c` | — |
| Discos | `/bin/discos --ventana` | discos y particiones | `userland/programas/discos.c` | D121 |
| Bluetooth | `/bin/bluetooth --ventana` | Bluetooth | `userland/programas/bluetooth.c` | D279 |
| Instalar SaguiOS | `/bin/instalar --ventana` | instalador de SaguiOS en un disco o pendrive | `userland/programas/instalar.c` | D179 |
| DEBUG-APP (depuración remota) | `/bin/debug-app` | depuración remota por SSH | `userland/programas/debug-app.c` | D362 |

---

## 3. Desarrollo: compiladores y herramientas

El menú no tiene un grupo «Desarrollo». Estas herramientas están en `/apps/bin` (o en `/bin`) y se
usan desde la terminal. El **Laboratorio de C** (en Educación) y el **Mico** (en Accesorios)
también sirven para programar.

| Herramienta | Orden | Qué hace | Código | Decisión |
| --- | --- | --- | --- | --- |
| Compilador de C de SaguiOS | `/apps/bin/cc` | compila programas en C a ejecutables de SaguiOS (genera código x86) | `userland/programas/cc.c`, `userland/programas/cc_biblioteca.h` | — |
| Compilador de Pascal | `/apps/bin/pascal` | compila programas en Pascal | `userland/programas/pascal.c`, `userland/programas/pascal_biblioteca.h` | D085-D087 |
| TCC (Tiny C Compiler) | `/apps/bin/tcc` | compilador de C rápido, en el propio sistema | `userland/terceros/tcc/` | — |
| GNU make | `/apps/bin/make` | construye proyectos según un `Makefile` | `userland/terceros/make/` | — |
| git | `/apps/bin/git` | control de versiones, sobre libgit2 | `userland/terceros/git/` | D400 |
| MicroPython | `/apps/bin/python` | Python 3 reducido (la orden `python` de SaguiOS) | `userland/micropython/` | D034 |
| miniJVM (`java`, `javac`) | `/apps/bin/java`, `/apps/bin/javac` | intérprete de Java y su compilador | `userland/terceros/minijvm/` | D376 |
| GNU Prolog | `/apps/bin/gprolog`, `/apps/bin/pl2wam` | intérprete de Prolog y su compilador | `userland/terceros/gprolog/` | D189 |
| TinyGo (ejemplos) | — | ejemplos en Go; se compilan con TinyGo | `userland/go/` (ver `docs/go.md`) | D364 |
| JavaScript (`qjs`) | `/apps/bin/qjs` | intérprete de JavaScript en la terminal | `userland/programas/qjs.c` | D188 |
| SQLite (`sqlite3`) | `/apps/bin/sqlite3` | intérprete de órdenes SQL sobre SQLite | `userland/programas/sqlite3.c`, `userland/terceros/sqlite/` | D116 |
| Shell `sagsh` | `/bin/sagsh` (copia: `/bin/sh`) | el intérprete de órdenes; ver el apartado 4 | `userland/sagsh/` | D037 |
| `debug-app` | `/bin/debug-app` | depuración remota por SSH (entra otro equipo de la red) | `userland/programas/debug-app.c` | D362 |

---

## 4. Órdenes de consola: `sagsh` y `/bin`

**`sagsh`** es el intérprete de órdenes de SaguiOS (la *shell*). Lee lo que escribes, busca el
programa (en `/bin`, `/apps/bin` y el `PATH`) y lo ejecuta. `/bin/sh` es una copia de `sagsh`. Su
código está en `userland/sagsh/` (analizador en `analise.c`, ejecución en `executar.c`, la ayuda en
`ayuda.c`). Decisión D037.

Cada fichero `.c` de `userland/programas/` se compila como una orden. El nombre del fichero es el
nombre de la orden. La tabla siguiente sale de la cabecera de cada fichero, así que la descripción es
la que dice el propio código. La columna **Orden** indica dónde queda el ejecutable en la imagen por
defecto (`/apps/bin` para las de la lista `APPS_MOVER_FIJO` del `Makefile`, `/bin` para el resto).

| Orden | Qué hace | Código |
| --- | --- | --- |
| `/bin/acel2d` | el 2D por la GPU del escritorio (D195) | `userland/programas/acel2d.c` |
| `/bin/admin` | ejecuta una orden como administrador (root) (D255, mejora 098) | `userland/programas/admin.c` |
| `/bin/afinidad` | las CPU donde puede correr un proceso (D174) | `userland/programas/afinidad.c` |
| `/bin/amd` | el estado del motor 3D y la identificación de la GPU de AMD (mejora 054, parte B2; D158) | `userland/programas/amd.c` |
| `/bin/ampliar-home` | «Ampliar /home para usar todo el espacio libre» (D318, v8.4.3) | `userland/programas/ampliar-home.c` |
| `/bin/arquivos` | el explorador de archivos de SaguiOS (H23; mejora 058, D136; renovado en la v8.5.5.2, D420) | `userland/programas/arquivos.c` |
| `/bin/arranque` | probar un kernel arrancándolo UNA sola vez, y cambiar la etapa 2 del disco (D392) | `userland/programas/arranque.c` |
| `/apps/bin/banana` | Sagui Pega Banana, el juego de SaguiOS (H27) | `userland/programas/banana.c` |
| `/bin/base64` | codifica o decodifica Base64 (D232) | `userland/programas/base64.c` |
| `/bin/basename` | quita la carpeta (y un sufijo) de una ruta (D097) | `userland/programas/basename.c` |
| `/bin/bc` | calculadora de línea de órdenes, versión sencilla (D232) | `userland/programas/bc.c` |
| `/bin/bench` | mide la velocidad de la memoria y de la interfaz gráfica (D042) | `userland/programas/bench.c` |
| `/bin/benchmark` | benchmark de monoNúcleo y multiNúcleo con puntuación (D176) | `userland/programas/benchmark.c` |
| `/bin/bip` | bip [frecuencia [duración]] - toca una nota en el altavoz de la PC (B3) | `userland/programas/bip.c` |
| `/bin/bloqueo` | el bloqueo de publicidad y rastreo por DNS, al estilo de Pi-hole (D218) | `userland/programas/bloqueo.c` |
| `/bin/bluetooth` | el Bluetooth con un controlador USB estándar, p. ej. el CSR8510 | `userland/programas/bluetooth.c` |
| `/bin/brillo` | el brillo de la pantalla del portátil (D403) | `userland/programas/brillo.c` |
| `/apps/bin/buscaminas` | el Buscaminas de SaguiOS (D248) | `userland/programas/buscaminas.c` |
| `/bin/cal` | calendario (D232): el mes de hoy, un mes cualquiera o un año entero | `userland/programas/cal.c` |
| `/bin/calculadora` | la Calculadora de SaguiOS (H26) | `userland/programas/calculadora.c` |
| `/apps/bin/camara` | camara [-p] [--modo N] [--info] [--foto archivo [--cuadros N]] - la aplicación Cámara (D277) | `userland/programas/camara.c` |
| `/bin/captura` | captura [archivo] [-v/--ventana] [-r x,y,ancho,alto] [-t segundos] (D125) Guarda la pantalla ya compuesta (sin cursor) como PNG | `userland/programas/captura.c` |
| `/bin/cat` | muestra archivos; sin argumentos (o con "-") copia la entrada a la salida (D097) | `userland/programas/cat.c` |
| `/apps/bin/cc` | el compilador de C de SaguiOS | `userland/programas/cc.c` |
| `/bin/certificados` | gestiona el almacén de raíces del TLS (D063) | `userland/programas/certificados.c` |
| `/apps/bin/chatia` | el chat con IA con ventana (D143, D239) | `userland/programas/chatia.c` |
| `/bin/chgrp` | cambia el grupo de archivos y carpetas (D282, mejora 098): root, o el dueño a su propio grupo | `userland/programas/chgrp.c` |
| `/bin/chmod` | cambia los permisos de archivos y carpetas (D282, mejora 098) | `userland/programas/chmod.c` |
| `/bin/chown` | cambia el dueño (y el grupo) de archivos y carpetas (D282, mejora 098) | `userland/programas/chown.c` |
| `/apps/bin/cipo` | cipo [dirección] (alias navegador) - el Cipó, navegador de SaguiOS (D047) | `userland/programas/cipo.c` |
| `/bin/cmp` | compara dos archivos byte a byte (D232) | `userland/programas/cmp.c` |
| `/bin/column` | da formato de columnas a un texto (D232) | `userland/programas/column.c` |
| `/bin/comm` | compara dos archivos ordenados línea a línea (D232) | `userland/programas/comm.c` |
| `/bin/componentes` | demostración del kit gráfico de la libsagui (H22): etiqueta, botón, caja de texto, área de texto con desplazamiento, lista y diálogos | `userland/programas/componentes.c` |
| `/bin/configuracion` | la aplicación «Configuración» de SaguiOS (D112, mejoras 007 y 047) | `userland/programas/configuracion.c` |
| `/bin/conky` | clon propio (no un porte) de Conky para el escritorio de SaguiOS (D365) | `userland/programas/conky.c` |
| `/bin/contador` | cuenta segundos, para mostrar la multitarea | `userland/programas/contador.c` |
| `/apps/bin/convertir-audio` | el Conversor de audio (D339) | `userland/programas/convertir-audio.c` |
| `/apps/bin/correo` | cliente de correo con ventana (082) y aspecto de Outlook clásico (113, D289) | `userland/programas/correo.c` |
| `/bin/cp` | copia archivos y carpetas (D097) | `userland/programas/cp.c` |
| `/bin/cron` | ejecuta tareas a horas fijas (D216, mejora 074) | `userland/programas/cron.c` |
| `/bin/crontab` | la tabla de cron del usuario, /home/.config/crontab (D216) | `userland/programas/crontab.c` |
| `/bin/curl` | descarga por HTTP/1.1 o HTTPS (D046, D062) | `userland/programas/curl.c` |
| `/bin/cut` | extrae campos o columnas de cada línea (D097) | `userland/programas/cut.c` |
| `/bin/data` | fecha y hora (D080) | `userland/programas/data.c` |
| `/bin/debug-app` | DEBUG-APP: deja entrar a otro equipo de la red local, por SSH y durante un rato, para depurar este (D362) | `userland/programas/debug-app.c` |
| `/bin/desmontar` | graba todo lo que esté pendiente y libera o pendrive | `userland/programas/desmontar.c` |
| `/bin/df` | espacio libre en los sistemas de archivos montados (D097) | `userland/programas/df.c` |
| `/bin/dhcp` | pide una dirección IP por DHCP y muestra el resultado (D046) | `userland/programas/dhcp.c` |
| `/bin/diff` | compara dos archivos línea a línea (D097) | `userland/programas/diff.c` |
| `/bin/dirname` | quita el último componente de una ruta (D097) | `userland/programas/dirname.c` |
| `/bin/discos` | los discos y el espacio que ocupa cada uno (H34, D040, D121) | `userland/programas/discos.c` |
| `/bin/dmesg` | el registro del kernel (D232): lo que escribió al arrancar y después | `userland/programas/dmesg.c` |
| `/bin/dnsbloqueo` | servidor DNS mínimo con el filtro del bloqueo de publicidad (D218): el «Pi-hole» de la casa | `userland/programas/dnsbloqueo.c` |
| `/bin/du` | espacio que ocupan archivos y carpetas (D097) | `userland/programas/du.c` |
| `/bin/echo` | escribe los argumentos separados por espacio | `userland/programas/echo.c` |
| `/bin/ecualizador` | ecualizador de 5 bandas para todo el sonido del sistema (D201) | `userland/programas/ecualizador.c` |
| `/bin/editor` | el Editor de texto de SaguiOS (H25) | `userland/programas/editor.c` |
| `/bin/eject` | eject [-t] [lector] - desmonta el CD/DVD y abre la bandeja (D233) | `userland/programas/eject.c` |
| `/bin/ejecutar` | los diálogos del menú Inicio (D065) | `userland/programas/ejecutar.c` |
| `/bin/energia` | salvapantallas, bloqueo y apagado de la pantalla por inactividad, batería y «no molestar» (D422) | `userland/programas/energia.c` |
| `/bin/env` | muestra el ambiente o ejecuta una orden con un ambiente modificado (D097) | `userland/programas/env.c` |
| `/apps/bin/es11demo` | demostración de OpenGL ES 1.1 con EGL (D425) | `userland/programas/es11demo.c` |
| `/bin/escaner` | escáner de red propio de SaguiOS (D398) | `userland/programas/escaner.c` |
| `/bin/expand` | cambia los tabuladores por espacios (D232) | `userland/programas/expand.c` |
| `/bin/expr` | evalúa una expresión de sus argumentos (D232), como el de Linux | `userland/programas/expr.c` |
| `/bin/falha` | provoca un error a propósito para mostrar la protección: solo este proceso es terminado por el kernel; el shell y el sistema siguen | `userland/programas/falha.c` |
| `/bin/file` | dice de qué tipo es cada archivo, mirando su contenido (D232) | `userland/programas/file.c` |
| `/bin/find` | busca archivos y carpetas por nombre, tipo, tamaño | `userland/programas/find.c` |
| `/bin/fold` | parte las líneas largas (D232) | `userland/programas/fold.c` |
| `/bin/frecuencia` | control de la frecuencia de la CPU (D106, mejora 061) | `userland/programas/frecuencia.c` |
| `/bin/free` | memoria total, usada y libre (D097) | `userland/programas/free.c` |
| `/bin/fuentes` | instalar, quitar y ver las fuentes del sistema (D356) | `userland/programas/fuentes.c` |
| `/apps/bin/gebramini` | geometría dinámica y funciones de SaguiOS (D309, mejora 128) | `userland/programas/gebramini.c` |
| `/bin/gpu3d` | el 3D por la GPU (D285) | `userland/programas/gpu3d.c` |
| `/apps/bin/grabadora` | la Grabadora de sonido (D292) | `userland/programas/grabadora.c` |
| `/bin/grabar` | grabar archivo.wav [segundos] [opciones] - graba el micrófono en un WAV (D292) | `userland/programas/grabar.c` |
| `/bin/grep` | busca líneas que coinciden con un patrón (D097) | `userland/programas/grep.c` |
| `/bin/gunzip` | gunzip = gzip -d (mejora 056): ver gzip.c | `userland/programas/gunzip.c` |
| `/bin/gzip` | gzip, gunzip y zcat (mejora 056, D103): un solo fuente; gunzip.c y zcat.c lo incluyen con MODO_GZ distinto | `userland/programas/gzip.c` |
| `/bin/head` | primeras líneas (o bytes) de archivos o de la entrada (D097) | `userland/programas/head.c` |
| `/bin/hostname` | nombre de la máquina (D097) | `userland/programas/hostname.c` |
| `/bin/htop` | monitor de procesos interactivo para el terminal (D366) | `userland/programas/htop.c` |
| `/apps/bin/ia` | cliente de chat con IA en la línea de órdenes (D134) | `userland/programas/ia.c` |
| `/bin/icones` | la biblioteca de iconos del SaguiOS, a la vista (D135) | `userland/programas/icones.c` |
| `/bin/id` | identidad del proceso (D232; de verdad desde D255): el uid y el gid que le da el kernel, con los nombres de /etc/usuarios | `userland/programas/id.c` |
| `/bin/ifconfig` | muestra y configura la placa de red (D046) | `userland/programas/ifconfig.c` |
| `/bin/infopc` | información del PC, al estilo de CPU-Z (D041) | `userland/programas/infopc.c` |
| `/bin/instalar` | copia SaguiOS a un disco duro o a otro pendrive (D179) | `userland/programas/instalar.c` |
| `/apps/bin/jabuti` | programación con bloques para niños (D377, mejora 137) | `userland/programas/jabuti.c` |
| `/apps/bin/jclic` | visor de actividades educativas JClic (D185) | `userland/programas/jclic.c` |
| `/apps/bin/jclicbajar` | "Actividades JClic (bajar)": el descargador del repositorio de Clic del XTEC (D185) | `userland/programas/jclicbajar.c` |
| `/bin/kconfig` | opciones del kernel que se guardan desde el sistema y valen al reiniciar (D353) | `userland/programas/kconfig.c` |
| `/bin/kill` | termina un proceso por su PID | `userland/programas/kill.c` |
| `/bin/less` | less / more (paginador) - muestra un archivo o la entrada de pantalla en pantalla (D097) | `userland/programas/less.c` |
| `/bin/login` | inicio de sesión en la consola de texto (D371, fase 6, P12) | `userland/programas/login.c` |
| `/bin/ls` | lista archivos y carpetas (D097; D385 lo pone en columnas) | `userland/programas/ls.c` |
| `/bin/lscpu` | información de la CPU en el formato de Linux (D232), sobre CPUID y lo que da el kernel | `userland/programas/lscpu.c` |
| `/bin/lspci` | lista los dispositivos PCI en el formato de Linux (D232), sobre lo que da el kernel | `userland/programas/lspci.c` |
| `/apps/bin/macaco` | la hoja de cálculo del SaguiOS (mejora 078, D140) | `userland/programas/macaco.c` |
| `/bin/mandos` | mandos y joysticks desde el terminal (D307; docs/mandos.md) | `userland/programas/mandos.c` |
| `/bin/markdown` | markdown [-t] [archivo.md] - visor y editor de Markdown de SaguiOS (D045) | `userland/programas/markdown.c` |
| `/bin/md5sum` | resumen MD5 de archivos, como en Linux (D232) | `userland/programas/md5sum.c` |
| `/bin/mem` | memoria total, usada y libre | `userland/programas/mem.c` |
| `/bin/midi` | midi [archivo / carpeta] | `userland/programas/midi.c` |
| `/bin/mkdir` | crea carpetas (D097) | `userland/programas/mkdir.c` |
| `/bin/mktemp` | crea un archivo o carpeta temporal con nombre único y escribe su ruta (D232) | `userland/programas/mktemp.c` |
| `/bin/montar` | monta el pendrive en /usb (sin nombre: el primero disponible para montar) | `userland/programas/montar.c` |
| `/bin/more` | paginador sencillo: el mismo programa que less (D097) | `userland/programas/more.c` |
| `/bin/mount` | monta sistemas de archivos (D040) | `userland/programas/mount.c` |
| `/apps/bin/musica` | musica [archivo / carpeta] | `userland/programas/musica.c` |
| `/bin/mv` | mueve o renombra archivos y carpetas (D097) | `userland/programas/mv.c` |
| `/bin/nc` | nc (netcat) básico - conexiones TCP por la línea de comandos (D046) | `userland/programas/nc.c` |
| `/bin/nl` | numera las líneas (D232) | `userland/programas/nl.c` |
| `/bin/nproc` | cuántas CPU hay (D232) | `userland/programas/nproc.c` |
| `/bin/nslookup` | nslookup (alias host) nombre [servidor[:puerto]] - pregunta al DNS (D046) | `userland/programas/nslookup.c` |
| `/bin/ntp` | pone el reloj en hora por SNTP (RFC 4330), sobre UDP (D080) | `userland/programas/ntp.c` |
| `/bin/od` | vuelca un archivo en octal, hexadecimal o caracteres (D232); con -C da el formato de "hexdump -C" | `userland/programas/od.c` |
| `/bin/ola` | imprime los argumentos recibidos (prueba del cargador, H13) | `userland/programas/ola.c` |
| `/apps/bin/paint` | el programa de dibujo de SaguiOS (D056) | `userland/programas/paint.c` |
| `/bin/panel` | el panel táctil (touchpad) desde la consola (D407) | `userland/programas/panel.c` |
| `/bin/pantalla` | resolución de la pantalla (D058, D059) | `userland/programas/pantalla.c` |
| `/bin/particiones` | la tabla de particiones de los discos, desde el terminal (D179) | `userland/programas/particiones.c` |
| `/apps/bin/pascal` | el compilador de Pascal de SaguiOS (D085-D087) | `userland/programas/pascal.c` |
| `/bin/passwd` | cambia la contraseña de una cuenta (D255, mejora 098) | `userland/programas/passwd.c` |
| `/bin/paste` | junta las líneas de varios archivos lado a lado (D232) | `userland/programas/paste.c` |
| `/bin/ping` | ping [-c N] [-s bytes] [-W ms] host - eco ICMP (D046) | `userland/programas/ping.c` |
| `/bin/planificador` | el planificador de procesos entre las CPU (D174) | `userland/programas/planificador.c` |
| `/apps/bin/planomini` | mini CAD 2D de SaguiOS (D312, mejora 129) | `userland/programas/planomini.c` |
| `/apps/bin/pluma` | el procesador de textos de SaguiOS (mejora 116, D291), con el aspecto de un Word 97 simplificado: barra de menús, barras de herramientas Estándar y Formato con iconos, regla con márgenes y sangrías, vista de página sobre fondo gris y barra de estado | `userland/programas/pluma.c` |
| `/bin/portatil` | lo propio de los portátiles por ACPI (D403) | `userland/programas/portatil.c` |
| `/bin/ps` | lista los procesos (D097) | `userland/programas/ps.c` |
| `/apps/bin/qjs` | el intérprete de JavaScript en la terminal (D188) | `userland/programas/qjs.c` |
| `/bin/radios` | estado de las radios (Wi-Fi, Bluetooth, WWAN) de los portátiles HP por WMI, y encenderlas o apagarlas (D405) | `userland/programas/radios.c` |
| `/bin/realpath` | ruta absoluta y limpia de un archivo o carpeta (D232) | `userland/programas/realpath.c` |
| `/bin/recortes` | recortes [--recorte] [-t segundos] - recortes de pantalla (D125) | `userland/programas/recortes.c` |
| `/bin/reparar` | repara un ext2 que quedó mal tras un corte o un cuelgue (D397) | `userland/programas/reparar.c` |
| `/bin/reproducir` | reproducir [-v volumen] archivo | `userland/programas/reproducir.c` |
| `/apps/bin/reproductor` | reproductor [archivo / carpeta / lista.m3u] | `userland/programas/reproductor.c` |
| `/bin/rev` | escribe cada línea al revés (D232) | `userland/programas/rev.c` |
| `/bin/rm` | borra archivos y, con -r, carpetas (D097) | `userland/programas/rm.c` |
| `/bin/rmdir` | borra carpetas vacías (D097) | `userland/programas/rmdir.c` |
| `/bin/saguimark` | el benchmark de SaguiOS en modo texto (D093, mejora 051) | `userland/programas/saguimark.c` |
| `/bin/scp` | copiar un archivo a otra máquina, o traerlo, por SSH (D317) | `userland/programas/scp.c` |
| `/bin/sed` | editor de flujo, lo básico (D232): s///, d, p, q, =, a, i, c con direcciones | `userland/programas/sed.c` |
| `/bin/sensores` | el monitor de hardware en modo texto (D092, mejora 050) | `userland/programas/sensores.c` |
| `/bin/seq` | escribe una secuencia de números enteros (D097) | `userland/programas/seq.c` |
| `/bin/servicio` | maneja los servicios en segundo plano desde el terminal (D216) | `userland/programas/servicio.c` |
| `/bin/servicios` | el supervisor del gestor de servicios (D216, mejora 074) | `userland/programas/servicios.c` |
| `/apps/bin/sgldemo` | cubo de colores girando con SaguiGL (mejora 054, v5; D104) | `userland/programas/sgldemo.c` |
| `/bin/sha1sum` | resumen SHA-1 de archivos, como en Linux (D232) | `userland/programas/sha1sum.c` |
| `/bin/sha256sum` | resumen SHA-256 de archivos (D060), como en Linux | `userland/programas/sha256sum.c` |
| `/bin/simd` | muestra y prueba la FPU x87, MMX, SSE y SSE2 (D032) | `userland/programas/simd.c` |
| `/apps/bin/sintonia` | sintonia [archivo / carpeta / lista.m3u / lista.pls / http://emisora] | `userland/programas/sintonia.c` |
| `/bin/sleep` | espera unos segundos (D097) | `userland/programas/sleep.c` |
| `/bin/sobre` | versión y créditos de SaguiOS-Extended (D101) | `userland/programas/sobre.c` |
| `/bin/sobre-o-sistema` | la ventana "Acerca de SaguiOS-Extended" (H28, D101): versión, tamaño de la imagen, procesos activos, memoria y créditos | `userland/programas/sobre-o-sistema.c` |
| `/bin/sonido` | diagnóstico y pruebas del códec de sonido HDA (D107) | `userland/programas/sonido.c` |
| `/bin/sonidos` | los sonidos del sistema (D421): toca el de un evento y vigila USB y batería | `userland/programas/sonidos.c` |
| `/bin/sort` | ordena las líneas de archivos o de la entrada (D097) | `userland/programas/sort.c` |
| `/apps/bin/spider` | Solitario Spider de SaguiOS (D173) | `userland/programas/spider.c` |
| `/bin/split` | parte un archivo en trozos (D232) | `userland/programas/split.c` |
| `/apps/bin/sqlite3` | intérprete de órdenes SQL sobre el SQLite de userland/terceros/sqlite (D116) | `userland/programas/sqlite3.c` |
| `/bin/ssh` | cliente SSH (D317): entrar desde SaguiOS en otra máquina | `userland/programas/ssh.c` |
| `/bin/ssh-keygen` | claves ed25519 para el cliente ssh (D317) | `userland/programas/ssh-keygen.c` |
| `/bin/sshd` | servidor SSH de administración (D250, mejora 052) | `userland/programas/sshd.c` |
| `/bin/stat` | datos de un archivo o carpeta (D097) | `userland/programas/stat.c` |
| `/bin/su` | abre un shell con otra cuenta, root por defecto (D255, mejora 098) | `userland/programas/su.c` |
| `/bin/sync` | graba la caché de bloques de todos los discos (D281) | `userland/programas/sync.c` |
| `/bin/tablasacpi` | vuelca las tablas ACPI (DSDT, SSDT, FACP...) de la máquina a ficheros .dat, para estudiar su AML en otro equipo (iasl -d) o con tools/uacpi_volcado (D405). Solo lee: no cambia nada del sistema | `userland/programas/tablasacpi.c` |
| `/bin/tac` | escribe las líneas en orden inverso: la última primero (D232) | `userland/programas/tac.c` |
| `/bin/tail` | últimas líneas (o bytes) de archivos o de la entrada (D097) | `userland/programas/tail.c` |
| `/bin/tar` | empaqueta y desempaqueta archivos (mejora 056, D103) | `userland/programas/tar.c` |
| `/bin/tareas` | el Administrador de tareas (D043, D079, D216) | `userland/programas/tareas.c` |
| `/bin/teclado` | muestra o cambia el diseño del teclado (ES o ABNT2) | `userland/programas/teclado.c` |
| `/bin/tee` | copia la entrada a la salida y a archivos (D097) | `userland/programas/tee.c` |
| `/bin/temp` | la temperatura de la CPU en una línea (D092, mejora 050) | `userland/programas/temp.c` |
| `/bin/terminal` | el Terminal en ventana (H24), con pestañas y preferencias (D231) | `userland/programas/terminal.c` |
| `/bin/time` | ejecuta una orden y mide cuánto tarda (D232) | `userland/programas/time.c` |
| `/bin/timeout` | ejecuta una orden y la termina si pasa de un tiempo (D232) | `userland/programas/timeout.c` |
| `/bin/top` | los procesos en vivo, en el terminal (D232) | `userland/programas/top.c` |
| `/bin/touch` | crea archivos vacíos si no existen y fija sus fechas (D097, D341) | `userland/programas/touch.c` |
| `/bin/touchpad` | touchpad = panel (D407): ver panel.c | `userland/programas/touchpad.c` |
| `/bin/tr` | traduce, borra o aprieta caracteres de la entrada (D232) | `userland/programas/tr.c` |
| `/apps/bin/traductor` | editor de traducciones de SaguiOS, al estilo de Poedit (D335, mejora 133) | `userland/programas/traductor.c` |
| `/bin/tree` | muestra carpetas y archivos como un árbol (D097) | `userland/programas/tree.c` |
| `/bin/truncate` | cambia el tamaño de un archivo (D232) | `userland/programas/truncate.c` |
| `/apps/bin/tucan` | el editor de presentaciones de SaguiOS (D369), con la cinta de la suite (Pluma, Macaco) | `userland/programas/tucan.c` |
| `/bin/umount` | umount <carpeta/dispositivo> - graba lo que falta y desmonta (D040) | `userland/programas/umount.c` |
| `/bin/uname` | nombre y versión del sistema (D097) | `userland/programas/uname.c` |
| `/bin/uniq` | quita (o cuenta) líneas repetidas consecutivas (D097) | `userland/programas/uniq.c` |
| `/bin/unzip` | lista, comprueba y extrae archivos .zip (mejora 056, D103) | `userland/programas/unzip.c` |
| `/bin/uptime` | hora actual, cuánto lleva encendido el sistema y cuántos procesos hay (D097) | `userland/programas/uptime.c` |
| `/bin/usb` | diagnóstico: controladores USB 2.0, puertos con dispositivo y dispositivos reconocidos (H29, H30) | `userland/programas/usb.c` |
| `/bin/visor` | el Visor de imágenes de SaguiOS (D055) | `userland/programas/visor.c` |
| `/apps/bin/visorpdf` | el visor de PDF de SaguiOS (mejora 059, D138; interfaz de escritorio completa en la mejora 105, D274) | `userland/programas/visorpdf.c` |
| `/bin/vnc` | cliente de escritorio remoto VNC (mejora 097 fase 2, D254) | `userland/programas/vnc.c` |
| `/bin/vncd` | servidor de escritorio remoto VNC (mejora 097, D252) | `userland/programas/vncd.c` |
| `/bin/volumen` | volumen [0-100 / +N / -N / silencio / activar / salida [N]] - volumen general del sonido (D088) | `userland/programas/volumen.c` |
| `/bin/watch` | repite una orden cada cierto tiempo y muestra su salida (D232) | `userland/programas/watch.c` |
| `/bin/wc` | cuenta líneas, palabras y letras (bytes) de archivos o de la entrada (D097) | `userland/programas/wc.c` |
| `/bin/wg` | la VPN WireGuard de SaguiOS (D363) | `userland/programas/wg.c` |
| `/bin/wg-quick` | enciende y apaga la VPN WireGuard (D363): «wg-quick up NOMBRE», «wg-quick down» | `userland/programas/wg-quick.c` |
| `/bin/wget` | descarga un archivo por HTTP o HTTPS y lo graba con el nombre de la URL (D062) | `userland/programas/wget.c` |
| `/bin/which` | dice qué programa se ejecutaría para un nombre (D097) | `userland/programas/which.c` |
| `/bin/whoami` | nombre del usuario (D097; de verdad desde D255): la cuenta del uid del proceso | `userland/programas/whoami.c` |
| `/bin/widget-calendario` | el mes en curso, con el día de hoy marcado (D365) | `userland/programas/widget-calendario.c` |
| `/bin/widget-notas` | una nota adhesiva en el escritorio (D365) | `userland/programas/widget-notas.c` |
| `/bin/widget-relogio` | el reloj de escritorio (D365): analógico o digital (un clic cambia de modo; la elección se guarda en relogio.modo de widgets.conf). Se repinta solo cuando cambia el segundo | `userland/programas/widget-relogio.c` |
| `/bin/widget-sistema` | monitor pequeño del sistema (D365): CPU con su gráfica, memoria y red (bajada y subida con la gráfica del último minuto). Una lectura por segundo; no repinta si los números no cambiaron | `userland/programas/widget-sistema.c` |
| `/bin/widgets` | el gestor de widgets de escritorio (D365): lista los disponibles, activa o quita cada uno y cambia sus opciones; lo que queda activo se guarda en /home/.config/widgets.conf y arranca solo al iniciar la sesión | `userland/programas/widgets.c` |
| `/bin/wifi` | el Wi-Fi con el pincho Realtek RTL8188EU (D215, D220) | `userland/programas/wifi.c` |
| `/bin/xargs` | construye y ejecuta órdenes con lo que llega por la entrada (D097) | `userland/programas/xargs.c` |
| `/bin/xxd` | volcado hexadecimal de un archivo, o su inverso con -r (D232) | `userland/programas/xxd.c` |
| `/bin/yes` | repite un texto (por defecto «y») hasta que se cierre la salida (D097) | `userland/programas/yes.c` |
| `/bin/zcat` | zcat = gzip -dc (mejora 056): ver gzip.c | `userland/programas/zcat.c` |
| `/bin/zip` | crea archivos .zip (mejora 056, D103) | `userland/programas/zip.c` |
| `/bin/zona` | zona horaria, reloj de la placa y la ventana "Fecha y hora" (D080) | `userland/programas/zona.c` |

> **Órdenes que el Makefile nombra y cuyo código no está en esta rama:** `sglv6` y `es11conf`.
> No se documentan aquí hasta que existan sus fuentes.

---

## 5. Bibliotecas

Una **biblioteca** es código que varios programas reutilizan. En SaguiOS hay dos grupos: las
propias del sistema (la *libc* y la *libsagui*) y las de cada tipo de aplicación.

| Biblioteca | Carpeta | Para qué sirve |
| --- | --- | --- |
| **libsagui** | `userland/libsagui/` | Biblioteca propia de SaguiOS. Su fichero `chamadas.c` envuelve cada llamada al sistema (`int 0x80`, ver la guía del kernel). Trae también ventanas y kit gráfico (`gui_*.c`), audio (`audio_*.c`), imágenes (JPEG, PNG, GIF), MPEG, compresión, red (`http.c`) y documentos (`pdf_*.c`, `presentacion_*.c`). |
| **libc** | `userland/libc/` | La biblioteca C común: entrada y salida (`es.c`), memoria (`memoria.c`), texto (`texto.c`) y la parte POSIX que tiene SaguiOS (`posix.c`: `write`, `read`, `open`...). Así los ports de terceros compilan casi sin cambios. |
| **libcxx** | `userland/libcxx/` | El soporte de C++ (`rt/`) para los programas escritos en C++ (OpenJazz, DOSBox, Gearsystem...). |
| **SDL 2** | `userland/libsdl2/` | Una SDL 2 escrita para SaguiOS: sus cabeceras son propias y el código llama a las ventanas y al audio de SaguiOS (D230). Sirve para portar juegos. |
| **SDL 1.2** | `userland/libsdl12/` | Lo mismo para la SDL 1.2, que usan algunos juegos (D228). |
| **OpenAL** | `userland/libopenal/` | Un subconjunto propio de OpenAL 1.1 para el sonido 3D de los juegos (D230). |
| **SaguiGL** (`libsgl`) | `userland/libsgl/` | El 3D propio de SaguiOS. Dibuja por software y, desde la v8.3, puede mandar los triángulos a la GPU (ver [`gpu3d.md`](gpu3d.md)). Incluye escenas fijas (`cenas.c`). |
| **libamd** | `userland/libamd/` | El armazón del driver 3D de las GPU AMD (D158). |
| **libtls** | `userland/libtls/` | Criptografía y TLS: AES, SHA, GCM, números grandes y las conexiones HTTPS. La usan, entre otros, `curl`, `wget` y el navegador. |
| **libdl** | `userland/libdl/` | `dlopen`, `dlsym` y `dlclose` dentro de `libsagui.so` (D378, [`enlace-dinamico.md`](enlace-dinamico.md)). |
| **libfontes** | `userland/libfontes/` | El motor de fuentes: familias, carga de fuentes, subconjuntos y texto. Lo usa Pluma como proveedor de fuentes de documento (D319). |
| **web** | `userland/web/` | El motor de la web del Cipó: CSS, JavaScript con DOM, Canvas 2D, SVG, WebGL por software y el descompresor Brotli para las fuentes WOFF2 (D141). |
| **pdf** | `userland/pdf/` | El motor de PDF del visor (D138). |
| **llm** | `userland/llm/` | Un motor propio para modelos de lenguaje locales (formato GGUF), con la orden `llm` en `/apps/bin` (D380). |
| **IA y correo** | `userland/ia/`, `userland/mail/` | Las bibliotecas del cliente de IA (`userland/ia/`) y del cliente de correo (`userland/mail/`). Las comparten, entre otras, `ia`, `chatia`, `pluma`, `macaco`, `tucan` y `correo`. |
| **MicroPython** | `userland/micropython/` | Python, con el «pegamento» de SaguiOS (`arquivos_sagui.c`, `janela_sagui.c`). Lo de MicroPython es de terceros (D034). |
| **litemonkey** | `litemonkey/` | Base de datos documental propia, en C (D117). Está fuera de `userland/`. |
| **Enlazadores** | `userland/programa.ld`, `userland/programa_cxx.ld` | Dicen cómo se coloca el código de un programa en C o C++. |
| **Cabeceras de la API** | `userland/inc/` | Las cabeceras OpenGL ES y EGL, además de las de la libc. |

---

## 6. Ports de terceros

Un **port** es un programa libre de otro autor que se ha adaptado para correr en SaguiOS. Cada
carpeta tiene un fichero `leeme.txt` con los cambios. El registro de todo lo ajeno, con licencia y
origen, está en [`terceros.md`](terceros.md).

| Port | Carpeta | Qué es | Orden | Decisión |
| --- | --- | --- | --- | --- |
| NetSurf | `userland/terceros/netsurf/` | navegador web ligero | `/apps/bin/netsurf` | D184 † |
| Freeciv 3.1.6 | `userland/terceros/freeciv/` | estrategia por turnos; servidor y cliente SDL 2 | `/apps/bin/freeciv-server`, `/apps/bin/freeciv-sdl2` | D374 † |
| OpenTTD | — (no está en esta rama) | simulador de transporte | `/apps/bin/openttd` (si existe) | D375 † |
| Chocolate Doom 3.1.1 | `userland/terceros/chocolatedoom/` | DOOM, Heretic, Hexen y Strife; con el contenido libre Freedoom | `/apps/bin/chocolate-doom` | D383 † |
| Quake | `userland/terceros/quake/` | el motor de Quake, con su render por software | `/apps/bin/quake` | D229 † |
| Vanilla Conquer | `userland/terceros/conquer/` | Command & Conquer (Tiberian Dawn y Red Alert) | `/apps/bin/conquer` | D230 † |
| OpenJazz | `userland/terceros/openjazz/` | Jazz Jackrabbit | `/apps/bin/openjazz` | D228 † |
| OpenTyrian | `userland/terceros/opentyrian/` | Tyrian 2.1 (los datos van en la partición SAGUI-JOGOS) | `/apps/bin/opentyrian` | D180, D181 † |
| Tzar | `userland/juegos/tzar/` | port en C de ReTzar | `/apps/bin/tzar` | D417 † |
| AoEN | `userland/juegos/aoen/` | juego de estrategia en tiempo real de este proyecto | `/apps/bin/aoen` | D382 |
| DOSBox 0.74-3 | `userland/terceros/dosbox/` | emulador de MS-DOS | `/apps/bin/dosbox` | D294 † |
| Recreativa | `userland/terceros/recreativa/` | lanzador de juegos de DOS (código propio) | `/apps/bin/recreativa` | D294 † |
| GNUBoy | `userland/terceros/gnuboy/` | Game Boy y Game Boy Color | `/apps/bin/gnuboy` | D235 † |
| mGBA 0.10.5 | `userland/terceros/mgba/` | Game Boy Advance | `/apps/bin/mgba` | D322 |
| Gearsystem | `userland/terceros/gearsystem/` | Master System, Game Gear y SG-1000 | `/apps/bin/gearsystem` | D321 |
| LaiNES | `userland/terceros/laines/` | NES (Famicom) | `/apps/bin/laines` | D321 |
| Entrada común de emuladores | `userland/terceros/emucomun/` | el punto de entrada que comparten los emuladores de consolas (código propio) | — | D321 |
| TCC 0.9.28rc | `userland/terceros/tcc/` | compilador de C; también es la libtcc que usa el Laboratorio de C | `/apps/bin/tcc` | — |
| GNU make 4.4.1 | `userland/terceros/make/` | construcción de proyectos | `/apps/bin/make` | — |
| git (libgit2) | `userland/terceros/git/` | control de versiones | `/apps/bin/git` | D400 |
| MicroPython | `userland/micropython/` | Python | `/apps/bin/python` | D034 |
| GNU Prolog 1.5.0 | `userland/terceros/gprolog/` | Prolog | `/apps/bin/gprolog`, `/apps/bin/pl2wam` | D189 |
| miniJVM | `userland/terceros/minijvm/` | Java | `/apps/bin/java`, `/apps/bin/javac` | D376 |
| SQLite | `userland/terceros/sqlite/` | base de datos | `/apps/bin/sqlite3` | D116 |
| Shine | `userland/terceros/shine/` | codificador MP3 de punto fijo (lo usa el Conversor de audio) | — (biblioteca) | — |
| openrsync | `userland/terceros/openrsync/` | `rsync` como cliente por SSH | `/apps/bin/rsync` | D314 |
| zlib 1.3.1 | `userland/terceros/zlib/` | biblioteca de compresión de datos | — (biblioteca) | — |
| Sonidos «Kenney» | `userland/terceros/kenney-sonidos/` | sonidos de la interfaz (datos, con su licencia) | — (datos) | D421 |
| Iconos «Fluent UI» | `userland/terceros/fluentui/` | iconos de la biblioteca del sistema (datos, con su licencia) | — (datos) | D135 |

Los juegos de este proyecto (Sagui Atrapa Bananas, Solitario Spider, Buscaminas, Sagui 3D y CHIP-8)
no son ports: están en `userland/programas/`, `userland/sagui3d/` y `userland/chip8/`.

---

## 7. Dónde leer más

| Tema | Documento |
| --- | --- |
| El kernel y las llamadas al sistema | [`guia-kernel.md`](guia-kernel.md) |
| Todas las decisiones de diseño | [`decisoes.md`](decisoes.md) |
| Código de terceros, licencias y origen | [`terceros.md`](terceros.md), [`licencia.md`](licencia.md) |
| Los ports de juegos, uno a uno | [`doom.md`](doom.md), [`freeciv.md`](freeciv.md), [`tzar.md`](tzar.md), [`aoen.md`](aoen.md), [`dosbox.md`](dosbox.md), [`mgba.md`](mgba.md), [`openttd.md`](openttd.md), [`carencias-netsurf.md`](carencias-netsurf.md) |
| Compilar herramientas y programas | [`make.md`](make.md), [`tcc.md`](tcc.md), [`git.md`](git.md), [`go.md`](go.md) |
| Aplicaciones de escritorio | [`pluma.md`](pluma.md), [`cipo.md`](cipo.md), [`macaco.md`](macaco.md), [`visualpythons.md`](visualpythons.md), [`litemonkey.md`](litemonkey.md), [`camara.md`](camara.md), [`microfono.md`](microfono.md) |
| Juegos y emuladores de DOS, Game Boy y más | [`recreativa.md`](recreativa.md), [`mandos.md`](mandos.md) |

---

> *Esta guía se generó a partir del código el 2026-11-21; si algo no cuadra, manda el código.*
