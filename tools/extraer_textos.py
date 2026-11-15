#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""extraer_textos.py - catálogos de traducción de SaguiOS (D313, mejora 118).

La cadena en español del código es la clave: _("texto") se traduce en el momento y N_("texto")
solo marca la cadena (tablas estáticas, que se traducen al usarlas). Esta herramienta:

  extraer              busca _() y N_() en las fuentes y deja idiomas/plantilla/<catalogo>.pot
                       y pone al día idiomas/<idioma>/<catalogo>.po (añade lo nuevo con msgstr
                       vacío, conserva lo traducido, quita lo que ya no está en el código)
  compilar SALIDA      genera SALIDA/<idioma>/<catalogo>.cat (formato de kernel/inc/catalogo.h)
                       desde los .po, y el pseudo-idioma «xx» desde las fuentes
  comprobar CAT...     lista lo que falta traducir (msgstr vacío o «fuzzy») en cada idioma de
                       los catálogos dados; sale con 1 si hay algo
  estadisticas         cuántas cadenas y cuántas traducidas hay por idioma y catálogo

Lector propio, sin código de GNU gettext. Los .po son los estándar (UTF-8, editables con Poedit o
Weblate); los .cat guardan los textos en Latin-1, que es lo que el compilador pone en el
programa (-fexec-charset=ISO-8859-1).

Catálogo de cada fuente: userland/programas/X.c -> X; userland/libsagui y userland/inc ->
libsagui (los textos comunes: diálogo de archivos, componentes); kernel/ -> kernel.
"""
import os
import re
import struct
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR_IDIOMAS = os.path.join(RAIZ, "idiomas")
# Dónde se buscan las marcas (no se recorre kernel/terceros ni los puertos).
BUSCAR = ["kernel/gui", "kernel/dispositivos/acpi.c", "kernel/nucleo/supervisor.c", "kernel/nucleo/energia.c", "userland/programas", "userland/libsagui", "userland/inc"]
# Fuentes que no están en UTF-8 (AGENTS.md, «Codificación de los ficheros»).
LATIN1 = {"userland/programas/arquivos.c", "userland/programas/cipo.c"}
IDIOMAS = ["pt_BR", "en"]          # los que se mantienen con .po; «es» es el original y «xx» el de pruebas
PSEUDO = "xx"
MAGIA = 0x54434753


def catalogo_de(ruta):
    if ruta.startswith("kernel/"):
        return "kernel"
    if ruta.startswith("userland/programas/"):
        return os.path.splitext(os.path.basename(ruta))[0]
    return "libsagui"


def hash_fnv(b):
    h = 2166136261
    for c in b:
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


# ------------------------------------------------------------------ extracción de las fuentes

ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "a": "\a", "b": "\b", "f": "\f", "v": "\v",
           "\\": "\\", '"': '"', "'": "'", "?": "?"}


def leer_literal(t, i):
    """t[i] es una comilla; devuelve (texto, posición tras la comilla de cierre) o None."""
    j = i + 1
    out = []
    while j < len(t):
        c = t[j]
        if c == '"':
            return "".join(out), j + 1
        if c == "\n":
            return None
        if c == "\\":
            j += 1
            e = t[j]
            if e in ESCAPES:
                out.append(ESCAPES[e]); j += 1
            elif e == "x":
                k = j + 1
                while k < len(t) and t[k] in "0123456789abcdefABCDEF":
                    k += 1
                out.append(chr(int(t[j + 1:k], 16))); j = k
            elif e in "01234567":
                k = j
                while k < len(t) and k < j + 3 and t[k] in "01234567":
                    k += 1
                out.append(chr(int(t[j:k], 8))); j = k
            elif e == "\n":
                j += 1
            else:
                out.append(e); j += 1
        else:
            out.append(c); j += 1
    return None


def saltar_espacios(t, i):
    while i < len(t):
        if t[i] in " \t\r\n":
            i += 1
        elif t.startswith("/*", i):
            j = t.find("*/", i + 2)
            i = len(t) if j < 0 else j + 2
        elif t.startswith("//", i):
            j = t.find("\n", i)
            i = len(t) if j < 0 else j
        else:
            break
    return i


def extraer_fuente(ruta):
    """Devuelve [(cadena, linea)] de las marcas _("...") y N_("...") de un archivo."""
    codif = "latin-1" if ruta in LATIN1 else "utf-8"
    with open(os.path.join(RAIZ, ruta), encoding=codif, errors="strict") as f:
        t = f.read()
    res = []
    i, n = 0, len(t)
    while i < n:
        c = t[i]
        if c == "/" and t.startswith("/*", i):
            j = t.find("*/", i + 2); i = n if j < 0 else j + 2
        elif c == "/" and t.startswith("//", i):
            j = t.find("\n", i); i = n if j < 0 else j
        elif c == '"':
            r = leer_literal(t, i)
            i = r[1] if r else i + 1
        elif c == "'":
            j = i + 1
            while j < n and t[j] != "'" and t[j] != "\n":
                j += 2 if t[j] == "\\" else 1
            i = j + 1
        elif c == "#" and (i == 0 or t[i - 1] == "\n"):
            # preprocesador: se salta la línea (con continuaciones), salvo #define de _() y N_()
            j = i
            while j < n and not (t[j] == "\n" and t[j - 1] != "\\"):
                j += 1
            i = j
        elif c == "_" or c == "N":
            ini = i
            ident = "N_" if t.startswith("N_", i) else "_" if c == "_" else None
            antes = t[i - 1] if i else " "
            if ident and not (antes.isalnum() or antes == "_"):
                k = i + len(ident)
                despues = t[k:k + 1]
                k = saltar_espacios(t, k)
                if t[k:k + 1] == "(":
                    k = saltar_espacios(t, k + 1)
                    partes, ok = [], False
                    while t[k:k + 1] == '"':
                        r = leer_literal(t, k)
                        if not r:
                            partes = []; break
                        partes.append(r[0]); k = saltar_espacios(t, r[1])
                        ok = True
                    if ok and partes and t[k:k + 1] == ")":
                        cad = "".join(partes)
                        if codif == "latin-1":
                            # fuente mezclada (AGENTS.md): lo que es UTF-8 válido el compilador lo pasa a Latin-1 y
                            # lo que no, lo deja como está; aquí los bytes vienen como caracteres Latin-1
                            try:
                                cad = cad.encode("latin-1").decode("utf-8")
                            except UnicodeDecodeError:
                                pass
                        res.append((cad, t.count("\n", 0, ini) + 1))
                    i = k
                    continue
            i = ini + 1
            while i < n and (t[i].isalnum() or t[i] == "_"):
                i += 1
        else:
            i += 1
    return res


def listar_fuentes():
    for d in BUSCAR:
        if os.path.isfile(os.path.join(RAIZ, d)):
            yield d
            continue
        for raiz, _, fich in os.walk(os.path.join(RAIZ, d)):
            for f in sorted(fich):
                if f.endswith((".c", ".h")):
                    yield os.path.relpath(os.path.join(raiz, f), RAIZ)


def extraer_todo():
    """{catalogo: {cadena: [referencias]}} con el orden de aparición."""
    cats = {}
    for ruta in sorted(listar_fuentes()):
        try:
            marcas = extraer_fuente(ruta)
        except UnicodeDecodeError as e:
            print("extraer_textos: %s no es %s válido: %s" % (ruta, "Latin-1" if ruta in LATIN1 else "UTF-8", e), file=sys.stderr)
            sys.exit(2)
        for cad, lin in marcas:
            if not re.search(r"[A-Za-z\u00c0-\u00ff]", cad):
                continue      # solo símbolos o números: nada que traducir
            cats.setdefault(catalogo_de(ruta), {}).setdefault(cad, []).append("%s:%d" % (ruta, lin))
    return cats


# ------------------------------------------------------------------ ficheros .po

def po_escapar(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r")


def po_cadena(s):
    """Una cadena larga se parte en líneas tras cada \\n, como hace xgettext."""
    trozos = re.findall(r"[^\n]*\n|[^\n]+", s) or [""]
    if len(trozos) == 1:
        return '"%s"' % po_escapar(s)
    return '""\n' + "\n".join('"%s"' % po_escapar(x) for x in trozos)


def po_leer(ruta):
    """{msgid: (msgstr, fuzzy)} de un .po (sin la cabecera)."""
    res = {}
    if not os.path.exists(ruta):
        return res
    id_, st, modo, fuzzy, flz = None, None, None, False, False

    def cerrar():
        nonlocal id_, st, fuzzy
        if id_ is not None and st is not None and id_ != "":
            res[id_] = (st, fuzzy)
        id_, st, fuzzy = None, None, False

    with open(ruta, encoding="utf-8") as f:
        for linea in f:
            linea = linea.rstrip("\n")
            if linea.startswith("#,"):
                if id_ is not None:
                    cerrar()
                fuzzy = "fuzzy" in linea
            elif linea.startswith("#"):
                if st is not None:
                    cerrar()
            elif linea.startswith("msgid "):
                if st is not None:
                    cerrar()
                id_, modo = po_desescapar(linea[6:]), "id"
            elif linea.startswith("msgstr "):
                st, modo = po_desescapar(linea[7:]), "st"
            elif linea.startswith('"'):
                if modo == "id":
                    id_ += po_desescapar(linea)
                elif modo == "st":
                    st += po_desescapar(linea)
            elif not linea.strip():
                if st is not None:
                    cerrar()
        cerrar()
    return res


def po_desescapar(s):
    s = s.strip()
    if not (s.startswith('"') and s.endswith('"')):
        raise ValueError("línea de .po rara: " + s)
    r = leer_literal(s + "\n", 0)
    return r[0] if r else ""


def po_escribir(ruta, catalogo, idioma, cadenas, previo):
    """cadenas = {msgid: [referencias]}; previo = lo que ya había traducido."""
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    cab = ['# Traducción de SaguiOS: catálogo «%s», idioma %s (D313).' % (catalogo, idioma or "plantilla"),
           '# Los msgid son los textos en español del código: no se cambian a mano.',
           'msgid ""', 'msgstr ""',
           '"Project-Id-Version: SaguiOS-Extended\\n"',
           '"Language: %s\\n"' % (idioma or ""),
           '"MIME-Version: 1.0\\n"',
           '"Content-Type: text/plain; charset=UTF-8\\n"',
           '"Content-Transfer-Encoding: 8bit\\n"', ""]
    out = cab
    for cad, refs in cadenas.items():
        out.append("#: " + " ".join(refs[:4]))
        if re.search(r"%[-0-9.]*[a-zA-Z]", cad):
            out.append("#, c-format")
        out.append("msgid " + po_cadena(cad))
        pr = previo.get(cad)
        out.append("msgstr " + po_cadena(pr[0] if pr and not pr[1] and idioma else ""))
        out.append("")
    with open(ruta, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")


def comando_extraer():
    cats = extraer_todo()
    # quita .po de catálogos que ya no existen
    for sub in [""] + IDIOMAS:
        d = os.path.join(DIR_IDIOMAS, sub or "plantilla")
        if os.path.isdir(d):
            for f in os.listdir(d):
                nombre = os.path.splitext(f)[0]
                if f.endswith((".po", ".pot")) and nombre not in cats:
                    os.remove(os.path.join(d, f))
    total = 0
    for cat, cadenas in sorted(cats.items()):
        po_escribir(os.path.join(DIR_IDIOMAS, "plantilla", cat + ".pot"), cat, None, cadenas, {})
        for idi in IDIOMAS:
            ruta = os.path.join(DIR_IDIOMAS, idi, cat + ".po")
            po_escribir(ruta, cat, idi, cadenas, po_leer(ruta))
        total += len(cadenas)
    print("extraer_textos: %d cadenas en %d catálogos" % (total, len(cats)))


# ------------------------------------------------------------------ catálogos binarios

def latin1(s, donde):
    try:
        return s.encode("latin-1") + b"\0"
    except UnicodeEncodeError as e:
        print("extraer_textos: %s: carácter que no cabe en Latin-1 (%s): %r" % (donde, e.reason, s), file=sys.stderr)
        sys.exit(2)


def escribir_cat(ruta, pares, donde):
    """pares = [(clave, traduccion)] -> archivo .cat."""
    ent = []
    for k, t in pares:
        kb = latin1(k, donde)
        ent.append((hash_fnv(kb[:-1]), kb, latin1(t, donde)))
    ent.sort(key=lambda e: (e[0], e[1]))
    base = 16 + 12 * len(ent)
    pool = bytearray()
    tabla = bytearray()
    for h, kb, tb in ent:
        ok = base + len(pool)
        pool += kb
        ot = base + len(pool)
        pool += tb
        tabla += struct.pack("<III", h, ok, ot)
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    with open(ruta, "wb") as f:
        f.write(struct.pack("<IIII", MAGIA, 1, len(ent), 0) + bytes(tabla) + bytes(pool))


def pseudo(cad):
    """«[Guardar...~~]»: entre corchetes y un 30 % más largo, sin tocar %d, \\n ni el atajo tras el \\t."""
    cab, tab, resto = cad.partition("\t")
    m = re.match(r"^(.*?)(\s*)$", cab, re.S)
    nucleo, cola = m.group(1), m.group(2)
    # las marcas de menú del kernel (>, <, -, *) y las líneas solo de símbolos no se tocan
    extra = max(1, (len(nucleo) * 3 + 9) // 10)
    return "[" + nucleo + "~" * extra + "]" + cola + tab + resto


def comando_compilar(salida):
    cats = extraer_todo()
    n = 0
    for cat, cadenas in cats.items():
        escribir_cat(os.path.join(salida, PSEUDO, cat + ".cat"), [(c, pseudo(c)) for c in cadenas], cat)
        n += 1
        for idi in IDIOMAS:
            po = po_leer(os.path.join(DIR_IDIOMAS, idi, cat + ".po"))
            pares = [(c, po[c][0]) for c in cadenas if c in po and po[c][0] and not po[c][1]]
            escribir_cat(os.path.join(salida, idi, cat + ".cat"), pares, "%s/%s" % (idi, cat))
            n += 1
    print("extraer_textos: %d catálogos en %s" % (n, salida))


def faltan(cat, idi):
    cats = extraer_todo()
    cadenas = cats.get(cat, {})
    po = po_leer(os.path.join(DIR_IDIOMAS, idi, cat + ".po"))
    return [c for c in cadenas if c not in po or not po[c][0] or po[c][1]]


def comando_comprobar(nombres):
    cats = extraer_todo()
    mal = 0
    for cat in nombres:
        if cat not in cats:
            print("comprobar: el catálogo «%s» no tiene ninguna cadena marcada" % cat)
            mal += 1
            continue
        for idi in IDIOMAS:
            po = po_leer(os.path.join(DIR_IDIOMAS, idi, cat + ".po"))
            for c in cats[cat]:
                if c not in po or not po[c][0] or po[c][1]:
                    print("sin traducir [%s/%s]: %s" % (idi, cat, c.replace("\n", "\\n")))
                    mal += 1
    print("comprobar: %d cadenas sin traducir" % mal)
    return 1 if mal else 0


def comando_estadisticas():
    cats = extraer_todo()
    for cat, cadenas in sorted(cats.items()):
        linea = "%-16s %5d" % (cat, len(cadenas))
        for idi in IDIOMAS:
            po = po_leer(os.path.join(DIR_IDIOMAS, idi, cat + ".po"))
            ok = sum(1 for c in cadenas if c in po and po[c][0] and not po[c][1])
            linea += "   %s %d/%d" % (idi, ok, len(cadenas))
        print(linea)


def main(a):
    if len(a) >= 1 and a[0] == "extraer":
        comando_extraer()
    elif len(a) == 2 and a[0] == "compilar":
        comando_compilar(a[1])
    elif len(a) >= 2 and a[0] == "comprobar":
        return comando_comprobar(a[1:])
    elif a[:1] == ["estadisticas"]:
        comando_estadisticas()
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
