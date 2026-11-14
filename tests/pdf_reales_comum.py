# SPDX-License-Identifier: GPL-2.0-or-later
"""Lo que comparten h205 (PDF reales) y tools/pdf_referencias.py.

Los PDF reales de tests/recursos/pdf/ se comparan con una referencia
dibujada por MuPDF (otro lector, no el nuestro). Para no guardar imágenes
grandes ni depender de PIL, la página se reduce a una rejilla de grises de
ANCHO_REJILLA columnas (media de cada celda) y se guarda como PGM (P5)
comprimido con gzip. La reducción también hace de desenfoque: las pequeñas
diferencias de suavizado entre los dos lectores casi no cuentan, pero una
página en blanco, el texto fuera de sitio o la basura sí.
"""
import gzip
import os

AQUI = os.path.dirname(os.path.abspath(__file__))
RECURSOS = os.path.join(AQUI, "recursos", "pdf")
ANCHO_REJILLA = 120
ZOOM = 50          # el zoom del render (visorpdf --ppm ... pagina 50)

# Los PDF de la prueba: (archivo, página, páginas que tiene, de dónde viene).
# Ver tests/recursos/pdf/LEEME para el origen y la licencia de cada uno.
PDFS = [
    ("dummy.pdf", 1, 1, "OpenOffice Writer: TrueType incrustada"),
    ("w4.pdf", 1, 5, "Adobe Designer: CFF CID Identity-H, Type1C, xref en flujo"),
    ("enumitem.pdf", 1, 24, "pdfTeX: Type 1 en subconjunto, ObjStm"),
    ("rfc9116.pdf", 1, 21, "cairo: TrueType y CIDFontType2"),
    ("naca.pdf", 2, 2, "escaneado: CCITT grupo 4"),
    ("nist.pdf", 1, 2, "Word/Acrobat: JPEG con SMask, Indexed"),
]


def gris_de_rgb(larg, alt, px):
    """Los píxeles RGB (bytes) a una lista de grises."""
    return [(px[i] * 299 + px[i + 1] * 587 + px[i + 2] * 114) // 1000
            for i in range(0, larg * alt * 3, 3)]


def rejilla(larg, alt, gris, ancho=ANCHO_REJILLA, alto=None):
    """Reduce la imagen a ancho x alto celdas (media de cada celda)."""
    if alto is None:
        alto = max(1, round(ancho * alt / larg))
    soma = [0.0] * (ancho * alto)
    cont = [0] * (ancho * alto)
    for y in range(alt):
        cy = min(alto - 1, y * alto // alt)
        base = y * larg
        fila = cy * ancho
        for x in range(larg):
            k = fila + min(ancho - 1, x * ancho // larg)
            soma[k] += gris[base + x]
            cont[k] += 1
    return ancho, alto, [int(soma[i] / cont[i] + 0.5) if cont[i] else 255 for i in range(ancho * alto)]


def gravar_pgm_gz(caminho, ancho, alto, valores):
    cab = b"P5\n%d %d\n255\n" % (ancho, alto)
    with gzip.open(caminho, "wb") as f:
        f.write(cab + bytes(valores))


def ler_pgm_gz(caminho):
    with gzip.open(caminho, "rb") as f:
        dados = f.read()
    partes = dados.split(None, 4)
    ancho, alto = int(partes[1]), int(partes[2])
    return ancho, alto, list(dados[len(dados) - ancho * alto:])


def referencia(nome, pagina):
    return os.path.join(RECURSOS, "ref", "%s-p%d.pgm.gz" % (os.path.splitext(nome)[0], pagina))


def comparar(a, b):
    """La diferencia media (0-255) y la proporción de celdas muy distintas."""
    n = len(a)
    dif = [abs(a[i] - b[i]) for i in range(n)]
    return sum(dif) / n, sum(1 for d in dif if d > 48) / n


def tinta(valores):
    """Cuánta tinta hay: la suma de (255 - gris), normalizada."""
    return sum(255 - v for v in valores) / (255.0 * len(valores))
