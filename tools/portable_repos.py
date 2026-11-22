#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""portable_repos.py - genera los repositorios git locales de Mico, Pluma, Macaco, Tucán, Paint, PlanoMini y GebraMini portables (D459).

Cada repositorio lleva una COPIA de lo que esa aplicacion compila (calculado con `gcc -MM`): las fuentes de la
aplicacion, la capa userland/portable/, los ficheros de la libsagui que enlaza, las cabeceras, las fuentes DejaVu (Pluma y
Macaco), README, licencias y un unico commit con el autor publico. No crea nada en GitHub ni sube nada.

Uso: portable_repos.py [DESTINO] [app ...]      (por omision /home/aanun/SO/repos-publicos y las siete aplicaciones)
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
HERMANOS = ["mico", "pluma", "macaco", "tucan", "paint", "planomini", "gebramini"]
NOMBRES = {"mico": "Mico", "pluma": "Pluma", "macaco": "Macaco", "tucan": "Tucán", "paint": "Paint",
           "planomini": "PlanoMini", "gebramini": "GebraMini"}


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


for app, a in APPS.items():
    if not _ARGS or app in _ARGS:
        generar(app, a)
