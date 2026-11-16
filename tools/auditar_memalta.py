#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""auditar_memalta.py - busca en el kernel sitios que usan como puntero la
dirección FÍSICA de una página de programa (D348).

Desde la D348 las páginas de los programas pueden estar en la memoria alta
(por encima de 0x78000000), que el kernel no ve por su dirección: convertir
esa dirección en puntero escribe en las ventanas de dispositivos o en el
programa que esté activo. Hay que pasar por fisica_mapear()/fisica_soltar()
o fisica_escrever()/fisica_ler()/fisica_zerar() (kernel/inc/memalta.h).

Busca:
  1. un molde a puntero aplicado directamente a espaco_traduzir(),
     espaco_tirar() o memfisica_alocar_usuario();
  2. una variable que recibe el resultado de una de ellas y que, en las
     líneas siguientes de la misma función, se usa como puntero (molde,
     memcpy/memset/memmove con ella como dirección).

  3. fuera de kernel/mem/paginacao.c, un molde a puntero de una expresión con
     MASCARA_END (la dirección física sacada de una entrada de tabla de páginas).

Es una RED, NO UNA GARANTÍA: mira texto línea a línea, no sigue la dirección
física a través de llamadas, estructuras ni otras funciones (por ejemplo, una
función que la recibe como parámetro y la usa como puntero). Un envoltorio
nuevo que devuelva direcciones físicas de páginas de programa debe añadirse a
FUNCOES. La garantía la dan las pruebas con memoria alta (h350) y la regla de
memalta.h.

Sale con 1 si encuentra algo. Uso: python3 tools/auditar_memalta.py [-v]
"""
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Las que devuelven la dirección física de una página de programa, y sus envoltorios.
FUNCOES = r"(?:espaco_traduzir|espaco_tirar|memfisica_alocar_usuario|palavra_fisica|pagina_usuario_zerada)"
MOLDE = r"\(\s*(?:const\s+)?(?:volatile\s+)?[A-Za-z_]\w*\s*\*+\s*\)"
DIRETO = re.compile(MOLDE + r"\s*" + FUNCOES + r"\s*\(")
ATRIB = re.compile(r"\b([A-Za-z_]\w*)\s*=\s*" + FUNCOES + r"\s*\(")
# Moldes a puntero de algo enmascarado con MASCARA_END (dirección física de una entrada).
MASCARA = re.compile(MOLDE + r"\s*\([^;]*\bMASCARA_END\b")
# Ficheros que pueden hacerlo: el propio mecanismo de las ventanas.
PERMITIDOS = {"kernel/mem/memalta.c"}
# Las tablas de páginas están en la zona baja: paginacao.c las lee 1:1 con MASCARA_END.
MASCARA_PERMITIDOS = {"kernel/mem/paginacao.c"}


def uso_como_ponteiro(var):
    return re.compile(MOLDE + r"\s*\(?\s*" + re.escape(var) + r"\b"
                      r"|\bmem(?:cpy|set|move)\s*\(\s*" + re.escape(var) + r"\b"
                      r"|\bmemcpy\s*\([^,]+,\s*\(?\s*(?:const\s+)?(?:void\s*\*\s*\))?\s*" + re.escape(var) + r"\b")


def auditar(verboso=False):
    achados = []
    for base, dirs, arqs in os.walk(os.path.join(RAIZ, "kernel")):
        if "terceros" in base.split(os.sep):
            continue
        for a in sorted(arqs):
            if not a.endswith(".c"):
                continue
            cam = os.path.join(base, a)
            rel = os.path.relpath(cam, RAIZ)
            if rel in PERMITIDOS:
                continue
            linhas = open(cam, encoding="utf-8", errors="replace").read().split("\n")
            for i, l in enumerate(linhas):
                if rel not in MASCARA_PERMITIDOS and MASCARA.search(l):
                    achados.append(f"{rel}:{i + 1}: molde a puntero de una dirección de MASCARA_END: {l.strip()}")
                if DIRETO.search(l):
                    achados.append(f"{rel}:{i + 1}: molde a puntero de una dirección física de programa: {l.strip()}")
                m = ATRIB.search(l)
                if not m:
                    continue
                var = m.group(1)
                padrao = uso_como_ponteiro(var)
                for j in range(i + 1, min(i + 40, len(linhas))):
                    if linhas[j].startswith("}"):
                        break   # fin de la función
                    if re.search(r"\b" + re.escape(var) + r"\s*=[^=]", linhas[j]) and not padrao.search(linhas[j]):
                        break   # la variable recibe otra cosa
                    if padrao.search(linhas[j]):
                        achados.append(f"{rel}:{j + 1}: '{var}' (física, de la línea {i + 1}) usada como puntero: "
                                       f"{linhas[j].strip()}")
                if verboso:
                    print(f"  {rel}:{i + 1}: {var} = {m.group(0)}...")
    return achados


def main():
    verboso = "-v" in sys.argv
    achados = auditar(verboso)
    for a in achados:
        print(a)
    print(f"auditar_memalta: {len(achados)} sitio(s) sospechoso(s)")
    return 1 if achados else 0


if __name__ == "__main__":
    sys.exit(main())
