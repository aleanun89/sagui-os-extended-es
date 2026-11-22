#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Manuales de lenguajes (v8.5.6, D456): prueba en el PC, sin QEMU.

Comprueba la carpeta rootfs/ayuda/lenguajes/:
  - existen cada guía (C, Go, Python, Java, sagsh) y el índice, en español, inglés (_EN) y portugués (_PT);
  - cada fichero es UTF-8 con un primer título «# ...»;
  - los enlaces relativos (a otros ficheros de ayuda o de docs) apuntan a ficheros que existen;
  - no hay caracteres de otros alfabetos (griego, cirílico, CJK, árabe, hebreo, coreano, devanagari);
  - los bloques de código de cada traducción son idénticos a los del original (las órdenes y el código no se traducen).
Uso: python3 tests/manuales_lenguajes.py   (sale con 0 si todo bien, 1 si algo falla)
"""
import glob
import io
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(RAIZ, "rootfs", "ayuda", "lenguajes")
BASES = ["indice", "c", "go", "python", "java", "sagsh"]
SUFIJOS = ["", "_EN", "_PT"]
OTROS_ALFABETOS = re.compile(
    "[Ͱ-ϿЀ-ӿ֐-׿؀-ۿऀ-ॿ"
    "぀-ヿ㐀-鿿가-힯]")
ENLACE = re.compile(r"\]\(([^)\s]+)\)")
BLOQUE = re.compile(r"```[a-z]*\n(.*?)```", re.S)

fallos = []


def verificar(cond, texto):
    if cond:
        print("ok    " + texto)
    else:
        print("FALLO " + texto)
        fallos.append(texto)


def leer(ruta):
    with io.open(ruta, encoding="utf-8") as f:
        return f.read()


def main():
    esperados = [b + s + ".md" for b in BASES for s in SUFIJOS]
    for nombre in esperados:
        verificar(os.path.isfile(os.path.join(DIR, nombre)), "existe rootfs/ayuda/lenguajes/" + nombre)

    for ruta in sorted(glob.glob(os.path.join(DIR, "*.md"))):
        nombre = os.path.basename(ruta)
        try:
            texto = leer(ruta)
        except UnicodeDecodeError:
            verificar(False, nombre + " es UTF-8")
            continue
        verificar(True, nombre + " es UTF-8")
        lineas = texto.splitlines()
        if lineas and lineas[0].startswith("<!-- categoria"):   # cabecera de la Ayuda (D455)
            lineas = lineas[1:]
        primera = lineas[0] if lineas else ""
        verificar(primera.startswith("# ") and len(primera) > 3, nombre + " tiene un primer título «# ...»")
        otros = OTROS_ALFABETOS.findall(texto)
        verificar(not otros, nombre + " no tiene caracteres de otros alfabetos")
        for destino in ENLACE.findall(texto):
            if "://" in destino or destino.startswith(("#", "mailto:")):
                continue
            ruta_dest = os.path.normpath(os.path.join(DIR, destino.split("#")[0]))
            verificar(os.path.isfile(ruta_dest), nombre + " enlaza a " + destino)

    for b in BASES:
        original = BLOQUE.findall(leer(os.path.join(DIR, b + ".md")))
        for s in ["_EN", "_PT"]:
            traduccion = BLOQUE.findall(leer(os.path.join(DIR, b + s + ".md")))
            verificar(original == traduccion,
                      "%s%s.md tiene los mismos bloques de código que %s.md" % (b, s, b))

    if fallos:
        print("\n%d comprobación(es) fallida(s)" % len(fallos))
        return 1
    print("\nmanuales de lenguajes: todo bien")
    return 0


if __name__ == "__main__":
    sys.exit(main())
