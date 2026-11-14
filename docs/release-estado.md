# Estado de la release: dónde estamos y qué toca

Documento de traspaso preparado al retomar el proyecto y **al día a 2026-11-12 (v7.0b)**. Resume la release
publicada, lo que hay que hacer en la siguiente y las diferencias con lo que dice la
documentación.

## 0.000000000. La v7.4 (2026-11-13)

**Versión:** v7.4 (D226), etiqueta `v7.4`. Segunda tanda del 2D por la GPU: Radeon R6xx/R7xx (HD 4550) por el DMA y GeForce 6/7 y nForce por el FIFO, sin firmware (D223, D224); Sintonía, el reproductor de música propio (D225); ecualizador de Configuración > Sonido recolocado; **OpenJazz** (D228) con runtime de C++ propio con excepciones, SDL 1.2 común, partición de juegos de 800 MB rw en `/jogos` y la carpeta `input_share/` (fuera de git); `make variantes` con las imágenes uACPI. Batería completa en main: 181/187 y los 6 explicados o arreglados; tras fusionar OpenJazz, 18 pruebas de lo afectado y generales, todas OK. La meta de 38 MB pasa a medir el sistema base (arranque y /apps, `tamanho-base.txt`). Desde esta versión, en git solo `saguios.img` y `saguios-smp.img` (43 MB), sin `input_share`; el resto con `make variantes`.

## 0.00000000. La v7.3.2 (2026-11-13)

**Versión:** v7.3.2 (D222), etiqueta `v7.3.2`. Seguridad Wi-Fi para el pincho RTL8188EU (D220): WPA/TKIP y modo mixto, WEP (con aviso), WPA3-SAE con PMF y transición WPA2/WPA3. Pruebas de lo cambiado y generales: h215 (200 comprobaciones, con el pincho real), h44, h165, h39, h81, h01, h05, h16 y h216, todas OK. La conexión a routers reales la prueba el mantenedor. Entrega con `make variantes DESTINO=entrega/v7.3.2`; en git solo `saguios.img`, `saguios-smp.img` y las hojas de sumas y datos.

## 0.0000000. La v7.3.1 (2026-11-13)

**Versión:** v7.3.1 (D221), etiqueta `v7.3.1`. Servicios en segundo plano y cron (D216; sin /logs, sus registros van a `/tmp/.servicios`), bloqueo de publicidad por DNS desactivado por defecto con servidor `dnsbloqueo` (D218), la vista del Archivos se conserva al hacer scroll y los componentes de lista y texto se endurecen (D217), y Configuración ya no se queda atrapada en Capturas (h83 falla sin el arreglo y pasa con él). Pruebas de lo cambiado y generales (23), sin batería completa: todas OK; h23 y h81 tras arreglos, h92 en carpeta aislada. Entrega con `make variantes DESTINO=entrega/v7.3.1`; en git solo `saguios.img`, `saguios-smp.img` y las hojas de sumas y datos.

## 0.000000. La v7.3 (2026-11-13)

**Versión:** v7.3 (D219), etiqueta `v7.3`. Imágenes con una sola primaria y el resto en una extendida, y Discos > Particiones (ampliar, crear, redimensionar ext2/FAT32) (D213); USB 3.0 xHCI con teclado y ratón USB, también por EHCI (D214); Wi-Fi con el pincho RTL8188EU (D215); libc común también para QuickJS y GNU Prolog (D210). Batería completa en main: 180/181; h100 (MPEG-1, 29 de 30 cuadros con carga) pasa sola 3 de 3. Entrega con `make variantes DESTINO=entrega/v7.3`; en git solo `saguios.img`, `saguios-smp.img` y las hojas de sumas y datos.

## 0.00000. La v7.2 (2026-11-13)

**Versión:** v7.2 (D212), etiqueta `v7.2`. `qjs` y GNU Prolog en `/apps` (D188, D189), 2D por la GPU con autoprueba (D195), fuentes de Typodermic (D196), batería por uACPI opcional (D192), SSE/SSE3/SSSE3 por medida y sistema base sin SSE2 (D208), `malloc` que devuelve la RAM y `mmap` (D209), libc común (D210), instalar desde CD/DVD (D207), `_PRT` y autoprueba del IOAPIC (D211). Batería completa en main: 174/177; las tres fallidas: h133 pasa sola (carga), h129 y h88 eran fallos de las pruebas (formato de `[RAPIDO]` de la D208 y el prompt redibujado por el servidor de lite-monkey), arregladas y pasando. Entrega con `make variantes DESTINO=entrega/v7.2`; en git solo `saguios.img`, `saguios-smp.img` y las hojas de sumas y datos.

## 0.0000. La v7.1.1 (2026-11-13)

**Versión:** v7.1.1 (D206), etiqueta `v7.1.1`, sacada de `main`. Visor de PDF con PDF reales (D205, prueba h205), OpenTyrian a pantalla completa (D204), tiempos del arranque (D202), `discos` sin llenar el log, `--usb-zip`/`--usb-bpb` (D203, el Amilo aún no arranca por USB), y de la v7.2 ya probados: ecualizador de 5 bandas (D201) y driver experimental `alc` para Atheros AR815x (D200). Pruebas: las de lo cambiado y generales (h205, h107, h108, h04, h56, h76, h76b, h01, h05, h16, h38, h39, h153, h204, h136, h150, h109, h200, h165, h92, h75, h83), sin batería completa (regla de las X.Y.Z). Entrega con `make variantes DESTINO=entrega/v7.1.1`; en git solo `saguios.img`, `saguios-smp.img` y las hojas de sumas y datos.

## 0.000. La v7.1 (2026-11-12)

**Versión:** v7.1 (D197), etiqueta `v7.1`. Lleva todo lo de la v7.0c más **Pila de usuario que crece bajo demanda** con página de guarda (D186); **las cabeceras del anfitrión ya no se cuelan en la compilación** (`CPATH`, D191); **constructores globales y división de 64 bits para todos los programas** (mejora 094, puntos 9 y 10); **visor de actividades JClic** con descargador del repositorio del XTEC (D185); **el navegador NetSurf** portado, con el informe de carencias del sistema (D184, `docs/carencias-netsurf.md`); **reproductor MIDI** con sintetizador propio, vista de 16 canales y karaoke (D190); **partición `SAGUI-APPS`** (ext2 de lectura y escritura en `/apps`) con NetSurf y `sqlite3` fuera del ramdisk (D194); y el reparto de las carencias entre la v7.1 y la v7.2 (mejora 094). Entrega con `make variantes DESTINO=entrega/v7.1` (en git solo `saguios.img`, `saguios-smp.img` y las hojas de sumas y datos). **Para compilar con NetSurf** hay que bajar antes sus fuentes: `tools/netsurf_fuentes.sh` (no van en git).

**Añadido antes de publicar (2026-11-13):** imágenes multinúcleo sin IOAPIC (en el Amilo las IRQ PCI no llegaban por él); PAT/MTRR de write-combining en cada CPU secundaria (D198); AHCI en hardware real (D199: el disco SATA del Amilo Pi 1536 no respondía al IDENTIFY y el arranque esperaba 20–40 s); aviso cuando un dispositivo USB 1.x pasa al compañero UHCI sin driver; persistentes multinúcleo en `make variantes`. Batería en main: **164/164** (KVM, `-j 3`).

**Pendiente para la v7.1.1:**
- **Visor de PDF:** muestra basura con PDF reales (los de h107 son sintéticos). Probar con PDF reales de varios orígenes (LibreOffice, LaTeX, Word, escaneado) contra un render de referencia, arreglar y meterlos en la batería.
- **`discos`** llenaba el log del kernel con el libre del ramdisk (rama `v7.1-arreglos`, babc28f; falta pasar h92).
- Confirmar en el Amilo (AHCI, USB 1.x) y en el Toshiba L635 en modo AHCI la imagen de D199.

## 0.00. La v7.0c (2026-11-12)

**Versión:** v7.0c (D193), etiqueta `v7.0c`. Lleva todo lo de la v7.0b más **El instalador a disco duro o pendrive** y la gestión de particiones (D179): tabla MBR con primarias, extendida y lógicas, crear, borrar, redimensionar (sin encoger sistemas de ficheros), formatear FAT32 y ext2, TRIM en AHCI, GPT de solo lectura, deshacer, en la orden `particiones` y en la ventana Discos; el asistente «Instalar SaguiOS» deja el disco arrancable con `/home`, `/jogos/opentyrian` y `/apps` en particiones montadas por etiqueta, con las reglas de seguridad en el kernel (`SIS_DISCO_BLOCOS`). **OpenTyrian completo**: no se cae al empezar la partida (D183) y se ve el fondo (D187, `SDL_Swap*`). Ajustes de pruebas tras la integración (h58). Entrega con `make variantes DESTINO=entrega/v7.0c`
(en git, como siempre, solo `saguios.img`, `saguios-smp.img` y las hojas de sumas y datos).

**Antes de usar el instalador en un disco con datos:** probarlo en uno vacío (D179).

## 0.0. La v7.0b (2026-11-12)

**Versión:** v7.0b (D182), etiqueta `v7.0b` en git. La entrega completa se genera con
`make variantes DESTINO=entrega/v7.0b`: imagen normal (`saguios.img`, una CPU, con la
partición de juegos), multinúcleo (`saguios-smp.img`, `--smp --ioapic`), VDI, ISO,
persistentes ext2 y FAT32, disco con particiones y la variante sin GPL, con
`SHA256SUMS.txt` y `VERSION.txt`.

**Lleva:** **El multinúcleo completo**: SMP opcional (`--smp`, D164), cerrojo general del kernel (D165) por turnos en hardware real (D169), IOAPIC (`--ioapic`, D166) con las IRQ repartidas entre CPU (D168), cuatro planificadores seleccionables con afinidad y topología SMT (D174), P-states por núcleo (D175) y la FPU guardada al quedarse una CPU ociosa (D178). **Hilos** en C (D167), Python con GIL (D170) y Pascal (D171). **Benchmark** mono y multinúcleo en Información del sistema (D176). EIST de los Intel antiguos en solo lectura (D177). **Solitario Spider** (D173) y **OpenTyrian** en Inicio > Juegos (D180) con sus datos en la partición `SAGUI-JOGOS` (D181) y su propia pila de 1 MB para no caerse al empezar la partida (D183). **Meta de imagen de 38 MB** (D181). `run_in_qemu.sh --smp/--ioapic/--planificador`, `make variantes` y tamaños de partición por variables de entorno. Las pruebas que fallaban desde antes, arregladas (D172).

**Pendiente de probar en hardware real** (en QEMU/KVM pasa todo):

- `saguios-smp.img` en el G470, el dv7 y el Amilo: que arranquen todas las CPU
  (`[SMP]` en el log), el IOAPIC (`[IOAPIC] activo`) y que red, sonido y USB sigan
  respondiendo con las IRQ repartidas.
- El cerrojo por turnos (en QEMU va sin cola por estar bajo un hipervisor, D169).
- P-states por núcleo en el dv7 (`frecuencia`, «Por CPU»), D175.
- El aviso de RAM del arranque y la partición de juegos arrancando desde pendrive (D181).
- EIST antiguo en el Amilo: traer `acpidump` para la fase C (D177).

**Siguiente:** la v7.0c (instalador y particiones, D179; fondo de OpenTyrian, D187); después la v7.1
(JClic, NetSurf, pila dinámica, MIDI, `CPATH` y `/apps`) y la v7.2 (libc común, memoria, red sin
bloquear, tiempo, teclado y ventanas, en bloques A-E). Plan completo en
`mejoras/094-carencias-libc-sistema.md`.

## 0. Lo hecho desde que se retomó el proyecto

Todo lo de la v6.1 está **fusionado en `main`** y la v6.1 se publicó el 2026-11-08.
Lo que se hizo:

- **Mejora 042 (componentes de la libsagui), primera parte (D126):** el **lienzo**
  (líneas, rectángulos, elipses, polígulos, texto e imágenes, con desplazamiento, zoom y
  detección del elemento bajo el ratón), la **barra de progreso** (determinada e
  indeterminada, en horizontal y en vertical) y la **casilla de verificación**. Van en
  `userland/libsagui/gui_componentes.c`, declarados en `userland/inc/gui.h`.
  Prueba nueva: `tests/h98_componentes_v61.py`.
- **El `cc` compila varios ficheros de origen** (mejora 003, D133): cada fichero es una
  **unidad de traducción** con su propio ámbito (lo declarado `static` —función, variable
  o etiqueta de `struct`— sólo se ve en su fichero), y el programa que sale es uno solo.
  La biblioteca del `cc` va partida en dos: la cabecera (tipos, macros y prototipos) se
  lee antes que el programa, como los `<...>` del C, y las funciones después, para que las
  que el programa no usa se sigan borrando. La biblioteca crece con **ficheros**
  (`fopen`, `fread`, `fwrite`, `fseek`…), **`qsort`**, **`strtol`** y el **reloj**
  (`time`, `clock`, `localtime`, `strftime`, con `struct tm`). Prueba nueva:
  `tests/h102_cc_multifichero.py`.
- **SaguiGL v6** (mejora 054, D130): el motor del SaguiGL ya dibujaba triángulos con
  color, Z y cull; ahora además tiene **texturas** (RGBA de 8 bits, con repetición o
  borde), **mezcla** alfa, **niebla** por profundidad y **una luz direccional** con
  ambiente. Hay dos rutas que dan **el mismo píxel bit a bit**: la de referencia y la
  SIMD (SSE2, cuatro píxeles a la vez, por bloques de 8x8). Sin SSE2 (el `cc` de dentro
  no trae intrínsecos) va la de referencia, así que el ejemplo sigue compilándose con el
  `cc` (D127). Si no se activa ninguna de las cuatro cosas, los píxeles no cambian (las
  sumas de la h77 siguen igual). Prueba nueva: `tests/h101_sagui_gl_v6.py`.
- **Datos de la compilación en el «Acerca de» (D126):** fecha, hora, commit de git y
  variante (con GPL o sin GPL). Los genera `tools/mkcompilacao.py`; con
  `SAGUI_DATA_COMPILACAO` y `SAGUI_HORA_COMPILACAO` se puede hacer una compilación
  reproducible.
- **Créditos:** el mantenimiento de Alejandro Anuncibay se hace con Claude (Anthropic) y
  con un agente de IA (MiniMax Code), como antes solo se nombraba a Claude.
- **Entorno local sin sudo** (mejora 005): `nasm`, `gcc` de 32 bits y QEMU instalados en
  `~/.local/saguitools` con `apt-get download`. El envoltorio de `gcc` **conserva `-m32`**
  (si se pierde, compila para x86-64) y `tools/elf32.py` comprueba la clase del ELF de
  todo lo que se liga, para que un binario de 64 bits no pase inadvertido.
- **Arreglo de una prueba vacía:** la h28 pulsaba en un punto donde no había ningún botón
  (la comprobación de la página de créditos no comprobaba nada). Ahora lee la posición de
  la ventana del registro y pulsa el botón de verdad.

## 0.b. Lo hecho desde la v6.2.1 (2026-11-11)

Todo esto está **fusionado en `main`** (commit `1122942`, ya subido al remoto) y con sus
pruebas en verde:

- **El Archivos de verdad** (mejora 058, D136): barra de herramientas, barra de ruta con las
  migas de pan, panel lateral con los lugares y las acciones, vistas de lista, de detalles y
  de iconos, selección múltiple, portapapeles de archivos propio, buscar por nombre con
  comodines, papelera y «Abrir con». Rama `v6.3-arquivos`, prueba `tests/h104_arquivos_pro.py`.
- **Visor de PDF con motor propio** (mejora 059, D138): el motor va en `userland/pdf/`
  (tipografías TrueType, CFF y Type 1; filtros Flate y LZW; cifrados RC4 y AES-256) y el
  programa es `visorpdf`. Rama `v6.3-pdf`, prueba `tests/h107_visorpdf.py` (26 comprobaciones).
- **Arreglos del escritorio** (mejora 080, D139): icono para todos los programas del menú
  Inicio (los que estaban sin él y los nuevos: Visor de PDF, Reproductor de vídeo e IA), la
  rejilla de iconos del Archivos que rueda por filas enteras (ya no se desplaza de lado) y
  acciones de grupo con varios archivos marcados (copiar, cortar, borrar, propiedades y
  comprimir todo). Además, la extensión decide el programa: un `.pdf` abre el visor y un
  `.avi` o `.mpg` el reproductor. Rama `FIXES`, pruebas `tests/h108_fixes_arquivos.py` y
  `tests/h109_menu_inicio.py`.

Y además, ya fusionados en `main`:

- **El motor de la web del Cipó** (081, D141): el motor de layout CSS, el de JavaScript
  con DOM, Canvas 2D, SVG, WebGL por software, el motor de fuentes de la web y la
  ruptura de línea, en `userland/web/`, con las fuentes DejaVu en `rootfs/fontes/`. El
  Cipó ya maqueta y pinta páginas de verdad (pruebas `h112` y `h113` en verde). Viene
  del repositorio `robson-apk/sagui-os` (ramo `cipo-web`) y queda registrado en
  `docs/terceros.md`, con la procedencia en `userland/web/ORIGEM.md`.
- **La hoja de cálculo «Macaco»** (078, D142): cuadrícula, fórmulas, gráficos de barras
  y de tarta, ficheros `.mxs` y deshacer, con su entrada en el menú Inicio (prueba `h110`).
- **El panel «Asistente» de IA dentro del Mico** (043, D140): streaming, «Detener» y las
  acciones Explicar/Corregir/Generar (prueba `h111`).
- **La aplicación de chat con IA** (043, D143): `/bin/chatia` es una ventana con la
  conversación, la caja de texto y los botones «Enviar», «Detener» y «Nueva
  conversación»; el **contexto es la conversación** (el cliente manda las últimas
  doce mensagens) y **todo está en castellano** (los rótulos y la instrucción al
  modelo). Tiene su icono en el menú Inicio. Prueba `h116` en verde.
- **La configuración de la IA, en la ventana de Configuración** (043, D145): la sección
  «IA» elige el proveedor (OpenAI, OpenRouter, llama.cpp, Ollama, LM Studio, Groq,
  DeepSeek o uno propio), la dirección del servidor y la clave (que se enseña con
  asteriscos), carga la lista de modelos del servidor y asigna **un modelo a cada
  programa** (el general, el del terminal, el del chat y el del Mico): el destino se elige
  con la fila «Modelo para», la casilla de destino queda marcada con un marco azul y la
  ventana se redibuja siempre que algo cambia. Los tres leen de
  ahí. Si `/home` no es un disco persistente, se guarda en `/tmp` para la sesión. Prueba
  `h117` en verde.
- **La interfaz del chat con IA, mejorada** (043): la barra de arriba dice el modelo, el
  proveedor y el estado (con un punto de color), los textos largos se recortan, los
  botones «Enviar» y «Detener» se ven activados o apagados según toque, la vista ya no
  salta al final si el usuario está leyendo algo anterior (avisa con «Hay texto nuevo»),
  la rueda desplaza la conversación, Ctrl+N abre una conversación nueva y Shift+Intro
  escribe una línea nueva. Prueba `h116` en verde.
- **Los arreglos del volcado del reproductor** (048, D129): el pendrive no se cuelga y el
  reproductor enseña el primer cuadro (prueba `h100`).
- **El MPEG-1 descodifica los cuadros P y B** (048, D144): el decodificador de MPEG-1 en
  MPEG-PS (`userland/libsagui/mpeg1.c`) reconstruye los cuadros inter con la compensación de
  movimiento de la norma; el vídeo de prueba da **47,98 dB de media** contra el ffmpeg (el
  peor cuadro, 47,19 dB), muy por encima de los 35 dB que pedía la ficha. El `reproductor`
  abre un `.mpg` con cuadros I, P y B. Pruebas `h115` (calidad contra el ffmpeg) y `h100`
  (dentro del SaguiOS) en verde. Rama `v6.2-mpeg1`, ya fundida en `main`.
- **El cliente de correo** (082, D146): el programa `/bin/correo` es una ventana con la
  lista de los mensajes (marcados con `*` si llevan adjunto), el cuerpo del que está
  elegido y los botones «Recibir», «Redactar» y «Cuenta». **Recibe por POP3 o IMAP y
  envía por SMTP** (la biblioteca va en `userland/mail/`: la sesión de red, los tres
  protocolos, base64, quoted-printable, el analizador y el constructor MIME, y el HTML
  convertido a texto). Los mensajes recibidos se guardan en `/home/correo/` y se pueden
  volver a ver; al redactar se puede **adjuntar un fichero**. La cuenta se configura en
  su propia ventana, con la clave escondida y **dos puertos** (recepción y envío, que no
  son el mismo). El `STARTTLS` solo se pide si el servidor lo anuncia. Está en el
  menú Inicio, en el grupo «Internet», con su icono de envelope. Rama
  `v6.6-correo`, prueba `tests/h118_correo.py` en verde (20 comprobaciones, con el
  servidor falso `tests/servidor_falso_correo.py`).
- **El navegador, segunda parte** (081, D141): el render va por bandas (una franja por
  vuelta del bucle de eventos, para que la ventana no se congele con una página larga),
  los recursos se piden de uno en uno con el aviso de carga encendido, el botón derecho
  abre un menú con «Guardar imagen» y «Guardar enlace», y hay límites de nodos, comandos
  y altura para que ninguna página pueda tumbar el navegador. Además, dos arreglos que
  salieron al validar la `h114`: las URL se codifican en `%XX` (antes `url_separar()`
  rechazaba cualquier dirección con un espacio, así que `/foto uno.png` nunca se pedía;
  esto arregla también el `wget` y el `curl`) y lo que se guarda va a `/home` si existe
  (partición de datos) o a `/tmp` si no (antes la descarga fallaba siempre sin
  partición de datos).

**En marcha, cada cosa en su rama:**

| Qué | Rama | Estado |
| --- | --- | --- |
| **El navegador con motor de verdad** (081, D141) | `v6.3-web-cipo` | **terminado y en `main`**: el motor (CSS, JavaScript, Canvas 2D, SVG, WebGL, fuentes de la web), el render por trozos, el menú del botón derecho para guardar y la robustez. Pruebas `h112`, `h113` y `h114` en verde |

**La batería completa de la v6.3** (`python3 tests/rodar_paralelo.py --kvm -j 2`) dio
**114/121**. Los siete fallos son conocidos y no son del sistema:

| Prueba | Por qué falla |
| --- | --- |
| `h39_infopc` | el QEMU se presenta como `GenuineIntel` y la prueba espera otra marca |
| `h44_red` | el contador de paquetes recibidos no llega; **repetida en solitario pasa** (3002 de 3000): es saturación del QEMU en paralelo, no un fallo |
| `h65_compat_antiguos` |processors que el QEMU no emula (pentium3, athlon) |
| `h75_codecs_audio` | tiempo de espera agotado con el FLAC en la USB |
| `h80_generador_opt` | `M_PI` sin declarar y el área de coma flotante |
| `b6_micropython` | la suma de senos, que usa SSE2 |
| `b7_cc_flotante` | `float` y `double`, que usan SSE2 |

## 1. Release publicada: v6.2.1

**SaguiOS-Extended v6.2.1** es la release actual (2026-11-10). `main` = `integ2` = `integracion`.
Es una **revisión de la v6.2** (que se publica a partes): la v6.2 sigue publicada en
`entrega/v6.2/`, la v6.1 en `entrega/v6.1/` y la v6.0 en `entrega/v6.0/`.

> **El sistema en desarrollo ya es la v6.3.** La versión vive en un solo sitio
> (`kernel/inc/versao.h`, D101) y ahora dice `v6.3`: es lo que muestran el
> «Información del PC», el «Acerca de» y el arranque del kernel. La v6.3 todavía
> **no está publicada** (lo que hay en `entrega/` sigue siendo la v6.2.1); se
> publicará cuando se corte la release.

| Artefacto | SHA-256 |
| --- | --- |
| `saguios.img` | `05f1eb0e4f22e630bbb31848bd417a32bc6493c766bd4bd612e93e17fa3b6532` |
| `saguios-sin-gpl.img` | `b82dc5f23480516c1ed72a8ee0571b2182d27de504208b53b7c8ee463598bb8d` |
| `saguios.iso` | `664976dc0acc4d86dd4aa8212471371b44c53b4fb3ffc92935c60211f7b6cc53` |
| `saguios.vdi` | `a0ce9daf8773c28d4cd4bcd6eafbf0b3566ea0637289f33ab31b72d9862eb0a1` |
| `saguios-persistente-fat32.img.xz` | `5bc160ad002c9d4a6aca98c574f5edd778b18beda59ba0b3fb05a0e4d0762009` |

Contenido de la v6.2.1 (todo en `entrega/v6.2.1/`), además de lo que ya iba en la v6.2
(biblioteca de iconos y reproductor de vídeo):

- **Cliente de IA** (mejora 043, D134): el comando `ia` del terminal y la biblioteca que lo
  sostiene (`userland/ia/ia.c`). Habla con cualquier servidor que acepte la API compatible
  con la de OpenAI (un `llama-server` de llama.cpp, el Ollama, el LM Studio, el OpenRouter)
  y escribe la respuesta **a medida que llega**. La clave va en el entorno o en
  `/home/.config/ia/chave`, nunca en el repositorio. Prueba: `tests/h105_ia_v62.py`.
- **SaguiGL v6** (mejora 054, D130): el motor del SaguiGL tiene ya **texturas** (RGBA de
  8 bits, con repetición o borde), **mezcla** alfa, **niebla** por profundidad y **una luz
  direccional** con ambiente. Hay dos rutas que dan **el mismo píxel bit a bit**: la de
  referencia y la SIMD (SSE2, cuatro píxeles a la vez, por bloques de 8x8). Sin SSE2 (el
  `cc` de dentro no trae intrínsecos) va la de referencia, así que el ejemplo sigue
  compilándose con el `cc`. Si no se activa ninguna de las cuatro cosas, los píxeles no
  cambian. Prueba: `tests/h101_sagui_gl_v6.py`.
- **El `cc` compila varios ficheros de origen** (mejora 003, D133): cada fichero es una
  **unidad de traducción** con su propio ámbito (lo declarado `static` —función, variable
  o etiqueta de `struct`— sólo se ve en su fichero), y el programa que sale es uno solo.
  La biblioteca va partida en dos: la cabecera (tipos, macros y prototipos) se lee antes
  que el programa, como los `<...>` del C, y las funciones después, para que las que el
  programa no usa se sigan borrando. La biblioteca crece con **ficheros** (`fopen`,
  `fread`, `fwrite`, `fseek`…), **`qsort`**, **`strtol`** y el **reloj** (`time`, `clock`,
  `localtime`, `strftime`, con `struct tm`). Prueba: `tests/h102_cc_multifichero.py`.

**Pruebas:** las afectadas están en verde (h77, h99, h100, h101, h102, h105 y b5). La
batería completa sigue pendiente de pasar, como estaba ya en la v6.2.


Contenido de la v6.2 (todo en `entrega/v6.2/`):

- **Biblioteca de iconos** (D135): **112 iconos de 16x16** generados de los «Fluent UI System
  Icons» de Microsoft (MIT) con un **rasterizador propio** en Python (`tools/gerar-icones.py`:
  muestreo 4x4, relleno par-impar, aplanado de curvas y degradados). El SVG original no se
  distribuye: en el repositorio queda solo el pixel art. Hay un programa `icones` (en el menú
  Inicio) que los enseña, y la atribución en `docs/terceros.md` y
  `userland/terceros/fluentui/`.
- **Vídeo** (mejora 048, D129, D144): **reproductor de MJPEG dentro de un AVI** (usa el decodificador
  de JPEG de la v6.1) y **descodificador de MPEG-1 en MPEG-PS**, que ya reconstruye los
  cuadros P y B con la compensación de movimiento de la norma y bate con el ffmpeg (~48 dB
  de PSNR en cada cuadro), con `tools/convertir-video.sh` y un vídeo de ejemplo propio.

**Lo que sigue pendiente de la v6.2:** el HMKD, el SaguiGL completo
(el grupo «3D» de SaguiMark y la ruta x87), el `cc` a fondo (`stdarg`, `goto`, avisos
`-Wall`) y las variables en registros, el emulador de NES y el sonido parte 2. El cliente de IA (D134) y el SaguiGL v6 (D130) ya están hechos y
fusionados. Las ramas `v6.2-video`, `v6.2-sagui3d`, `v6.2-nes`, `v6.2-cc`, `v6.2-ia`,
`v6.2-icones`, `v6.2-hmkd` y `v6.2-sonido2` ya están creadas en sus *worktree*.

**Estado del build aquí:** la imagen ocupa **7.438.336 bytes**, el 44 % de la meta de 16 MB (D122).

### Ramas

Todas las ramas están fusionadas en `main` **excepto `licencia`**, que es lo previsto:
`AUTORES.md` y `docs/terceros.md` están preparados, pero **no se fusiona ni se publica sin
permiso de Eli Rigobeli y M4R4G0N**.

## 2. Qué toca en la v6.1

Tema de la v6.1: **interfaz, imágenes y discos** (`docs/ROADMAP.md`).

| Contenido | Mejora | Estado | Peso |
| --- | --- | --- | --- |
| Componentes de la libsagui (lienzo, barra de progreso, pestañas, árbol) y `tkinter` sobre ellos | 042 | **en curso**: lienzo, barra de progreso y casilla hechos y publicados en la v6.1 (D126); faltan los demás componentes y el `tkinter` | medio |
| JPEG y temas del escritorio | 047 | parcial: fondo y acento (D112) y **JPEG baseline hechos y publicados en la v6.1**; faltan los temas y la vista previa | medio |
| Interfaz y explorador de archivos (ocultos, vistas, rutas, papelera, «Abrir con»…) | 058 | **hecho** (D136, rama `v6.3-arquivos`, prueba h104) | medio-alto |
| Ogg Vorbis (fase 4 de los códecs) | 053 | fases 1-3 hechas (D098-D100) | medio |
| Partición `SAGUI-SIS` de solo lectura en `/sistema` (fase 0 del 035) | 035 | solo diseño | alto |
| Turbo opcional (Intel Turbo Boost / AMD Core Performance Boost) | 061 | hecha en QEMU (D106); falta el HP dv7 | alto |
| Sonido (1): salida (altavoces, auriculares, línea, HDMI), sonidos del sistema, teclas multimedia | 075 | propuesta | medio |
| AHCI (discos SATA desde ~2006) | 055 | propuesta | medio |

**Orden recomendado** (el `ROADMAP` dice que cada parte se apoya en la anterior):

1. **042 (componentes)** — desbloquea 047, 058 y el `tkinter` de la 033.
2. **047 (JPEG y temas)** — el JPEG lo necesita el vídeo de la v6.2 (048).
3. **058 (interfaz y explorador)** — es lo más visible, pero necesita los componentes de 042.
4. **053 fase 4 (Ogg Vorbis)** — independiente de lo anterior, puede ir en paralelo.
5. **061 turbo** — toca el driver de frecuencia (ya hecho), cambio acotado.
6. **035 fase 0 (`SAGUI-SIS`)** — **sola**: cada fase de la 035 va sin otros hilos de kernel
   en paralelo y con la batería completa entre fases.
7. **075 sonido (1)** y **055 AHCI** — los dos más pesados; AHCI es hardware real y pide
   Opus según la regla del proyecto.

## 3. Diferencias con la documentación

Lo que queda desfasado en `docs/` (no bloquea el trabajo):

1. **`docs/diario.md`**: la última entrada es del **2026-11-04** (fase bonus). Las releases
   v2 → v6.1 (del 2026-11-07 y 08) no están anotadas ahí.
2. **`docs/ROADMAP.md`**: al día desde el 2026-11-11 (la v6.2.1 publicada, el explorador,
   el visor de PDF y los arreglos del escritorio marcados como hechos, y el trabajo en
   curso de la v6.4 anotado en su sitio).

Nota: `status.md` (a 2026-11-04) es el marcador de fases del **proyecto original de Eli**
(fases 1-7, 100 %) y **no** describe el estado de SaguiOS-Extended. Las v2-v6.0 están en
`docs/ROADMAP.md` y `docs/CONTINUAR.md`.

## 4. Pendiente del usuario (hardware real)

Sin cambios respecto a `docs/CONTINUAR.md` §4 — nada de esto se puede cerrar desde aquí:

- **HP dv7** (con la v5.3): frecuencia y sensores, sonido por los altavoces, apagado por ACPI.
  Instrucciones en `testes_hw/`.
- **Ryzen 5 4500 / B450**: `entrega/prueba-realtek/saguios-correccion.img` y apagado ACPI.
  Sin xHCI no hay USB tras arrancar (046, v8).
- **Tablas del MP3**: cotejar `tools/dados/mp3_*.txt` con la norma y quitar cualquier
  rastro de origen LGPL (D109).

## 5. Entorno de trabajo en este equipo

El proyecto se compila aquí sin sudo, con la vía de la mejora 005:

```bash
. ~/.local/saguitools/env.sh && make
python3 tests/rodar_paralelo.py -j 2 --repetir-falhas
```

Detalles que no son evidentes y quecostaron tiempo:

- El entorno local no trae `nasm`, `gcc` de 32 bits ni QEMU: se descargan con `apt-get
  download` y se extraen en `~/.local/saguitools/root`.
- `gcc` es un envoltorio que llama al `gcc-13` del sistema y le añade `-B` con el soporte
  de 32 bits **solo si** aparece `-m32`; con `-m32` en herramientas de 64 bits el enlazado falla.
- El `libc6-dev` no está instalado en el sistema, así que el `libc.so` local (guion de
  enlazado) se corrigió para que apunte a la `libc_nonshared.a` extraída.
- QEMU necesita `QEMU_MODULE_DIR` (el acelerador TCG va como módulo en Ubuntu) y `-L` con
  la carpeta de datos (`~/.local/saguitools/datos`, con el BIOS de seabios y las ROM de iPXE).
- **Los procesos de QEMU que quedan vivos estorban**: mientras quedan, las pruebas fallan
  con «could not load PC BIOS». Matar por PID, nunca con `pkill -f`.
- KVM está disponible con `sg kvm -c "..."`, pero `sg` **pierde `LD_LIBRARY_PATH`**: hay que
  cargar el entorno dentro del `sg` (`sg kvm -c ". ~/.local/saguitools/env.sh; ..."`).

## 6. Convenciones para seguir

Sin cambios respecto a `docs/CONTINUAR.md` §6 y a los subagentes de `.claude/agents/`:

- Una rama por mejora, en su propio *worktree*; se fusiona en `integracion`, se pasa la
  batería o las pruebas afectadas y se avanza `main`.
- Comentarios en español (D096); identificadores y nombres de archivo en portugués;
  interfaz en español (ISO-8859-1).
- Sin código de terceros salvo MicroPython (D034) y drivers de Linux/Mesa/linux-firmware
  (D073), aislados en `kernel/terceros/` y registrados en `docs/terceros.md`.
- Números libres (verificados en `kernel/inc/abi.h` y `docs/decisoes.md`):
  - decisiones desde **D148** (la última es D147);
  - syscalls desde **118** (los usados llegan hasta `SIS_COLAR_IMAGEM` = 117; `SIS_MAX` = 128);
  - `INFO_` desde **61** (los usados llegan hasta `INFO_BANDEJA` = 60);
  - pruebas desde **h120** (las más altas son h118 y h119);
  - mejoras desde **083** (las más altas son 077, 078, 080 y 082; la **079** queda para el
    Bluetooth, que estaba apuntando a la 059 por error).
- Los cuatro subagentes de `.claude/agents/` (Haiku/Sonnet/Opus) se reparten igual: **ante
  la duda, `trabajador-opus`**.
