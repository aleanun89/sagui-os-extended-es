#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H162 (D196) - fuentes vectoriales: la libfontes, las fuentes de Typodermic y su uso.

Imagen con la partición SAGUI-APPS (build/apps/raiz, con /apps/fuentes si el make encontró el zip
de Typodermic en NEW_FONTS/; sin él la prueba se salta con 77):

  A. /apps/fuentes lleva la selección (28 fuentes .otf) y LICENCIA-Typodermic.txt; el programa
     /testes/pruebafuentes dibuja una muestra con cinco fuentes distintas: en la captura hay
     píxeles negros y grises intermedios (antialias) en cada fila, y la anchura de la tinta
     coincide con la que midió la biblioteca (fonte_medir) y con la que devolvió al dibujar.
  B. La pestaña «Fuentes» de Configuración: lista y vista previa (capturas H162-2 y H162-3: al
     elegir otra fuente de la lista la vista cambia), «Para documentos» y «Para títulos» guardan
     la elección en /tmp/fuentes.conf (y en sagui.conf cuando /home es persistente).
  C. Paint escribe su texto con la fuente de documentos elegida (antialias en el lienzo,
     captura H162-4) y el Editor la usa en su vista (botón «Vista», captura H162-5; A+ cambia el tamaño).
Capturas en docs/capturas/H162-*.png.
"""
import os
import re
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, opcoes_apps, BUILD  # noqa: E402
import h57_paint as h57  # noqa: E402

APPS = os.path.join(BUILD, "apps", "raiz")
TITULOS_PAG = ("Teclado", "Fecha y hora", "Pantalla", "Sonido", "Escritorio", "Capturas", "IA", "Fuentes")
Y_PAG = 50
FN_VX, FN_VY, FN_VL, FN_VA = 24 + 270 + 16, Y_PAG + 24, 740 - (24 + 270 + 16) - 24, 190
BORDA, TITULO = 2, 24


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]), y=int(g[3]))


def esperar(m, padrao, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def _leer_png(ruta):
    """Lee un PNG de 8 bits RGB o RGBA (sin entrelazado) -> (ancho, alto, canales, bytes). Sin ImageMagick:
    en el entorno de pruebas `convert` es un sustituto mínimo (tools/convert) que no recorta."""
    import struct
    import zlib
    d = open(ruta, "rb").read()
    pos, idat, ancho, alto, tipo = 8, b"", 0, 0, 2
    while pos < len(d):
        n, nom = struct.unpack(">I4s", d[pos:pos + 8])
        cuerpo = d[pos + 8:pos + 8 + n]
        if nom == b"IHDR":
            ancho, alto, _, tipo = struct.unpack(">IIBB", cuerpo[:10])
        elif nom == b"IDAT":
            idat += cuerpo
        pos += 12 + n
    canales = {2: 3, 6: 4}[tipo]
    raw = zlib.decompress(idat)
    paso = ancho * canales
    filas, ant = [], bytearray(paso)
    for y in range(alto):
        f = raw[y * (paso + 1)]
        fila = bytearray(raw[y * (paso + 1) + 1:(y + 1) * (paso + 1)])
        for i in range(paso):
            a_ = fila[i - canales] if i >= canales else 0
            b_ = ant[i]
            c_ = ant[i - canales] if i >= canales else 0
            if f == 1:
                fila[i] = (fila[i] + a_) & 255
            elif f == 2:
                fila[i] = (fila[i] + b_) & 255
            elif f == 3:
                fila[i] = (fila[i] + ((a_ + b_) >> 1)) & 255
            elif f == 4:
                p = a_ + b_ - c_
                pa, pb, pc = abs(p - a_), abs(p - b_), abs(p - c_)
                pr = a_ if pa <= pb and pa <= pc else (b_ if pb <= pc else c_)
                fila[i] = (fila[i] + pr) & 255
        filas.append(fila)
        ant = fila
    return ancho, alto, canales, filas


def region(png, x, y, l, a):
    """Luminancias (0-255) de una región de la captura, como lista de filas."""
    ancho, alto, c, filas = _leer_png(png)
    salida = []
    for yy in range(y, min(y + a, alto)):
        f = filas[yy]
        salida.append([(f[xx * c] * 3 + f[xx * c + 1] * 6 + f[xx * c + 2]) // 10 for xx in range(x, min(x + l, ancho))])
    return salida


def tinta(filas, umbral_blanco=235):
    """(oscuros, grises, x mínimo, x máximo) de la tinta de una región sobre fondo blanco."""
    oscuros = grises = 0
    xs = []
    for fila in filas:
        for x, v in enumerate(fila):
            if v < 60:
                oscuros += 1
                xs.append(x)
            elif v < umbral_blanco:
                grises += 1
                xs.append(x)
    return oscuros, grises, (min(xs) if xs else -1), (max(xs) if xs else -1)


def parte_a(m):
    out = m.comando("ls /apps/fuentes | wc -l")
    n = int(re.findall(r"\b(\d+)\b", out.replace("\r", ""))[-1])
    verificar(n >= 25, f"A: /apps/fuentes lleva la selección y la licencia ({n} entradas)")
    out = m.comando("ls /apps/fuentes")
    verificar("LICENCIA-Typodermic.txt" in out and "Kingsbridge-Rg.otf" in out, "A: y la licencia CC0 junto a las fuentes")
    out = m.comando("cat /apps/fuentes/LICENCIA-Typodermic.txt")
    verificar("whatever you want to do with this font" in out.lower(), "A: la licencia dice que la respuesta es siempre sí")

    ini = len(m.serial())
    m.comando("/testes/pruebafuentes &")
    esperar(m, r"pruebafuentes: lista", 60, ini)
    time.sleep(1)
    s = m.serial()[ini:]
    verificar(int(re.findall(r"pruebafuentes: n=(\d+)", s)[-1]) >= 25, "A: el catálogo encuentra las fuentes de /apps/fuentes")
    filas = re.findall(r"fila (\d+) y=(\d+) x=(\d+) medido=(\d+) dibujado=(\d+) familia=(.*)", s)
    verificar(len(filas) == 5, f"A: cinco filas dibujadas ({len(filas)})")
    j = janela(m.serial(), "Prueba de fuentes")
    png = m.captura("H162-1-prueba-fuentes")
    ox, oy = j["x"] + BORDA, j["y"] + TITULO
    familias = set()
    for i, y, x, med, dib, fam in filas:
        y, x, med, dib = int(y), int(x), int(med), int(dib)
        familias.add(fam.strip())
        verificar(med == dib, f"A: {fam.strip()}: la anchura medida ({med}) es la dibujada ({dib})")
        osc, gri, xmin, xmax = tinta(region(png, ox + x, oy + y, 520, 38))
        verificar(osc > 40, f"A: {fam.strip()}: píxeles del color del texto ({osc})")
        verificar(gri > 20, f"A: {fam.strip()}: grises intermedios del antialias ({gri})")
        # la tinta cae dentro de lo medido (los costados de los glifos pueden quedar un poco dentro)
        verificar(xmax <= med + 4 and xmax >= med * 0.6, f"A: {fam.strip()}: la tinta llega a x={xmax} y se midieron {med}")
    verificar(len(familias) >= 4, f"A: fuentes distintas ({sorted(familias)})")
    j2 = j
    m.clique(j2["x"] + j2["l"] - 2 - 14, j2["y"] + 12)   # cerrar la ventana de prueba
    time.sleep(1)


def parte_b(m):
    ini = len(m.serial())
    m.comando("configuracion &")
    esperar(m, r'"Configuración" \d+x\d+', 30, ini)
    time.sleep(1.5)
    j = janela(m.serial(), "Configuración")
    ox, oy = j["x"] + BORDA, j["y"] + TITULO
    x = 10
    pos = []
    for t in TITULOS_PAG:
        l = len(t) * 8 + 16
        pos.append((x, l))
        x += l + 4
    m.clique(ox + pos[7][0] + pos[7][1] // 2, oy + 24)
    time.sleep(1.5)
    png1 = m.captura("H162-2-config-fuentes")
    o1, g1, _, _ = tinta(region(png1, ox + FN_VX + 2, oy + FN_VY + 2, FN_VL - 4, FN_VA - 4))
    verificar(o1 > 300 and g1 > 100, f"B: la vista previa dibuja texto con antialias (oscuros {o1}, grises {g1})")
    # otra fuente de la lista: la vista cambia
    m.clique(ox + 24 + 100, oy + Y_PAG + 34 + 3 * 20 + 10)
    time.sleep(1.2)
    png2 = m.captura("H162-3-config-otra-fuente")
    r1 = region(png1, ox + FN_VX + 2, oy + FN_VY + 2, FN_VL - 4, FN_VA - 4)
    r2 = region(png2, ox + FN_VX + 2, oy + FN_VY + 2, FN_VL - 4, FN_VA - 4)
    difer = sum(1 for a, b in zip(r1, r2) for p, q in zip(a, b) if abs(p - q) > 40)
    verificar(difer > 500, f"B: al elegir otra fuente cambia la vista previa ({difer} píxeles distintos)")
    # «Para documentos» (fila 3) y «Para títulos» (fila 5)
    m.clique(ox + FN_VX + 75, oy + Y_PAG + 276 + 14)
    time.sleep(1)
    m.clique(ox + 24 + 100, oy + Y_PAG + 34 + 5 * 20 + 10)
    time.sleep(1)
    m.clique(ox + FN_VX + 158 + 60, oy + Y_PAG + 276 + 14)
    time.sleep(1)
    m.captura("H162-4-config-elegidas")
    m.clique(ox + 740 - 110 + 48, oy + 500 - 40 + 14)   # Cerrar
    time.sleep(1)
    out = m.comando("cat /tmp/fuentes.conf").replace("\r", "")
    d = re.search(r"fuente_documentos=(/apps/fuentes/\S+\.otf)", out)
    t = re.search(r"fuente_titulos=(/apps/fuentes/\S+\.otf)", out)
    verificar(d and t and d.group(1) != t.group(1), f"B: las fuentes por defecto se guardan ({out.strip()[-160:]})")
    return d.group(1)


def parte_c(m):
    # Paint
    ini = len(m.serial())
    m.comando("paint /tmp/h162.bmp")
    esperar(m, r"paint: pronto", 40, ini)
    time.sleep(1)
    p = h57.Paint(m, h57.janela(m.serial(), "Paint - sin nombre"))
    p.ferramenta(h57.F_TEXTO)
    m.clique(*p.tela(100, 100))
    esperar(m, r'"Texto" \d+x\d+', 15, ini)
    time.sleep(0.5)
    m.digitar("Hola Typodermic", 0.05)
    m.tecla("ret")
    esperar(m, r'paint: texto "Hola Typodermic" en \(100,100\) con fuente vectorial', 15, ini)
    time.sleep(0.8)
    png = m.captura("H162-5-paint-texto")
    x0, y0 = p.tela(100, 100)
    osc, gri, _, _ = tinta(region(png, x0, y0, 400, 40))
    verificar(osc > 40 and gri > 20, f"C: Paint escribe con la fuente vectorial: antialias en el lienzo (oscuros {osc}, grises {gri})")
    m.tecla("ctrl-s")   # guardar antes de cerrar: si no, Paint pregunta por los cambios
    esperar(m, r"paint: guardado /tmp/h162.bmp", 15, ini)
    p.fechar()
    m.esperar_serial("paint: fechado", 15)
    time.sleep(0.5)
    # Editor
    m.comando("echo Titulo del documento > /tmp/h162.txt")
    m.comando("echo El veloz murcielago hindu comia feliz cardillo y kiwi >> /tmp/h162.txt")
    ini = len(m.serial())
    m.comando("editor /tmp/h162.txt")
    esperar(m, r"editor: pronto", 30, ini)
    time.sleep(1)
    j = janela(m.serial(), "Editor - sin nombre")
    ox, oy = j["x"] + BORDA, j["y"] + TITULO
    m.clique(ox + 390 + 40, oy + 7 + 13)
    esperar(m, r"editor: vista con fuente \S.* 18 px", 15, ini)
    time.sleep(1)
    png = m.captura("H162-6-editor-vista")
    osc, gri, _, _ = tinta(region(png, ox + 14, oy + 40 + 10, 600, 120))
    verificar(osc > 200 and gri > 60, f"C: la vista del Editor usa la fuente vectorial (oscuros {osc}, grises {gri})")
    m.clique(ox + 518 + 18, oy + 7 + 13)   # A+
    esperar(m, r"editor: vista 20 px", 15, ini)
    time.sleep(0.8)
    png2 = m.captura("H162-7-editor-vista-mayor")
    a = region(png, ox + 14, oy + 50, 600, 120)
    b = region(png2, ox + 14, oy + 50, 600, 120)
    verificar(sum(1 for f, g in zip(a, b) for p, q in zip(f, g) if abs(p - q) > 40) > 500, "C: A+ agranda el texto")
    m.clique(ox + 390 + 40, oy + 7 + 13)   # Edición
    esperar(m, r"editor: edicion", 15, ini)


def main():
    if not os.path.exists(os.path.join(APPS, "fuentes", "LICENCIA-Typodermic.txt")):
        print("H162: sin las fuentes de Typodermic (falta NEW_FONTS/*.zip al hacer make); no aplica")
        return 77
    imagem = imagem_de_teste("", ["--vesa"] + opcoes_apps())
    with Maquina("h162", "ide", imagem=imagem) as m:
        m.esperar_serial("sagui:/$", 150)
        time.sleep(3)
        parte_a(m)
        parte_b(m)
        parte_c(m)
    print("H162: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
