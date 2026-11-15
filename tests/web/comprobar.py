#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Comprueba una captura de una página de tests/web/ (D271).

La página dice cuántas filas tiene (`data-filas`) y cada casilla (clase «f»)
lleva sus sondas en `data-p`; ver generar.py. Aquí solo se leen el HTML y los
píxeles: sirve igual para una captura de QEMU que para cualquier otra imagen.

Sondas (coordenadas relativas a la esquina de la casilla):
    x,y,RRGGBB[~t]          el píxel es de ese color (tolerancia t, 8 de fábrica)
    x,y,!RRGGBB             el píxel no es de ese color
    x0,y0,x1,y1,?RRGGBB[~t] algún píxel del rectángulo es de ese color (t = 40)
    x0,y0,x1,y1,!?RRGGBB    algún píxel del rectángulo no es de ese color
Sin sondas: toda la casilla (40x16) es verde #008000 y no hay verde a su
derecha (44,8) ni debajo (20,19).
"""
import re

X0, Y0, PASO, LARG, ALT = 10, 10, 24, 40, 16
VERDE = (0, 128, 0)


def filas_de(html):
    """[(rótulo, sondas)] de una página."""
    n = int(re.search(r'data-filas="(\d+)"', html).group(1))
    filas = []
    for m in re.finditer(r'<div class="f[ "][^>]*?id="f(\d+)"([^>]*)>.*?<div class="e" style="[^"]*">(.*?)</div>', html, re.S):
        p = re.search(r'data-p="([^"]*)"', m.group(2))
        rotulo = m.group(3).replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&")
        filas.append((rotulo, p.group(1) if p else ""))
    assert len(filas) == n, "la página dice %d filas y tiene %d" % (n, len(filas))
    return filas


def _cerca(c, d, tol):
    return c is not None and all(abs(c[i] - d[i]) <= tol for i in range(3))


def _sonda(pixel, ox, oy, texto):
    p = texto.split(",")
    cor = p[-1]
    modo = ""
    while cor[0] in "!?":
        modo += cor[0]
        cor = cor[1:]
    tol = None
    if "~" in cor:
        cor, t = cor.split("~")
        tol = int(t)
    d = (int(cor[0:2], 16), int(cor[2:4], 16), int(cor[4:6], 16))
    v = [int(x) for x in p[:-1]]
    if len(v) == 2:
        c = pixel(ox + v[0], oy + v[1])
        ok = _cerca(c, d, 8 if tol is None else tol)
        return (not ok) if modo == "!" else ok
    pts = [pixel(ox + x, oy + y) for y in range(v[1], v[3]) for x in range(v[0], v[2])]
    if modo == "?":
        return any(_cerca(c, d, 40 if tol is None else tol) for c in pts)
    return any(c is not None and not _cerca(c, d, 8 if tol is None else tol) for c in pts)


def comprobar(html, pixel, ox=0, oy=0):
    """pixel(x, y) -> (r, g, b) o None; (ox, oy) es el origen de la página en la
    imagen. Devuelve [(rótulo, bien)] por fila."""
    res = []
    for i, (rotulo, sondas) in enumerate(filas_de(html)):
        fx, fy = ox + X0, oy + Y0 + PASO * i
        if sondas:
            bien = all(_sonda(pixel, fx, fy, t) for t in sondas.split())
        else:
            bien = all(_cerca(pixel(fx + x, fy + y), VERDE, 8) for y in range(1, ALT, 2) for x in range(1, LARG, 2))
            bien = bien and not _cerca(pixel(fx + 44, fy + 8), VERDE, 8) and not _cerca(pixel(fx + 20, fy + 19), VERDE, 8)
        res.append((rotulo, bien))
    return res


def origen(pixel, larg, alt):
    """La esquina de la página en la imagen: la marca magenta de 4x4 (#f0f)."""
    for y in range(alt - 3):
        for x in range(larg - 3):
            if pixel(x, y) == (255, 0, 255) and pixel(x + 3, y + 3) == (255, 0, 255) and pixel(x + 3, y) == (255, 0, 255):
                return x, y
    return None


def leer_ppm(camino):
    with open(camino, "rb") as f:
        d = f.read()
    p = d.split(b"\n", 3)
    larg, alt = (int(v) for v in p[1].split())
    px = p[3]

    def pixel(x, y):
        if x < 0 or y < 0 or x >= larg or y >= alt:
            return None
        i = (y * larg + x) * 3
        return (px[i], px[i + 1], px[i + 2])
    return pixel, larg, alt


def leer_png(camino):
    """Un PNG de 8 bits RGB o RGBA sin entrelazar (las capturas de QEMU)."""
    import struct
    import zlib
    with open(camino, "rb") as f:
        bruto = f.read()
    pos, dados = 8, bytearray()
    while pos < len(bruto):
        n = struct.unpack(">I", bruto[pos:pos + 4])[0]
        tipo = bruto[pos + 4:pos + 8]
        parte = bruto[pos + 8:pos + 8 + n]
        pos += 12 + n
        if tipo == b"IHDR":
            larg, alt, prof, cor = struct.unpack(">IIBB", parte[:10])
        elif tipo == b"IDAT":
            dados.extend(parte)
    c = {2: 3, 6: 4}[cor]
    assert prof == 8, "PNG de %d bits" % prof
    d = zlib.decompress(bytes(dados))
    paso = larg * c
    lin, ant, p = [], bytearray(paso), 0
    for _ in range(alt):
        ft = d[p]
        at = bytearray(d[p + 1:p + 1 + paso])
        p += paso + 1
        if ft == 1:
            for i in range(c, paso):
                at[i] = (at[i] + at[i - c]) & 255
        elif ft == 2:
            for i in range(paso):
                at[i] = (at[i] + ant[i]) & 255
        elif ft == 3:
            for i in range(paso):
                at[i] = (at[i] + ((at[i - c] if i >= c else 0) + ant[i]) // 2) & 255
        elif ft == 4:
            for i in range(paso):
                x = at[i - c] if i >= c else 0
                y = ant[i]
                z = ant[i - c] if i >= c else 0
                q = x + y - z
                qa, qb, qc = abs(q - x), abs(q - y), abs(q - z)
                at[i] = (at[i] + (x if qa <= qb and qa <= qc else y if qb <= qc else z)) & 255
        lin.append(bytes(at))
        ant = at

    def pixel(x, y):
        if x < 0 or y < 0 or x >= larg or y >= alt:
            return None
        return tuple(lin[y][x * c:x * c + 3])
    return pixel, larg, alt


def leer(camino):
    return leer_png(camino) if camino.lower().endswith(".png") else leer_ppm(camino)


if __name__ == "__main__":
    import sys
    with open(sys.argv[1], encoding="utf-8") as f:
        h = f.read()
    pixel, larg, alt = leer(sys.argv[2])
    o = origen(pixel, larg, alt)
    if o is None:
        print("sin marca de origen")
        sys.exit(2)
    r = comprobar(h, pixel, o[0], o[1])
    for i, (rotulo, bien) in enumerate(r):
        print("%s %2d %s" % ("ok   " if bien else "FALLA", i + 1, rotulo))
    print("%d de %d" % (sum(1 for _, b in r if b), len(r)))
