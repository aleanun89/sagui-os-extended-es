#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""mp_qstr_agregar.py - añade a mano los qstr (MP_QSTR_xxx) de un *_sagui.c nuevo a
userland/micropython/micropython_embed/genhdr/qstrdefs.generated.h (D469).

La D034 deja el MicroPython 1.24.1 sin tocar y los genhdr se regeneran con `make -f micropython_embed.mk
MICROPYTHON_TOP=...` (hace falta el repositorio de MicroPython, que no siempre está a mano). Mientras no esté,
este guion hace lo mismo para los nombres nuevos: calcula el hash igual que py/makeqstrdata.py (djb2 de 2 bytes,
`hash = hash * 33 ^ byte`) y los inserta en la sección QDEF1 en su sitio (orden alfabético por el texto).

Uso:
  tools/mp_qstr_agregar.py [--comprobar] userland/micropython/grafico_sagui.c [...]
    --comprobar  no escribe: sale con 1 si falta algún qstr (y dice cuáles)
  tools/mp_qstr_agregar.py --autoprueba   comprueba que el hash reproduce el de las entradas ya generadas
Los símbolos codificados (_dot_, _brace_open_...) se decodifican (D479); los _0x0a_ se dejan a la herramienta oficial."""
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAB = os.path.join(RAIZ, "userland", "micropython", "micropython_embed", "genhdr", "qstrdefs.generated.h")
ENT = re.compile(r'^QDEF([01])\((MP_QSTR\w*), (\d+), (\d+), "((?:[^"\\]|\\.)*)"\)$')


def hash_qstr(s):
    h = 5381
    for b in s.encode("utf-8"):
        h = ((h * 33) ^ b) & 0xFFFFFFFF
    return (h & 0xFFFF) or 1


def leer():
    with open(CAB, encoding="utf-8") as f:
        return f.read().split("\n")


def existentes(lineas):
    ids = set()
    for l in lineas:
        m = ENT.match(l)
        if m:
            ids.add(m.group(2))
    return ids


def autoprueba():
    mal = 0
    n = 0
    for l in leer():
        m = ENT.match(l)
        if not m or "\\" in m.group(5) or m.group(1) != "1":
            continue
        n += 1
        if hash_qstr(m.group(5)) != int(m.group(3)) or len(m.group(5).encode()) != int(m.group(4)):
            mal += 1
            print("no coincide:", l)
    print("autoprueba: %d entradas, %d distintas" % (n, mal))
    return 1 if mal or not n else 0


# Caracteres especiales: makeqstrdata.py escribe cada uno como «_nombre_» (D479: para los de ulab, «ulab.numpy»).
CODIGOS = {"space": " ", "squot": "'", "comma": ",", "dot": ".", "colon": ":", "semicolon": ";", "slash": "/",
           "percent": "%", "hash": "#", "paren_open": "(", "paren_close": ")", "bracket_open": "[",
           "bracket_close": "]", "brace_open": "{", "brace_close": "}", "star": "*", "bang": "!",
           "backslash": "\\", "plus": "+", "dollar": "$", "equals": "=", "question": "?", "at_sign": "@",
           "caret": "^", "pipe": "|", "tilde": "~", "hyphen": "-", "lt": "<", "gt": ">", "amp": "&",
           "quot": '"'}


def texto_de(nombre):
    """Inversa del nombre MP_QSTR_xxx -> texto del qstr (cada carácter especial va como _nombre_)."""
    sal = []
    i = 0
    while i < len(nombre):
        if nombre[i] == "_":
            for cod in sorted(CODIGOS, key=len, reverse=True):
                if nombre.startswith(cod + "_", i + 1):
                    sal.append(CODIGOS[cod])
                    i += len(cod) + 2
                    break
            else:
                sal.append("_")
                i += 1
                continue
            continue
        sal.append(nombre[i])
        i += 1
    return "".join(sal)


def usados(rutas):
    nombres = set()
    for r in rutas:
        with open(r, encoding="utf-8") as f:
            t = f.read()
        t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
        t = re.sub(r"//[^\n]*", "", t)
        nombres.update(re.findall(r"MP_QSTR_(\w+)", t))
    return nombres


def main(arg):
    if "--autoprueba" in arg:
        return autoprueba()
    comprobar = "--comprobar" in arg
    rutas = [a for a in arg if not a.startswith("--")]
    if not rutas:
        print(__doc__)
        return 2
    lineas = leer()
    ya = existentes(lineas)
    nuevos = sorted(n for n in usados(rutas) if "MP_QSTR_" + n not in ya and re.fullmatch(r"[A-Za-z0-9_]*", n) and n)
    if not nuevos:
        print("qstr: nada que añadir")
        return 0
    if comprobar:
        print("faltan qstr: " + " ".join(nuevos))
        return 1
    # sección QDEF1: desde la primera línea QDEF1 hasta la última
    idx = [i for i, l in enumerate(lineas) if l.startswith("QDEF1(")]
    ini, fin = idx[0], idx[-1]
    sec = lineas[ini:fin + 1]
    for n in nuevos:
        t = texto_de(n)
        nueva = 'QDEF1(MP_QSTR_%s, %d, %d, "%s")' % (n, hash_qstr(t), len(t.encode()), t.replace("\\", "\\\\").replace('"', '\\"'))
        pos = len(sec)
        for i, l in enumerate(sec):
            m = ENT.match(l)
            if m and m.group(5) > t:
                pos = i
                break
        sec.insert(pos, nueva)
    lineas[ini:fin + 1] = sec
    with open(CAB, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lineas))
    print("qstr: añadidos %d: %s" % (len(nuevos), " ".join(nuevos)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
