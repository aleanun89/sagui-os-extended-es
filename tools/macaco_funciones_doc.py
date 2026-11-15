#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Genera la lista de funciones de Macaco en Markdown a partir del catálogo único
(userland/programas/macaco_funciones.h): compila un programa pequeño con el compilador del PC que
imprime la tabla. Con --escribir sustituye el bloque entre las marcas del catálogo de docs/macaco.md.

  python3 tools/macaco_funciones_doc.py              # imprime el Markdown
  python3 tools/macaco_funciones_doc.py --escribir   # actualiza docs/macaco.md
"""
import os
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C = r'''
#include <stdio.h>
#include "macaco_funciones.h"
int main(void)
{
    for (int c = 0; c < FXC_N; c++) {
        printf("\n### %s\n\n| Función | Inglés | Sintaxis | Descripción | Ejemplo |\n|---|---|---|---|---|\n", FX_CATS[c]);
        for (int i = 0; i < FX_N; i++) {
            const fx_t *f = &FX_TABELA[i];
            if (f->cat != c) continue;
            fx_arg_t da[8];
            int nd = fx_args_ler(f, da, 8);
            printf("| `%s` | `%s` | `%s(", f->nome, f->alias ? f->alias : "", f->nome);
            for (int k = 0; k < nd && k < 8; k++) {
                printf("%s%s%s%s", k ? "; " : "", da[k].opcional ? "[" : "", da[k].nome, da[k].opcional ? "]" : "");
                if (da[k].repite) printf("; ...");
            }
            printf(")` | %s | `%s` |\n", f->desc, f->ejemplo);
        }
    }
    printf("\n%d funciones.\n", FX_N);
    return 0;
}
'''
INI = "<!-- catalogo-funciones:inicio -->"
FIN = "<!-- catalogo-funciones:fin -->"


def generar():
    with tempfile.TemporaryDirectory() as d:
        c = os.path.join(d, "doc.c")
        exe = os.path.join(d, "doc")
        with open(c, "w", encoding="utf-8") as f:
            f.write(C)
        subprocess.run(["gcc", "-std=gnu11", "-I", os.path.join(RAIZ, "userland", "programas"), "-o", exe, c],
                       check=True, env={**os.environ, "CPATH": os.environ.get("HOST_CPATH", "")})
        return subprocess.run([exe], check=True, capture_output=True, text=True).stdout


def principal():
    md = generar()
    if "--escribir" not in sys.argv:
        print(md)
        return 0
    ruta = os.path.join(RAIZ, "docs", "macaco.md")
    with open(ruta, encoding="utf-8") as f:
        t = f.read()
    a, b = t.index(INI) + len(INI), t.index(FIN)
    with open(ruta, "w", encoding="utf-8") as f:
        f.write(t[:a] + "\n" + md + t[b:])
    return 0


if __name__ == "__main__":
    sys.exit(principal())
