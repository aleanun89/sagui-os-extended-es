# Aplicaciones portables: Mico, Pluma, Macaco, Tucán, Paint, PlanoMini, GebraMini, VisualPythonS y Arara sobre SDL 2 (D459, D465)

Mico, Pluma, Macaco, Tucán, Paint, PlanoMini, GebraMini y Arara se pueden compilar para Linux con las **mismas fuentes** que en SaguiOS. Lo único que cambia es la
capa que hace de kernel: `userland/portable/`.

## Compilar

    sudo apt install build-essential libsdl2-dev python3
    make -f userland/portable/Makefile -j4        # build/portable/{mico,pluma,macaco,tucan,paint,planomini,gebramini,arara}
    SAGUI_DATOS=rootfs ./build/portable/pluma documento.rtf

`SAGUI_DATOS` es la carpeta con `fontes/` (las siete caras DejaVu de `rootfs/fontes/`); sin ella Pluma y Macaco usan solo la
fuente 8x16. Por omisión se busca `datos/` junto al ejecutable o `../share/sagui-portable`.

## Variables

| Variable | Efecto |
| --- | --- |
| `SAGUI_DATOS` | carpeta de datos (fuentes, catálogos de idioma en `sistema/idiomas/`) |
| `SAGUI_ESCALA` | 1 a 4: ventana ampliada por números enteros |
| `SAGUI_REGISTRO` | escribe en stderr el registro del programa |
| `SAGUI_ROTEIRO` | guion de eventos para pruebas (ver abajo) |
| `LANG` | idioma, como en SaguiOS (con catálogos instalados) |

## Rutas

`/home` es `$HOME`; `/sistema`, `/fontes`, `/apps/fuentes`, `/ejemplos` e `/imagenes` son la carpeta de datos; `/home/usuario/...` y todo lo demás
son rutas reales del PC. El diálogo de abrir y guardar es el de la libsagui, dibujado igual.

## Guion de pruebas (`SAGUI_ROTEIRO`)

Una orden por línea: `esperar ms`, `texto ...` (UTF-8, `\n` y `\t` escapados), `ctrl x`, `tecla 0xNNN [mods]` (códigos de
`abi.h`: 0x116 = Esc, 0x105 = Fin; mods 1 Mayús, 2 Ctrl, 4 Alt), `clique x y [boton]`, `arrastar x1 y1 x2 y2 [boton]` (pulsa, mueve en 8 pasos y suelta), `mover x y`, `captura archivo.png`,
`sair`. Con `SDL_VIDEODRIVER=dummy` no hace falta pantalla. Las capturas salen del búfer de píxeles (con el menú abierto
encima, si lo hay).

## Qué cubre la capa y qué no

Cubre: ventanas y varias a la vez (diálogos), ratón, teclado, rueda, redimensionar, menús contextuales, portapapeles de
texto, archivos, carpetas, reloj, fuentes e iconos incrustados, conversiones por línea de órdenes. No cubre: terminal
integrado de Mico, red y TLS (el asistente de IA no conecta), audio, discos, procesos, esquema del escritorio, cuentas.
Windows/MinGW sin probar.

## Repositorios

`tools/portable_repos.py [app ...]` genera los repositorios locales (`mico`, `pluma`, `macaco`, `tucan`, `paint`, `planomini`,
`gebramini`, `visualpythons`, `arara`) con un único commit. Cada uno lleva copia de lo que enlaza. Regenerar uno borra el repositorio local y cambia su
commit: nombra solo los que quieras regenerar.

## Qué funciona fuera de SaguiOS, app por app (segunda tanda)

| App | Funciona | No funciona |
| --- | --- | --- |
| Tucán | crear y editar presentaciones (Ctrl+M, texto, formas), guardar (`.tucan` y PPTX), exportar a PDF, PPTX y PNG, modo presentación (F5), `--demo`, `--info`, `--convertir` | asistente de IA (sin red) |
| Paint | todas las herramientas con el ratón, deshacer, abrir BMP/PPM/PGM/PNG, guardar BMP de 24 bits y PPM | nada propio (no guarda PNG, tampoco en SaguiOS) |
| PlanoMini | órdenes y ratón, capas, cotas, modificar, DXF (importar y exportar), PDF y PNG, `.pmini`, modo sin ventana (`--orden`, `--abrir`, `--listar`) | nada propio; los ejemplos salen de `datos/ejemplos/planomini/` |
| Arara | figuras de las cuatro paletas, conectores rectos y en ángulo que siguen a las figuras, texto, inspector, deshacer, páginas, `.arara` y exportar a PNG, SVG y PDF, modo sin ventana (`--abrir`, `--png`, `--svg`, `--pdf`) | nada propio; los ejemplos salen de `datos/ejemplos/arara/` |
| GebraMini | puntos, rectas, circunferencias, funciones y raíces, deslizadores, `.gbm`, PNG y PDF, modo sin ventana | nada propio; los ejemplos salen de `datos/ejemplos/gebramini/` |

PlanoMini y GebraMini usan la FPU x87 con ensamblador en línea (`gbm_mat.h`); en x86-64 funciona igual y la prueba compara
con los mismos resultados que en SaguiOS. Fuera de x86 no compilarían tal cual.

Pruebas: `python3 tests/h459_apps_portables.py [--docs]` (Mico, Pluma, Macaco) y `python3 tests/h459b_apps_portables.py [--docs]`
(Tucán, Paint, PlanoMini, GebraMini); Arara la prueba `tests/h465_arara.py` (parte portable, sin QEMU, y parte de SaguiOS).

## VisualPythonS y el MicroPython de SaguiOS (`visualpythons`)

`make -f userland/portable/Makefile python` compila el **MicroPython de SaguiOS** (`userland/micropython/`: el `micropython_embed`
oficial más nuestros módulos `sagui`, `os`, `socket`, `ssl`, `sqlite3`, `hashlib`, `_thread`...) para Linux con la misma capa. Con
`SAGUI_DATOS=rootfs` (o la carpeta `datos/` del repositorio) corren `tkinter`, `vps`, `requests`, `urllib`, `threading` y `basedatos`:

    SAGUI_DATOS=rootfs ./build/portable/python /lib/python/vps/disenador.py          # el diseñador de VisualPythonS
    SAGUI_DATOS=rootfs ./build/portable/python /ejemplos/vps/calculadora/lanzador.py
    SAGUI_DATOS=rootfs ./build/portable/python                                       # el REPL

| Qué | Fuera de SaguiOS |
| --- | --- |
| Diseñador, editor de código, depurador, 24 ejemplos, autopruebas | funciona (la misma interfaz; las ventanas son de SDL 2) |
| `socket`, `requests`, `urllib` | funciona, con los sockets del PC (IPv4, TCP y UDP; `red_sdl.c`) |
| `ssl` / HTTPS | funciona (TLS 1.2 y 1.3 de la libtls propia, probado contra OpenSSL); raíces en `datos/etc/ssl/certs` (32), `verify=False` o `cafile=` |
| `sqlite3`, `basedatos`, `hashlib`, `json`, `re`, `math` | funciona |
| `_thread`, `threading` | funciona, sobre pthreads (`mpthreadport_pc.c`), con GIL |
| Terminal | UTF-8; los archivos de texto de `open()` siguen en Latin-1 por omisión, como en SaguiOS |
| Rutas | `/home` = `$HOME`; `/lib/python`, `/ejemplos`, `/imagenes`, `/etc`, `/sistema`, `/fontes` = la carpeta de datos |
| `os.system`, procesos, sonido, teclas mantenidas, `sagui.pantalla()`, menú Inicio | `OSError` limpio (o 0, 0): lo pide el kernel de SaguiOS |
| Windows | sin probar |

Guion de pruebas: además de lo anterior, `duplo x y [boton]` (doble clic); `esperar_evento(0, ...)` (cualquier ventana) lleva el guion a
la ventana de encima, la última creada. Prueba: `python3 tests/h459c_visualpythons_portable.py [--repo] [--docs]` (con `--repo` genera
el repositorio, lo clona limpio y lo prueba).
