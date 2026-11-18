#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Genera unos DRS SINTÉTICOS mínimos para probar AoEN (D382, prueba h389).

    aoen_drs_sintetico.py CARPETA          crea CARPETA/GRAPHICS.DRS, INTERFAC.DRS, TERRAIN.DRS (y SOUNDS.DRS vacío)

No usa ni copia nada del Age of Empires original: la paleta, las losetas del terreno, las unidades y los edificios
se dibujan aquí con formas geométricas. El formato DRS/SLP es el conocido: una cabecera de 64 bytes, listas de
tablas (tipo, desplazamiento, número) y tablas de elementos (id, desplazamiento, tamaño); el SLP lleva 32 bytes
de cabecera, 32 por cuadro, la tabla de bordes (izquierda, derecha) y las órdenes de dibujo por fila (rellenar
bloques de píxeles, 0x0F = fin de fila; el color de jugador se marca con la orden 0x0A).
"""
import os
import struct
import sys

# --- identificadores que pide el motor (render.c) ---
UNIDADES = [418, 657, 224, 314, 373, 425, 664, 212, 321, 380, 415, 654, 205, 311, 370, 437, 678, 226, 335, 371,
            413, 652, 203, 309, 368, 414, 653, 204, 412, 665, 202, 318, 377, 422, 661, 209, 424, 663, 211, 320, 379,
            423, 662, 210, 319, 378, 416, 655, 206, 312, 371, 443, 685, 634, 341, 402, 475, 696, 385, 386, 428, 667,
            215, 440, 682, 625, 338, 399, 441, 683, 633, 339, 400, 632]
EDIFICIOS = [280, 230, 254, 165, 164, 166, 354, 27, 26, 29, 17, 19, 31]
EDIFICIOS += list(range(15, 59)) + list(range(59, 103)) + list(range(103, 159)) + list(range(159, 200)) + [30, 77, 121, 177]
RECURSOS = [240, 481, 622] + list(range(463, 467)) + list(range(492, 496)) + [636]


def paleta():
    """Paleta JASC de 256 colores: 0 transparente (negro), tonos de color, 16 de jugador en 16..23."""
    p = [(0, 0, 0)] * 256
    p = list(p)
    for i in range(1, 256):
        t = i / 255.0
        p[i] = (int(40 + 200 * t), int(30 + 180 * (1 - abs(2 * t - 1))), int(30 + 160 * (1 - t)))
    base = {1: (60, 140, 50), 2: (200, 170, 100), 3: (40, 90, 170), 4: (20, 50, 110),
            5: (120, 80, 50), 6: (180, 180, 180), 7: (30, 100, 30), 8: (90, 60, 40), 9: (230, 210, 150)}
    for k, c in base.items():
        for s in range(4):   # 4 sombras por color base: índices k*4 .. k*4+3
            f = 0.75 + 0.1 * s
            p[k * 4 + s] = tuple(min(255, int(v * f)) for v in c)
    for j in range(8):
        for s in range(4):   # colores de jugador: índices 64 + j*4 + s (el motor los cambia)
            c = [(40, 90, 255), (230, 40, 40), (40, 190, 60), (240, 230, 40), (255, 140, 30), (150, 100, 45),
                 (190, 190, 190), (70, 190, 190)][j]
            p[64 + j * 4 + s] = tuple(min(255, int(v * (0.7 + 0.1 * s))) for v in c)
    txt = "JASC-PAL\r\n0100\r\n256\r\n" + "".join("%d %d %d\r\n" % c for c in p)
    return txt.encode("ascii")


def slp(cuadros):
    """cuadros: lista de (w, h, hx, hy, filas) con filas = lista de (izq, der, [(indice, jugador)]) o None."""
    n = len(cuadros)
    cab = struct.pack("<4sI24s", b"2.0N", n, b"AoEN sintetico".ljust(24, b"\0"))
    cuerpo = bytearray()
    info = []
    base = 32 + 32 * n
    for (w, h, hx, hy, filas) in cuadros:
        out_off = base + len(cuerpo)
        for f in filas:
            if f is None:
                cuerpo += struct.pack("<hh", -32768, -32768)
            else:
                cuerpo += struct.pack("<hh", f[0], f[1])
        cmd_off = base + len(cuerpo)
        cuerpo += b"\0" * (4 * h)   # tabla de desplazamientos de fila (el motor la salta)
        for f in filas:
            if f is None:
                continue
            px = f[2]
            i = 0
            while i < len(px):
                if px[i][1]:   # tramo de color de jugador: orden 0x0A (cuenta<16, un solo índice)
                    j = i
                    while j < len(px) and px[j][1] and px[j][0] == px[i][0] and j - i < 15:
                        j += 1
                    cuerpo += bytes([0x0A | ((j - i) << 4), px[i][0]])
                else:
                    j = i
                    while j < len(px) and not px[j][1] and j - i < 63:
                        j += 1
                    cuerpo += bytes([(j - i) << 2]) + bytes(p[0] for p in px[i:j])
                i = j
            cuerpo += b"\x0f"
        info.append(struct.pack("<IIIIiiii", cmd_off, out_off, 0, 0, w, h, hx, hy))
    return cab + b"".join(info) + bytes(cuerpo)


def cuadro(w, h, hx, hy, pix):
    """pix(x, y) -> None o (indice, jugador). Calcula bordes y filas."""
    filas = []
    for y in range(h):
        fila = [pix(x, y) for x in range(w)]
        xs = [x for x, v in enumerate(fila) if v]
        if not xs:
            filas.append(None)
            continue
        izq, der = xs[0], w - 1 - xs[-1]
        # los huecos transparentes del medio se rellenan con el índice 1 (no se usan: las formas son convexas)
        filas.append((izq, der, [fila[x] if fila[x] else (1, 0) for x in range(xs[0], xs[-1] + 1)]))
    return (w, h, hx, hy, filas)


def loseta(base, variante):
    w, h = 65, 33   # como las losetas reales: rombo de 64x32 con el punto de anclaje en la esquina superior izquierda
    def pix(x, y):
        dx, dy = abs(x - 32) / 32.0, abs(y - 16) / 16.0
        if dx + dy > 1.0:
            return None
        ruido = ((x * 7 + y * 13 + variante * 31) % 5)
        if base == 2:   # agua: bandas onduladas
            ruido = (x // 6 + y // 3 + variante) % 4
        return (base * 4 + (ruido % 4), 0)
    return cuadro(w, h, 0, 0, pix)


def unidad(semilla):
    w, h = 32, 44
    def pix(x, y):
        if (x - 16) ** 2 / 100.0 + (y - 30) ** 2 / 169.0 < 1 and y > 14:
            return (64 + (semilla % 3), 1) if y < 28 else (5 * 4 + 1, 0)
        if (x - 16) ** 2 + (y - 9) ** 2 < 49:
            return (9 * 4 + 1, 0)
        return None
    return cuadro(w, h, 16, 40, pix)


def edificio(semilla, grande):
    w, h = (130, 100) if grande else (90, 70)
    def pix(x, y):
        dx = abs(x - w / 2) / (w / 2)
        if y < h * 0.45:
            return (5 * 4 + 2, 0) if dx < (y + 3) / (h * 0.45) else None
        if dx < 0.95:
            if y > h * 0.8 and (x // 6) % 2:
                return (64 + (semilla % 3), 1)
            return (6 * 4 + 1 + (x % 2), 0)
        return None
    return cuadro(w, h, w // 2, h - 6, pix)


def recurso(semilla):
    w, h = 36, 40
    def pix(x, y):
        if (x - 18) ** 2 / 300.0 + (y - 22) ** 2 / 300.0 < 1:
            return (7 * 4 + (semilla % 4), 0)
        return None
    return cuadro(w, h, 18, 34, pix)


def drs(elementos, tipo=b"slp "):
    """elementos: lista de (id, bytes). Devuelve el DRS con una tabla."""
    cab = (b"Copyright (c) AoEN sintetico (pruebas)".ljust(40, b"\0") + b"1.00" + b"tribe".ljust(12, b" "))
    cab += struct.pack("<II", 1, 64 + 12)   # número de listas (en el byte 56) y desplazamiento de la primera
    cab = cab.ljust(64, b"\0")
    n = len(elementos)
    tabla_off = 64 + 12
    datos_off = tabla_off + 12 * n
    lista = struct.pack("<4sII", tipo, tabla_off, n)
    tabla = b""
    datos = b""
    for (i, d) in sorted(elementos):
        tabla += struct.pack("<III", i, datos_off + len(datos), len(d))
        datos += d
    return cab + lista + tabla + datos


def generar(carpeta):
    os.makedirs(carpeta, exist_ok=True)
    gr = []
    for k, i in enumerate(UNIDADES):
        gr.append((i, slp([unidad(k + f) for f in range(2)])))
    for k, i in enumerate(sorted(set(EDIFICIOS))):
        gr.append((i, slp([edificio(k, i in (280, 177, 30, 77, 121))])))
    for k, i in enumerate(RECURSOS):
        gr.append((i, slp([recurso(k)])))
    for i in (2, 3, 4):   # llamas
        gr.append((i, slp([recurso(i)])))
    gr = list({i: d for i, d in gr}.items())
    tr = [(15000 + t, slp([loseta(b, v) for v in range(9)])) for t, b in enumerate((2, 1, 3, 4))]
    # orden de TileKind: DESERT, GRASS, WATER, DEEP -> bases 2 (arena), 1 (hierba), 3 (agua), 4 (agua honda)
    itf = [(50500, paleta())]
    escrituras = {"GRAPHICS.DRS": drs(gr), "TERRAIN.DRS": drs(tr), "INTERFAC.DRS": drs(itf), "SOUNDS.DRS": drs([])}
    for nombre, datos in escrituras.items():
        with open(os.path.join(carpeta, nombre), "wb") as f:
            f.write(datos)
    return escrituras


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stderr.write(__doc__)
        sys.exit(2)
    for k, v in generar(sys.argv[1]).items():
        print("%s: %d bytes" % (k, len(v)))
