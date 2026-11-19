#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Texturas HD SINTÉTICAS para AoEN (D393): las seis PNG de 512x512 que busca el terreno HD
(terrain_hd.c: g_gr2, g_gr6, g_des, g_pal, g_wt5 y g_wt3 con el sufijo _00_color.png), hechas aquí
con ruido y ondas en Python puro. No tienen nada de ninguna textura de terceros: sirven para que las
pruebas y las medidas (h398) ejerciten el terreno HD sin los datos del mantenedor.

    python3 tools/aoen_texturas_sintetico.py CARPETA
"""
import math
import os
import struct
import sys
import zlib

LADO = 512

# color base y amplitud del ruido de cada textura
TEXTURAS = {
    "g_gr2": ((70, 120, 45), 28),
    "g_gr6": ((95, 130, 50), 30),
    "g_des": ((196, 170, 112), 22),
    "g_pal": ((180, 150, 100), 26),
    "g_wt5": ((40, 90, 150), 18),
    "g_wt3": ((25, 55, 115), 14),
}


def png(ruta, ancho, alto, filas):
    def trozo(tipo, datos):
        c = struct.pack(">I", len(datos)) + tipo + datos
        return c + struct.pack(">I", zlib.crc32(tipo + datos) & 0xFFFFFFFF)
    crudo = b"".join(b"\x00" + f for f in filas)
    with open(ruta, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(trozo(b"IHDR", struct.pack(">IIBBBBB", ancho, alto, 8, 2, 0, 0, 0)))
        f.write(trozo(b"IDAT", zlib.compress(crudo, 6)))
        f.write(trozo(b"IEND", b""))


def textura(semilla, base, amp):
    # ruido de valor en una rejilla de 16x16 que se repite (la textura no tiene costuras) y ondas suaves
    n = 16
    paso = LADO // n
    rej = [[((i * 73856093) ^ (j * 19349663) ^ (semilla * 83492791)) % 1000 / 1000.0 for i in range(n)] for j in range(n)]
    filas = []
    for y in range(LADO):
        gy, fy = divmod(y, paso)
        ty = fy / paso
        ty = ty * ty * (3 - 2 * ty)
        r0, r1 = rej[gy % n], rej[(gy + 1) % n]
        onda_y = math.sin(y * 2 * math.pi * 3 / LADO + semilla)
        fila = bytearray(LADO * 3)
        for x in range(LADO):
            gx, fx = divmod(x, paso)
            tx = fx / paso
            tx = tx * tx * (3 - 2 * tx)
            a, b = r0[gx % n], r0[(gx + 1) % n]
            c, d = r1[gx % n], r1[(gx + 1) % n]
            v = (a + (b - a) * tx) * (1 - ty) + (c + (d - c) * tx) * ty
            v += 0.25 * onda_y * math.sin(x * 2 * math.pi * 5 / LADO)
            v += ((x * 7 + y * 13 + semilla) % 5) * 0.04   # grano fino
            k = int((v - 0.5) * 2 * amp)
            o = x * 3
            fila[o] = max(0, min(255, base[0] + k))
            fila[o + 1] = max(0, min(255, base[1] + k))
            fila[o + 2] = max(0, min(255, base[2] + k // 2))
        filas.append(bytes(fila))
    return filas


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    d = sys.argv[1]
    os.makedirs(d, exist_ok=True)
    for k, (nombre, (base, amp)) in enumerate(sorted(TEXTURAS.items())):
        png(os.path.join(d, nombre + "_00_color.png"), LADO, LADO, textura(k + 1, base, amp))
    print("texturas sintéticas en", d)
    return 0


if __name__ == "__main__":
    sys.exit(main())
