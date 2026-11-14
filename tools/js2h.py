#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""js2h.py - convierte un .js en una cadena C (para embeberlo en un programa).

Uso: js2h.py entrada.js salida.h NOMBRE
Genera `static const char NOMBRE[] = "...";` (UTF-8, una línea de C por línea de JS)."""
import sys

src = open(sys.argv[1], encoding="utf-8").read()
lineas = []
for l in src.split("\n"):
    l = l.replace("\\", "\\\\").replace('"', '\\"')
    lineas.append('"' + l + '\\n"')
open(sys.argv[2], "w", encoding="utf-8").write(
    "/* generado por tools/js2h.py: no editar */\nstatic const char %s[] =\n%s;\n" % (sys.argv[3], "\n".join(lineas)))
