# SPDX-License-Identifier: GPL-3.0-only
"""Lector mínimo de PNG (RGB/RGBA de 8 bits) para las pruebas de estilos (D428): decodifica una captura y suma píxeles.

Solo usa zlib: no hace falta ImageMagick (en este entorno `convert` es un sustituto sin comparación)."""
import struct
import zlib


def leer(datos):
    """datos: bytes de un PNG o una ruta. Devuelve (ancho, alto, bytes RGB)."""
    if isinstance(datos, str):
        with open(datos, "rb") as h:
            datos = h.read()
    i, idat, l, a, tipo = 8, b"", 0, 0, 2
    while i < len(datos):
        n, t = struct.unpack(">I4s", datos[i:i + 8])
        c = datos[i + 8:i + 8 + n]
        i += 12 + n
        if t == b"IHDR":
            l, a, _prof, tipo = struct.unpack(">IIBB", c[:10])
        elif t == b"IDAT":
            idat += c
    bpp = {2: 3, 6: 4, 0: 1, 4: 2}[tipo]
    raw = zlib.decompress(idat)
    st = l * bpp
    out = bytearray()
    prev = bytearray(st)
    k = 0
    for _ in range(a):
        f = raw[k]
        lin = bytearray(raw[k + 1:k + 1 + st])
        k += 1 + st
        if f == 1:
            for x in range(bpp, st):
                lin[x] = (lin[x] + lin[x - bpp]) & 255
        elif f == 2:
            for x in range(st):
                lin[x] = (lin[x] + prev[x]) & 255
        elif f == 3:
            for x in range(st):
                e = lin[x - bpp] if x >= bpp else 0
                lin[x] = (lin[x] + (e + prev[x]) // 2) & 255
        elif f == 4:
            for x in range(st):
                e = lin[x - bpp] if x >= bpp else 0
                ar = prev[x]
                ae = prev[x - bpp] if x >= bpp else 0
                pp = e + ar - ae
                pa, pb, pc = abs(pp - e), abs(pp - ar), abs(pp - ae)
                lin[x] = (lin[x] + (e if pa <= pb and pa <= pc else ar if pb <= pc else ae)) & 255
        out += lin
        prev = lin
    if bpp == 3:
        return l, a, bytes(out)
    rgb = bytearray()
    for j in range(l * a):
        rgb += bytes(out[j * bpp:j * bpp + 3]) if bpp >= 3 else bytes([out[j * bpp]] * 3)
    return l, a, bytes(rgb)


def pixel(img, x, y):
    l, _a, rgb = img
    o = (y * l + x) * 3
    return rgb[o], rgb[o + 1], rgb[o + 2]


def suma(img, x0=0, y0=0, x1=None, y1=None):
    """Suma de los valores de los píxeles de un rectángulo (la «suma de píxeles» de las comparaciones)."""
    l, a, rgb = img
    x1 = l if x1 is None else x1
    y1 = a if y1 is None else y1
    return sum(sum(rgb[(y * l + x0) * 3:(y * l + x1) * 3]) for y in range(y0, y1))


def diferencias(a, b):
    """Número de píxeles distintos entre dos imágenes del mismo tamaño (None si el tamaño difiere)."""
    if a[0] != b[0] or a[1] != b[1]:
        return None
    n = 0
    ra, rb = a[2], b[2]
    if ra == rb:
        return 0
    for i in range(0, len(ra), 3):
        if ra[i:i + 3] != rb[i:i + 3]:
            n += 1
    return n


def contraste(c1, c2):
    """Relación de contraste WCAG entre dos colores 0xRRGGBB."""
    def lum(c):
        t = []
        for sh in (16, 8, 0):
            v = ((c >> sh) & 255) / 255
            t.append(v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4)
        return 0.2126 * t[0] + 0.7152 * t[1] + 0.0722 * t[2]
    a, b = lum(c1), lum(c2)
    if a < b:
        a, b = b, a
    return (a + 0.05) / (b + 0.05)
