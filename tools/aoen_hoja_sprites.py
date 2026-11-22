#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Hoja de contacto de sprites de AoEN (herramienta del PC, para comprobar el mapeo unidad -> SLP).

Lee GRAPHICS.DRS e INTERFAC.DRS del AoE original (datos comerciales: la hoja resultante NO se versiona, va a
/home/.../private/aoen/sprites/) y escribe un PNG con una fila por unidad: el cuadro central de cada animacion
(quieta, andar, atacar, morir, descomposicion) con su identificador SLP. Se puede pasar el conjunto de unidades
por --tabla (fichero con lineas "nombre: id id id id id"); sin el usa la tabla unit_art_ids de render.c.

Uso: aoen_hoja_sprites.py DIR_DATOS SALIDA.png [--tabla f] [--cuadro n] [--slp id ...]
"""
import os, re, struct, sys, zlib


def leer_drs(ruta):
    d = open(ruta, 'rb').read()
    nl = struct.unpack_from('<I', d, 56)[0]
    items = {}
    for i in range(nl):
        _, off, n = struct.unpack_from('<III', d, 64 + i * 12)
        for j in range(n):
            ident, o, tam = struct.unpack_from('<III', d, off + j * 12)
            items[ident] = (o, tam)
    return d, items


def sacar(drs, ident):
    d, items = drs
    if ident not in items:
        return None
    o, t = items[ident]
    return d[o:o + t]


def paleta(drs, ident=50500):
    txt = sacar(drs, ident).decode('latin-1').split('\n')
    return [tuple(int(x) for x in txt[3 + i].split()[:3]) for i in range(256)]


def num_cuadros(s):
    return struct.unpack_from('<i', s, 4)[0] if s and len(s) >= 32 else 0


def _cuenta(c, pos, n):
    v = c[pos] >> n
    if v:
        return v, pos
    pos += 1
    return (c[pos] if pos < len(c) else 0), pos


def decodificar(s, idx):
    """Misma logica que slp_decode de data.c. Devuelve (w, h, hx, hy, filas de indices; None = transparente)."""
    fi = 32 + idx * 32
    cmd_off, out_off, _, _, w, h, hx, hy = struct.unpack_from('<IIIIiiii', s, fi)
    if w <= 0 or h <= 0 or w > 2048 or h > 2048:
        return None
    px = [[None] * w for _ in range(h)]
    c = s[cmd_off + h * 4:] + b'\0' * 8
    n = len(c) - 8
    pos = 0
    for y in range(h):
        l, r = struct.unpack_from('<hh', s, out_off + y * 4)
        if l == -32768 or r == -32768 or l < 0 or l > w or r < 0:
            continue
        x = l
        fin = l + (w - l - r)
        i = l
        while i <= fin and pos < n:
            bc = c[pos]
            k = bc & 0xf
            if k == 0xf:
                i = fin
            elif k in (0x0a, 0x06, 0x07):
                cnt, pos = _cuenta(c, pos, 4)
                if k == 0x06:
                    while cnt and x < w and pos + 1 < n:
                        pos += 1; px[y][x] = c[pos]; x += 1; cnt -= 1
                else:
                    pos += 1
                    while cnt and x < w:
                        px[y][x] = c[pos]; x += 1; cnt -= 1
            elif k == 0x03:
                pos += 1; x += c[pos]
            elif k == 0x02:
                cnt = ((bc & 0xf0) << 4) + c[pos + 1]
                pos += 1
                while cnt and x < w and pos + 1 < n:
                    pos += 1; px[y][x] = c[pos]; x += 1; cnt -= 1
            elif k in (0x0d, 0x0b, 0x09, 0x05, 0x01):
                cnt, pos = _cuenta(c, pos, 2)
                x += cnt
            elif k in (0x0c, 0x08, 0x04, 0x00):
                cnt, pos = _cuenta(c, pos, 2)
                while cnt and x < w and pos + 1 < n:
                    pos += 1; px[y][x] = c[pos]; x += 1; cnt -= 1
            elif k == 0x0e and (bc & 0xf0) in (0x40, 0x60):
                if x < w and pos + 1 < n:
                    pos += 1; px[y][x] = c[pos]; x += 1
            else:
                while pos < n and c[pos] != 0x0f:
                    pos += 1
                i = fin
            pos += 1
            i += 1
    return w, h, hx, hy, px


def escribir_png(ruta, w, h, rgb):
    filas = b''.join(b'\0' + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))

    def chunk(t, b):
        x = struct.pack('>I', len(b)) + t + b
        return x + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    open(ruta, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(filas, 6)) + chunk(b'IEND', b''))


FUENTE = {'0': '111101101101111', '1': '010110010010111', '2': '111001111100111', '3': '111001111001111',
          '4': '101101111001001', '5': '111100111001111', '6': '111100111101111', '7': '111001001001001',
          '8': '111101111101111', '9': '111101111001111'}


class Lienzo:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.b = bytearray(bytes((60, 60, 60)) * (w * h))

    def sprite(self, spr, pal, cx, cy, maxw, maxh):
        w, h, hx, hy, px = spr
        f = max(1, -(-w // maxw), -(-h // maxh))   # escala entera hacia abajo si no cabe
        ox, oy = cx - (w // f) // 2, cy - (h // f)
        for y in range(0, h, f):
            for x in range(0, w, f):
                v = px[y][x]
                if v is None:
                    continue
                X, Y = ox + x // f, oy + y // f
                if 0 <= X < self.w and 0 <= Y < self.h:
                    o = (Y * self.w + X) * 3
                    self.b[o:o + 3] = bytes(pal[v])

    def texto(self, x, y, t):
        for ch in t:
            g = FUENTE.get(ch)
            if g:
                for i, bit in enumerate(g):
                    if bit == '1':
                        X, Y = x + i % 3, y + i // 3
                        if 0 <= X < self.w and 0 <= Y < self.h:
                            o = (Y * self.w + X) * 3
                            self.b[o:o + 3] = bytes((255, 255, 0))
            x += 4


def tabla_de_render_c(ruta):
    src = open(ruta, encoding='utf-8').read()
    m = re.search(r'unit_art_ids\[NUM_UNIT_ART\]\[5\] = \{(.*?)\n\};', src, re.S)
    filas = []
    for l in m.group(1).split('\n'):
        mm = re.match(r'\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\},\s*/\*\s*(.*?)\s*\*/', l)
        if mm:
            filas.append((mm.group(6), [int(mm.group(i)) for i in range(1, 6)]))
    return filas


def main():
    a = sys.argv[1:]
    if len(a) < 2:
        print(__doc__)
        return 2
    datos, salida = a[0], a[1]
    tabla, slps, cuadro, i = None, [], None, 2
    while i < len(a):
        if a[i] == '--tabla':
            tabla = []
            for l in open(a[i + 1], encoding='utf-8'):
                if ':' in l:
                    n, v = l.rsplit(':', 1)
                    tabla.append((n.strip(), [int(x) for x in v.split()]))
            i += 2
        elif a[i] == '--slp':
            slps = [int(x) for x in a[i + 1:]]
            break
        elif a[i] == '--cuadro':
            cuadro = int(a[i + 1]); i += 2
        else:
            i += 1

    def abrir(n):
        for dd in ('', 'DATA/', 'Data/', 'data/'):
            for nn in (n, n.lower()):
                p = os.path.join(datos, dd + nn)
                if os.path.exists(p):
                    return leer_drs(p)
        raise SystemExit('no encuentro ' + n)
    gr, it = abrir('GRAPHICS.DRS'), abrir('INTERFAC.DRS')
    pal = paleta(it)
    if slps:
        tabla = [(str(s), [s]) for s in slps]
    if tabla is None:
        tabla = tabla_de_render_c(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'userland/juegos/aoen/render.c'))
    cw, ch = 110, 120
    cols = max(len(v) for _, v in tabla)
    L = Lienzo(cols * cw + 150, len(tabla) * ch)
    for r, (nom, ids) in enumerate(tabla):
        L.texto(2, r * ch + 4, str(r))
        for c, ident in enumerate(ids):
            s = sacar(gr, ident)
            nf = num_cuadros(s)
            if nf:
                n = max(1, nf // 5)
                f = cuadro if cuadro is not None else min(nf - 1, 2 * n + n // 2)
                spr = decodificar(s, f)
                if spr:
                    L.sprite(spr, pal, 150 + c * cw + cw // 2, r * ch + ch - 22, cw - 6, ch - 30)
            L.texto(150 + c * cw + 4, r * ch + ch - 8, '%d' % ident)
    escribir_png(salida, L.w, L.h, L.b)
    with open(salida + '.txt', 'w', encoding='utf-8') as f:
        for r, (nom, ids) in enumerate(tabla):
            f.write('%d %s %s\n' % (r, nom, ids))
    print('escrito', salida, L.w, 'x', L.h)


if __name__ == '__main__':
    sys.exit(main())
