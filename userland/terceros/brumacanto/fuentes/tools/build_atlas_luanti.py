#!/usr/bin/env python3
"""Anade al atlas de Brumacanto (D449) las texturas de datos/luanti y las herramientas de hierro y diamante.

Sin dependencias (solo la biblioteca estandar): lee los PNG de 16x16 de datos/luanti, los amplia a 60x60 sin suavizar
(estilo de pixeles) y los pega en las casillas 42-62 de datos/atlas.rgba. Las herramientas de hierro y de diamante
se derivan de los iconos CC0 de Kenney (casillas 31, 32 y 34), asi que el resultado es repetible: solo se leen
casillas que este programa no escribe.

Uso: python3 build_atlas_luanti.py [carpeta_datos]     (por defecto, ../../datos)
Las texturas de Luanti/Minetest Game son CC BY-SA 3.0: ver datos/luanti/CREDITOS.md.
"""
import struct
import sys
import zlib
from pathlib import Path

TILE = 60
ATLAS = 512


def leer_png(ruta):
    """Devuelve (ancho, alto, filas de [r,g,b,a]) de un PNG no entrelazado de 1-8 bits (gris, RGB, paleta, RGBA)."""
    d = Path(ruta).read_bytes()
    assert d[:8] == b"\x89PNG\r\n\x1a\n", ruta
    i, idat, paleta, trns = 8, b"", [], {}
    while i < len(d):
        n, t = struct.unpack(">I4s", d[i:i + 8])
        c = d[i + 8:i + 8 + n]
        if t == b"IHDR":
            w, h, prof, tipo, _, _, entrelazado = struct.unpack(">IIBBBBB", c)
            assert entrelazado == 0
        elif t == b"PLTE":
            paleta = [tuple(c[k:k + 3]) for k in range(0, n, 3)]
        elif t == b"tRNS":
            trns = {k: c[k] for k in range(n)} if tipo == 3 else {}
        elif t == b"IDAT":
            idat += c
        i += 12 + n
    canales = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[tipo]
    bits = canales * prof
    paso = (w * bits + 7) // 8
    bpp = max(1, bits // 8)
    raw = zlib.decompress(idat)
    filas, prev, o = [], bytearray(paso), 0
    for _ in range(h):
        f = raw[o]
        lin = bytearray(raw[o + 1:o + 1 + paso])
        o += 1 + paso
        for x in range(paso):
            a = lin[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                lin[x] = (lin[x] + a) & 255
            elif f == 2:
                lin[x] = (lin[x] + b) & 255
            elif f == 3:
                lin[x] = (lin[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                lin[x] = (lin[x] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        filas.append(lin)
        prev = lin
    salida = []
    for lin in filas:
        fila = []
        for x in range(w):
            if prof == 8:
                v = [lin[x * canales + k] for k in range(canales)]
            else:
                bit = x * prof
                v = [(lin[bit // 8] >> (8 - prof - bit % 8)) & ((1 << prof) - 1)]
            if tipo == 3:
                r, g, bl = paleta[v[0]]
                fila.append([r, g, bl, trns.get(v[0], 255)])
            elif tipo == 0:
                g = v[0] * 255 // ((1 << prof) - 1)
                fila.append([g, g, g, 255])
            elif tipo == 4:
                fila.append([v[0], v[0], v[0], v[1]])
            elif tipo == 2:
                fila.append([v[0], v[1], v[2], 255])
            else:
                fila.append(v)
        salida.append(fila)
    return w, h, salida


def ampliar(fila_de_filas, w, h):
    """16x16 -> 60x60 por vecino mas cercano."""
    return [[list(fila_de_filas[y * h // TILE][x * w // TILE]) for x in range(TILE)] for y in range(TILE)]


def sobre(base, capa):
    """Compone capa (con alfa) sobre base opaca, en sitio."""
    for y in range(TILE):
        for x in range(TILE):
            r, g, b, a = capa[y][x]
            p = base[y][x]
            for k, v in enumerate((r, g, b)):
                p[k] = (v * a + p[k] * (255 - a)) // 255
            p[3] = max(p[3], a)


def aplanar(tile, fondo):
    base = [[list(fondo) + [255] for _ in range(TILE)] for _ in range(TILE)]
    sobre(base, tile)
    return base


class Atlas:
    def __init__(self, ruta):
        self.ruta = ruta
        self.px = bytearray(ruta.read_bytes())
        assert len(self.px) == ATLAS * ATLAS * 4

    def origen(self, i):
        return (i % 8) * 64 + 2, (i // 8) * 64 + 2

    def leer(self, i):
        x0, y0 = self.origen(i)
        return [[list(self.px[((y0 + y) * ATLAS + x0 + x) * 4:((y0 + y) * ATLAS + x0 + x) * 4 + 4]) for x in range(TILE)]
                for y in range(TILE)]

    def poner(self, i, tile):
        """Pega la casilla de 60x60 con dos pixeles de margen repetidos, como el constructor original."""
        x0, y0 = self.origen(i)
        for gy in range(-2, TILE + 2):
            for gx in range(-2, TILE + 2):
                sx = min(max(gx, 0), TILE - 1)
                sy = min(max(gy, 0), TILE - 1)
                o = ((y0 + gy) * ATLAS + x0 + gx) * 4
                self.px[o:o + 4] = bytes(tile[sy][sx])

    def guardar(self):
        self.ruta.write_bytes(bytes(self.px))


def recolorear(tile, modo):
    """Cambia el color del metal (pixeles poco saturados) y deja el mango marron."""
    sal = []
    for fila in tile:
        nueva = []
        for r, g, b, a in fila:
            mx, mn = max(r, g, b), min(r, g, b)
            sat = (mx - mn) / mx if mx else 0
            lum = (r * 3 + g * 6 + b) // 10
            if a and sat < .28:
                if modo == "hierro":
                    k = min(255, 70 + lum * 4 // 5)
                    r, g, b = k * 215 // 255, k * 225 // 255, k
                else:
                    k = min(255, 60 + lum)
                    r, g, b = k * 90 // 255, min(255, k * 250 // 255 + 20), min(255, k * 235 // 255 + 20)
            nueva.append([r, g, b, a])
        sal.append(nueva)
    return sal


def main():
    datos = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent.parent / "datos"
    L = datos / "luanti"
    atlas = Atlas(datos / "atlas.rgba")

    def tex(nombre):
        w, h, filas = leer_png(L / nombre)
        return ampliar(filas, w, h)

    piedra = atlas.leer(3)
    for casilla, mineral in ((42, "default_mineral_copper.png"), (43, "default_mineral_diamond.png")):
        base = [[list(p) for p in fila] for fila in piedra]
        sobre(base, tex(mineral))
        atlas.poner(casilla, base)
    simples = {44: "default_obsidian.png", 45: "default_pine_tree.png", 46: "default_pine_tree_top.png",
               47: "default_pine_wood.png", 49: "default_sandstone.png", 50: "default_stone_brick.png",
               51: "default_furnace_front.png", 52: "default_lava.png", 53: "flowers_rose.png",
               54: "default_copper_ingot.png", 55: "default_steel_ingot.png", 56: "default_diamond.png"}
    for casilla, nombre in simples.items():
        atlas.poner(casilla, tex(nombre))
    atlas.poner(48, aplanar(tex("default_pine_needles.png"), (24, 58, 38)))
    # Herramientas derivadas de los iconos CC0 de Kenney: pico (31), hacha (32) y espada (34).
    for casilla, origen, modo in ((57, 31, "hierro"), (58, 32, "hierro"), (59, 34, "hierro"),
                                  (60, 31, "diamante"), (61, 32, "diamante"), (62, 34, "diamante")):
        atlas.poner(casilla, recolorear(atlas.leer(origen), modo))
    atlas.guardar()
    print("atlas actualizado:", datos / "atlas.rgba")


main()
