<div align="center">

# 🐒 SaguiOS-Extended

[Español](README.md) · [Português](README_PT.md) · [English](README_EN.md)

*Basado en [SaguiOS](https://github.com/erigobeli/sagui-os) de Eli Rigobeli.*

**Un sistema operativo escrito desde cero por IA**
para PC x86 de 32 bits, funcionando de verdad en un portátil de 2011.

Hecho por Claude (Anthropic) para el proyecto de **Eli Rigobeli**,
y mantenido por **Alejandro Anuncibay**, con mejoras escritas con Claude
(Anthropic) y con un agente de IA (MiniMax Code).

`562 KB` · `sin GRUB` · `sin libc` · `sin código de otros sistemas (salvo drivers, D073)` · `más de 130 pruebas automáticas`

<img src="docs/capturas/B4-3-fase1-corredor.png" width="49%" alt="Sagui 3D, el juego en primera persona de SaguiOS">
<img src="docs/capturas/H28-tres-programas.png" width="49%" alt="Escritorio de SaguiOS con tres programas abiertos">

</div>

---

## Qué es

> **Nota sobre esta versión:** esta es una traducción al español del proyecto original, escrito en
> portugués de Brasil. La interfaz del sistema está en español y el teclado admite las
> distribuciones **ES** (por defecto) y **ABNT2**, que se eligen con el programa `teclado`
> (`teclado es` / `teclado abnt2`). Los identificadores del código, los comentarios, los nombres
> de archivo y los comandos se mantienen en portugués.

SaguiOS es un sistema operativo completo, desde el cargador de arranque hasta el juego en 3D, escrito
íntegramente en este proyecto, siguiendo la especificación de [`prd.md`](prd.md). Arranca desde un
pendrive en un **Lenovo G470** (2011), en VirtualBox y en QEMU, y cabe en **28.959.232 bytes**: 12,2 MB
de arranque (kernel y ramdisk) y 16 MB de las particiones del sistema, la de juegos y la de aplicaciones
(la meta de la imagen es de 38 MB desde la v7.0b, D181; antes 16 MB; la versión original, sin las mejoras, ocupaba 575.488 bytes, el
39 % de un disquete de 1,44 MB, que era la meta del PRD hasta la D050).

Lo que va dentro de la imagen se ha escrito aquí, salvo lo de terceros: cargador de arranque en dos etapas, kernel,
drivers (teclado ES y ABNT2, touchpad, vídeo VESA, USB 2.0, pendrive), sistema de archivos FAT32,
gestor de ventanas, la biblioteca de los programas (`libsagui`), el shell, los programas, la fuente
tipográfica de 8x16 y los dibujos, salvo lo de terceros que lista
[`docs/terceros.md`](docs/terceros.md) (las caras DejaVu del sistema y los iconos Fluent, MIT). Cada decisión de diseño está explicada en
[`docs/decisoes.md`](docs/decisoes.md) y cada error encontrado, con su causa y su corrección, en
[`docs/erros.md`](docs/erros.md).

## Funcionando en el hardware real

Fotos de Eli en cada fase, en el Lenovo G470:

| Fase 1: arranque | Fase 2: kernel y terminal | Fase 3: procesos y shell |
| :---: | :---: | :---: |
| <img src="testes_eli/fase_1_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_2_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_3_no_notebook.jpg" width="260"> |
| **Fase 4: gráficos y ratón** | **Fase 5: ventanas y programas** | **Extra: Sagui 3D (VM)** |
| <img src="testes_eli/fase_4_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_5_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_bonus_vm.png" width="260"> |

## Las fases

| Fase | Qué se construyó | En pantalla |
| --- | --- | :---: |
| **1. Arranque** | Cargador propio en dos etapas (MBR + etapa 2), línea A20, mapa de memoria E820, copia por encima de 1 MB en *unreal mode*, suma de verificación, modo protegido, kernel en C | <img src="docs/capturas/H04-boot-vdi.png" width="200"> |
| **2. Kernel y terminal** | GDT, IDT, TSS, pantalla de error, PIC, reloj a 100 Hz, memoria física, `kmalloc`, teclado ABNT2 con acentos (y ES en esta versión), sistema de archivos SGFS | <img src="docs/capturas/H09-ls-cd.png" width="200"> |
| **3. Procesos y programas** | Paginación con un espacio por proceso, planificador expropiativo, anillo 3, llamadas al sistema, cargador ELF, pipes, shell `sagsh` | <img src="docs/capturas/H18-edicao-e-segundo-plano.png" width="200"> |
| **4. Gráficos y ratón** | VESA 1024x768x32 con doble búfer, fuente 8x16 propia con acentos, terminal gráfico, touchpad PS/2 | <img src="docs/capturas/H18-fonte-acentos.png" width="200"> |
| **5. Ventanas y programas** | Escritorio, gestor de ventanas, Archivos, Terminal, Editor, Calculadora, Sagui Atrapa Bananas | <img src="docs/capturas/H28-programas-abertos.png" width="200"> |
| **6. Pendrive de datos** | PCI, driver USB 2.0 (EHCI) con hubs y conexión en caliente, almacenamiento USB, FAT32 con lectura y escritura | <img src="docs/capturas/H34-2-arquivos-no-pendrive.png" width="200"> |
| **Extra** | B3: pitido por el altavoz del PC. B4: **Sagui 3D**. (B1 y B2 cancelados por Eli.) | <img src="docs/capturas/B4-6-macaco-tonto.png" width="200"> |

## Sagui 3D

El sagüí ha quedado atrapado en un laberinto de piedra en plena Mata Atlántica. Tiene que reunir bananas
para abrir la verja de cada fase, mientras unos monos capuchinos guardianes intentan quitarle las bananas
del bolsillo. El tirachinas dispara semillas de jabuticaba (con tres, el mono queda aturdido y huye),
cada banana comida deja una cáscara que hace resbalar al mono, y cada fase esconde un racimo
dorado detrás de una pared de lianas.

<div align="center">
<img src="docs/capturas/B4-1-tela-de-inicio.png" width="32%">
<img src="docs/capturas/B4-7-macaco-escorregou.png" width="32%">
<img src="docs/capturas/B4-5-vitoria.png" width="32%">
</div>

El motor 2,5D se diseñó a partir de la geometría, sin consultar ningún código, tutorial ni
repositorio de juegos: salas convexas con altura propia de suelo y de techo, dibujo columna a
columna con una "rendija" que se estrecha en cada paso, perspectiva sin ojo de pez, punto fijo
(los programas no usan la FPU), suelo y techo texturizados y luz por sala. Son tres fases en
archivos de texto, con texturas y personajes dibujados en el propio código. Explicación completa
en la sección "Sagui 3D" de [`docs/decisoes.md`](docs/decisoes.md).

## Extras: GNU Prolog (código de terceros)

- **GNU Prolog 1.5.0** (D189, v7.2): `gprolog` (o `prolog`) abre el intérprete con la biblioteca ISO;
  `['/ejemplos/familia.pl'].` carga el ejemplo de parentesco y listas. LGPL-3/GPL-2, aislado en
  `userland/terceros/gprolog/` y fuera de la variante sin GPL. Prueba: `tests/h156_gprolog.py`.

## Extras: OpenTyrian (código de terceros)

- **OpenTyrian** (D156, v6.6): el tiro vertical de 1995 con el motor del proyecto original (GPL-2.0),
  aislado en `userland/terceros/opentyrian/` y con una plataforma propia que hace de SDL 1.2 sobre las
  ventanas de SaguiOS. No entra en la variante sin GPL, ni el juego ni sus datos.
- Desde la v7.0b está en **Inicio > Juegos > OpenTyrian** (D180) y sus datos (Tyrian 2.1) van en una
  partición ext2 de la propia imagen, `SAGUI-JOGOS`, que el sistema monta al arrancar en `/jogos` (de
  lectura y escritura desde la v7.4, D228), con los datos en `/jogos/opentyrian` (D181): no ocupan RAM
  (el ramdisk no crece).
- El juego **encuentra sus datos solo**: en `/jogos/opentyrian` y en `jogos/opentyrian` de cada disco
  montado (`/usb`, `/datos`…). En el CD, o con una imagen sin la partición, se ponen en un pendrive
  (`tools/imagem_tyrian.sh` lo arma desde el anfitrión). Si no los encuentra, una ventana explica cómo
  conseguirlos y ofrece «Buscar otra vez». Pruebas: `tests/h136_opentyrian.py`, `tests/h150_opentyrian_menu.py`.

## Extras: OpenJazz (código de terceros, C++)

- **OpenJazz** (D228, v7.4): el motor libre de *Jazz Jackrabbit* (1994), versión 20260301 del proyecto
  original (GPL-2.0), en `userland/terceros/openjazz/` sin tocar una línea. Es **el primer programa de C++**
  del sistema: lleva un runtime de C++ propio (`userland/libcxx/`: `new`/`delete`, excepciones con un
  desenrollador DWARF nuestro, unas cabeceras mínimas de la biblioteca estándar) y la SDL 1.2 común
  (`userland/libsdl12/`). Va en `/apps/bin/openjazz` e **Inicio > Juegos > OpenJazz** (también a pantalla
  completa); no entra en la variante sin GPL.
- **Los datos del juego no son libres** y no vienen con SaguiOS. OpenJazz los busca en cualquier carpeta de
  `/jogos` (o en `jogos/` de un pendrive) y prefiere la versión completa al shareware. Para tenerlos:
  `tools/openjazz_datos.sh` baja el shareware (episodio 1) antes de compilar, o se pone el juego completo en
  `input_share/games/` ([docs/input_share.md](docs/input_share.md)); o se copian a `/jogos` desde SaguiOS.
  Sin datos, una ventana explica cómo conseguirlos. La configuración y las partidas van a `/home/openjazz`.
  Prueba: `tests/h228_openjazz.py`.

## Extras: Quake (código de terceros)

- **Quake** (D229, v7.4.1): el motor de *Quake* (1996) que id Software publicó con licencia GPL-2.0, el
  WinQuake con su **renderizador por software**, en `userland/terceros/quake/` sin tocar una línea (C puro,
  sin el ensamblador). Sobre la SDL 1.2 común: 320x200 en una ventana al doble (o 640x480), pantalla
  completa en 4:3, sonido a 22050 Hz. Va en `/apps/bin/quake` e **Inicio > Juegos > Quake** (también a
  pantalla completa); no entra en la variante sin GPL. Sin red ni música de CD.
- **Los datos del juego no son libres** y no vienen con SaguiOS. Quake los busca en cualquier carpeta de
  `/jogos` (o en `jogos/` de un pendrive): `id1/pak0.pak`, o el `pak0.pak` suelto; prefiere la versión
  registrada. Para tenerlos: `tools/quake_datos.sh` baja el shareware 1.06 (episodio 1) antes de compilar, o
  se pone el juego en `input_share/games/Quake/` ([docs/input_share.md](docs/input_share.md)). Sin datos,
  una ventana explica cómo conseguirlos. La configuración y las partidas van a `/home/quake/id1`.
  Prueba: `tests/h229_quake.py`.

## Extras: Game Boy (GNUBoy, código de terceros, C)

- **SDL2-GNUBoy** (D235, v7.4.2): el emulador de **Game Boy y Game Boy Color** (GPL-2.0, commit `c8642d1`),
  en `userland/terceros/gnuboy/` sin tocar una línea, sobre la SDL 2 propia (`userland/libsdl2/`). Va en
  `/apps/bin/gnuboy` e **Inicio > Juegos > Game Boy (GNUBoy)**; no entra en la variante sin GPL. Ventana
  de 480x432 (160x144 por 3), pantalla completa con la proporción y bandas (F11), sonido, flechas, Z/X
  (A/B), Intro (Start), Mayús (Select), F5/F7 para guardar y cargar el estado; la batería del cartucho
  (`.sav`) y los estados van a `/home/gnuboy`.
- **Las ROM no vienen con SaguiOS.** Se buscan en `/jogos/gb`, `/home/gnuboy/roms` y los discos montados, y
  una ventana deja elegir (nombre de la cabecera del cartucho); sin ROM, otra explica dónde ponerlas.
  `input_share/games/gb/` las mete en la imagen ([docs/input_share.md](docs/input_share.md));
  `tools/gnuboy_roms_libres.sh` baja una ROM libre (Libbet, zlib) para probar. Prueba:
  `tests/h235_gnuboy.py` (la parte «unidad» corre en el PC; ROM de prueba propia en
  `tools/gnuboy_rom_teste.py`).

## Extras: programar dentro de SaguiOS

- **`cc`, un compilador de C escrito en este proyecto** (B5): compila dentro del propio
  SaguiOS un subconjunto amplio de C (int/char/short/unsigned, punteros, vectores, struct,
  union, enum, typedef, punteros a función, `switch`, macros con parámetros,
  `#include "..."`, varios ficheros de origen (D133), `long long` (D128), `float`/`double` con SSE2 o, en CPU sin SSE2, con la x87) directamente a un ejecutable ELF,
  sin ensamblador ni enlazador, con `printf`, `malloc`, `<math.h>` y compañía incluidos.
  `cc /ejemplos/hola.c -o /tmp/hola` y luego `/tmp/hola`; prueba también
  `/ejemplos/mandelbrot.c`. Cómo se pensó: D033 y D039 en [`docs/decisoes.md`](docs/decisoes.md).
- **`pascal`, un compilador de Pascal también propio** (mejora 040): Pascal al estilo de
  Turbo Pascal (enteros, `real` con SSE2 o x87, string, conjuntos, registros con variante,
  punteros, archivos, procedimientos anidados, `{$R+}`) directo a un ejecutable ELF, con el
  mismo generador de código que `cc`. Unidades `crt` (`clrscr`, `gotoxy`, `readkey`...) y
  `sagui` (gráficos en una ventana). `pascal /ejemplos/pascal/hola.pas -o /tmp/hola`; más
  ejemplos en `/ejemplos/pascal` y el manual en `/ayuda/pascal.md`. Detalles: D085-D087.
- **libsagui** (v6.1, D126): componentes nuevos, lienzo con imágenes, barra de progreso y casilla;
  y la biblioteca de iconos (`icones`, D135), con 112 iconos de 16x16.
- **`qjs`, JavaScript en la terminal** (D188, v7.2): el QuickJS del navegador (D149) como intérprete:
  `qjs archivo.js [args]`, `qjs -e 'código'` y un REPL con historial. `console`, `print`, temporizadores,
  módulos ES locales y los módulos `std`/`os` (ficheros, entorno, `sleep`). Errores con traza y código de salida 1.
- **`python`, MicroPython 1.24** (B6): REPL en el terminal, `python archivo.py` y
  `python -c`. **Es código de terceros** (licencia MIT), la única excepción a la regla de
  autoría, aislado en `userland/micropython/` y opcional: `make MICROPYTHON=0` genera la
  imagen sin él (D034). Trae `float` en double por SSE2 o x87 (D038, D082) y una biblioteca estándar
  (mejora 032, D067-D069):
  - archivos con `open()` (texto en Latin-1 como el Editor, o binario), `io.StringIO`/`BytesIO`,
    `sys.stdin`/`stdout`/`stderr`; `str` unicode (`len("ñ") == 1`) con acentos en el terminal;
  - `import` de `.py` y paquetes: la carpeta del script y `/lib/python` están en `sys.path`;
  - módulos `os` y `os.path`, `sys`, `io`, `json`, `re`, `random`, `time` (con `localtime`,
    `strftime`), `struct`, `collections` (`deque`, `namedtuple`, `OrderedDict`,
    `defaultdict`), `heapq`, `binascii`, `hashlib` (md5, sha1, sha256, sha384, sha512),
    `errno`, `array`, `select`, `math`, `cmath`, `gc`, `micropython`, y `sqlite3` (SQLite 3.54.1, subconjunto de
    la DB-API 2.0; mejora 064, D116, [`docs/sqlite.md`](docs/sqlite.md));
  - red: `socket` (TCP cliente y servidor, UDP, DNS), `ssl` (la libtls, con los certificados de
    `/etc/ssl/certs`) y, en `/lib/python`, `requests`, `urllib.parse` y `litemonkey`;
  - el heap empieza con 4 MB y crece solo (`-X heap=16M` lo fija). Ejemplos:
    `/ejemplos/archivos.py`, `/ejemplos/red.py` y `/ejemplos/json_http.py`;
  - **ventanas** (mejora 033, D083-D084): el módulo `sagui` (en C: `sagui.Ventana` con
    rectángulos, elipses, polígonos, texto, imágenes BMP/PNG, eventos del ratón y del teclado,
    el menú contextual y un bucle que duerme hasta el próximo evento) y, encima, un
    **`tkinter`** escrito en Python con el aspecto de SaguiOS: `Tk`, `Button`, `Entry`, `Text`,
    `Listbox`, `Canvas`, `Scale`, `Menu`, `pack`/`grid`/`place`, `StringVar`, `bind`, `after`,
    `messagebox`, `simpledialog`, `filedialog` y un `ttk` mínimo. Prueba
    `python /ejemplos/tk/calculadora.py` o `/ejemplos/tk/juego.py`; lo que hay, en
    `/ayuda/python-tk.md`;
  - sin SSE2 (Pentium III, Athlon XP), `python` usa solo `/bin/python-x87`, con los mismos
    resultados (D082).
- **`simd`**: los programas pueden usar la FPU x87, MMX y SSE; el kernel guarda su estado
  por proceso (D032).
- **`pantalla`**: lista los modos de vídeo y cambia la resolución; en QEMU, Bochs y
  VirtualBox sin reiniciar (`pantalla 1280x1024`), en un PC de verdad al siguiente arranque
  (`--guardar` la deja en el disco de arranque). El framebuffer usa *write-combining* cuando
  la CPU lo permite (D057-D059).

## Extras: PC antiguos (mejora 036, fase A)

- **Sin SSE2 de forma automática** (Pentium III, Athlon XP): el kernel y la libsagui copian
  con `rep movsd`, `cc` genera código x87 (`-sse2`/`-x87` lo fuerzan) y `simd` prueba solo lo
  que hay (D076). Python con números reales aún necesita SSE2.
- **CD arrancable:** `make iso` → `build/saguios.iso` (El Torito, hecha por
  `tools/mkiso.py`); `./run_in_qemu.sh --cd`. Desde el CD no se guarda nada en el disco de
  arranque (D077).
- **Vídeo a 16 y 15 bits** (RGB565/RGB555) para tarjetas de 2-8 MB: el escritorio sigue a 32
  bits y se convierte al copiar a la tarjeta (en C o con SSE2); `mkimagem.py --bpp 16` lo
  prefiere (D078).
- **`infopc --informe`**: un `.txt` con la CPU, la memoria, cada dispositivo PCI con su driver
  (o la mejora que lo prevé), los modos VESA, los discos, las tablas ACPI y el log del kernel,
  para enviarlo. Máquinas probadas: [`docs/compatibilidad.md`](docs/compatibilidad.md).
- **Monitor de hardware** (D092, mejora 050): `sensores` y `temp` muestran la temperatura de la
  CPU (Intel por MSR; AMD K8, 10h-16h y Ryzen por PCI/SMN), la frecuencia real (APERF/MPERF),
  el multiplicador, los P-states, la tensión pedida, el turbo y el microcódigo; también en la
  pestaña CPU de `infopc`, en el informe y en la pestaña Rendimiento del Administrador de
  tareas (gráficas de 60 s, en amarillo cerca del máximo y en rojo si la CPU limita). Es
  **solo lectura**: un MSR que no existe no tumba el sistema (el #GP se absorbe), y la placa
  (ventiladores, SPD) no se toca. En una máquina virtual, «no disponible».
- **SaguiMark** (D093, mejora 051): `saguimark [--rapido]` o la pestaña Benchmark de `infopc`
  miden enteros, FPU x87, SIMD (SSE2) y memoria con pruebas verificadas (una CPU que calcula
  mal da «ERROR») y dan una puntuación; 1000 = referencia, por ahora provisional (QEMU/KVM; la
  definitiva será el HP dv7). El resultado se guarda en `/home/saguimark-AAAAMMDD-HHMM.txt` y
  se compara con `/lib/saguimark/referencias.txt`.
- **`benchmark`** (D176): `benchmark [--mono | --multi [N]] [--rapido]` mide ocho cargas con un núcleo y con todos a la vez (procesos e hilos), con puntuación (1000 = QEMU con KVM y una CPU), factor de escala y eficiencia; también en la pestaña Rendimiento de `infopc`. El historial queda en `/home/.benchmark`.
- **SaguiGL** (D104, mejora 054 v5): `libsgl`, API de OpenGL ES 1.1 por software (matrices con
  pila, `glFrustumf`/`glOrthof`, `glDrawArrays`/`glDrawElements`/`glBegin`, búfer Z de 16 o
  32 bits, Gouraud y plano, recorte contra el frustum, descarte de caras traseras; desde la v6, además, texturas,
  mezcla, niebla y luces, con ruta SIMD (D130)). `sgldemo` muestra un cubo girando con los
  fps (`-t N` mide sin ventana). La prueba `tests/h77_sgl.py` compara el resultado, píxel a
  píxel (suma), con el mismo motor compilado en el anfitrión. `infopc` (pestaña Vídeo)
  identifica la familia de la GPU por su PCI (AMD R300-Evergreen, Intel Gen2-Gen6) y dice si
  SaguiGL tendría motor por hardware.
- **Armazón de los drivers 3D de AMD** (D158, mejora 054 B2, `/bin/amd`): la tabla de familias
  (Evergreen, R5xx y R300/R420, y R6xx/R7xx), los registros de identificación y la lectura de los
  registros MMIO del BAR. Es el armazón y el mínimo del kernel para leerlos, no el motor 3D por GPU.
- **Discos SATA en modo AHCI** (D155, mejora 087): entran en la misma capa de discos que los IDE,
  con las mismas reglas de solo lectura. El USB 3.0 (xHCI) queda como esqueleto, según
  [`docs/usb3.md`](docs/usb3.md).

## Extras: lite-monkey, la base de datos de documentos

- **`litemonkey`** (mejora 067, D117): base de datos **documental** propia, en C, inspirada en
  MongoDB pero más básica, en el **puerto 7979**. El mismo código funciona en el PC
  (`make -C litemonkey`) y en SaguiOS (`litemonkey &`; datos en `/home/litemonkey`). Documentos
  BSON, CRUD con operadores de consulta y actualización, índices (con `unique` y multiclave),
  agregación básica (`$match`, `$group`, `$sort`...), cursores, archivo de solo añadir con CRC
  que se recupera tras un corte de luz y se compacta renombrando, y autenticación HMAC-SHA256
  opcional.
- **Cliente Python** `litemonkey` con API parecida a pymongo, el mismo en CPython y en el
  MicroPython de SaguiOS: `c = litemonkey.Cliente("127.0.0.1"); c.tienda.productos.find({...})`.
  Consola **`lmk`** (`show dbs`, `use`, `db.col.find(...)`, `it`). Desde el PC a SaguiOS con la
  redirección de puertos de QEMU; dentro de SaguiOS por 127.0.0.1. Manual:
  [`docs/litemonkey.md`](docs/litemonkey.md).

## Extras: Mico, el editor de código

- **Mico** (`mico carpeta`, o Inicio > Accesorios): explorador de la carpeta, pestañas,
  colores para C, Python, sh, Markdown y ensamblador de CHIP-8, deshacer ilimitado,
  sangría automática, comentar con Ctrl+/, mover líneas con Alt+↑/↓, buscar y reemplazar,
  buscar en toda la carpeta, ir a línea, paleta de órdenes (Ctrl+Mayús+P) y temas claro y
  oscuro. **F5** compila y ejecuta en el terminal de abajo (C, Pascal, Python, sh); los errores de `cc` y `pascal` y las trazas
  de Python salen en **Problemas** y un clic lleva a la línea. Guarda de forma atómica y hace
  copias de recuperación cada 5 s. Prueba con `mico /ejemplos/mico`; manual en
  `/ayuda/mico.md`. Detalles: D075.
- **Portapapeles del sistema:** copiar y pegar texto entre el Mico, el Editor, el Markdown y
  el Terminal (en el Terminal, Ctrl+Mayús+C/V o el botón derecho). Detalles: D074.

## Extras: hoja de cálculo y correo

- **Macaco** (D142, v6.3): hoja de cálculo con cuadrícula, fórmulas típicas (suma, promedio, mínimo,
  máximo, contar, si, redondear…), gráficos de barras y de tarta, y ficheros propios `.mxs`
  (Macaco eXcel Sheet).
- **Cliente de correo** (D146, v6.3 y v6.6): recibe por POP3 o IMAP y envía por SMTP, con ventana,
  lista de mensajes, adjuntos y cuerpo en texto plano o en HTML (convertido a texto).

## Extras: barra de tareas y menú Inicio

- **Botón Inicio** (o la tecla Windows, o Ctrl+Esc): los programas más usados arriba, los
  demás agrupados en submenús (Accesorios, Gráficos, Internet, Juegos, Sistema), y abajo
  «Ejecutar...», «Configuración de pantalla», «Acerca de SaguiOS» y «Reiniciar...»
  («Apagar» sale desactivada: aún no hay ACPI).
- **Un botón por ventana** en la barra superior: clic para traerla delante o minimizarla,
  botón derecho para el menú de la ventana; si no caben, se estrechan y las que sobran van a
  «»». Clic en el reloj: la fecha completa; botón derecho: «Ajustar fecha y hora...».
- **Botón derecho en el escritorio:** «Configuración de pantalla...» y el submenú
  «Resolución», que cambia el modo al momento. Detalles: D064-D066.
- **Bandeja** a la izquierda del reloj: volumen (clic: control deslizante y «Silenciar»; la
  rueda sobre el icono sube y baja), red (la IP al pasar el ratón; clic: IP, puerta de enlace,
  DNS y «Renovar DHCP») y temperatura de la CPU (sólo si se puede leer: no en QEMU; naranja
  desde 70 °C y roja desde 79 °C). Detalles: D114, prueba `tests/h85_bandeja.py`.
- **Atajos de escritorio** (v6.0 beta): Alt+F4 cierra la ventana activa (el programa recibe el aviso,
  como con la X), Alt+Tab cambia de ventana (Alt+Mayús+Tab, a la anterior), la tecla Windows abre el
  menú, Windows+D muestra el escritorio y Ctrl+Alt+Supr abre el Administrador de tareas.
- **Capturas de pantalla** (beta 3, D125): Impr Pant, Alt+Impr Pant, Windows+Mayús+S, el programa
  Recortes y el comando `captura`.
- **Utilidad «Discos»** (v6.0 beta): cada disco y partición con una barra de espacio ocupado y libre,
  sistema de archivos, etiqueta y punto de montaje; montar, desmontar y abrir en Archivos. El comando
  `discos` del terminal muestra también el uso.
- **Explorador de archivos** (v6.3, D136): ver los ocultos (Ctrl+H), vistas de detalles y miniaturas,
  barra de ruta y panel lateral, selección múltiple, copiar, cortar, pegar y arrastrar, renombrar,
  buscar, «Abrir con», propiedades y papelera. Alt+Tab muestra miniaturas.

## Extras: imágenes y dibujo

- **Visor de imágenes** (`visor`, icono «Visor»): abre BMP, PPM/PGM/PBM, **PNG** (con un
  descompresor *deflate* escrito aquí) y **JPEG** de línea base (fotos de cámaras y teléfonos; el progresivo y el de 12 bits
  no se leen; mejora 047), ajustado a la ventana o a tamaño real, con zoom
  (rueda, `+`/`-`), arrastre, imagen anterior/siguiente de la carpeta (AvPág/RePág) y
  pantalla completa (F11). Archivos abre las imágenes con él. Hay ejemplos en `/imagenes`,
  dibujados por `tools/mkimagens.py`. Detalles: D054 y D055.
- **Paint** (`paint`, icono «Paint»): lápiz, pincel, goma, aerógrafo, líneas, rectángulos y
  elipses (contorno o rellenos), bote de relleno, cuentagotas y texto; color principal (clic
  izquierdo) y secundario (derecho); deshacer de varios niveles (Ctrl+Z); guarda en BMP
  (Ctrl+S). Detalles: D056.
- **Vídeo MJPEG en AVI** (icono «Reproductor de vídeo», v6.2, D129), con el decodificador JPEG y
  `tools/convertir-video.sh` para preparar los vídeos.
- **Visor de PDF** (`visorpdf`, D138): motor propio en `userland/pdf/`, con tipografías, filtros y
  cifrados RC4 y AES-256.

## Extras: fuentes vectoriales (v7.2, D196)

- **libfontes** (`userland/libfontes/`, `userland/inc/fontes.h`): TrueType, OpenType con contornos CFF,
  WOFF y WOFF2, con antialias por cobertura exacta. Es el motor del navegador Cipó, ahora compartido:
  `fonte_abrir_archivo`, `fonte_medir`, `fonte_texto` (sobre una ventana o un búfer, con color y tamaño en
  píxeles), `fonte_nombre` y el catálogo de `/apps/fuentes` y `/home/fuentes`.
- **Fuentes de Typodermic** (Raymond Larabie, **CC0**): 28 en `/apps/fuentes` (partición `SAGUI-APPS`) y
  `tools/fuentes_typodermic.py` para sacarlas del zip (`NEW_FONTS/`, fuera de git) o empaquetar las 731 en
  una carpeta o un `.tar.gz` que copiar a un disco instalado.
- **Configuración > Fuentes:** lista, vista previa y fuente por defecto para documentos y títulos. **Paint**
  la usa en su herramienta de texto y el **Editor** tiene un botón «Vista» que muestra el documento con ella.
  El escritorio sigue con la fuente de mapa de bits 8x16 (siguiente paso).

## Extras: actividades educativas JClic

- **Inicio > Educación > Actividades JClic** (D185, v7.1) abre los proyectos del repositorio de Clic del XTEC
  (https://projectes.xtec.cat/clic/es/repo/): `.jclic.zip` o `.jclic`, también con doble clic en Archivos.
  Se juegan puzles (doble, de intercambio y de agujero), asociaciones simples y complejas, memoria, explorar,
  identificar, sopas de letras y crucigramas, con contadores de aciertos, intentos y tiempo. Las actividades
  de texto y el antiguo Clic 3.0 (`.pac`) no se leen todavía: se saltan avisando.
- **Inicio > Educación > Actividades JClic (bajar)** lista el repositorio (unos 2.600 proyectos, filtro de
  idioma y búsqueda), baja los que se quieran a `/home/jclic/` y los abre. Sin red se usa el índice guardado.
  **Ningún proyecto del repositorio viaja con SaguiOS** (cada uno tiene su licencia, normalmente Creative
  Commons); solo un ejemplo propio en `/jogos/jclic`. Prueba: `tests/h152_jclic.py`.

## Extras: Solitario Spider

- **Solitario Spider** (`spider`, D173; Inicio > Juegos): el Spider del Windows Me con 104 cartas, 10 columnas,
  niveles de 1, 2 y 4 palos, arrastrar y soltar, clic para mover a la mejor columna, pista (H), deshacer
  (Ctrl+Z), partida nueva (F2) y puntuación de Windows; cartas, palos y figuras dibujados en código, con
  animación al repartir, al retirar una escalera y al ganar. `spider --autoprueba` comprueba las reglas sin ventana.

<div align="center">
<img src="docs/capturas/H143-2-tapete.png" width="32%">
<img src="docs/capturas/H143-4-arrastre.png" width="32%">
<img src="docs/capturas/H143-9b-pantalla-completa.png" width="32%">
</div>

## Extras: CHIP-8

- **Emulador `chip8`** (icono «CHIP-8», D070): CHIP-8, **SUPER-CHIP 1.1** y **XO-CHIP**
  (128x64, desplazamiento, sprites de 16x16, 64 KB, cuatro colores y el patrón de sonido real
  por la tarjeta de sonido; sin tarjeta, aproximado por el altavoz), escrito desde la documentación pública. Los *quirks* de cada máquina se eligen por
  la extensión (`.ch8`, `.sc8`, `.xo8`) o uno a uno (`-q`); ventana redimensionable con
  pantalla completa, paletas (una con los colores de SaguiOS), pausa, paso a paso,
  depurador (F10) y `chip8 -t` para ejecutar sin ventana.
- **Ensamblador `chip8asm`** (D071): etiquetas, constantes, `db`/`dw`, `include` y errores con
  archivo y línea; `-d` desensambla. El mismo código monta en el build las ROM incluidas.
- **Cuatro juegos propios** en `/jogos/chip8`, con su fuente en ensamblador (D072): RAMAS (el
  sagüí recoge bananas y esquiva palos), SUMAS (un puzzle de 4x4 con el teclado hexadecimal),
  CANOA (bajar el río en SUPER-CHIP) y AURORA (demostración XO-CHIP a 4 colores con melodía).
  Manual: `/ayuda/chip8.md` (`markdown /ayuda/chip8.md`).

<div align="center">
<img src="docs/capturas/H63-chip8-ramas-jogo.png" width="32%">
<img src="docs/capturas/H63-chip8-depurador.png" width="32%">
<img src="docs/capturas/H63-chip8-aurora.png" width="32%">
</div>

## Extras: asistente de IA

- **Cliente `ia`** (D134, v6.2.1): comando del terminal con una API compatible con la de OpenAI y
  respuesta en *streaming*. Proveedores: **OpenRouter** o cualquier endpoint compatible con OpenAI
  (llama.cpp, Ollama, LM Studio…).
- **Configuración compartida** (D145, v6.6): en Configuración, pestaña «IA», se eligen el proveedor, la
  dirección, la clave y los modelos (general, terminal, chat y Mico). Cada programa usa su modelo; la
  clave se guarda aparte y no se muestra.
- **Aplicación de chat** con ventana, contexto y en castellano (D143), y el **panel Asistente** de Mico
  (D140). Según el ROADMAP, siguen pendientes la lista lateral de conversaciones, el Markdown en las
  respuestas y el selector de proveedor y modelo dentro de la ventana del chat.

## Extras: sonido

- **Tarjetas de sonido AC'97 e Intel HDA** (D088-D090, mejora 041): AC'97 de Intel (ICH),
  SiS, NVIDIA y AMD, y cualquier controlador HDA, con el recorrido de los *widgets* del códec
  hasta el altavoz, los auriculares y la salida de línea. PCM de 16 bits estéreo a 48 kHz, con
  interrupciones y, si no llegan, por sondeo. El altavoz del PC sigue como reserva.
- **Mezclador en el núcleo**: varios programas suenan a la vez, cada uno a su frecuencia
  (4-96 kHz, 8 o 16 bits, mono o estéreo), con volumen por flujo y general.
  - `reproducir archivo.wav|mp3|flac` y `volumen [0-100|silencio|activar]`; `infopc` muestra la
    tarjeta y el códec.
  - **Formatos de audio con decodificadores propios** (D098-D100, mejora 053): MP3 (MPEG-1 y 2
    Layer III, VBR, etiquetas ID3), FLAC sin pérdidas y WAV IMA ADPCM, reconocidos por el
    contenido y tocados en *streaming*; `reproducir` muestra formato, frecuencia, canales,
    duración y etiquetas; el doble clic en Archivos abre `.wav`, `.mp3` y `.flac`. Un MP3 estéreo
    cuesta ~18 Mciclos por segundo de audio (tiempo real con holgura en un Pentium III). Ogg
    Vorbis, pendiente. Preparar audio: `tools/convertir-audio.sh`.
  - **Reproductor de música gráfico** (`musica`, menú Inicio > Accesorios > «Música», D115): lista
    de reproducción (archivos o una carpeta entera), anterior/siguiente, reproducir/pausa/parar,
    barra de progreso con tiempo y salto con un clic, volumen, formato y etiquetas
    (ID3, comentarios FLAC); espacio, flechas ±5 s, N, P. No bloquea la interfaz. El doble clic en
    `.wav`, `.mp3`, `.flac` u `.ogg` en Archivos lo abre. Prueba `h86_musica.py`.
  - **Reproductor de MIDI** (`midi`, Inicio > Accesorios > «MIDI», D190): toca `.mid`, `.midi` y `.kar` con un
    sintetizador propio de la libsagui (`audio_midi.c`, sin ficheros de datos); vista de los 16 canales con el
    instrumento de GM, teclado de 88 notas y medidor, letra karaoke y lista de la carpeta. `musica` y
    `reproducir` también tocan MIDI. Ejemplos en `/ejemplos/musica`. Prueba `h157_midi.py`.
  - CHIP-8 toca el patrón de audio XO-CHIP de verdad; Banana y Sagui 3D, sus efectos; en
    Python, `sagui.Sonido(22050).escribir(datos)`.
  - En QEMU: `./run_in_qemu.sh --sonido` (HDA por PulseAudio; `--ac97` para la otra).

## Extras: fecha, hora y zona horaria

- **El núcleo guarda la hora en UTC** y el reloj de la placa puede estar en UTC o en hora
  local, como en los PC con Windows (por defecto, local en un PC y UTC en una máquina
  virtual). Las zonas usan reglas **POSIX TZ** con un intérprete escrito aquí y una tabla de
  84 zonas («España (Madrid, península)», «Brasil (São Paulo, Brasilia, Río)»...): la barra
  cambia sola al horario de verano. Detalles: D080 y D081, mejora 039.
  - `zona` (la actual, `zona lista`, `zona Europe/Madrid`, `zona --rtc local`), la ventana
    **«Fecha y hora»** (Inicio → Sistema, o botón derecho en el reloj), `date` con la
    abreviatura (CET/CEST) y `-u`, la variable `TZ` por programa, `ls -l` con fechas y
    `ntp` (SNTP) para poner en hora. Con `/home` persistente la zona se guarda en
    `/home/.zona`.

## Red y HTTPS

- **Red cableada** (D046, D047): drivers Intel e1000 y Realtek RTL8139, pila IPv4 propia
  (ARP, ICMP, UDP, DHCP, DNS, TCP), bucle local `127.0.0.1` y sockets como descriptores.
  Comandos `ifconfig`, `dhcp`, `ping`, `nslookup`, `nc` y el navegador de texto **Cipó**
  (`cipo`).
- **Pestaña «Red» del Administrador de tareas** (D079, mejora 038): interfaces con estado,
  direcciones, contadores y velocidad, gráfico del último minuto, conexiones con su proceso,
  y «Renovar DHCP», «Desactivar interfaz» y «Cerrar conexión».
- **HTTPS con criptografía escrita aquí** (D060-D063, mejora 027): TLS 1.3 y TLS 1.2 (ECDHE con
  AES-GCM o ChaCha20-Poly1305), X25519, P-256/P-384, certificados RSA y ECDSA, validación X.509
  completa (cadena, fechas, nombre, usos) con mensajes claros ("certificado caducado", "nombre
  no coincide", "emisor desconocido"). Los números aleatorios salen de un generador del núcleo
  (RDRAND si lo hay; si no, el reloj de las interrupciones) también como `/dev/urandom`.
  - `curl https://...` (`-v` enseña la conexión y el certificado, `-k` no lo comprueba,
    `--cacert ca.pem`), `wget` con barra de progreso, `sha256sum`, y Cipó abre páginas HTTPS.
  - `certificados` gestiona las raíces de confianza de `/etc/ssl/certs` (32 de fábrica:
    Let's Encrypt, DigiCert, GlobalSign, Google, Amazon...): `lista`, `ver`, `añadir`,
    `quitar` y `probar host` (conecta y explica la validación). Con `/home` persistente los
    cambios sobreviven al reinicio.
  - Sin auditoría y sin revocación (OCSP/CRL): es un sistema educativo; los límites están en
    D063.

## Extras: el navegador Cipó

- **Motor web propio** (D141, v6.3): CSS, JavaScript, Canvas, SVG y WebGL por software, con el dibujado
  por bandas; descargas de uno en uno y botón derecho para guardar.
- **Pestañas, marcadores y búsqueda** (D148, D152): cada pestaña con su caché y su posición de lectura,
  marcadores, copiar, búsqueda sin tildes e imágenes JPEG reducidas. Ctrl+clic abre el enlace en otra
  pestaña, y la posición de lectura de cada sitio se recuerda.
- **JavaScript** (D149, D160): el motor QuickJS, portado al SO. Cada `<script>` ve un DOM que puede
  consultar (`getElementById`, `getElementsByTagName`, `querySelector`), cambiar atributos y
  `textContent`, y poner tratadores de clic.
- **Reenvíos** (D159): `window`, `location` y `meta refresh`, con la navegación diferida hasta que el
  script termina.
- **Transiciones CSS** (D153) y **pseudo-elementos** sin romper el DOM (D161): `https://www.amd.com/`
  carga entera.

## Extras: compresión (mejora 056)

- **`tar`, `gzip`/`gunzip`/`zcat`, `zip` y `unzip`** con DEFLATE propio (LZ77 + Huffman
  dinámico/fijo, niveles 0-9), CRC-32, gzip de varios miembros, tar ustar/GNU/pax y zip stored y
  deflate (sin cifrado ni ZIP64). La extracción nunca sale de la carpeta de destino. Probado en
  los dos sentidos con `tar`, `gzip`, `zip`, `unzip`, `tarfile` y `zipfile` del anfitrión
  (`tests/h76_compresor.py`). Detalles y límites: D103.

## Discos: particiones e instalador (v7.0b, D179)

- **Particiones:** `particiones` en el terminal y el botón **Particiones** de la ventana Discos
  (`discos --particiones [disco]`): tabla MBR con primarias, **extendida y lógicas**, alineadas a
  1 MB. Crear, borrar, cambiar el tamaño (un FAT32 crece con la partición si su FAT da para ello;
  encoger un sistema de archivos no se hace), formatear en **FAT32 o ext2**, marcar la activa,
  **TRIM** (en discos SATA/AHCI que lo admiten) y **deshacer** el último cambio. La GPT se muestra
  pero no se toca.
- **Instalador:** Inicio > Sistema > **Instalar SaguiOS** (o `instalar <disco>`). Copia el sistema
  que está corriendo a un disco duro o a otro pendrive (el disco entero, un hueco libre o en lugar de
  una partición) y lo deja arrancable por BIOS, con `/home`, los juegos (en `/jogos/opentyrian`) y las
  aplicaciones (en `/apps`, con lo que haya en el `/apps` del sistema) en particiones ext2 propias que se
  montan por etiqueta, como en la imagen. Opcionalmente copia los datos de juegos de `/jogos`.
- **Seguridad:** el kernel solo deja escribir en un disco **reservado**, y solo lo reserva si no hay
  nada montado de él; el disco de arranque pide **dos confirmaciones**; cada diálogo dice qué se
  borra; la tabla se valida antes de escribirla (EBR primero, MBR al final) y se relee; el instalador
  deja el disco **no arrancable** hasta haberlo verificado todo. Detalles en
  [`docs/decisoes.md`](docs/decisoes.md#d179-gestión-de-particiones-trim-y-un-instalador-de-saguios-a-disco-duro-o-pendrive-v70b).

## Particiones del sistema: juegos y aplicaciones (v7.0b y v7.1, D181 y D194)

El ramdisk, que se copia entero a la RAM al arrancar, lleva solo **la base** del sistema (el shell, el
escritorio, Archivos, Discos, el instalador, Configuración y las bibliotecas). Lo que no es esencial va en
particiones de la propia imagen, montadas por su etiqueta, que no ocupan RAM:

| Partición | Formato | Se monta en | Qué lleva | Variables |
| --- | --- | --- | --- | --- |
| `SAGUI-JOGOS` | ext2, lectura y escritura | `/jogos` | un juego por carpeta: `/jogos/opentyrian` (D181), `/jogos/openjazz`, `/jogos/quake` (D229) y lo que se meta, como la versión completa de un juego (D228); también los juegos de la base del ramdisk (chip8, jclic, sagui3d), que el montaje tapa | `JOGOS_MB` (800 en las persistentes, la de particiones y el instalador), `JOGOS_IMG_MB` (0 = la justa en `saguios.img`), `JOGOS=0` |
| `SAGUI-APPS` | ext2, lectura y escritura | `/apps` | las aplicaciones no fundamentales: `/apps/bin` (en el `PATH`: NetSurf, `sqlite3`) y los datos de cada una en `/apps/<app>` (D194) | `APPS_MB` (0 = lo justo más margen), `APPS=0` |
| `SAGUI-CONF` | ext2, 16 MB, solo root | la monta el kernel (mejora 098) | la configuración del sistema (mejora 098, D256): usuarios, claves, red; de momento solo un `LEEME`. Va la última, en todas las imágenes de disco o pendrive y en las instalaciones; **la ISO no la lleva** (modo en vivo). El instalador conserva la que ya haya (`--formatear-conf` la borra); `mkimagem.py --sin-conf` la quita | (fija) |

- Si falta una partición, el sistema **arranca igual**: sin `SAGUI-APPS`, NetSurf no sale en el menú
  Inicio y `netsurf` no se encuentra.
- `make APPS=0` deja las aplicaciones **dentro del ramdisk** (en `/apps`, con las mismas rutas): una
  imagen de un solo bloque. El **CD** no lee particiones, así que la ISO las lleva siempre en su ramdisk.
- Para instalar otra aplicación basta con copiar su ejecutable a `/apps/bin` y sus datos a `/apps/<app>`;
  el instalador copia todo `/apps` al disco instalado. Prueba: `tests/h159_sagui_apps.py`.

## Multinúcleo e hilos

SaguiOS arranca con un procesador, pero ya puede usar varios. Cada pieza tiene su opción al crear la imagen.

- **SMP opcional** (D164, v6.7; cerrojo general del kernel en D165, v6.8): con `--smp` la imagen enciende los
  procesadores secundarios. Un cerrojo general del kernel (`kernel/nucleo/trava_geral.c`) deja que cada CPU
  atienda sus llamadas al sistema, sus fallos y sus excepciones, así que varios programas corren a la vez.
  Las tareas del kernel siguen en la CPU 0.
- **IOAPIC opcional** (D166, v6.8): con `--ioapic` las IRQ de los dispositivos llegan por el IOAPIC y no por
  el 8259, con vuelta atrás al PIC si el tic no llega.
- **Interrupciones repartidas** (D168, v6.8): con `--ioapic --smp` las IRQ 1-15 se reparten por turnos entre
  las CPU secundarias; el PIT (IRQ 0) se queda en la CPU 0. En las pruebas, el teclado va a la CPU 1 y la red
  o el sonido (IRQ 11), a otra CPU.
- **Hilos de usuario** (D167, v6.8): un hilo comparte el espacio, los ficheros, la carpeta y las ventanas de su
  proceso. La libsagui trae `thread_create`, `thread_join`, `thread_yield`, `thread_sair`, `mutex_t` y
  `semaforo_t` (que duermen en el kernel, por futex), y `malloc` es seguro entre hilos. La misma API está en la
  biblioteca del `cc`; ejemplo: `/ejemplos/hilos.c`. Python y Pascal todavía no tienen hilos
  ([`docs/multihilo.md`](docs/multihilo.md)).

**Por defecto la imagen arranca con una sola CPU.** Para crear la imagen con varios procesadores, tras `make`:

```bash
python3 tools/mkimagem.py --mbr build/boot/mbr.bin --estagio2 build/boot/estagio2.bin \
    --kernel build/kernel.bin --kernel-elf build/kernel.elf --ramdisk build/ramdisk.sgfs \
    --saida build/saguios.img --vesa --smp --ioapic
```

Las dos opciones son independientes: `--smp` enciende los secundarios, `--ioapic` cambia el controlador de
interrupciones, y con las dos las IRQ se reparten. La orden sustituye a `build/saguios.img`, que es la que
arranca `./run_in_qemu.sh --nucleos 4` (4 CPU en la máquina); el siguiente `make` que cambie algo la vuelve a
crear sin estas opciones. Sin `--smp` en la imagen, las CPU extra no se usan.

**Medidas** (KVM, 4 CPU, [`docs/multihilo.md`](docs/multihilo.md)): el cálculo con `bench 4 c` gana entre 2,7x
y 3,2x, y cuatro hilos (`bench 4 h`) entre 3,8x y 4,1x.

**Límites:** hay un solo cerrojo general, así que una IRQ atendida en una CPU secundaria espera a que la CPU
que está en el kernel lo suelte; el reparto de IRQ es fijo por turnos; y el IOAPIC y las líneas PCI sin `_PRT`
no se han probado en hardware real (D166, D168).

## Cifras

| | |
| --- | --- |
| Imagen de arranque | **28.959.232 bytes** útiles: 12.182.016 de arranque (11.740.144 de ramdisk) y 16 MB de las particiones SAGUI-JOGOS y SAGUI-APPS (meta de 38 MB desde la v7.0b, D181 y D194; el original, 575.488) |
| Líneas de código | arranque 903, kernel 9.614, programas 6.317 (más 3.591 de herramientas y pruebas) |
| Programas | decenas en `/bin`: shell, comandos tipo Unix (`ls`, `grep`, `find`, `sort`, `head`, `tail`... ver `docs/comandos.md`, D097) y programas gráficos |
| Pruebas automáticas | más de 130 (`tests/h*.py`, una por historia), en QEMU |
| Decisiones registradas | 163 entradas (D001 a D168), con el razonamiento de cada una |
| Errores registrados | 25, con síntoma, causa y corrección |
| Commits | 711 en `main` (`git log`) |

## Probarlo

### Qué imágenes hay en el repositorio y cuáles se generan

Desde la v7.4 (decisión del mantenedor, 2026-11-13), **en GitHub solo se suben las dos imágenes
básicas** de cada versión, en `entrega/vX.Y[.Z]/`: `saguios.img` (un núcleo) y `saguios-smp.img`
(multinúcleo), con `SHA256SUMS.txt` y `VERSION.txt`. Así el repositorio no crece con cientos de MB
por versión (GitHub rechaza ficheros de más de 100 MB). En estas dos la partición de juegos tiene
el tamaño justo de lo que lleva.

**Todas las demás se generan en tu equipo** una vez descargado el repositorio:

```
make variantes DESTINO=entrega/mia      # todas: persistentes ext2 y FAT32, con y sin multinúcleo, con uACPI,
                                        # la de particiones, la sin GPL, la ISO y el .vdi
make imagem-persistente-smp             # o solo una (mira «make help»)
make imagem-persistente-smp-uacpi       # la de portátiles: persistente, multinúcleo y con
                                        # el indicador de batería (uACPI, D192)
```

Las persistentes, la de particiones y el instalador crean la **partición de juegos de 800 MB, de
lectura y escritura** (`/jogos`, una carpeta por juego), donde puedes copiar después la versión
completa de un juego desde el propio SaguiOS. Los tamaños se cambian con variables
(`JOGOS_MB`, `APPS_MB`, `DATOS_MB`…).

**Contenido propio:** lo que no puede ir en el repositorio ni descargarse con un guion al compilar
(por ejemplo, la versión completa de un juego que tengas) se pone en la carpeta `input_share/`
(ignorada por git: nunca se sube) y entra en las imágenes que generes. Por ejemplo,
`input_share/games/Jazz/` acaba en `/jogos/Jazz` (con `make` normal; en `make variantes` hay que
pedirlo con `make variantes GERAR_OPC=--con-input-share`, para que una entrega publicada nunca lo
lleve por descuido). Detalles en
[`docs/input_share.md`](docs/input_share.md).

### Cómo usarlas

Las imágenes listas están en [`entrega/final/`](entrega/final/) y en `entrega/vX.Y[.Z]/` (las básicas):

- **VirtualBox:** `saguios.vdi` (máquina "Other/Unknown", 32 bits, EFI desactivado).
- **PC de verdad:** graba `saguios.img` en un pendrive en modo DD (Rufus, balenaEtcher o `dd`)
  y arranca con la BIOS tradicional; en PC que no arrancan desde USB, graba `saguios.iso`
  (`make iso`) en un CD.
- **QEMU con ventana:** `./run_in_qemu.sh` (compila si hace falta y arranca `build/saguios.img`;
  `--usb` arranca como pendrive, `--cd` desde la ISO, `--nucleos N` con N procesadores (ver «Multinúcleo e hilos»), `--mem MB`
  (2048 por defecto), `--cpu pentium3` sin SSE2, `--sonido`
  activa el altavoz y una tarjeta Intel HDA y `--help` muestra el resto).
- **Generar una versión:** `./gerar_versao.sh` (el hermano de `run_in_qemu.sh`): compila, arma las imágenes de arranque y deja la entrega en `entrega/latest/` con sus sumas SHA-256 y una hoja `VERSION.txt`; `--help` muestra las opciones, que incluyen la variante de licencia, si lleva MicroPython, el nivel de optimización y `--probar` (arrancar la imagen antes de publicarla).

El paso a paso está en [`docs/como-gravar.md`](docs/como-gravar.md).

## Compilar y probar

Requisitos (Ubuntu): `nasm gcc gcc-multilib binutils make python3 qemu-system-x86 qemu-utils imagemagick mtools dosfstools`.

**Guía completa para compilar tus propias imágenes** (requisitos, todas las opciones, descargas, juegos propios y problemas frecuentes), en español, inglés y portugués: [`compilar_versiones.md`](compilar_versiones.md).

```bash
make            # build/saguios.img
make vdi        # build/saguios.vdi (VirtualBox)
make iso        # build/saguios.iso (CD arrancable, D077)
make test       # todos os testes no QEMU (sem janela)
make rodar      # abre no QEMU com janela
make versao     # genera la versión y deja la entrega (./gerar_versao.sh)
```

Lo que quien compila quiera meter en su imagen y no puede ir en el repositorio (por ejemplo la versión
completa de un juego) va en `input_share/` (ignorada por git): `input_share/games/<carpeta>` acaba en
`/jogos/<carpeta>` y `input_share/apps/` en `/apps`. Ver [docs/input_share.md](docs/input_share.md).
Los programas de C++ (OpenJazz) piden el `cc1plus` de g++-13, que `tools/entorno-local.sh` instala.

Las pruebas automáticas están en `tests/`, una por historia. Una sola: `python3 tests/h101_sagui_gl_v6.py`.
La batería completa: `python3 tests/rodar_todos.py` (en serie) o `python3 tests/rodar_paralelo.py --help`
(varias a la vez, con `-j N` y `--kvm` si hay KVM). En paralelo, **`-j 2` o `-j 3` como mucho**: cada QEMU
ocupa CPU y memoria, y con más el ordenador puede colgarse o reiniciarse.

## Estructura

```
boot/        estágio 1 (MBR) e estágio 2 do carregador, em NASM
kernel/      kernel em C freestanding + NASM: núcleo, memória, drivers, USB, arquivos, janelas
userland/    libsagui e os programas; userland/sagui3d/ é o jogo do bônus B4;
             userland/micropython/ é o MicroPython (terceiros, MIT, opcional)
             userland/web/ (motor del Cipó y QuickJS), userland/pdf/ (visor de PDF), userland/ia/,
             userland/mail/ (cliente de correo) y userland/terceros/ (OpenTyrian, D156)
rootfs/      arquivos que vão para o disco na RAM (textos, fases do Sagui 3D)
tools/       build (mksgfs.py, mkimagem.py, mkiso.py), painel (status.py), verificador de fases
tests/       testes automáticos no QEMU, um por história
docs/        histórias, decisões, diário, erros, roteiros de teste e capturas de tela
dashboard/   painel do projeto (abra dashboard/index.html no navegador)
entrega/     imagens .img e .vdi de cada fase, com SHA-256
testes_eli/  fotos dos testes do Eli no G470
```

## Documentación

> **Roadmap de versiones**: [`docs/ROADMAP.md`](docs/ROADMAP.md) · versión visual: [`docs/roadmap.html`](docs/roadmap.html).
>
> **Para continuar el trabajo en otro PC**: [`docs/CONTINUAR.md`](docs/CONTINUAR.md) (estado, ramas a medias, pruebas pendientes y cola de mejoras).

| Documento | Qué contiene |
| --- | --- |
| [`docs/resumo-final.md`](docs/resumo-final.md) | Resumen del proyecto, tiempo de cada fase y la tabla de conceptos del libro de Tanenbaum completada |
| [`docs/decisoes.md`](docs/decisoes.md) | Las 163 entradas de decisiones de diseño (D001 a D168) y la sección de Sagui 3D |
| [`docs/erros.md`](docs/erros.md) | Los 25 errores de la tabla, incluidos los hallados en el G470 |
| [`docs/compatibilidad.md`](docs/compatibilidad.md) | Máquinas probadas (✓, ⚠, ✗) y cómo enviar un informe (`infopc --informe`) |
| [`docs/diario.md`](docs/diario.md) | El diario del proyecto, fase a fase |
| [`docs/multihilo.md`](docs/multihilo.md) | Multihilo y multinúcleo: lo que hay, lo que falta y cómo se mide la ganancia |
| [`docs/licencia.md`](docs/licencia.md) | La licencia GPL v3 explicada: capas, SPDX, compatibilidades, qué se puede meter y qué no |
| [`docs/terceros.md`](docs/terceros.md) | El código de terceros: qué es, de dónde viene, con qué licencia y en qué decisión se aceptó |
| [`docs/historias/`](docs/historias/) | Una página por historia |
| [`status.md`](status.md) y [`dashboard/`](dashboard/index.html) | Estado de cada fase e historia |

## Licencia

SaguiOS-Extended es software libre bajo la **GNU General Public License, versión 3** (texto completo
en [`LICENSE`](LICENSE)). Es un fork de **[SaguiOS](https://github.com/erigobeli/sagui-os)**, el
sistema operativo creado por **Eli Rigobeli** (GPL v3), que autorizó expresamente traducirlo,
probarlo y publicar esta versión mientras siga siendo GPL v3 y con el código fuente disponible
(ver [`AUTORES.md`](AUTORES.md)). **Si te sirve este proyecto, visita y apoya el original:
https://github.com/erigobeli/sagui-os**

El esquema, por capas (explicado con calma en [`docs/licencia.md`](docs/licencia.md)):

| Capa | Licencia |
| --- | --- |
| El conjunto (el repositorio y las imágenes) | GPL v3 |
| Ficheros que ya existían en el SaguiOS original (commit `dba0fffc`) y los derivados de ellos | `GPL-3.0-only` |
| Ficheros nuevos de este fork | `GPL-2.0-or-later` (que se puede usar bajo la GPL v3) |
| Terceros (MicroPython, uACPI, SQLite, los juegos portados...) | la suya, registrada en [`docs/terceros.md`](docs/terceros.md) |

Cada fichero propio lleva su línea `SPDX-License-Identifier`. NetSurf, GNUBoy y el inicio moderno de
la red Realtek r8169 (GPL-2.0 sin «o posterior») **no van en las imágenes ni en el código
publicados**: solo quien compila puede incluirlos, para uso privado. Detalles y la decisión: D258 en [`docs/decisoes.md`](docs/decisoes.md).

## Las reglas del proyecto

SaguiOS nació con una idea sencilla (ver [`prd.md`](prd.md), la especificación original de Eli
Rigobeli): **un sistema operativo escrito desde cero**. Este fork mantiene ese espíritu con estas reglas:

- **Lo esencial es propio:** el cargador de arranque (sin GRUB), el kernel, la libc, la biblioteca
  gráfica, el escritorio y las aplicaciones del sistema están escritos en el proyecto.
- **El código de terceros es la excepción, y siempre está aislado y registrado** en
  [`docs/terceros.md`](docs/terceros.md), con su origen y su licencia: intérpretes y bibliotecas
  (MicroPython, QuickJS, SQLite, uACPI), los juegos portados (Quake, OpenTyrian, OpenJazz, Command &
  Conquer…), tablas de hardware tomadas de drivers libres (FreeBSD, OpenBSD, Mesa y alguna de Linux con
  licencia «GPL v2 o posterior»), iconos y fuentes.
- **Solo licencias compatibles con la GPL v3** en lo que se publica. Lo que no lo es (NetSurf,
  GNUBoy y el inicio moderno de la red Realtek r8169, que son «solo GPL v2») queda fuera de las
  imágenes y del código publicados; cada uno puede compilarlo para su propio uso
  ([`compilar_versiones.md`](compilar_versiones.md)).
- **Una prueba automática por funcionalidad**, que arranca el sistema en QEMU y comprueba el resultado
  ([`tests/`](tests/)).
- **Las decisiones quedan escritas** en [`docs/decisoes.md`](docs/decisoes.md) (D001, D002…) y las
  propuestas en [`mejoras/`](mejoras/README.md).
- **Probado en hardware real** antes de dar una versión por buena, no solo en máquinas virtuales.
- Los identificadores y nombres de fichero vienen del original, en portugués; los comentarios, la
  documentación y la interfaz están en español.
