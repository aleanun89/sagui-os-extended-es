#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""portable_repos.py - genera los repositorios git locales de Mico, Pluma, Macaco, Tucán, Paint, PlanoMini, GebraMini y VisualPythonS portables (D459).

Cada repositorio lleva una COPIA de lo que esa aplicacion compila (calculado con `gcc -MM`): las fuentes de la
aplicacion, la capa userland/portable/, los ficheros de la libsagui que enlaza, las cabeceras, las fuentes DejaVu (Pluma y
Macaco), README, licencias y un unico commit con el autor publico. No crea nada en GitHub ni sube nada.

VisualPythonS (`visualpythons`) es distinto: lleva el MicroPython de SaguiOS (embed + nuestro port), la capa, los modulos de Python
(tkinter propio, vps, requests...), los ejemplos y el manual, y se genera con generar_vps().

Uso: portable_repos.py [DESTINO] [app ...]      (por omision /home/aanun/SO/repos-publicos y las ocho aplicaciones)
Regenerar BORRA el repositorio local de esa aplicacion y lo crea de nuevo con otro commit: si ya se subio a GitHub, el
historial local deja de coincidir con el remoto. Por eso se pueden nombrar solo las aplicaciones que se quieren regenerar.
"""
import os
import re
import shutil
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_ARGS = sys.argv[1:]
DESTINO = _ARGS.pop(0) if _ARGS and "/" in _ARGS[0] else "/home/aanun/SO/repos-publicos"
# Autor de los commits de los repositorios públicos: el correo no va en el código (publicar.sh no deja correos);
# se lee de PORTABLE_CORREO o de ~/.config/saguios/portable.env (línea PORTABLE_CORREO=...).
def _correo():
    c = os.environ.get("PORTABLE_CORREO", "")
    try:
        for l in open(os.path.expanduser("~/.config/saguios/portable.env")):
            if not c and l.startswith("PORTABLE_CORREO="):
                c = l.split("=", 1)[1].strip()
    except OSError:
        pass
    return c


AUTOR = ("aleanun89", _correo())

LIB = ("compressao deflado_continuo difer edicao_rica edicao_rica_dialogos edicao_rica_obj formatacao formatar gui "
       "gui_arquivos gui_arvore gui_barras gui_cinta gui_componentes gui_explorador gui_moderno gui_pestanas gui_revisar "
       "hora icones iconos98 idioma presentacion presentacion_io presentacion_pptx imagem imagem_gif imagem_idct imagem_jpeg imagem_png_escribir inflado_continuo "
       "pdf_escribir regex rtf texto texto_rico utilitarios wordart entrada zonas sincronia http sonidos conf_maquina "
       "contas empacotar dialogos_cf dialogos_objeto").split()
PORT = "janelas_sdl sistema_sdl sistema_nulo rapido_c".split()
FLAGS = ["-DSAGUI_HOST", "-DSAGUI_PORTABLE", "-w", "-I" + RAIZ + "/userland/inc", "-I" + RAIZ + "/userland/portable",
         "-I" + RAIZ + "/userland/libsagui", "-I" + RAIZ + "/userland/libfontes", "-I" + RAIZ + "/userland/ia", "-I" + RAIZ + "/userland/planomini", "-I" + RAIZ + "/userland/gebramini",
         "-idirafter", RAIZ + "/kernel/inc", "-I/usr/include/SDL2"]

def _c(d):
    return lambda: sorted(ls(d, ".c"))


APPS = {
    "mico": dict(desc_es="editor de código", desc_en="code editor", desc_pt="editor de código",
                 fuentes=lambda: sorted(ls("userland/programas/mico", ".c")), fontes=False,
                 extra=["userland/programas/mico/mico.h"], captura="H459-mico.png",
                 funciona_es="editar y guardar, menú contextual, portapapeles, modo `-t` de texto",
                 falta_es="el terminal integrado y el asistente de IA (sin red)"),
    "pluma": dict(desc_es="procesador de textos", desc_en="word processor", desc_pt="processador de textos",
                  fuentes=lambda: ["userland/programas/pluma.c"], fontes=True, extra=["userland/programas/pluma_cinta.h"],
                  captura="H459-pluma.png",
                  funciona_es="RTF, TXT, exportar a PDF, conversiones por línea de órdenes (`--convertir`)",
                  falta_es="el asistente de IA (sin red)"),
    "macaco": dict(desc_es="hoja de cálculo", desc_en="spreadsheet", desc_pt="planilha",
                   fuentes=lambda: ["userland/programas/macaco.c"], fontes=True,
                   extra=["userland/programas/macaco_cinta.h", "userland/programas/macaco_formatos.h", "userland/programas/macaco_funciones.h"],
                   captura="H459-macaco.png",
                   funciona_es="fórmulas, CSV, XLSX, formato propio, conversiones por línea de órdenes (`--convertir`)",
                   falta_es="el asistente de IA (sin red)"),
    "tucan": dict(desc_es="presentaciones", desc_en="presentations", desc_pt="apresentações",
                  fuentes=lambda: ["userland/programas/tucan.c"], fontes=True,
                  extra=["userland/programas/tucan_cinta.h", "userland/programas/tucan_modos.h", "userland/programas/tucan_pres.h",
                         "userland/programas/tucan_agente.h"],
                  captura="H459b-tucan.png",
                  funciona_es="crear y editar presentaciones, guardar (formato propio y PPTX), exportar a PDF, PPTX y PNG, "
                              "modo presentación, `--demo`, `--info` y `--convertir` por línea de órdenes",
                  falta_es="el asistente de IA (sin red)"),
    "paint": dict(desc_es="editor de dibujo", desc_en="drawing editor", desc_pt="editor de desenho",
                  fuentes=lambda: ["userland/programas/paint.c"], fontes=True, extra=[], captura="H459b-paint.png",
                  funciona_es="lápiz, pincel, goma, líneas, rectángulos, elipses, bote de pintura, texto, deshacer; abre BMP, PPM, PGM y PNG; "
                              "guarda BMP de 24 bits y PPM",
                  falta_es="nada relevante (no guarda PNG: es lo mismo que en SaguiOS)"),
    "planomini": dict(desc_es="CAD 2D", desc_en="2D CAD", desc_pt="CAD 2D",
                      fuentes=lambda: ["userland/programas/planomini.c"] + sorted(ls("userland/planomini", ".c")) + sorted(ls("userland/gebramini", ".c")),
                      fontes=False, extra=[], captura="H459b-planomini.png", ejemplos="planomini",
                      funciona_es="dibujo por órdenes y con el ratón, capas, cotas, modificar, DXF (importar y exportar), PDF y PNG, "
                                  "guardado `.pmini`, modo sin ventana (`--orden`, `--abrir`, `--pdf`)",
                      falta_es="nada relevante"),
    "gebramini": dict(desc_es="geometría dinámica", desc_en="dynamic geometry", desc_pt="geometria dinâmica",
                      fuentes=lambda: ["userland/programas/gebramini.c"] + sorted(ls("userland/gebramini", ".c")),
                      fontes=False, extra=[], captura="H459b-gebramini.png", ejemplos="gebramini",
                      funciona_es="puntos, rectas, circunferencias, funciones y raíces, deslizadores, `.gbm`, PNG y PDF, "
                                  "modo sin ventana (`--orden`, `--abrir`, `--pdf`)",
                      falta_es="nada relevante"),
}
HERMANOS = ["mico", "pluma", "macaco", "tucan", "paint", "planomini", "gebramini", "visualpythons"]
NOMBRES = {"mico": "Mico", "pluma": "Pluma", "macaco": "Macaco", "tucan": "Tucán", "paint": "Paint",
           "planomini": "PlanoMini", "gebramini": "GebraMini", "visualpythons": "VisualPythonS"}


def ls(d, ext):
    return [d + "/" + f for f in os.listdir(os.path.join(RAIZ, d)) if f.endswith(ext)]


def dependencias(fuentes):
    """Todos los ficheros (fuentes y cabeceras) de RAIZ que entran al compilar esas fuentes."""
    out = set(fuentes)
    for f in fuentes:
        r = subprocess.run(["gcc", "-M", *FLAGS, os.path.join(RAIZ, f)], capture_output=True, text=True)
        if r.returncode:
            sys.exit("gcc -MM %s: %s" % (f, r.stderr))
        for t in r.stdout.replace("\\\n", " ").split()[1:]:
            t = os.path.realpath(t)
            if t.startswith(RAIZ + "/"):
                out.add(t[len(RAIZ) + 1:])
    return out


def leer(p):
    return open(os.path.join(RAIZ, p), encoding="utf-8").read()


def readme(app, a):
    nombre = NOMBRES[app]
    con_ia = app in ("mico", "pluma", "macaco", "tucan")
    ia_en, ia_pt = (" (no AI assistant)", " (sem assistente de IA)") if con_ia else ("", "")
    hermanos = ", ".join("[%s](https://github.com/aleanun89/%s)" % (NOMBRES[h], h) for h in HERMANOS if h != app)
    ejemplos = ("\nLos ejemplos de SaguiOS (menú Ejemplos) van en `datos/ejemplos/%s/` y se encuentran solos desde el directorio del repositorio.\n" % a["ejemplos"]) if a.get("ejemplos") else ""
    url = "https://github.com/aleanun89/sagui-os-extended-es"
    return f"""# {nombre} portable

**{nombre}** ({a['desc_es']}) de [SaguiOS-Extended]({url}), compilado para Linux con SDL 2.

Las fuentes de la aplicación son **las mismas que en SaguiOS**: no hay una versión aparte. Lo que cambia es una capa
pequeña, `userland/portable/`, que hace en Linux lo que en SaguiOS hace el kernel (ventanas, eventos, archivos, reloj,
portapapeles) con la SDL 2. El resto, incluida la biblioteca gráfica de SaguiOS (la libsagui), se compila sin tocar.

![{nombre}](docs/{app}.png)

## Compilar

    sudo apt install build-essential libsdl2-dev python3
    make -j4
    ./build/portable/{app} [archivo]

Hace falta un compilador de C (gcc o clang), `make`, Python 3 (genera la fuente de 8x16 de la interfaz) y los archivos
de desarrollo de SDL 2 (`sdl2-config`). También hay un `CMakeLists.txt` que llama al mismo Makefile.
Sin ventana (pruebas): `SDL_VIDEODRIVER=dummy`; con un guion de eventos, `SAGUI_ROTEIRO=guion.txt`
(ver `docs/portable.md`).

## Qué funciona y qué no

Funciona: la interfaz tal como en SaguiOS, ratón, teclado, menús, diálogos de abrir y guardar, portapapeles del sistema,
archivos del usuario (`/home` es tu carpeta personal): {a['funciona_es']}.
No funciona fuera de SaguiOS: {a['falta_es']}; tampoco red, audio, información del sistema ni cuentas de usuario.
Solo Linux; Windows sin probar. Interfaz en español.{ejemplos}

## Variables de entorno

`SAGUI_DATOS` (carpeta de datos con `fontes/`), `SAGUI_ESCALA` (1 a 4, amplía la ventana), `SAGUI_REGISTRO`, `SAGUI_ROTEIRO`.

## Licencia

GPL: las fuentes de la aplicación son GPL-2.0-or-later y la libsagui mezcla GPL-2.0-or-later y GPL-3.0-only, así que el
programa enlazado se distribuye bajo la **GPL v3** (`LICENSE`); el texto de la GPL v2 está en `LICENSES/GPL-2.0.txt` y cada
fichero declara la suya en `SPDX-License-Identifier`. Avisos de terceros en `AVISOS-TERCEROS.md`.

## Origen

Parte de [SaguiOS-Extended]({url}); la fuente de verdad es ese repositorio. Este se regenera con `tools/portable_repos.py`.

Repositorios hermanos (mismas capa y libsagui): {hermanos}.

---

## English

**{nombre}** ({a['desc_en']}) from [SaguiOS-Extended]({url}), built for Linux on SDL 2. The application sources are the
very same ones used in SaguiOS; a small layer (`userland/portable/`) replaces the kernel calls (windows, events, files,
clock, clipboard) with SDL 2, and the rest of SaguiOS' GUI library is compiled untouched.

    sudo apt install build-essential libsdl2-dev python3
    make -j4 && ./build/portable/{app} [file]

Not available outside SaguiOS: networking{ia_en}, audio. Linux only (Windows untested).
Licence: GPL v3 as a combined work (see `LICENSE`); individual files are GPL-2.0-or-later or GPL-3.0-only (SPDX tags).
Third-party notices: `AVISOS-TERCEROS.md`. The interface is in Spanish. Sibling repositories: see the Spanish section above.

## Português (Brasil)

**{nome_pt(nombre)}** ({a['desc_pt']}) do [SaguiOS-Extended]({url}), compilado para Linux com SDL 2. Os fontes do aplicativo
são os mesmos do SaguiOS; uma camada pequena (`userland/portable/`) substitui as chamadas ao kernel (janelas, eventos,
arquivos, relógio, área de transferência) pela SDL 2, e o resto da biblioteca gráfica do SaguiOS é compilado sem alterações.

    sudo apt install build-essential libsdl2-dev python3
    make -j4 && ./build/portable/{app} [arquivo]

Não funciona fora do SaguiOS: rede{ia_pt}, áudio. Só Linux (Windows não testado).
Licença: GPL v3 como obra combinada (veja `LICENSE`); cada arquivo é GPL-2.0-or-later ou GPL-3.0-only (tags SPDX).
Avisos de terceiros: `AVISOS-TERCEROS.md`. A interface está em espanhol.
"""


def nome_pt(n):
    return n


TERCEROS = """# Avisos de terceros

Este repositorio es código propio de SaguiOS-Extended (GPL) más lo siguiente:

- **Fluent UI System Icons** (Microsoft Corporation, MIT): los iconos de 16x16 de `userland/libsagui/icones.c` (ya
  convertidos a píxeles; el SVG original no se distribuye). Licencia en `userland/terceros/fluentui/LICENSE`.
- **Métricas AFM de las fuentes base de PDF** (Adobe, Core 14): `userland/libsagui/pdf_metricas.h`, solo números, de uso y
  redistribución libres con su aviso de copyright.
{dejavu}"""

DEJAVU = """- **DejaVu Fonts** (Bitstream Vera y Arev, licencia permisiva): las siete caras recortadas de `datos/fontes/`; el texto
  íntegro está en `datos/fontes/LICENCA-DejaVu.txt`. Son datos, no código; Pluma, Macaco y Tucán las incrustan (subconjunto) en los PDF; Paint las usa para su herramienta de texto.
"""

CMAKE = """cmake_minimum_required(VERSION 3.10)
project({app} C)
# Envoltorio: compila con el Makefile (la fuente de la verdad) y deja el binario en build/portable/.
add_custom_target({app} ALL COMMAND ${{CMAKE_COMMAND}} -E env make -C ${{CMAKE_SOURCE_DIR}} -j4
                  WORKING_DIRECTORY ${{CMAKE_SOURCE_DIR}} VERBATIM)
"""


def generar(app, a):
    dest = os.path.join(DESTINO, app)
    shutil.rmtree(dest, ignore_errors=True)
    os.makedirs(dest)
    fuentes = a["fuentes"]()
    todas = list(fuentes) + ["userland/libsagui/%s.c" % n for n in LIB] + ["userland/portable/%s.c" % n for n in PORT] \
        + ["kernel/lib/fuso.c", "kernel/lib/catalogo.c", "kernel/lib/sha256.c", "userland/libtls/hash.c"] \
        + ["userland/ia/" + f for f in os.listdir(os.path.join(RAIZ, "userland/ia")) if f.endswith(".c")]
    if a["fontes"]:
        todas += ["userland/libfontes/" + f for f in os.listdir(os.path.join(RAIZ, "userland/libfontes")) if f.endswith(".c")]
    ficheros = dependencias(todas) | set(a["extra"]) | {
        "userland/portable/Makefile", "userland/portable/portavel.h", "tools/mkfonte.py", "tools/fonte/sagui8x16.txt",
        "userland/terceros/fluentui/LICENSE"}
    for f in sorted(ficheros):
        d = os.path.join(dest, f)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copy(os.path.join(RAIZ, f), d)
    if a["fontes"]:
        os.makedirs(os.path.join(dest, "datos"))
        shutil.copytree(os.path.join(RAIZ, "rootfs/fontes"), os.path.join(dest, "datos/fontes"))
    if a.get("ejemplos"):
        shutil.copytree(os.path.join(RAIZ, "rootfs/ejemplos", a["ejemplos"]), os.path.join(dest, "datos/ejemplos", a["ejemplos"]))
    open(os.path.join(dest, "Makefile"), "w", encoding="utf-8").write(
        "# %s portable (D459): compila con la capa userland/portable/ y la SDL 2.\nAPPS := %s\ninclude userland/portable/Makefile\n" % (app, app))
    open(os.path.join(dest, "CMakeLists.txt"), "w", encoding="utf-8").write(CMAKE.format(app=app))
    open(os.path.join(dest, ".gitignore"), "w", encoding="utf-8").write("/build/\n*.o\n*.a\n__pycache__/\n")
    open(os.path.join(dest, "README.md"), "w", encoding="utf-8").write(readme(app, a))
    shutil.copy("/usr/share/common-licenses/GPL-3", os.path.join(dest, "LICENSE"))
    os.makedirs(os.path.join(dest, "LICENSES"))
    shutil.copy("/usr/share/common-licenses/GPL-2", os.path.join(dest, "LICENSES/GPL-2.0.txt"))
    open(os.path.join(dest, "AVISOS-TERCEROS.md"), "w", encoding="utf-8").write(TERCEROS.format(dejavu=DEJAVU if a["fontes"] else ""))
    os.makedirs(os.path.join(dest, "docs"))
    shutil.copy(os.path.join(RAIZ, "docs/apps-portables.md"), os.path.join(dest, "docs/portable.md"))
    cap = os.path.join(RAIZ, "docs/capturas/" + a["captura"])
    if os.path.exists(cap):
        shutil.copy(cap, os.path.join(dest, "docs/%s.png" % app))
    if not AUTOR[1]:
        sys.exit("portable_repos: falta PORTABLE_CORREO (o ~/.config/saguios/portable.env)")
    env = dict(os.environ, GIT_AUTHOR_NAME=AUTOR[0], GIT_AUTHOR_EMAIL=AUTOR[1], GIT_COMMITTER_NAME=AUTOR[0], GIT_COMMITTER_EMAIL=AUTOR[1])
    run = lambda *c: subprocess.run(["git", "-c", "user.name=" + AUTOR[0], "-c", "user.email=" + AUTOR[1], *c], cwd=dest, env=env, check=True, capture_output=True)
    run("init", "-q", "-b", "main")
    run("add", "-A")
    run("commit", "-q", "-m", "%s portable: capa SDL 2 y fuentes de SaguiOS-Extended" % NOMBRES[app])
    print(app, len(ficheros), "ficheros ->", dest)


# ---------------------------------------------------------------------------------------------------------------------
# VisualPythonS: el MicroPython de SaguiOS en Linux (D459, apartado «VisualPythonS»)

VPS_LIB = ("compressao contas dialogos_cf edicao_rica edicao_rica_dialogos edicao_rica_obj entrada formatar gui "
           "gui_arquivos gui_barras gui_moderno gui_revisar hora icones idioma imagem imagem_gif imagem_idct imagem_jpeg "
           "pdf_escribir sonidos texto utilitarios wordart zonas gui_grafico grafico_calc").split()
# lo de userland/micropython/ que no se usa fuera de SaguiOS (libc y libm propias de 32 bits, el arranque con CPUID, la receta del embed)
VPS_MP_FUERA = {"libc", "libm_dbl", "libm_sagui.c", "LICENCA-LIBM.txt", "micropython_embed.mk", "inicio_python.c"}
VPS_FLAGS = FLAGS + ["-I" + RAIZ + "/userland/micropython", "-I" + RAIZ + "/userland/micropython/micropython_embed",
                     "-I" + RAIZ + "/userland/micropython/micropython_embed/port", "-I" + RAIZ + "/userland/terceros/sqlite",
                     "-D_GNU_SOURCE"]

VPS_README = """# VisualPythonS portable

**VisualPythonS** (un entorno al estilo del Visual Basic clásico para hacer aplicaciones de ventanas en Python, con su
diseñador, su editor de código con depurador y 26 proyectos de ejemplo) junto con la versión **modificada de MicroPython
de [SaguiOS-Extended](https://github.com/aleanun89/sagui-os-extended-es)** sobre la que corre, compilados para Linux con SDL 2.

Es el mismo MicroPython 1.24.1 de SaguiOS (el `micropython_embed` oficial, MIT) con nuestro port: los módulos `sagui`
(ventanas, dibujo, eventos, imágenes y diálogos de archivos), `os`, `socket`, `ssl` (TLS 1.2 y 1.3), `sqlite3`, `hashlib`,
`_thread`, `json`, `re`... y, en Python, el `tkinter` propio, `vps` (VisualPythonS), `requests`, `urllib`, `threading`,
`collections` y `basedatos`. Una capa pequeña (`userland/portable/`) hace en Linux, con la SDL 2, lo que en SaguiOS hace el
kernel: ventanas, eventos, archivos, reloj y portapapeles; la biblioteca gráfica de SaguiOS (la libsagui) se compila sin tocar.

Entre los controles del diseñador están, además de los clásicos, las notificaciones (`vps.notificar`, de cuatro tipos y apiladas),
el árbol (`Arbol`), el calendario y los gráficos (`Grafico`: barras, líneas, áreas, tarta, dispersión, histograma, velas...,
animados; los dibuja la biblioteca de gráficos de la libsagui, la misma que usan los programas de SaguiOS).

![El diseñador de VisualPythonS](docs/visualpythons.png)

## Compilar y ejecutar

    sudo apt install build-essential libsdl2-dev python3
    make -j4
    ./visualpythons                       # el diseñador
    ./visualpythons datos/ejemplos/vps/calculadora     # abre un proyecto de ejemplo
    ./python-sagui datos/ejemplos/vps/calculadora/lanzador.py    # ejecuta un ejemplo sin el diseñador
    ./python-sagui                        # el MicroPython de SaguiOS (REPL), con sus módulos
    make probar                           # autopruebas sin ventana (SDL_VIDEODRIVER=dummy)

Hace falta un compilador de C (gcc o clang), `make`, Python 3 (genera la fuente de 8x16 de la interfaz) y los archivos de
desarrollo de SDL 2 (`sdl2-config`). También hay un `CMakeLists.txt` que llama al mismo Makefile. Si el entorno exporta
`CPATH` con cabeceras de otra libc, compila con `env -u CPATH make`.

En el diseñador: caja de herramientas a la izquierda, formulario con rejilla en el centro, propiedades a la derecha; doble clic
en un control (o F7) abre el editor de código con su evento; F5 ejecuta la aplicación; el manual está en
`docs/visualpythons-manual.md` y el resumen en `docs/visualpythons.md`. Un proyecto es una carpeta con `formulario.vpf` y `codigo.py`.

## Qué funciona fuera de SaguiOS y qué no

Funciona: el diseñador completo, el editor de código y el depurador, los 26 ejemplos, `tkinter`/`ttk` propios, red con los
sockets del PC (`socket`, `requests`, `urllib`), TLS con las 32 raíces de `datos/etc/ssl/certs` (o `verify=False`, o `cafile=`),
`sqlite3` y `basedatos`, `hashlib`, hilos (`_thread`, `threading`, sobre pthreads), `json`, `re`, `math`, portapapeles del
sistema y la salida por el terminal en UTF-8. `/home` es tu carpeta personal (`$HOME`); `/lib/python`, `/ejemplos`, `/sistema`
y `/etc` son la carpeta `datos/` (o `SAGUI_DATOS`); el resto de rutas son las reales.

No funciona (da un error limpio, como en la capa): `os.system` y la ejecución de otros programas, el sonido (`sagui.Sonido`,
`sagui.bip`), la lectura de teclas mantenidas, el tamaño de pantalla (`sagui.pantalla()` da 0, 0), la integración con el menú
Inicio de SaguiOS (generar aplicación escribe en `~/vps`) y todo lo que pide el kernel (discos, cuentas). Solo Linux (x86-64);
Windows sin probar. Los archivos de texto que abre `open()` siguen siendo Latin-1 por omisión, como en SaguiOS
(`open(f, encoding="utf-8")` para UTF-8): así los proyectos se llevan de un sistema a otro sin cambios.

## Variables de entorno

`SAGUI_DATOS` (carpeta de datos; por omisión `datos/` junto al ejecutable), `SAGUI_ESCALA` (1 a 4, amplía las ventanas),
`SAGUI_ROTEIRO` (guion de eventos para pruebas, ver `docs/portable.md`), `PYTHONHEAP` (tamaño del heap, p. ej. `64M`).

## Licencia

GPL: las fuentes propias son GPL-2.0-or-later y la libsagui mezcla GPL-2.0-or-later y GPL-3.0-only, así que el programa
enlazado se distribuye bajo la **GPL v3** (`LICENSE`); el texto de la GPL v2 está en `LICENSES/GPL-2.0.txt` y cada fichero
declara la suya en `SPDX-License-Identifier`. MicroPython es MIT (`userland/micropython/LICENCA-MICROPYTHON.txt`), SQLite es de
dominio público y hay otros avisos en `AVISOS-TERCEROS.md`.

## Origen

Parte de [SaguiOS-Extended](https://github.com/aleanun89/sagui-os-extended-es); la fuente de verdad es ese repositorio y este se
regenera con `tools/portable_repos.py`. Repositorios hermanos (misma capa y libsagui): {hermanos}.

---

## English

**VisualPythonS** (a classic-Visual-Basic-style environment for building windowed apps in Python: form designer, code editor
with debugger, 26 sample projects, toast notifications, tree, calendar and animated charts) together with the **modified MicroPython of
[SaguiOS-Extended](https://github.com/aleanun89/sagui-os-extended-es)** it runs on, built for Linux with SDL 2. Same MicroPython
1.24.1 (official embed port, MIT) plus our port (`sagui` windowing module, `os`, `socket`, `ssl`, `sqlite3`, `hashlib`,
`_thread`...) and the pure-Python modules (own `tkinter`, `vps`, `requests`, `urllib`, `threading`, `basedatos`). A small layer
(`userland/portable/`) replaces the kernel calls with SDL 2 and the host libc; SaguiOS' GUI library is compiled untouched.

    sudo apt install build-essential libsdl2-dev python3
    make -j4 && ./visualpythons

Works: designer, editor, debugger, examples, networking and TLS, sqlite, threads. Not available (clean errors): `os.system`,
sound, held-key polling, SaguiOS' Start menu. Linux only. Text files opened with `open()` are Latin-1 by default, as in
SaguiOS. Licence: GPL v3 as a combined work (see `LICENSE`); MicroPython is MIT; third-party notices in `AVISOS-TERCEROS.md`.
The interface is in Spanish. Sibling repositories: see the Spanish section.

## Português (Brasil)

**VisualPythonS** (um ambiente no estilo do Visual Basic clássico para criar aplicativos de janelas em Python: designer de
formulários, editor de código com depurador, 26 projetos de exemplo, notificações, árvore, calendário e gráficos animados) junto com o **MicroPython modificado do
[SaguiOS-Extended](https://github.com/aleanun89/sagui-os-extended-es)** em que ele roda, compilados para Linux com SDL 2. É o
mesmo MicroPython 1.24.1 (port embed oficial, MIT) com o nosso port (módulo `sagui` de janelas, `os`, `socket`, `ssl`, `sqlite3`,
`hashlib`, `_thread`...) e os módulos em Python (`tkinter` próprio, `vps`, `requests`, `urllib`, `threading`, `basedatos`). Uma
camada pequena (`userland/portable/`) troca as chamadas ao kernel pela SDL 2 e pela libc do PC; a biblioteca gráfica do SaguiOS
é compilada sem alterações.

    sudo apt install build-essential libsdl2-dev python3
    make -j4 && ./visualpythons

Funciona: designer, editor, depurador, exemplos, rede e TLS, sqlite, threads. Não funciona (erro limpo): `os.system`, som,
leitura de teclas seguradas, menu Iniciar do SaguiOS. Só Linux. Os arquivos de texto abertos com `open()` são Latin-1 por
padrão, como no SaguiOS. Licença: GPL v3 como obra combinada (veja `LICENSE`); o MicroPython é MIT; avisos de terceiros em
`AVISOS-TERCEROS.md`. A interface está em espanhol. Repositórios irmãos: veja a seção em espanhol.
"""

VPS_TERCEROS = """# Avisos de terceros

Este repositorio es código propio de SaguiOS-Extended (GPL) más lo siguiente:

- **MicroPython 1.24.1** (Damien P. George y colaboradores, MIT): `userland/micropython/micropython_embed/` es el paquete que genera
  el «embed port» oficial, sin cambios en los fuentes del núcleo; los `*_sagui.c`, `mpconfigport.h` y compañía son el port propio
  de SaguiOS. Licencia en `userland/micropython/LICENCA-MICROPYTHON.txt`. Incluye `lib/re1.5` (Russ Cox/Paul Sokolovsky, BSD-3) y
  `shared/` del mismo proyecto, cada uno con su aviso de copyright.
- **SQLite** (dominio público): `userland/terceros/sqlite/sqlite3.c` y `sqlite3.h`, la amalgama oficial; aviso en
  `userland/terceros/sqlite/LICENCA-SQLITE.txt`.
- **Fluent UI System Icons** (Microsoft Corporation, MIT): los iconos de 16x16 de `userland/libsagui/icones.c` (ya convertidos a
  píxeles; el SVG original no se distribuye). Licencia en `userland/terceros/fluentui/LICENSE`.
- **Métricas AFM de las fuentes base de PDF** (Adobe, Core 14): `userland/libsagui/pdf_metricas.h`, solo números, de uso y
  redistribución libres con su aviso de copyright.
- **DejaVu Fonts** (Bitstream Vera y Arev, licencia permisiva): las siete caras recortadas de `datos/fontes/`; el texto íntegro está
  en `datos/fontes/LICENCA-DejaVu.txt`. Son datos, no código.
- **Raíces de certificados públicas** (32, de la lista de Mozilla vía el paquete `ca-certificates` de Ubuntu; MPL 2.0): `datos/etc/ssl/certs/`
  (`LEEME.txt` detalla cada una). Son datos públicos, no código.
- **SDL 2** (zlib): se enlaza dinámicamente desde el sistema; no se distribuye.
"""


def vps_dependencias(fuentes, flags):
    out = set(fuentes)
    for f in fuentes:
        r = subprocess.run(["gcc", "-M", *flags, os.path.join(RAIZ, f)], capture_output=True, text=True)
        if r.returncode:
            sys.exit("gcc -M %s: %s" % (f, r.stderr))
        for t in r.stdout.replace("\\\n", " ").split()[1:]:
            t = os.path.realpath(t)
            if t.startswith(RAIZ + "/"):
                out.add(t[len(RAIZ) + 1:])
    return out


def generar_vps():
    app = "visualpythons"
    dest = os.path.join(DESTINO, app)
    shutil.rmtree(dest, ignore_errors=True)
    os.makedirs(dest)
    mp = [f for f in sorted(os.listdir(os.path.join(RAIZ, "userland/micropython"))) if f not in VPS_MP_FUERA]
    mp_c = ["userland/micropython/" + f for f in mp if f.endswith(".c")]
    lib = ["userland/libsagui/%s.c" % n for n in VPS_LIB + ["sqlite_vfs_sagui"]]
    port = ["userland/portable/%s.c" % n for n in PORT + ["inicio_python", "red_sdl"]]
    kern = ["kernel/lib/fuso.c", "kernel/lib/catalogo.c", "kernel/lib/sha256.c", "userland/libtls/hash.c"]
    tls = ["userland/libtls/" + f for f in sorted(os.listdir(os.path.join(RAIZ, "userland/libtls"))) if f.endswith(".c")
           and f not in ("conexao.c", "sshcifra.c", "wireguard.c")]
    fontes = ["userland/libfontes/" + f for f in sorted(os.listdir(os.path.join(RAIZ, "userland/libfontes"))) if f.endswith(".c")]
    ficheros = vps_dependencias(mp_c + lib + port + kern + tls + fontes, VPS_FLAGS)
    ficheros |= {"userland/portable/Makefile", "userland/portable/portavel.h", "tools/mkfonte.py", "tools/fonte/sagui8x16.txt",
                 "userland/terceros/fluentui/LICENSE", "userland/terceros/sqlite/sqlite3.c", "userland/terceros/sqlite/sqlite3.h",
                 "userland/terceros/sqlite/LICENCA-SQLITE.txt"}
    for f in mp:   # todo el port, sin lo de 32 bits de SaguiOS
        p = os.path.join(RAIZ, "userland/micropython", f)
        if os.path.isfile(p):
            ficheros.add("userland/micropython/" + f)
    for d, _, fs in os.walk(os.path.join(RAIZ, "userland/micropython/micropython_embed")):
        for f in fs:
            ficheros.add(os.path.relpath(os.path.join(d, f), RAIZ))
    for f in sorted(ficheros):
        d = os.path.join(dest, f)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copy(os.path.join(RAIZ, f), d)
    # datos: modulos de Python, ejemplos, fuentes, raices de certificados e imagenes de muestra
    datos = os.path.join(dest, "datos")
    ign = shutil.ignore_patterns("__pycache__", "*.pyc", "chip8roms.py")
    shutil.copytree(os.path.join(RAIZ, "rootfs/lib/python"), os.path.join(datos, "lib/python"), ignore=ign)
    shutil.copytree(os.path.join(RAIZ, "rootfs/ejemplos/vps"), os.path.join(datos, "ejemplos/vps"), ignore=ign)
    shutil.copytree(os.path.join(RAIZ, "rootfs/fontes"), os.path.join(datos, "fontes"))
    shutil.copytree(os.path.join(RAIZ, "rootfs/etc/ssl/certs"), os.path.join(datos, "etc/ssl/certs"))
    shutil.copytree(os.path.join(RAIZ, "rootfs/imagenes"), os.path.join(datos, "imagenes"))   # imagenes de muestra propias
    # Makefile, CMake, scripts de arranque y prueba
    open(os.path.join(dest, "Makefile"), "w", encoding="utf-8").write(
        "# VisualPythonS portable (D459): el MicroPython de SaguiOS y el diseñador, con la capa userland/portable/ y la SDL 2.\n"
        "APPS := python\n"
        "# Solo lo que enlaza el MicroPython de este repositorio (el resto de la libsagui no hace falta).\n"
        "LIB_NOMES := " + " ".join(VPS_LIB) + "\n"
        "include userland/portable/Makefile\n\n"
        "probar: all\n\tsh tools/probar.sh\n.PHONY: probar\n")
    open(os.path.join(dest, "CMakeLists.txt"), "w", encoding="utf-8").write(CMAKE.format(app="visualpythons").replace(
        "# Envoltorio: compila con el Makefile (la fuente de la verdad) y deja el binario en build/portable/.",
        "# Envoltorio: compila con el Makefile (la fuente de la verdad) y deja el binario en build/portable/python."))
    open(os.path.join(dest, ".gitignore"), "w", encoding="utf-8").write("/build/\n*.o\n*.a\n__pycache__/\n*.pyc\n")
    sh = {
        "visualpythons": '#!/bin/sh\n# Abre el diseñador de VisualPythonS (opcional: la carpeta de un proyecto).\nd=$(cd "$(dirname "$0")" && pwd)\n'
                         'export SAGUI_DATOS="${SAGUI_DATOS:-$d/datos}"\nexec "$d/build/portable/python" /lib/python/vps/disenador.py "$@"\n',
        "python-sagui": '#!/bin/sh\n# El MicroPython de SaguiOS con sus módulos (sagui, tkinter, vps, requests...). Sin argumentos: el REPL.\n'
                        'd=$(cd "$(dirname "$0")" && pwd)\nexport SAGUI_DATOS="${SAGUI_DATOS:-$d/datos}"\nexec "$d/build/portable/python" "$@"\n',
        "tools/probar.sh": '#!/bin/sh\n# Autopruebas de VisualPythonS sin ventana: los módulos, el diseñador, las notificaciones, los gráficos y los ejemplos.\nset -e\n'
                           'd=$(cd "$(dirname "$0")/.." && pwd)\nexport SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SAGUI_DATOS="$d/datos"\n'
                           'export HOME=$(mktemp -d)\nPY="$d/build/portable/python"\n'
                           'for t in autoprueba autoprueba_disenador autoprueba_toast autoprueba_grafico; do\n  s=$("$PY" /lib/python/vps/$t.py 2>&1)\n'
                           '  echo "$s" | grep -q "FALLO" && { echo "$s" | grep FALLO; exit 1; }\n  echo "$t: $(echo "$s" | grep -c \': ok\') comprobaciones sin fallos"\ndone\n'
                           'for e in "$d"/datos/ejemplos/vps/*/lanzador.py; do\n  printf \'esperar 400\\nsair\\n\' > "$HOME/guion.txt"\n'
                           '  SAGUI_ROTEIRO="$HOME/guion.txt" "$PY" "$e" >/dev/null 2>&1 || { echo "FALLO: $e"; exit 1; }\ndone\n'
                           'echo "ejemplos de VisualPythonS: todos arrancan y se cierran"\n'}
    for n, t in sh.items():
        p = os.path.join(dest, n)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, "w", encoding="utf-8").write(t)
        os.chmod(p, 0o755)
    hermanos = ", ".join("[%s](https://github.com/aleanun89/%s)" % (NOMBRES[h], h) for h in HERMANOS if h != app)
    open(os.path.join(dest, "README.md"), "w", encoding="utf-8").write(VPS_README.replace("{hermanos}", hermanos))
    shutil.copy("/usr/share/common-licenses/GPL-3", os.path.join(dest, "LICENSE"))
    os.makedirs(os.path.join(dest, "LICENSES"))
    shutil.copy("/usr/share/common-licenses/GPL-2", os.path.join(dest, "LICENSES/GPL-2.0.txt"))
    open(os.path.join(dest, "AVISOS-TERCEROS.md"), "w", encoding="utf-8").write(VPS_TERCEROS)
    os.makedirs(os.path.join(dest, "docs"))
    shutil.copy(os.path.join(RAIZ, "docs/apps-portables.md"), os.path.join(dest, "docs/portable.md"))
    for d in ("visualpythons.md", "visualpythons-manual.md"):
        shutil.copy(os.path.join(RAIZ, "docs", d), os.path.join(dest, "docs", d))
    cap = os.path.join(RAIZ, "docs/capturas/H459c-visualpythons.png")
    if os.path.exists(cap):
        shutil.copy(cap, os.path.join(dest, "docs/visualpythons.png"))
    # las capturas (de SaguiOS, con datos de los ejemplos) que enlaza el manual
    os.makedirs(os.path.join(dest, "docs/capturas"))
    for c in sorted(set(re.findall(r"\(capturas/([A-Za-z0-9_.-]+\.png)\)", leer("docs/visualpythons-manual.md")))):
        shutil.copy(os.path.join(RAIZ, "docs/capturas", c), os.path.join(dest, "docs/capturas", c))
    env = dict(os.environ, GIT_AUTHOR_NAME=AUTOR[0], GIT_AUTHOR_EMAIL=AUTOR[1], GIT_COMMITTER_NAME=AUTOR[0], GIT_COMMITTER_EMAIL=AUTOR[1])
    run = lambda *c: subprocess.run(["git", "-c", "user.name=" + AUTOR[0], "-c", "user.email=" + AUTOR[1], *c], cwd=dest, env=env, check=True, capture_output=True)
    run("init", "-q", "-b", "main")
    run("add", "-A")
    run("commit", "-q", "-m", "VisualPythonS portable: MicroPython de SaguiOS, diseñador y capa SDL 2 de SaguiOS-Extended")
    print(app, len(ficheros), "ficheros ->", dest)


for app, a in APPS.items():
    if not _ARGS or app in _ARGS:
        generar(app, a)
if not _ARGS or "visualpythons" in _ARGS:
    generar_vps()
