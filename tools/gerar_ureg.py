#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Genera kernel/terceros/bsd/ure/ureg.h (D433): los #define de registros del
RTL8152/8153/8156 de OpenBSD (sys/dev/usb/if_urereg.h, ISC/BSD de dos clausulas).

Uso: gerar_ureg.py <if_urereg.h> [salida]

Conserva el aviso de copyright y el texto de la licencia de la cabecera original
y copia sin cambios todos sus #define (con sus lineas de continuacion). No copia
las estructuras ni el firmware (solo los 8157/8159, que no entran aqui)."""
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    src = open(sys.argv[1], encoding="utf-8", errors="replace").read().split("\n")
    salida = sys.argv[2] if len(sys.argv) > 2 else os.path.join(RAIZ, "kernel", "terceros", "bsd", "ure", "ureg.h")

    # la cabecera de licencia: desde el primer /*- hasta el cierre */ del bloque inicial
    ini = next(i for i, l in enumerate(src) if l.startswith("/*"))           # la linea $OpenBSD$
    ref = next(i for i, l in enumerate(src) if l.startswith("/*-"))          # el bloque de licencia
    fin = next(i for i in range(ref, len(src)) if src[i].rstrip().endswith("*/"))
    licencia = src[ini:fin + 1]
    if not any("Kevin Lo" in l for l in licencia):
        raise SystemExit("ERROR: no encuentro el copyright de Kevin Lo en la cabecera")

    defs = []
    i = fin + 1
    while i < len(src):
        l = src[i]
        if re.match(r"#define\s+\w", l):
            bloque = [l]
            while bloque[-1].rstrip().endswith("\\") and i + 1 < len(src):
                i += 1
                bloque.append(src[i])
            defs.append("\n".join(bloque))
        i += 1

    cab = [
        "// SPDX-License-Identifier: BSD-2-Clause",
        "/* kernel/terceros/bsd/ure/ureg.h - registros del Realtek RTL8152/8153/8156 (D433).",
        " * Copia sin cambios de los #define de OpenBSD sys/dev/usb/if_urereg.h (GENERADO por",
        " * tools/gerar_ureg.py; no editar a mano). El aviso de copyright y la licencia originales",
        " * van abajo, tal como estaban en el archivo. */",
        "",
    ]
    texto = "\n".join(cab + licencia + [
        "",
        "/* ---- #define de registros (extraidos de if_urereg.h) ---- */",
        "#ifndef SAGUI_URE_UREG_H",
        "#define SAGUI_URE_UREG_H",
        "",
    ] + defs + ["", "#endif", ""])
    with open(salida, "w", encoding="utf-8", newline="\n") as f:
        f.write(texto)
    print("escrito %s: %d #define" % (salida, len(defs)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
