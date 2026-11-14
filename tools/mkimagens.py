#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
mkimagens.py - genera las imágenes de ejemplo de rootfs/imagenes/ (D054).

Todo se dibuja aquí, con fórmulas o en pixel art: degradados, el sagui del
escritorio, un fractal de Mandelbrot y una puesta de sol. Cada imagen sale
en un formato distinto, para que el visor y la prueba H56 ejerciten todos los
lectores de libsagui:

  degradado.bmp     BMP 24 bits, 160x120
  sagui.bmp         BMP de 8 bits con paleta, 128x128 (el sagui de arte.c, 4x)
  vidrio.bmp        BMP 32 bits, 64x64
  mandelbrot.png    PNG con paleta (tipo 3), 320x240, filtros variados
  atardecer.png     PNG RGB entrelazado (Adam7), 160x120
  hojas.png         PNG gris+alfa (tipo 4), 64x64 (alfa mezclado con blanco)
  selva.ppm         PPM P6, 160x120
  ondas.pgm         PGM P5, 128x96
  mini.ppm          PPM P3 (texto), 8x6

Uso: mkimagens.py [carpeta]   (por defecto rootfs/imagenes)

Las funciones que dibujan las usa también la prueba (tests/h56_visor_imagenes.py):
esperado(nome) devuelve (anchura, altura, lista de (r, g, b)) como el visor
debe leer.
"""
import os
import struct
import sys
import zlib

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ------------------------------------------------------------ dibujos

def degradado():
    l, a = 160, 120
    px = []
    for y in range(a):
        for x in range(l):
            r = x * 255 // (l - 1)
            g = y * 255 // (a - 1)
            b = 255 - (x + y) * 255 // (l + a - 2)
            px.append((r, g, b))
    return l, a, px


def _sagui_arte():
    """El sagui de kernel/gui/arte.c, con su misma paleta."""
    src = open(os.path.join(RAIZ, "kernel", "gui", "arte.c"), encoding="utf-8").read()
    ini = src.index("static const char *const sagui[] = {")
    fim = src.index("};", ini)
    linhas = [l.strip().strip(",").strip('"') for l in src[ini:fim].splitlines()[1:] if '"' in l]
    paleta = {"P": (0x14, 0x12, 0x10), "E": (0x3C, 0x2D, 0x23), "M": (0x6E, 0x55, 0x3C),
              "C": (0x96, 0x8C, 0x7D), "c": (0xBE, 0xB4, 0xA5), "B": (0xF5, 0xF5, 0xF0),
              "R": (0xAA, 0x78, 0x64), "b": (0xFF, 0xFF, 0xFF)}
    return linhas, paleta


def sagui():
    """Índices de paleta: 0..15 fondo en degradado azul cielo, luego los colores del sagui."""
    linhas, cores = _sagui_arte()
    paleta = [(40 + i * 8, 120 + i * 6, 200 + i * 3) for i in range(16)]
    letras = sorted(cores)
    paleta += [cores[k] for k in letras]
    l = a = 128
    idx = []
    for y in range(a):
        for x in range(l):
            c = linhas[y // 4][x // 4]
            idx.append(16 + letras.index(c) if c in cores else y // 8)
    return l, a, idx, paleta


def vidrio():
    l = a = 64
    px = []
    for y in range(a):
        for x in range(l):
            d = (x - 32) ** 2 + (y - 32) ** 2
            if d < 24 * 24:
                px.append((30 + d // 8, 160, 220 - d // 6))
            else:
                px.append((250, 250, 250) if (x // 8 + y // 8) % 2 else (200, 200, 200))
    return l, a, px


def mandelbrot():
    l, a = 320, 240
    paleta = []
    for i in range(64):   # del azul oscuro al amarillo, pasando por el verde de SaguiOS
        t = i / 63
        if t < 0.5:
            u = t * 2
            paleta.append((int(10 + u * 1), int(20 + u * 102), int(80 - u * 18)))
        else:
            u = (t - 0.5) * 2
            paleta.append((int(11 + u * 244), int(122 + u * 91), int(62 - u * 20)))
    paleta.append((0, 0, 0))   # dentro del conjunto
    idx = []
    for y in range(a):
        for x in range(l):
            cr = -2.2 + x * 3.2 / l
            ci = -1.2 + y * 2.4 / a
            zr = zi = 0.0
            n = 0
            while n < 64 and zr * zr + zi * zi < 4:
                zr, zi = zr * zr - zi * zi + cr, 2 * zr * zi + ci
                n += 1
            idx.append(64 if n == 64 else n)
    return l, a, idx, paleta


def atardecer():
    l, a = 160, 120
    px = []
    for y in range(a):
        for x in range(l):
            if y > 84:   # mar
                r, g, b = 20, 40 + (y - 84) * 2, 90 + (y - 84) * 3
                if (x + y * 3) % 17 == 0:
                    r, g, b = 240, 180, 90
            else:
                t = y * 255 // 84
                r, g, b = 255 - t // 4, 120 + t // 3, 60 + t // 2
                if (x - 80) ** 2 + (y - 70) ** 2 < 22 * 22:
                    r, g, b = 255, 230, 120
            px.append((r, g, b))
    return l, a, px


def hojas():
    """Gris con alfa: (gris, alfa) por píxel."""
    l = a = 64
    ga = []
    for y in range(a):
        for x in range(l):
            dentro = abs(x - 32) + abs(y - 32) * 2 < 40
            g = 40 + (x * 3) % 120
            ga.append((g, 255 if dentro else (x * 4) % 256))
    return l, a, ga


def selva():
    l, a = 160, 120
    px = []
    for y in range(a):
        for x in range(l):
            r = (x * 7 + y * 3) % 60
            g = 90 + (x * y) % 120
            b = 30 + (x ^ y) % 50
            px.append((r, g, b))
    return l, a, px


def ondas():
    l, a = 128, 96
    tab = [int(127 + 127 * __import__("math").sin(i * 6.2831853 / 32)) for i in range(32)]
    return l, a, [tab[(x + tab[y % 32] // 16) % 32] for y in range(a) for x in range(l)]


def mini():
    l, a = 8, 6
    cores = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (0, 0, 0), (255, 255, 255)]
    return l, a, [cores[(x + y) % 6] for y in range(a) for x in range(l)]


# ------------------------------------------------------------ grabación

def bmp24(l, a, px):
    passo = (l * 3 + 3) & ~3
    dados = bytearray()
    for y in range(a - 1, -1, -1):
        lin = bytearray()
        for x in range(l):
            r, g, b = px[y * l + x]
            lin += bytes((b, g, r))
        dados += lin + bytes(passo - len(lin))
    cab = struct.pack("<2sIHHI", b"BM", 54 + len(dados), 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, l, a, 1, 24, 0, len(dados), 2835, 2835, 0, 0)
    return cab + dib + bytes(dados)


def bmp32_cima_baixo(l, a, px):
    """32 bits BI_RGB, de arriba abajo (altura negativa)."""
    dados = b"".join(bytes((b, g, r, 255)) for r, g, b in px)
    cab = struct.pack("<2sIHHI", b"BM", 54 + len(dados), 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, l, -a, 1, 32, 0, len(dados), 2835, 2835, 0, 0)
    return cab + dib + dados


def bmp8(l, a, idx, paleta):
    passo = (l + 3) & ~3
    pal = b"".join(bytes((b, g, r, 0)) for r, g, b in paleta)
    dados = bytearray()
    for y in range(a - 1, -1, -1):
        lin = bytes(idx[y * l:(y + 1) * l])
        dados += lin + bytes(passo - l)
    ini = 54 + len(pal)
    cab = struct.pack("<2sIHHI", b"BM", ini + len(dados), 0, 0, ini)
    dib = struct.pack("<IiiHHIIiiII", 40, l, a, 1, 8, 0, len(dados), 2835, 2835, len(paleta), 0)
    return cab + dib + pal + bytes(dados)


def _pedaco(tipo, dados):
    return struct.pack(">I", len(dados)) + tipo + dados + struct.pack(">I", zlib.crc32(tipo + dados) & 0xFFFFFFFF)


def _filtrar(linhas, bpp):
    """Aplica los cinco filtros del PNG, uno por línea en rotación (para que la prueba
    ejercite todos)."""
    def paeth(a, b, c):
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        return a if pa <= pb and pa <= pc else (b if pb <= pc else c)
    saida = bytearray()
    ant = bytes(len(linhas[0])) if linhas else b""
    for i, lin in enumerate(linhas):
        f = i % 5
        out = bytearray()
        for x in range(len(lin)):
            esq = lin[x - bpp] if x >= bpp else 0
            cima = ant[x]
            diag = ant[x - bpp] if x >= bpp else 0
            pred = (0, esq, cima, (esq + cima) // 2, paeth(esq, cima, diag))[f]
            out.append((lin[x] - pred) & 255)
        saida += bytes((f,)) + out
        ant = lin
    return bytes(saida)


def png(l, a, tipo, linhas_cruas, bpp, paleta=None, entrelacado=False, nivel=9):
    """linhas_cruas(x0, y0, dx, dy) -> lista de líneas en bytes del (sub)cuadro."""
    if entrelacado:
        dados = b""
        for x0, y0, dx, dy in ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)):
            lin = linhas_cruas(x0, y0, dx, dy)
            if lin and lin[0]:
                dados += _filtrar(lin, bpp)
    else:
        dados = _filtrar(linhas_cruas(0, 0, 1, 1), bpp)
    s = b"\x89PNG\r\n\x1a\n" + _pedaco(b"IHDR", struct.pack(">IIBBBBB", l, a, 8, tipo, 0, 0, 1 if entrelacado else 0))
    if paleta:
        s += _pedaco(b"PLTE", b"".join(bytes(c) for c in paleta))
    z = zlib.compress(dados, nivel)
    # Dos IDAT, para que la prueba vea que el lector une los trozos.
    meio = len(z) // 2
    s += _pedaco(b"IDAT", z[:meio]) + _pedaco(b"IDAT", z[meio:]) + _pedaco(b"IEND", b"")
    return s


def _linhas(l, a, valores, por_px):
    def f(x0, y0, dx, dy):
        return [b"".join(por_px(valores[y * l + x]) for x in range(x0, l, dx)) for y in range(y0, a, dy)]
    return f


# ------------------------------------------------------------ catálogo

def esperado(nome):
    """(l, a, [(r, g, b)]) como debe leer la imagen el visor."""
    if nome == "degradado.bmp":
        return degradado()
    if nome == "sagui.bmp":
        l, a, idx, pal = sagui()
        return l, a, [pal[i] for i in idx]
    if nome == "vidrio.bmp":
        return vidrio()
    if nome == "mandelbrot.png":
        l, a, idx, pal = mandelbrot()
        return l, a, [pal[i] for i in idx]
    if nome == "atardecer.png":
        return atardecer()
    if nome == "hojas.png":
        l, a, ga = hojas()
        def mist(g, al):
            v = (g * al + 255 * (255 - al) + 127) // 255
            return (v, v, v)
        return l, a, [mist(g, al) for g, al in ga]
    if nome == "selva.ppm":
        return selva()
    if nome == "ondas.pgm":
        l, a, g = ondas()
        return l, a, [(v, v, v) for v in g]
    if nome == "mini.ppm":
        return mini()
    raise KeyError(nome)


def arquivos():
    """nombre -> bytes del archivo."""
    out = {}
    out["degradado.bmp"] = bmp24(*degradado())
    out["sagui.bmp"] = bmp8(*sagui())
    out["vidrio.bmp"] = bmp32_cima_baixo(*vidrio())
    l, a, idx, pal = mandelbrot()
    out["mandelbrot.png"] = png(l, a, 3, _linhas(l, a, idx, lambda i: bytes((i,))), 1, paleta=pal)
    l, a, px = atardecer()
    out["atardecer.png"] = png(l, a, 2, _linhas(l, a, px, bytes), 3, entrelacado=True)
    l, a, ga = hojas()
    out["hojas.png"] = png(l, a, 4, _linhas(l, a, ga, bytes), 2, nivel=1)
    l, a, px = selva()
    out["selva.ppm"] = b"P6\n# selva - SaguiOS\n%d %d\n255\n" % (l, a) + b"".join(bytes(p) for p in px)
    l, a, g = ondas()
    out["ondas.pgm"] = b"P5\n%d %d\n255\n" % (l, a) + bytes(g)
    l, a, px = mini()
    txt = "P3\n# mini: seis colores\n%d %d\n255\n" % (l, a)
    for y in range(a):
        txt += " ".join("%d %d %d" % px[y * l + x] for x in range(l)) + "\n"
    out["mini.ppm"] = txt.encode()
    return out


def main():
    pasta = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "rootfs", "imagenes")
    os.makedirs(pasta, exist_ok=True)
    total = 0
    for nome, dados in sorted(arquivos().items()):
        with open(os.path.join(pasta, nome), "wb") as f:
            f.write(dados)
        total += len(dados)
        print(f"mkimagens: {nome} {len(dados)} bytes")
    print(f"mkimagens: {total} bytes em {pasta}")


if __name__ == "__main__":
    main()
