#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Genera las referencias de h205 (PDF reales) con MuPDF.

Uso (MuPDF no está en el PC de desarrollo: se instala en un entorno aparte):

    python3 -m venv /tmp/venv-pdf && /tmp/venv-pdf/bin/pip install pymupdf
    /tmp/venv-pdf/bin/python tools/pdf_referencias.py

Escribe tests/recursos/pdf/ref/<nombre>-p<página>.pgm.gz: la página dibujada
por MuPDF al mismo zoom que la prueba y reducida a la rejilla de grises de
tests/pdf_reales_comum.py. Solo hace falta volver a generarlas si se cambia la
lista de PDF o la rejilla.
"""
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, "..", "tests"))
import pdf_reales_comum as comum  # noqa: E402

try:
    import pymupdf
except ImportError:
    import fitz as pymupdf  # versiones antiguas


def main():
    os.makedirs(os.path.join(comum.RECURSOS, "ref"), exist_ok=True)
    for nome, pagina, _n, _desc in comum.PDFS:
        doc = pymupdf.open(os.path.join(comum.RECURSOS, nome))
        pg = doc[pagina - 1]
        # El mismo tamaño que visorpdf: los puntos enteros por el zoom.
        larg = int(pg.rect.width) * comum.ZOOM // 100
        alt = int(pg.rect.height) * comum.ZOOM // 100
        mat = pymupdf.Matrix(larg / pg.rect.width, alt / pg.rect.height)
        pix = pg.get_pixmap(matrix=mat, alpha=False, colorspace=pymupdf.csRGB)
        gris = comum.gris_de_rgb(pix.width, pix.height, pix.samples)
        ancho, alto, val = comum.rejilla(pix.width, pix.height, gris)
        destino = comum.referencia(nome, pagina)
        comum.gravar_pgm_gz(destino, ancho, alto, val)
        print("%s: %dx%d -> %dx%d (tinta %.3f)" % (destino, pix.width, pix.height,
                                                   ancho, alto, comum.tinta(val)))


if __name__ == "__main__":
    main()
