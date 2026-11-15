#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Genera userland/libsagui/pdf_metricas.h: anchos (1/1000 de em) de las 14 fuentes
estandar de PDF (Helvetica, Times y Courier) para los codigos 32..255 en WinAnsi.

Los anchos son los de las AFM de Adobe (datos publicos, ver docs/terceros.md). Se leen
de las tablas _fontdata_widths_* que trae reportlab (solo hace falta al regenerar el
archivo, no para compilar):

    python3 tools/gerar_metricas_pdf.py <carpeta reportlab/pdfbase> > userland/libsagui/pdf_metricas.h
"""
import sys, os, importlib.util

def cargar(ruta):
    spec = importlib.util.spec_from_file_location("m", ruta)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

d = sys.argv[1]
nomes = ["helvetica", "helveticabold", "timesroman", "timesbold", "timesitalic", "timesbolditalic", "courier"]
etiq = ["Helvetica", "Helvetica-Bold", "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic", "Courier"]
win = cargar(os.path.join(d, "_fontdata_enc_winansi.py")).WinAnsiEncoding
print("// SPDX-License-Identifier: GPL-2.0-or-later")
print("/* pdf_metricas.h - GENERADO por tools/gerar_metricas_pdf.py; no editar a mano.")
print(" * Anchos (1/1000 de em) de las fuentes estandar de PDF para los codigos 32..255")
print(" * (WinAnsi = Latin-1 en 32..126 y 160..255). Datos de las AFM de Adobe (docs/terceros.md).")
print(" * Las variantes Oblique/Italic de Helvetica y Courier usan los anchos de la recta. */")
print("#ifndef SAGUI_PDF_METRICAS_H\n#define SAGUI_PDF_METRICAS_H\n#include "sagui.h"\n")
print("static const int16_t pdf_larguras[7][224] = {")
for n, e in zip(nomes, etiq):
    w = cargar(os.path.join(d, "_fontdata_widths_%s.py" % n)).widths
    fila = []
    for c in range(32, 256):
        g = win[c] if c < len(win) else None
        fila.append(w.get(g, 0) if g else 0)
    print("    /* %s */ {" % e)
    for i in range(0, 224, 16):
        print("        " + ", ".join("%d" % v for v in fila[i:i+16]) + ",")
    print("    },")
print("};\n#endif")
