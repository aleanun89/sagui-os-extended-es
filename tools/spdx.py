#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Marca los ficheros propios con su línea SPDX-License-Identifier (D258).

Esquema de capas (docs/licencia.md):
  - los ficheros que ya existían en el commit del proyecto original
    (dba0fffc, de Eli Rigobeli) y cualquier fichero derivado de ellos
    (renombrado o copiado, aunque esté muy cambiado): GPL-3.0-only;
  - los ficheros nuevos de este proyecto: GPL-2.0-or-later;
  - el pegamento propio con NetSurf y GNUBoy (D261: netsurf.mk, gnuboy.mk, plataforma/, libc/)
    es nuevo del fork: GPL-2.0-or-later aunque viva en userland/terceros/;
  - los terceros (userland/terceros/, kernel/terceros/, MicroPython, QuickJS y
    todo lo registrado en docs/terceros.md) no se tocan.

La línea va como primera línea del fichero (después de «#!» y de la línea de
codificación en los scripts), en el comentario propio del lenguaje. Se trabaja
con bytes: la codificación del fichero (arquivos.c y cipo.c son Latin-1, el
resto UTF-8) y sus finales de línea no cambian. Es idempotente.

Uso:
  tools/spdx.py --comprobar        solo informa: recuentos, ficheros sin marca, dudosos
  tools/spdx.py                    escribe las marcas que faltan
  tools/spdx.py --raiz DIR         trabaja sobre otra copia del árbol (pruebas)
  tools/spdx.py --lista            con --comprobar, escribe también cada fichero
Sin git (un árbol exportado para el repositorio público) solo se puede comprobar: se exige que cada
fichero propio lleve alguna SPDX propia (no se puede saber de cuál de las dos capas es).
"""
import argparse
import os
import re
import subprocess
import sys

BASE = "dba0fffc"          # «SaguiOS original (portugués de Brasil)»
LIC_ELI = "GPL-3.0-only"   # no se sabe sin red si Eli dice «or later»: ver docs/licencia.md
LIC_NUEVA = "GPL-2.0-or-later"
# Derivados que git no ve como renombrado (el original se partió en varios ficheros):
# userland/sagsh/ sale de userland/programas/sagsh.c (commit 73ffef4c, D037).
DERIVADOS_EXTRA = ("userland/sagsh/",)
# D259: el código de M4R4G0N (PR #3 del original: e1000, base de la pila TCP/IP y el navegador
# Cipó) es GPL-3.0-only como el de Eli. Está registrado como «tercero» en docs/terceros.md, pero
# solo para esto se saca de esa exclusión y se marca GPL-3.0-only.
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
# D261: ficheros propios dentro de carpetas de terceros (van al repositorio público).
PEGAMENTO = ("userland/terceros/netsurf/netsurf.mk", "userland/terceros/netsurf/plataforma/",
             "userland/terceros/netsurf/libc/", "userland/terceros/gnuboy/gnuboy.mk",
             "userland/terceros/gnuboy/plataforma/")
EXCLUIDOS = ("userland/terceros/", "kernel/terceros/", "userland/micropython/", "userland/web/quickjs/", "userland/web/terceros/",
             "build/", "entrega/", "testes_hw/", "dashboard/", "firmware/",
             # Ejemplos que la prueba h64 (Mico) lee por números de línea: una cabecera los desplazaría.
             "rootfs/ejemplos/mico/")
# extensión -> prefijo de comentario
COMENTARIO = {".c": "//", ".h": "//", ".cpp": "//", ".hpp": "//", ".cc": "//", ".ld": "/*",
              ".asm": ";", ".inc": ";", ".s": "#", ".S": "#", ".py": "#", ".sh": "#", ".mk": "#"}
NOMBRES = {"Makefile": "#"}
RE_SPDX = re.compile(rb"SPDX-License-Identifier:\s*([^\r\n*]+?)\s*(?:\*/)?\r?$", re.M)
RE_COPY = re.compile(rb"Copyright\s*(?:\(c\)|\xa9|\xc2\xa9|\d)", re.I)
PROPIOS = (b"sagui", b"aanun", b"anuncibay")


def git(raiz, *args):
    return subprocess.run(["git", "-C", raiz] + list(args), capture_output=True, check=True).stdout.decode("utf-8", "replace")


def rutas_de_terceros(raiz):
    """Rutas de la primera columna de las tablas de docs/terceros.md (como la auditoría)."""
    rutas = set()
    ruta = os.path.join(raiz, "docs", "terceros.md")
    if not os.path.exists(ruta):
        return rutas
    with open(ruta, encoding="utf-8") as f:
        for l in f:
            if not l.startswith("|"):
                continue
            celdas = [c.strip() for c in l.strip().strip("|").split("|")]
            for c in celdas[1:2] + celdas[:1]:
                for r in re.findall(r"`([^`]+)`", c):
                    if "/" in r and "<" not in r and " " not in r and not r.startswith("make"):
                        rutas.add(r.rstrip("/"))
    return rutas


def de_origen_eli(raiz):
    """Conjunto de rutas actuales que descienden de un fichero del commit BASE (renombres y copias)."""
    base = set(git(raiz, "ls-tree", "-r", "--name-only", BASE).splitlines())
    estado = {p: p for p in base}        # ruta actual -> ruta original en BASE (o None si es nueva)
    log = git(raiz, "log", "--topo-order", "--reverse", "-M30%", "-C30%", "--name-status",
              "--format=@@%H", BASE + "..HEAD")
    for l in log.splitlines():
        if not l or l.startswith("@@"):
            continue
        t = l.split("\t")
        s = t[0][0]
        if s == "R" and len(t) == 3:
            estado[t[2]] = estado.pop(t[1], None)
        elif s == "C" and len(t) == 3:
            if estado.get(t[1]):
                estado.setdefault(t[2], estado[t[1]])
        elif s == "A":
            estado.setdefault(t[1], None)
        elif s == "D":
            estado.pop(t[1], None)
    return {p for p, o in estado.items() if o}


def comentario_de(ruta):
    b = os.path.basename(ruta)
    if b in NOMBRES:
        return NOMBRES[b]
    return COMENTARIO.get(os.path.splitext(b)[1])


def linea_spdx(com, lic):
    if com == "/*":
        return "/* SPDX-License-Identifier: %s */" % lic
    return "%s SPDX-License-Identifier: %s" % (com, lic)


def punto_de_insercion(datos, ruta):
    """Posición (en bytes) donde va la línea: tras «#!» y la línea de codificación."""
    pos = 0
    lineas = datos.splitlines(keepends=True)
    for i, l in enumerate(lineas[:2]):
        if (i == 0 and l.startswith(b"#!")) or (ruta.endswith(".py") and re.match(rb"#.*coding[:=]", l)):
            pos += len(l)
        else:
            break
    return pos


def hay_git(raiz):
    """True si raiz es la raíz de un repositorio git (un árbol exportado no lo es)."""
    try:
        t = git(raiz, "rev-parse", "--show-toplevel").strip()
    except (subprocess.CalledProcessError, OSError):
        return False
    return os.path.realpath(t) == os.path.realpath(raiz)


def lista_de_ficheros(raiz, con_git):
    if con_git:
        return git(raiz, "ls-files").splitlines()
    r = []
    for d, ds, fs in os.walk(raiz):
        ds[:] = [x for x in ds if x not in (".git", "__pycache__")]
        r += [os.path.relpath(os.path.join(d, f), raiz).replace(os.sep, "/") for f in fs]
    return sorted(r)


def procesar(raiz, aplicar, listar):
    con_git = hay_git(raiz)
    if aplicar and not con_git:
        sys.exit("spdx.py: sin git no se puede decidir la licencia de cada fichero; usa --comprobar")
    ajenos = rutas_de_terceros(raiz)
    eli = de_origen_eli(raiz) if con_git else set()
    res = {"GPL-3.0-only": [], "GPL-2.0-or-later": [], "ya": [], "ajeno": [], "otra": []}
    for ruta in lista_de_ficheros(raiz, con_git):
        peg = ruta.startswith(PEGAMENTO)
        if (ruta.startswith(EXCLUIDOS) and not peg) or not os.path.isfile(os.path.join(raiz, ruta)):
            continue
        com = comentario_de(ruta)
        if com is None:
            continue
        if not peg and ruta not in M4R4G0N and ruta not in ROBSON and any(ruta == a or ruta.startswith(a + "/") for a in ajenos):
            continue
        with open(os.path.join(raiz, ruta), "rb") as f:
            datos = f.read()
        cab = b"\n".join(datos.splitlines()[:40])
        m = RE_SPDX.search(cab)
        if m:
            (res["ya"] if m.group(1).decode() in (LIC_ELI, LIC_NUEVA) else res["otra"]).append(ruta)
            continue
        if any(RE_COPY.search(l) and not any(p in l.lower() for p in PROPIOS) for l in cab.splitlines()):
            res["ajeno"].append(ruta)
            continue
        lic = LIC_ELI if (ruta in eli or ruta in M4R4G0N or ruta in ROBSON or ruta.startswith(DERIVADOS_EXTRA)) else LIC_NUEVA
        res[lic].append(ruta)
        if aplicar:
            nl = b"\r\n" if b"\r\n" in datos[:2000] else b"\n"
            pos = punto_de_insercion(datos, ruta)
            nuevo = datos[:pos] + linea_spdx(com, lic).encode("ascii") + nl + datos[pos:]
            with open(os.path.join(raiz, ruta), "wb") as f:
                f.write(nuevo)
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--comprobar", action="store_true", help="solo informa, no escribe")
    ap.add_argument("--raiz", default=os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")))
    ap.add_argument("--lista", action="store_true")
    a = ap.parse_args()
    res = procesar(a.raiz, not a.comprobar, a.lista)
    verbo = "irían a" if a.comprobar else "marcados"
    print("ficheros propios %s GPL-3.0-only (existían en %s o derivan de ellos): %d" % (verbo, BASE, len(res["GPL-3.0-only"])))
    print("ficheros propios %s GPL-2.0-or-later (nuevos del fork): %d" % (verbo, len(res["GPL-2.0-or-later"])))
    print("ya marcados: %d; con otra SPDX (revisar): %d; con copyright ajeno (no se tocan, revisar): %d"
          % (len(res["ya"]), len(res["otra"]), len(res["ajeno"])))
    for k in ("otra", "ajeno"):
        for r in res[k]:
            print("  dudoso (%s): %s" % (k, r))
    if a.lista:
        for k in ("GPL-3.0-only", "GPL-2.0-or-later"):
            for r in res[k]:
                print("%s %s" % (k, r))
    # con --comprobar el código de salida dice si falta alguna marca
    return 1 if a.comprobar and (res["GPL-3.0-only"] or res["GPL-2.0-or-later"]) else 0


sys.exit(main())
