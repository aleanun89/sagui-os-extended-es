#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Genera userland/llm/llm_unicode.h (D380): los rangos de letras (\\p{L}) y números
(\\p{N}) de Unicode que el pretokenizador del motor `llm` necesita, sacados de la tabla
de Unicode que trae Python (unicodedata). Es un dato, no código de terceros.

Uso: python3 tools/llm_unicode.py [salida]"""
import os
import sys
import unicodedata

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def rangos(pred):
    r, ini = [], None
    for c in range(0x110000):
        ok = pred(c)
        if ok and ini is None:
            ini = c
        elif not ok and ini is not None:
            r.append((ini, c - 1))
            ini = None
    if ini is not None:
        r.append((ini, 0x10FFFF))
    return r


def main():
    saida = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "userland", "llm", "llm_unicode.h")
    letras = rangos(lambda c: unicodedata.category(chr(c)).startswith("L"))
    numeros = rangos(lambda c: unicodedata.category(chr(c)).startswith("N"))
    with open(saida, "w", encoding="utf-8") as f:
        f.write("// SPDX-License-Identifier: GPL-3.0-or-later\n")
        f.write("/* llm_unicode.h - GENERADO por tools/llm_unicode.py (Unicode %s): rangos de\n"
                " * letras (\\p{L}) y números (\\p{N}) para el pretokenizador. No editar a mano. */\n"
                % unicodedata.unidata_version)
        for nome, rr in (("uni_letras", letras), ("uni_numeros", numeros)):
            f.write("static const uint32_t %s[][2] = {\n" % nome)
            for i in range(0, len(rr), 4):
                f.write("    " + " ".join("{0x%X, 0x%X}," % x for x in rr[i:i + 4]) + "\n")
            f.write("};\n")
    print("%s: %d rangos de letras, %d de números" % (saida, len(letras), len(numeros)))


if __name__ == "__main__":
    main()
