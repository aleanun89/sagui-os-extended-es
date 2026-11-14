#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Cliente RFB mínimo y decodificadores (Raw, Hextile, ZRLE) en Python, comunes a h250 y h252.

No es una prueba: lo importan tests/h250_vnc.py (el servidor vncd) y tests/h252_vnc_tunel.py
(el escritorio remoto por ssh -L). Habla RFB 3.8 con «VNC Authentication».
"""
import os
import sys
import socket
import struct
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import FalhaTeste, SAIDA, ler_ppm  # noqa: E402

CLAVE = "secreto"


# ------------------------------------------------------------ formatos de píxel y decodificadores (Python)

class PF:
    def __init__(self, bpp=32, big=0, maxs=(255, 255, 255), desp=(16, 8, 0), prof=None):
        self.bpp, self.big, self.max, self.desp = bpp, big, maxs, desp
        self.prof = prof or (24 if bpp == 32 else bpp)

    @property
    def nb(self):
        return self.bpp // 8

    def valor(self, rgb):
        r, g, b = (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255
        return (((r * self.max[0] + 127) // 255) << self.desp[0]) | (((g * self.max[1] + 127) // 255) << self.desp[1]) \
            | (((b * self.max[2] + 127) // 255) << self.desp[2])

    def cp(self):
        """Bytes de un CPIXEL y si va en los tres bytes altos."""
        usados = sum(m << d for m, d in zip(self.max, self.desp))
        if self.bpp == 32 and self.prof <= 24:
            if not usados & 0xFF000000:
                return 3, False
            if not usados & 0xFF:
                return 3, True
        return self.nb, False

    def pix(self, datos, pos):
        v = int.from_bytes(datos[pos:pos + self.nb], "big" if self.big else "little")
        return v

    def cpix(self, datos, pos):
        n, alto = self.cp()
        if n == self.nb:
            return self.pix(datos, pos), n
        v = int.from_bytes(datos[pos:pos + 3], "big" if self.big else "little")
        return (v << 8 if alto else v), 3

    def mensaje(self):
        return struct.pack(">BBBBHHHBBB3x", self.bpp, self.prof, self.big, 1, *self.max, *self.desp)


NATIVO = PF()
FORMATOS = [
    ("32 nativo", PF()),
    ("32 big endian", PF(32, 1)),
    ("32 colores en los bytes altos", PF(32, 0, (255, 255, 255), (24, 16, 8))),
    ("16 RGB565", PF(16, 0, (31, 63, 31), (11, 5, 0))),
    ("16 RGB555 big endian", PF(16, 1, (31, 31, 31), (10, 5, 0), 15)),
    ("8 BGR233", PF(8, 0, (7, 7, 3), (0, 3, 6))),
]


def dec_raw(datos, w, h, pf):
    return [pf.pix(datos, i * pf.nb) for i in range(w * h)]


def dec_hextile(datos, w, h, pf):
    img = [0] * (w * h)
    pos = 0
    fondo = fg = 0
    for ty in range(0, h, 16):
        for tx in range(0, w, 16):
            tw, th = min(16, w - tx), min(16, h - ty)
            fl = datos[pos]
            pos += 1
            if fl & 1:
                for y in range(th):
                    for x in range(tw):
                        img[(ty + y) * w + tx + x] = pf.pix(datos, pos)
                        pos += pf.nb
                continue
            if fl & 2:
                fondo = pf.pix(datos, pos)
                pos += pf.nb
            if fl & 4:
                fg = pf.pix(datos, pos)
                pos += pf.nb
            for y in range(th):
                for x in range(tw):
                    img[(ty + y) * w + tx + x] = fondo
            if fl & 8:
                n = datos[pos]
                pos += 1
                for _ in range(n):
                    c = fg
                    if fl & 16:
                        c = pf.pix(datos, pos)
                        pos += pf.nb
                    xy, wh = datos[pos], datos[pos + 1]
                    pos += 2
                    sx, sy, sw, sh = xy >> 4, xy & 15, (wh >> 4) + 1, (wh & 15) + 1
                    for y in range(sh):
                        for x in range(sw):
                            img[(ty + sy + y) * w + tx + sx + x] = c
    assert pos == len(datos), "hextile: sobran o faltan bytes"
    return img


def dec_zrle(datos, w, h, pf):
    img = [0] * (w * h)
    pos = 0
    for ty in range(0, h, 64):
        for tx in range(0, w, 64):
            tw, th = min(64, w - tx), min(64, h - ty)
            n = tw * th
            sub = datos[pos]
            pos += 1
            pix = []
            if sub == 0:
                for _ in range(n):
                    v, k = pf.cpix(datos, pos)
                    pos += k
                    pix.append(v)
            elif sub == 1:
                v, k = pf.cpix(datos, pos)
                pos += k
                pix = [v] * n
            elif 2 <= sub <= 16:
                pal = []
                for _ in range(sub):
                    v, k = pf.cpix(datos, pos)
                    pos += k
                    pal.append(v)
                bits = 1 if sub == 2 else 2 if sub <= 4 else 4
                for y in range(th):
                    acum, nb = 0, 0
                    fila = []
                    for x in range(tw):
                        if nb == 0:
                            acum, nb = datos[pos], 8
                            pos += 1
                        nb -= bits
                        fila.append(pal[(acum >> nb) & ((1 << bits) - 1)])
                    pix += fila
            elif sub == 128:
                while len(pix) < n:
                    v, k = pf.cpix(datos, pos)
                    pos += k
                    ln = 1
                    while datos[pos] == 255:
                        ln += 255
                        pos += 1
                    ln += datos[pos]
                    pos += 1
                    pix += [v] * ln
            elif sub >= 130:
                pal = []
                for _ in range(sub - 128):
                    v, k = pf.cpix(datos, pos)
                    pos += k
                    pal.append(v)
                while len(pix) < n:
                    idx = datos[pos]
                    pos += 1
                    ln = 1
                    if idx & 128:
                        idx &= 127
                        while datos[pos] == 255:
                            ln += 255
                            pos += 1
                        ln += datos[pos]
                        pos += 1
                    pix += [pal[idx]] * ln
            else:
                raise AssertionError(f"subcodificación ZRLE {sub}")
            assert len(pix) == n, "ZRLE: la baldosa no tiene los píxeles que debe"
            for y in range(th):
                img[(ty + y) * w + tx:(ty + y) * w + tx + tw] = pix[y * tw:(y + 1) * tw]
    assert pos == len(datos), "ZRLE: sobran o faltan bytes"
    return img


def vnc_respuesta(pw, reto):
    from cryptography.hazmat.primitives.ciphers import Cipher, modes
    from cryptography.hazmat.primitives.ciphers.algorithms import TripleDES
    k = bytearray(8)
    for i, c in enumerate(pw.encode()[:8]):
        k[i] = int(format(c, "08b")[::-1], 2)
    return Cipher(TripleDES(bytes(k) * 3), modes.ECB()).encryptor().update(reto)


# ------------------------------------------------------------ cliente RFB mínimo

class Rfb:
    def __init__(self, porta, clave=CLAVE, version=b"003.008", saludar=True, tempo=30):
        self.s = socket.create_connection(("127.0.0.1", porta), timeout=tempo)
        self.s.settimeout(tempo)
        self.pf = NATIVO
        self.fb = None
        self.w = self.h = 0
        self.zd = zlib.decompressobj()
        self.nrect = 0
        self.ultimos = []
        if saludar:
            self.saludo(clave, version)

    def leer(self, n):
        b = b""
        while len(b) < n:
            x = self.s.recv(n - len(b))
            if not x:
                raise EOFError("el servidor cerró la conexión")
            b += x
        return b

    def saludo(self, clave, version=b"003.008"):
        self.ver_servidor = self.leer(12)
        if not self.ver_servidor.startswith(b"RFB 003."):
            raise FalhaTeste(f"saludo inesperado: {self.ver_servidor!r}")
        self.s.sendall(b"RFB " + version + b"\n")
        self.tipos = None
        if version == b"003.003":
            t = struct.unpack(">I", self.leer(4))[0]
            if t == 0:
                self.motivo = self.leer(struct.unpack(">I", self.leer(4))[0])
                raise PermissionError(self.motivo)
            self.tipos = [t]
        else:
            n = self.leer(1)[0]
            if n == 0:
                self.motivo = self.leer(struct.unpack(">I", self.leer(4))[0])
                raise PermissionError(self.motivo)
            self.tipos = list(self.leer(n))
            self.s.sendall(bytes([2]))
        reto = self.leer(16)
        self.s.sendall(vnc_respuesta(clave, reto))
        r = struct.unpack(">I", self.leer(4))[0]
        if r != 0:
            self.motivo = b""
            if version == b"003.008":
                self.motivo = self.leer(struct.unpack(">I", self.leer(4))[0])
            raise PermissionError(self.motivo or b"fallo de autenticacion")
        self.s.sendall(b"\x01")   # ClientInit, compartido
        sw, sh = struct.unpack(">HH", self.leer(4))
        self.w, self.h = sw, sh
        pf = self.leer(16)
        bpp, prof, big, tc, rm, gm, bm, rs, gs, bs = struct.unpack(">BBBBHHHBBB", pf[:13])
        self.pf_servidor = PF(bpp, big, (rm, gm, bm), (rs, gs, bs), prof)
        self.pf = self.pf_servidor
        self.nombre = self.leer(struct.unpack(">I", self.leer(4))[0])
        self.fb = [0] * (self.w * self.h)

    def cerrar(self):
        try:
            self.s.close()
        except OSError:
            pass

    def set_pf(self, pf):
        self.s.sendall(b"\x00\x00\x00\x00" + pf.mensaje())
        self.pf = pf

    def set_encodings(self, lista):
        self.s.sendall(struct.pack(">BxH", 2, len(lista)) + b"".join(struct.pack(">i", e) for e in lista))

    def pedir(self, incremental, x=0, y=0, w=None, h=None):
        self.s.sendall(struct.pack(">BBHHHH", 3, 1 if incremental else 0, x, y, w if w is not None else self.w, h if h is not None else self.h))

    def tecla(self, ks, abajo=True):
        self.s.sendall(struct.pack(">BBxxI", 4, 1 if abajo else 0, ks))

    def raton(self, mascara, x, y):
        self.s.sendall(struct.pack(">BBHH", 5, mascara, x, y))

    def cut_text(self, texto):
        self.s.sendall(struct.pack(">B3xI", 6, len(texto)) + texto)

    def leer_actualizacion(self, tempo=30):
        """Una FramebufferUpdate; aplica los rectángulos a self.fb. Devuelve (bytes recibidos, lista de rectángulos)."""
        self.s.settimeout(tempo)
        cab = self.leer(4)
        if cab[0] != 0:
            raise FalhaTeste(f"mensaje del servidor inesperado: {cab[0]}")
        n = struct.unpack(">H", cab[2:])[0]
        total = 4
        rects = []
        for _ in range(n):
            x, y, w, h, enc = struct.unpack(">HHHHi", self.leer(12))
            total += 12
            if enc == -223:
                self.w, self.h = w, h
                self.fb = [0] * (w * h)
                rects.append((x, y, w, h, enc))
                continue
            if enc == 0:
                datos = self.leer(w * h * self.pf.nb)
                total += len(datos)
                pix = dec_raw(datos, w, h, self.pf)
            elif enc == 5:
                datos = self.leer_hextile(w, h)
                total += len(datos)
                pix = dec_hextile(datos, w, h, self.pf)
            elif enc == 16:
                ln = struct.unpack(">I", self.leer(4))[0]
                z = self.leer(ln)
                total += 4 + ln
                pix = dec_zrle(self.zd.decompress(z), w, h, self.pf)
            else:
                raise FalhaTeste(f"codificación inesperada {enc}")
            for fy in range(h):
                self.fb[(y + fy) * self.w + x:(y + fy) * self.w + x + w] = pix[fy * w:(fy + 1) * w]
            rects.append((x, y, w, h, enc))
        self.nrect += n
        self.ultimos = rects
        return total, rects

    def leer_hextile(self, w, h):
        """Hextile no lleva longitud: se lee baldosa a baldosa siguiendo el formato."""
        out = b""
        nb = self.pf.nb
        for ty in range(0, h, 16):
            for tx in range(0, w, 16):
                tw, th = min(16, w - tx), min(16, h - ty)
                fl = self.leer(1)
                out += fl
                f = fl[0]
                if f & 1:
                    out += self.leer(tw * th * nb)
                    continue
                if f & 2:
                    out += self.leer(nb)
                if f & 4:
                    out += self.leer(nb)
                if f & 8:
                    c = self.leer(1)
                    out += c
                    out += self.leer(c[0] * ((nb if f & 16 else 0) + 2))
        return out

    def rgb(self):
        """La imagen actual en 0xRRGGBB (para compararla)."""
        pf = self.pf
        sal = []
        for v in self.fb:
            r = ((v >> pf.desp[0]) & pf.max[0]) * 255 // pf.max[0]
            g = ((v >> pf.desp[1]) & pf.max[1]) * 255 // pf.max[1]
            b = ((v >> pf.desp[2]) & pf.max[2]) * 255 // pf.max[2]
            sal.append(r << 16 | g << 8 | b)
        return sal


def banner(porta, tempo=4):
    """Lo primero que manda quien escucha en el puerto (b'' = nadie o rechazado)."""
    try:
        c = socket.create_connection(("127.0.0.1", porta), timeout=tempo)
    except OSError:
        return b""
    c.settimeout(tempo)
    try:
        return c.recv(100)
    except OSError:
        return b""
    finally:
        c.close()


def pantalla_qemu(m, nome):
    ppm = os.path.join(SAIDA, nome + ".ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    w, h, px = ler_ppm(ppm)
    return w, h, [px[i * 3] << 16 | px[i * 3 + 1] << 8 | px[i * 3 + 2] for i in range(w * h)]


def diferentes(a, b):
    return sum(1 for x, y in zip(a, b) if x != y)


