#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Auditoría automática de licencias (mejora 057, D109).

Recorre el árbol del proyecto y falla (código de salida 1) si algo no cuadra:

  1. Todo archivo con cabecera SPDX o con «Copyright» ajeno tiene que estar
     cubierto por una fila de docs/terceros.md (columna «Ubicación»).
  2. Todo archivo con licencia GPL (SPDX GPL/AGPL/LGPL o el texto «GNU General
     Public License») tiene que estar aislado en kernel/terceros/linux/.
  3. Todo archivo de kernel/terceros/ tiene cabecera SPDX, y todo lo que hay en
     kernel/terceros/linux/ está en docs/terceros.md.
  4. Las rutas de docs/terceros.md existen (no hay filas obsoletas).
  5. La lista REAL de archivos que entran en la variante `make SEM_GPL=1` (las
     órdenes de compilación del Makefile con `make -n`, más las cabeceras que
     incluyen, vistas con `gcc -MM`) no contiene nada de kernel/terceros/linux/
     ni ningún archivo con licencia GPL.
  6. Si existe el kernel de una compilación sin GPL (por defecto
     build-sem-gpl/kernel.elf), no contiene ningún símbolo ni cadena de
     kernel/terceros/linux/ (se extraen de los propios archivos).

  7. (D258) Ningún fichero tiene GPL-2.0 solo la v2 (SPDX GPL-2.0 / GPL-2.0-only, o el texto
     «version 2 of the License» sin «or later») salvo donde se permite: NetSurf y, mientras exista,
     kernel/terceros/linux/. La v2 sola no se puede combinar con la libsagui (GPL-3.0).
  8. (D258) NetSurf no entra en lo publicable: gerar_versao.sh pasa NETSURF=0 sin
     --con-input-share, el Makefile respeta NETSURF=0 y, si hay fuentes de NetSurf, la lista real
     de órdenes de `make -n` con esas variables no lo contiene.
  8b. (D259) Lo mismo para GNUBoy (GNUBOY=0) y el r8169 de Linux (PUBLICABLE=1, -DSAGUI_PUBLICABLE);
     con --elf-publicable RUTA, el kernel de esa compilación no lleva marcas del r8169 de Linux.
  9. (D258) kernel/terceros/linux/ puede no existir (se está retirando); si LINUX_RETIRADO es
     True (ya fusionada la rama v8.0b7-sin-linux), que reaparezca es un fallo.
 10. Con --estricto (cuando ya se ha ejecutado tools/spdx.py): todo fichero propio tiene su
     SPDX-License-Identifier (GPL-3.0-only o GPL-2.0-or-later). Los ficheros con esas marcas son
     propios: no cuentan como «licencia ajena» ni como GPL a aislar.

     (D261) El pegamento propio con NetSurf y GNUBoy (netsurf.mk, gnuboy.mk, plataforma/, libc/curl)
     es código propio (GPL-2.0-or-later): puede ir en el repositorio público y, en modo estricto,
     debe llevar SPDX. El código de NetSurf y de GNUBoy, no.
 11. Con --publico (árbol exportado para el repositorio público): no existen
     userland/terceros/netsurf/fuentes/ ni userland/terceros/gnuboy/jogo/ ni binarios de ellos, y
     ningún fichero lleva las cabeceras de copyright de NetSurf (y sus bibliotecas) o de GNUBoy.

Uso: tools/auditar_licencias.py [--sin-make] [--elf RUTA] [--elf-publicable RUTA] [--estricto] [--publico]
  --sin-make   salta el punto 5 y la parte dinámica del 8 (no ejecuta make ni gcc)
  --estricto   exige SPDX en todo fichero propio (punto 10)
  --publico    comprobaciones del árbol público (punto 11)
"""
import argparse
import concurrent.futures
import os
import re
import shlex
import subprocess
import sys
import tempfile

RAIZ = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
EXT = {".c", ".h", ".asm", ".inc", ".s", ".S", ".py", ".ld", ".sh", ".txt", ".cfg", ".mk"}
NOMBRES = {"Makefile", "README", "LICENSE", "COPYING"}
# Carpetas que no son fuentes del proyecto (salida de compilación, entregas, fotos).
IGNORAR = ("build", "entrega", ".git", "testes_hw", "__pycache__", "dashboard",
           # datos privados de quien compila, fuera de git (D228): no son fuentes del proyecto
           "input_share", "NEW_FONTS", "data_extra")


def ignorada(nombre):
    """Carpeta que no se audita: las de IGNORAR y cualquier salida build-* (build-sem-gpl...)."""
    return nombre in IGNORAR or nombre.startswith("build-")
GPL_AISLADA = "kernel/terceros/linux/"
# Carpetas donde puede vivir el codigo GPL: aisladas del resto y fuera de la
# variante sin GPL (D109). El kernel/terceros/linux/ es el codigo derivado de
# Linux (D073), el userland/terceros/opentyrian/ es el juego OpenTyrian, que
# es GPL-2.0 (D156), el userland/terceros/netsurf/ el navegador NetSurf
# (GPL-2.0; sus fuentes, en fuentes/, no van en git: D184) y el
# userland/terceros/gprolog/ es GNU Prolog, LGPL-3 o GPL-2 a elegir (D189).
# userland/terceros/openjazz/ es OpenJazz, el motor de Jazz Jackrabbit, GPL-2.0 o
# posterior (D228). userland/terceros/conquer/ es Vanilla Conquer, el motor de
# Command & Conquer, GPL-3.0 con los términos adicionales de EA (D230).
# userland/terceros/quake/ es el motor de Quake (el WinQuake de id Software),
# GPL-2.0 o posterior (D229). userland/terceros/gnuboy/ es SDL2-GNUBoy, el emulador
# de Game Boy, GPL-2.0 (D235). userland/terceros/dosbox/ es DOSBox 0.74-3, el emulador de
# MS-DOS, GPL-2.0 o posterior (D294); sus ficheros de MAME (src/hardware/mame/) son BSD-3-Clause.
# userland/terceros/mgba/ es mGBA 0.10.5, el emulador de GBA/GB/GBC, MPL-2.0 (D322; con blip_buf LGPL-2.1+ e
# inih BSD): compatible con la GPL v3, se aísla como los demás porque sus textos citan la GPL.
# userland/terceros/laines/ es LaiNES (NES, BSD-2-Clause, con el APU LGPL de blargg) y
# userland/terceros/gearsystem/ es Gearsystem (Master System y Game Gear, GPL-3.0) (D321).
# userland/web/terceros/ffmpeg-h264-tablas/ (LGPL-2.1+) y userland/web/terceros/faad2-huffman/
# (GPL-2.0+) son las tablas del H.264 y del AAC del reproductor de vídeo (D276): fuera de la
# variante sin GPL (el MP4 no se decodifica en ella: video_mp4.c y el Makefile lo respetan).
GPL_AISLADAS = (GPL_AISLADA, "userland/terceros/opentyrian/", "userland/terceros/netsurf/", "userland/terceros/gprolog/",
                "userland/terceros/openjazz/", "userland/terceros/conquer/", "userland/terceros/quake/",
                "userland/terceros/gnuboy/", "userland/terceros/dosbox/", "userland/terceros/mgba/",
                "userland/terceros/laines/", "userland/terceros/gearsystem/",
                "userland/web/terceros/ffmpeg-h264-tablas/",
                "userland/web/terceros/faad2-huffman/")
RE_SPDX = re.compile(r"^\s*(?://|/\*|\*|#|;)\s*SPDX-License-Identifier:\s*(.+?)\s*(?:\*/)?\s*$")
RE_COPY = re.compile(r"^\s*(?://|/\*|\*|#|;)?\s*Copyright\s*(?:\(c\)|\xa9|©|\d)", re.I)
RE_GPL_TEXTO = re.compile(r"GNU (?:Lesser |Library |Affero )?General Public License")
# Autores de este proyecto: una línea de copyright con ellos no es «ajena».
PROPIOS = ("sagui", "aanun", "anuncibay")

# D258: las marcas SPDX de los ficheros propios (capas de docs/licencia.md).
SPDX_PROPIOS = {"GPL-3.0-only", "GPL-3.0-or-later", "GPL-2.0-or-later"}
# Poner a True al fusionar la rama v8.0b7-sin-linux: kernel/terceros/linux/ ya no debe existir.
LINUX_RETIRADO = False
# Donde puede haber código GPL-2.0 solo la v2 (nunca en lo publicable: ver punto 8).
# D259: NetSurf, GNUBoy y el r8169 de Linux (solo la v2, solo en lo privado); el resto de
# kernel/terceros/linux/ (IDT, k10temp) es GPL-2.0-or-later y va en todas las variantes.
SOLO_V2_PERMITIDO = ("userland/terceros/netsurf/", "userland/terceros/gnuboy/", "kernel/terceros/linux/r8169/")
# Lo que lo publicable (PUBLICABLE=1) deja fuera: (variable de make, carpeta, texto de la auditoría).
FUERA_DE_LO_PUBLICABLE = (("NETSURF", "userland/terceros/netsurf/", "NetSurf"),
                          ("GNUBOY", "userland/terceros/gnuboy/", "GNUBoy"))
R8169_LINUX = "kernel/terceros/linux/r8169/"
# D261: nuestro pegamento con NetSurf y GNUBoy. Es propio (GPL-2.0-or-later) aunque viva en
# userland/terceros/<x>/ y va en el repositorio público; el código del upstream no.
PEGAMENTO = ("userland/terceros/netsurf/netsurf.mk", "userland/terceros/netsurf/plataforma/",
             "userland/terceros/netsurf/libc/", "userland/terceros/gnuboy/gnuboy.mk",
             "userland/terceros/gnuboy/plataforma/", "userland/terceros/gnuboy/roms-leeme.txt",
             "userland/terceros/mgba/mgba.mk", "userland/terceros/mgba/plataforma/", "userland/terceros/mgba/leeme.txt")
# D259: código de M4R4G0N (PR #3 del original), GPL-3.0-only como el de Eli; figura en terceros.md
# pero es propio de la capa GPL v3: no es «GPL a aislar».
M4R4G0N = ("kernel/rede/e1000.c", "kernel/rede/rede.c", "kernel/rede/tcp.c", "kernel/rede/socket.c",
           "kernel/inc/rede.h", "userland/programas/cipo.c")
# D262: el motor de la web del Cipó (código de robson-apk, D141) es GPL-3.0-only: lo aportó al
# repositorio original (GPL-3.0) con el PR #6 de erigobeli/sagui-os (Términos de GitHub, D.6).
# Como el de M4R4G0N, figura en terceros.md pero es de la capa GPL v3. Quedan fuera las tablas de
# datos (brotli_tab.h: RFC 7932; quebra_tab.h: Unicode) y el js_base.h generado.
ROBSON = ("userland/web/motor.c",
          "userland/web/motor.h",
          "userland/web/js.c",
          "userland/web/js_nativo.h",
          "userland/web/js_num.h",
          "userland/web/js_big.h",
          "userland/web/js_quadros.h",
          "userland/web/canvas2d.c",
          "userland/web/svg.c",
          "userland/web/webgl.c",
          "userland/web/glsl.c",
          "userland/web/glsl.h",
          "userland/web/glsl_emb.h",
          "userland/web/glsl_gen.h",
          "userland/web/quebra.c",
          "userland/web/brotli.c",
          "userland/libfontes/fonte.c",
          "userland/libsagui/h264.c",
          "userland/libsagui/aac.c",
          "userland/libsagui/mp4.c",
          "userland/inc/h264.h",
          "userland/inc/aac.h",
          "userland/inc/mp4.h",
          "userland/web/referencia/cipo_robson.c")
# Lo que no se redistribuye (opción A del mantenedor): lo descargan los guiones de tools/.
NO_REDISTRIBUIBLE = ("userland/terceros/netsurf/fuentes", "userland/terceros/gnuboy/jogo", R8169_LINUX.rstrip("/"))
# Marcas del código de NetSurf (y sus bibliotecas) y de GNUBoy, para el punto 11.
RE_MARCAS_AJENAS = re.compile(
    r"This file is part of (?:NetSurf|libdom|LibCSS|Hubbub|LibParserUtils|libnsfb|nsgenbind|RUfl|Libsvgtiny|libnsutils)"
    r"|Copyright[^\n]{0,40}(?:NetSurf|John-Mark Bell|Rob Kendrick|Vincent Sanders)"
    r"|\b(?:Laguna|Gilgamesh)\b")
# Ficheros que citan esas marcas a propósito (esta herramienta y lo que la documenta o prueba).
CITAN_MARCAS = ("tools/auditar_licencias.py", "tools/publicar.sh", "tools/gnuboy_fuentes.sh", "tools/netsurf_fuentes.sh",
                "tests/h260_publicar.py", "tests/h81_licencias.py")
# Carpetas de lo que NO es propio (para exigir SPDX en el modo estricto).
NO_PROPIO = ("userland/terceros/", "kernel/terceros/", "userland/micropython/", "userland/web/quickjs/",
             "userland/web/terceros/")
EXT_SPDX = {".c", ".h", ".cpp", ".hpp", ".cc", ".asm", ".inc", ".s", ".S", ".py", ".sh", ".mk", ".ld"}
ARCHIVOS_DE_LICENCIA = {"LICENSE"}   # el texto de la GPL v3 de la raíz
errores = []


def es_pegamento(ruta):
    return ruta in M4R4G0N or ruta in ROBSON or any(ruta == p or ruta.startswith(p) for p in PEGAMENTO)


def falla(msg):
    errores.append(msg)
    print("FALLO:", msg)


def rel(p):
    return os.path.relpath(p, RAIZ).replace(os.sep, "/")


def archivos_del_arbol():
    """Archivos de texto del proyecto (git ls-files si hay git; si no, os.walk)."""
    try:
        s = subprocess.run(["git", "-C", RAIZ, "ls-files"], capture_output=True, text=True, check=True).stdout
        lista = [l for l in s.splitlines() if l and os.path.exists(os.path.join(RAIZ, l))]
    except Exception:
        lista = []
        for d, ds, fs in os.walk(RAIZ):
            ds[:] = [x for x in ds if not ignorada(x)]
            lista += [rel(os.path.join(d, f)) for f in fs]
    r = []
    for l in lista:
        if ignorada(l.split("/")[0]) or l == "tools/auditar_licencias.py":
            continue
        b = os.path.basename(l)
        if l in ARCHIVOS_DE_LICENCIA:
            continue
        if os.path.splitext(b)[1] in EXT or b in NOMBRES:
            r.append(l)
    return r


def cabecera(ruta, n=80):
    try:
        with open(os.path.join(RAIZ, ruta), "r", encoding="latin-1") as f:
            return [next(f, "") for _ in range(n)]
    except OSError:
        return []


def analizar(ruta):
    """(ids SPDX, hay copyright ajeno, texto GPL) de la cabecera de un archivo."""
    spdx, copia, gpl = [], False, False
    for l in cabecera(ruta):
        m = RE_SPDX.match(l)
        if m:
            spdx.append(m.group(1))
        if RE_COPY.match(l) and not any(p in l.lower() for p in PROPIOS):
            copia = True
        if RE_GPL_TEXTO.search(l):
            gpl = True
    return spdx, copia, gpl


def es_gpl(spdx, texto):
    return texto or any(re.search(r"GPL", s) for s in spdx)


def solo_v2(ruta):
    """True si la cabecera del fichero dice GPL-2.0 solo la v2 (SPDX o texto sin «or later»)."""
    spdx, _, _ = analizar(ruta)
    if any(x.strip() in ("GPL-2.0", "GPL-2.0-only") for x in spdx):
        return True
    if os.path.basename(ruta) in ("COPYING", "LICENSE") or ruta.endswith(".txt"):
        return False   # el texto de la licencia, no una cabecera
    t = re.sub(r"[\s*/#;]+", " ", " ".join(cabecera(ruta)))
    if re.search(r"version 2 only", t, re.I):
        return True
    for m in re.finditer(r"version 2(?![\d.])(?: of the (?:GNU General Public )?License)?", t, re.I):
        antes = t[max(0, m.start() - 160):m.start()]
        if not re.search(r"General Public License|Free Software Foundation", antes, re.I):
            continue
        if not re.search(r"later|\+", t[m.end():m.end() + 70], re.I):
            return True
    return False


def rutas_de_terceros():
    """Rutas (en `...`) de la primera columna de las tablas de docs/terceros.md."""
    rutas = []
    with open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8") as f:
        for l in f:
            if not l.startswith("|"):
                continue
            celdas = [c.strip() for c in l.strip().strip("|").split("|")]
            for c in celdas[1:2] + celdas[:1]:
                for r in re.findall(r"`([^`]+)`", c):
                    if "/" in r and "<" not in r and " " not in r and not r.startswith("make"):
                        rutas.append(r)
    return sorted(set(rutas))


def cubierto(ruta, rutas):
    for r in rutas:
        base = r.rstrip("/")
        if ruta == base or ruta.startswith(base + "/"):
            return True
    return False


# Carpetas de kernel/terceros/ cuyo upstream no pone cabecera SPDX en cada fichero sino un
# LICENSE en la raíz: la licencia vale para toda la carpeta (tiene que existir ese LICENSE).
LICENCIA_POR_CARPETA = {"kernel/terceros/uacpi/": "MIT"}


def comprobar_arbol(archivos, rutas):
    ajenos = gpl = 0
    for a in archivos:
        spdx, copia, texto = analizar(a)
        if es_pegamento(a) or not any(a.startswith(d) for d in GPL_AISLADAS):
            # marca de la licencia del propio proyecto (D258, D261): también en lo heredado del original
            # que figura en docs/terceros.md (el código de M4R4G0N, GPL-3.0 como el de Eli) y en el
            # pegamento propio que vive en carpetas de terceros
            spdx = [x for x in spdx if x not in SPDX_PROPIOS]
        for carpeta, lic in LICENCIA_POR_CARPETA.items():
            if not spdx and a.startswith(carpeta):
                if not os.path.exists(os.path.join(RAIZ, carpeta, "LICENSE")):
                    falla("%s: %s no tiene LICENSE para la licencia de la carpeta" % (a, carpeta))
                spdx = [lic]
        en_terceros = a.startswith("kernel/terceros/")
        if en_terceros and not spdx:
            falla("%s: está en kernel/terceros/ y no tiene cabecera SPDX" % a)
        if spdx or copia or texto:
            ajenos += 1
            if not cubierto(a, rutas):
                falla("%s: licencia/copyright ajeno (%s) sin fila en docs/terceros.md" % (a, ", ".join(spdx) or "copyright"))
        if es_gpl(spdx, texto):
            gpl += 1
            if not any(a.startswith(d) for d in GPL_AISLADAS):
                falla("%s: licencia GPL fuera de %s (%s)" % (a, " ni ".join(GPL_AISLADAS), ", ".join(spdx) or "texto GPL"))
        if any(a.startswith(d) for d in GPL_AISLADAS) and not cubierto(a, rutas):
            falla("%s: está en una carpeta de GPL y no figura en docs/terceros.md" % a)
        if solo_v2(a) and not any(a.startswith(d) for d in SOLO_V2_PERMITIDO):
            falla("%s: GPL-2.0 solo la v2 (sin «or later») fuera de %s: no se puede enlazar con la libsagui (D258)"
                  % (a, " ni ".join(SOLO_V2_PERMITIDO)))
    excluidas = []   # D260: en el árbol público faltan a propósito las rutas de tools/publicar-excluir.txt
    lista = os.path.join(RAIZ, "tools", "publicar-excluir.txt")
    if os.path.exists(lista):
        for l in open(lista, encoding="utf-8"):
            l = l.split("#", 1)[0].strip()
            if l:
                excluidas.append(l.strip("/"))
    for r in rutas:
        # D261: lo que se descarga (fuentes de NetSurf, jogo/ de GNUBoy) y el r8169 no están en el
        # árbol público, aunque terceros.md los registre.
        if r.rstrip("/").startswith(NO_REDISTRIBUIBLE):
            continue
        if not os.path.exists(os.path.join(RAIZ, r.rstrip("/"))):
            if any(r.rstrip("/") == e or r.startswith(e + "/") for e in excluidas):
                continue   # no se publica (NetSurf, GNUBoy, r8169 de Linux): solo en lo privado
            falla("docs/terceros.md: la ruta `%s` no existe" % r)
    if os.path.isdir(os.path.join(RAIZ, "userland/terceros/gnuboy/jogo")):
        print("AVISO: userland/terceros/gnuboy/jogo/ es GPL-2.0 casi sin cabeceras «or later»: solo en lo privado (D259): "
              "fuera de lo publicable (docs/licencia.md)")
    print("árbol: %d archivos de texto, %d con licencia o copyright ajeno, %d GPL (todos aislados)" % (len(archivos), ajenos, gpl))


def comprobar_terceros_web(rutas):
    """D262: cada carpeta de userland/web/terceros/ lleva su ORIGEM.md, su licencia y su fila en terceros.md."""
    base = os.path.join(RAIZ, "userland/web/terceros")
    if not os.path.isdir(base):
        return
    n = 0
    for d in sorted(os.listdir(base)):
        ruta = "userland/web/terceros/" + d
        if not os.path.isdir(os.path.join(base, d)):
            continue
        n += 1
        if not os.path.exists(os.path.join(base, d, "ORIGEM.md")):
            falla("%s: falta ORIGEM.md (origen y licencia, D262)" % ruta)
        if not any(os.path.exists(os.path.join(base, d, f)) for f in ("LICENSE", "COPYING")):
            falla("%s: falta el texto de la licencia (LICENSE o COPYING, D262)" % ruta)
        if not any(r.rstrip("/") == ruta for r in rutas):
            falla("%s: sin fila en docs/terceros.md (D262)" % ruta)
    print("terceros del motor web: %d carpetas con ORIGEM.md, licencia y fila" % n)


def comprobar_linux_retirado():
    if LINUX_RETIRADO and os.path.exists(os.path.join(RAIZ, GPL_AISLADA)):
        falla("%s ha reaparecido: se retiró en la v8.0b7 (D258)" % GPL_AISLADA)


def comprobar_estricto(archivos):
    """Todo fichero propio con extensión de código tiene su SPDX (punto 10)."""
    sin = [a for a in archivos if (os.path.splitext(a)[1] in EXT_SPDX or os.path.basename(a) == "Makefile")
           and (not a.startswith(NO_PROPIO) or es_pegamento(a))
           # ejemplos que el usuario abre en Mico: sin línea SPDX a propósito (tools/spdx.py, EXCLUIDOS)
           and not a.startswith(("rootfs/ejemplos/mico/", "rootfs/ejemplos/vps/"))]
    n = 0
    for a in sin:
        spdx, _, _ = analizar(a)
        if not any(x in SPDX_PROPIOS for x in spdx):
            if not es_pegamento(a) and (analizar(a)[1] or cubierto(a, rutas_de_terceros())):
                continue   # copyright ajeno o registrado como tercero: no se exige
            falla("%s: fichero propio sin SPDX-License-Identifier (ejecutar tools/spdx.py)" % a)
        else:
            n += 1
    print("estricto: %d ficheros propios con SPDX" % n)


def comprobar_publico():
    """Punto 11 (D261): en el árbol público no hay código de NetSurf ni de GNUBoy, solo el pegamento."""
    for d in NO_REDISTRIBUIBLE:
        if os.path.exists(os.path.join(RAIZ, d)):
            falla("árbol público: existe %s (código de terceros solo v2: se descarga, no se publica; D261)" % d)
    n = 0
    for pasta, ds, fs in os.walk(RAIZ):
        ds[:] = [x for x in ds if not ignorada(x) and x != "docs"]
        for f in fs:
            r = rel(os.path.join(pasta, f))
            if r in CITAN_MARCAS or f.endswith(".md"):
                continue
            ruta = os.path.join(pasta, f)
            try:
                if os.path.getsize(ruta) > 2_000_000:
                    continue
                with open(ruta, "rb") as fh:
                    datos = fh.read()
            except OSError:
                continue
            if b"\0" in datos[:4096]:
                if re.search(rb"netsurf|gnuboy", f.encode().lower()) and not r.startswith("userland/terceros/"):
                    falla("árbol público: binario sospechoso %s" % r)
                continue
            n += 1
            m = RE_MARCAS_AJENAS.search(datos.decode("latin-1"))
            if m:
                falla("árbol público: %s lleva una marca de código de NetSurf/GNUBoy («%s»; D261)" % (r, m.group(0)[:50]))
    print("público: %d ficheros de texto sin código de NetSurf ni de GNUBoy" % n)


def netsurf_fuera_de_lo_publicable(sin_make):
    """Punto 8 (D258, D259): NetSurf, GNUBoy y el r8169 de Linux no entran en las entregas publicables
    (gerar_versao.sh sin --con-input-share)."""
    with open(os.path.join(RAIZ, "gerar_versao.sh"), encoding="utf-8") as f:
        guion = f.read()
    m = re.search(r"else\s*\n(?:[^\n]*\n)*?\s*ENTRADA_VAR=\(([^)]*)\)", guion)
    variables = re.findall(r'"([A-Za-z_]+=[^"]*)"', m.group(1)) if m else []
    with open(os.path.join(RAIZ, "Makefile"), encoding="utf-8") as f:
        mk = f.read()
    for var, carpeta, nombre in FUERA_DE_LO_PUBLICABLE:
        if var + "=0" not in variables:
            falla("gerar_versao.sh: lo publicable (sin --con-input-share) no pasa %s=0: %s entraría (D258, D259)" % (var, nombre))
        if not re.search(r"^%s\s*\?=\s*1" % var, mk, re.M) or not re.search(r"ifeq \(\$\(%s\),1\)" % var, mk):
            falla("Makefile: %s=0 no deja %s fuera de la compilación (D258, D259)" % (var, nombre))
    if "PUBLICABLE=1" not in variables:
        falla("gerar_versao.sh: lo publicable (sin --con-input-share) no pasa PUBLICABLE=1: el r8169 de Linux entraría (D259)")
    if "-DSAGUI_PUBLICABLE" not in mk:
        falla("Makefile: PUBLICABLE=1 no define SAGUI_PUBLICABLE: el r8169 de Linux entraría (D259)")
    with open(os.path.join(RAIZ, "kernel/rede/r8169.c"), encoding="utf-8") as f:
        r8169 = f.read()
    if not re.search(r"defined\(SAGUI_PUBLICABLE\)", r8169):
        falla("kernel/rede/r8169.c: SAGUI_PUBLICABLE no deja fuera el código de %s (D259)" % R8169_LINUX)
    if errores:
        return
    if sin_make:
        print("lo publicable: gerar_versao.sh pasa %s y el Makefile los respeta" % " ".join(variables))
        return
    with tempfile.TemporaryDirectory() as tmp:
        b = os.path.join(tmp, "b")
        out = subprocess.run(["make", "-n", "-B", "-s"] + variables + ["B=" + b, b + "/saguios.img"], cwd=RAIZ,
                             capture_output=True, text=True).stdout
        out = out.replace("\\\n", " ")
        defs = [l for l in out.splitlines() if "kernel/rede/r8169.c" in l and " -c " in l]
    for var, carpeta, nombre in FUERA_DE_LO_PUBLICABLE:
        if carpeta in out:
            falla("lo publicable (make con %s) compila %s (D258, D259)" % (" ".join(variables), nombre))
        else:
            print("%s: `make -n` con las variables de lo publicable no lo compila" % nombre)
    if not defs or not all("-DSAGUI_PUBLICABLE" in l for l in defs):
        falla("lo publicable (make con %s) compila el r8169 sin -DSAGUI_PUBLICABLE (D259)" % " ".join(variables))
    else:
        print("r8169: `make -n` con las variables de lo publicable lo compila con -DSAGUI_PUBLICABLE (sin Linux)")


def archivos_de_la_variante_sin_gpl():
    """Fuentes y cabeceras que entran en `make SEM_GPL=1`, según el propio Makefile."""
    with tempfile.TemporaryDirectory() as tmp:
        b = os.path.join(tmp, "b")
        out = subprocess.run(["make", "-n", "-B", "-s", "SEM_GPL=1", "B=" + b, b + "/saguios.img"], cwd=RAIZ,
                             capture_output=True, text=True).stdout
    out = out.replace("\\\n", " ")
    fuentes, ordenes = set(), []
    for l in out.splitlines():
        l = l.strip()
        if re.match(r"(gcc|nasm)\b", l) and (" -c " in l or l.startswith("nasm")):
            try:
                t = shlex.split(l)
            except ValueError:
                continue
            fuentes.add(rel(os.path.join(RAIZ, t[-1])))
            if t[0] == "gcc":
                u = []
                saltar = False
                for x in t:
                    if saltar:
                        saltar = False
                    elif x == "-o":
                        saltar = True
                    elif x == "-c":
                        u.append("-MM")
                    elif x != "-g":
                        u.append(x)
                ordenes.append(u)
    todos = set(f for f in fuentes if os.path.exists(os.path.join(RAIZ, f)))

    def cabeceras(u):
        r = subprocess.run(u, cwd=RAIZ, capture_output=True, text=True)
        txt = r.stdout.replace("\\\n", " ")
        return [rel(os.path.join(RAIZ, x)) for x in txt.split()[2:] if os.path.exists(os.path.join(RAIZ, x))]

    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        for lista in ex.map(cabeceras, ordenes):
            todos.update(lista)
    return sorted(todos), len(fuentes)


def comprobar_variante():
    todos, nfuentes = archivos_de_la_variante_sin_gpl()
    if nfuentes < 200:
        falla("variante sin GPL: sólo %d fuentes en `make -n` (¿falló make?)" % nfuentes)
    for a in todos:
        if any(a.startswith(d) for d in GPL_AISLADAS):
            falla("variante sin GPL: entra %s" % a)
        spdx, _, texto = analizar(a)
        spdx = [x for x in spdx if x not in SPDX_PROPIOS]   # marca de un fichero propio (D258)
        if es_gpl(spdx, texto):
            falla("variante sin GPL: entra %s con licencia GPL" % a)
    print("variante sin GPL: %d fuentes y %d archivos en total (con cabeceras), nada GPL" % (nfuentes, len(todos)))


def marcas_gpl(carpetas=None):
    """Símbolos y cadenas distintivos de las carpetas de GPL, para buscarlos en el binario."""
    marcas = set()
    for d in (carpetas or GPL_AISLADAS):
        if not os.path.isdir(os.path.join(RAIZ, d)):
            continue
        for pasta, ds, fs in os.walk(os.path.join(RAIZ, d)):
            # lo bajado: NetSurf (D184), datos de OpenJazz (D228) y de Quake (D229), Red Alert (D230)
            ds[:] = [x for x in ds if x not in ("fuentes", "datos", "redalert")]
            for f in fs:
                txt = open(os.path.join(pasta, f), encoding="latin-1").read()
                marcas.update(m for m in re.findall(r"\b(?:rtl_|r8169_)[A-Za-z0-9_]{6,}", txt))
                marcas.update(m for m in re.findall(r'"([A-Za-z0-9/ ._-]{8,})"', txt) if re.search(r"RTL|81\d\d", m))
                marcas.update(m for m in re.findall(r"\b(?:tabela_linux_)[A-Za-z0-9_]+", txt))
                # O jogo (D156): cadeias que so estao no codigo do jogo.
                marcas.update(m for m in re.findall(r"\b(?:OPENTYRIAN|TYRIAN_)[A-Za-z0-9_]*", txt))
                marcas.update(m for m in re.findall(r'"([^"\n]*tyrian[^"\n]*)"', txt, re.I))
                # OpenJazz (D228): las cadenas con su nombre (el lanzador, el motor).
                marcas.update(m for m in re.findall(r'"([^"\n]*openjazz[^"\n]*)"', txt, re.I))
                # Vanilla Conquer (D230): las cadenas con el nombre del motor o del juego.
                marcas.update(m for m in re.findall(r'"([^"\n]*(?:Vanilla Conquer|Command & Conquer|C&C95)[^"\n]*)"', txt))
                # Quake (D229): las cadenas con su nombre (el lanzador, la plataforma, el motor).
                marcas.update(m for m in re.findall(r'"([^"\n]*quake[^"\n]*)"', txt, re.I))
                # DOSBox (D294): las cadenas con su nombre (el lanzador, Recreativa, el motor).
                marcas.update(m for m in re.findall(r'"([^"\n]*dosbox[^"\n]*)"', txt, re.I))
                # SDL2-GNUBoy (D235): las cadenas con el nombre del emulador (el lanzador, el motor).
                marcas.update(m for m in re.findall(r'"([^"\n]*gnuboy[^"\n]*)"', txt, re.I))
    return sorted(marcas)


def comprobar_binario(elf, carpetas=None):
    if not os.path.exists(elf):
        print("binario: %s no existe, se omite (compilar con make SEM_GPL=1)" % rel(elf))
        return
    datos = open(elf, "rb").read()
    marcas = marcas_gpl(carpetas)
    # Una marca que forma parte de la ruta de esta copia de trabajo (miniOS_master_os_dosbox) sale en la
    # información de depuración del binario sin ser del port: no cuenta.
    malas = [m for m in marcas if m.encode("latin-1") in datos and m.lower() not in RAIZ.lower()]
    for m in malas:
        falla("%s contiene «%s» de %s" % (rel(elf), m, " ni ".join(carpetas or GPL_AISLADAS)))
    print("binario: %s sin ninguna de las %d marcas de %s" % (rel(elf), len(marcas), " ni ".join(carpetas or GPL_AISLADAS)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sin-make", action="store_true")
    ap.add_argument("--estricto", action="store_true")
    ap.add_argument("--publico", action="store_true")
    ap.add_argument("--elf", default=os.path.join(RAIZ, "build-sem-gpl", "kernel.elf"))
    # D259: el kernel de una compilación PUBLICABLE=1 (con IDT y k10temp, que son «or later»)
    # no puede llevar nada del r8169 de Linux.
    ap.add_argument("--elf-publicable", default=None)
    a = ap.parse_args()
    archivos = archivos_del_arbol()
    comprobar_arbol(archivos, rutas_de_terceros())
    comprobar_terceros_web(rutas_de_terceros())
    comprobar_linux_retirado()
    netsurf_fuera_de_lo_publicable(a.sin_make)
    if a.estricto:
        comprobar_estricto(archivos)
    if a.publico:
        comprobar_publico()
    if not a.sin_make:
        comprobar_variante()
    comprobar_binario(a.elf)
    if a.elf_publicable:
        comprobar_binario(a.elf_publicable, (R8169_LINUX,))   # NetSurf y GNUBoy: los vigila `make -n` (el kernel solo guarda rutas de menú)
    if errores:
        print("\nAuditoría de licencias: %d problema(s)" % len(errores))
        return 1
    print("\nAuditoría de licencias: todo cuadra")
    return 0


sys.exit(main())
