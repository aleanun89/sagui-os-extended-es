#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H253 (D254, mejora 097 fase 2) - cliente de escritorio remoto VNC: vnc.

En el anfitrión (tests/vnc_cliente_unidad.c, el mismo código que va al SaguiOS):
  1. el inflado continuo (zinflar) contra zlib de Python: un solo flujo, trozo a trozo, con
     bloques fijos, dinámicos y guardados, y el diccionario que pasa de un trozo al siguiente;
  2. los decodificadores Raw, CopyRect, Hextile, ZRLE y Tight (relleno, copy, palette de 2 y de
     varios colores, gradient, JPEG) con 6 formatos de píxel y datos que llegan en trozos de
     1, 7 o 100 bytes; datos truncados o corruptos dan error (también con ASan y UBSan);
  3. la forma del cursor, el JPEG de la libsagui contra la referencia y las teclas.

En QEMU (red de usuario: el servidor RFB mínimo de esta prueba, en Python, está en 10.0.2.2):
  - RFB 3.8 con «VNC Authentication» (--clave-fichero): el cliente manda la versión, elige la seguridad,
    contesta el reto, SetPixelFormat (32 bpp) y SetEncodings con Tight, ZRLE, Hextile, CopyRect, Raw,
    Cursor y DesktopSize;
  - una imagen conocida enviada con Raw, Hextile, ZRLE y Tight (relleno, copy corto y largo, palette de
    2 y de 8 colores, gradient y JPEG), más CopyRect: la ventana la pinta igual (comparada píxel a píxel
    con la captura de QEMU; el JPEG, con la salida del decodificador de la libsagui);
  - las teclas y los clics llegan al servidor como los keysyms y las posiciones esperadas, con el
    botón derecho; la rueda;
  - la forma del cursor que manda el servidor se dibuja sobre la imagen; DesktopSize cambia el tamaño;
  - si el servidor cierra, aviso en la ventana y «R» reconecta;
  - contraseña mala (mensaje de error) y contraseña pedida en un cuadro de la ventana;
  - RFB 3.3 con seguridad «None», RFB 3.7, --color 16 y --color 8, --solo-ver (no manda nada);
  - un escritorio mayor que la ventana se escala y el clic se traduce a la posición remota;
  - contra el vncd de este mismo SaguiOS (127.0.0.1) con ZRLE.
"""
import os
import random
import re
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste, ler_ppm  # noqa: E402
from h250_vnc import PF, vnc_respuesta  # noqa: E402

TRAB = os.path.join(SAIDA, "h253")
CLAVE = "secreto"
BORDA, TITULO = 2, 24
BARRA = 26   # la barra de abajo del cliente (BARRA_A de vnc.c)
JPEG = os.path.join(RAIZ, "tests", "datos", "vnc_prueba.jpg")

PF32 = PF()
PF16 = PF(16, 0, (31, 63, 31), (11, 5, 0))
PF8 = PF(8, 0, (7, 7, 3), (5, 2, 0))
FORMATOS = [
    ("32 nativo", PF()),
    ("32 big endian", PF(32, 1)),
    ("32 colores en los bits altos", PF(32, 0, (255, 255, 255), (24, 16, 8))),
    ("16 bpp 565", PF16),
    ("16 bpp 555 big endian", PF(16, 1, (31, 31, 31), (10, 5, 0))),
    ("8 bpp 332", PF8),
]


# ------------------------------------------------------------ píxeles, codificadores (el lado del servidor)

def canal(pf, rgb):
    return [(((rgb >> s) & 255) * pf.max[i] + 127) // 255 for i, s in enumerate((16, 8, 0))]


def visible(pf, rgb):
    """El color que el cliente ve para este rgb con el formato dado."""
    out = 0
    for i, v in enumerate(canal(pf, rgb)):
        out = out << 8 | ((v * 255 + pf.max[i] // 2) // pf.max[i])
    return out


def pix(pf, rgb):
    return pf.valor(rgb).to_bytes(pf.nb, "big" if pf.big else "little")


def tpix3(pf):
    return pf.bpp == 32 and pf.prof == 24 and pf.max == (255, 255, 255)


def tpix(pf, rgb):
    if tpix3(pf):
        return bytes([(rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255])
    return pix(pf, rgb)


def cpix(pf, rgb):
    n, alto = pf.cp()
    if n != 3:
        return pix(pf, rgb)
    v = pf.valor(rgb)
    if alto:
        v >>= 8
    v &= 0xFFFFFF
    return v.to_bytes(3, "big" if pf.big else "little")


def raw(pf, img, w, h):
    return b"".join(pix(pf, c) for c in img)


def hextile(pf, img, w, h):
    out = bytearray()
    bg_ant = None
    for ty in range(0, h, 16):
        for tx in range(0, w, 16):
            tw, th = min(16, w - tx), min(16, h - ty)
            t = [img[(ty + y) * w + tx + x] for y in range(th) for x in range(tw)]
            cuenta = {}
            for c in t:
                cuenta[c] = cuenta.get(c, 0) + 1
            bg = max(cuenta, key=lambda k: cuenta[k])
            subs = []
            hecho = [False] * len(t)
            for y in range(th):
                for x in range(tw):
                    if t[y * tw + x] == bg or hecho[y * tw + x]:
                        continue
                    c = t[y * tw + x]
                    rw = 1
                    while x + rw < tw and t[y * tw + x + rw] == c and not hecho[y * tw + x + rw]:
                        rw += 1
                    rh = 1
                    while y + rh < th and all(t[(y + rh) * tw + x + k] == c and not hecho[(y + rh) * tw + x + k] for k in range(rw)):
                        rh += 1
                    for yy in range(rh):
                        for xx in range(rw):
                            hecho[(y + yy) * tw + x + xx] = True
                    subs.append((c, x, y, rw, rh))
            tam_raw = 1 + tw * th * pf.nb
            pon_bg = bg != bg_ant
            colores = len({s[0] for s in subs})
            # con un solo color de primer plano se manda este una vez (bandera 4)
            if len(subs) <= 255 and colores <= 1:
                fl = (2 if pon_bg else 0) | (4 if subs else 0) | (8 if subs else 0)
                cuerpo = bytearray([fl])
                if pon_bg:
                    cuerpo += pix(pf, bg)
                if subs:
                    cuerpo += pix(pf, subs[0][0]) + bytes([len(subs)])
                    for c, x, y, rw, rh in subs:
                        cuerpo += bytes([x << 4 | y, (rw - 1) << 4 | (rh - 1)])
                if len(cuerpo) < tam_raw:
                    out += cuerpo
                    bg_ant = bg
                    continue
            elif len(subs) <= 255:
                cuerpo = bytearray([(2 if pon_bg else 0) | 8 | 16])
                if pon_bg:
                    cuerpo += pix(pf, bg)
                cuerpo += bytes([len(subs)])
                for c, x, y, rw, rh in subs:
                    cuerpo += pix(pf, c) + bytes([x << 4 | y, (rw - 1) << 4 | (rh - 1)])
                if len(cuerpo) < tam_raw:
                    out += cuerpo
                    bg_ant = bg
                    continue
            out += b"\x01" + b"".join(pix(pf, c) for c in t)
    return bytes(out)


def zrle_datos(pf, img, w, h, modo="auto"):
    out = bytearray()
    k = 0
    for ty in range(0, h, 64):
        for tx in range(0, w, 64):
            tw, th = min(64, w - tx), min(64, h - ty)
            t = [img[(ty + y) * w + tx + x] for y in range(th) for x in range(tw)]
            pal = []
            for c in t:
                if c not in pal:
                    pal.append(c)
                if len(pal) > 127:
                    break
            elegir = modo if modo != "auto" else ("raw", "rle", "paleta", "rlepaleta")[k % 4]
            k += 1
            if len(pal) == 1:
                out += b"\x01" + cpix(pf, pal[0])
                continue
            if elegir == "paleta" and 2 <= len(pal) <= 16:
                np_ = len(pal)
                bits = 1 if np_ == 2 else 2 if np_ <= 4 else 4
                out += bytes([np_]) + b"".join(cpix(pf, c) for c in pal)
                for y in range(th):
                    acum, nb = 0, 0
                    for x in range(tw):
                        acum = acum << bits | pal.index(t[y * tw + x])
                        nb += bits
                        if nb == 8:
                            out.append(acum)
                            acum, nb = 0, 0
                    if nb:
                        out.append(acum << (8 - nb))
                continue
            if elegir == "rlepaleta" and 2 <= len(pal) <= 127:
                out += bytes([128 + len(pal)]) + b"".join(cpix(pf, c) for c in pal)
                i = 0
                while i < len(t):
                    j = i + 1
                    while j < len(t) and t[j] == t[i]:
                        j += 1
                    ln = j - i
                    idx = pal.index(t[i])
                    if ln == 1:
                        out.append(idx)
                    else:
                        out.append(idx | 128)
                        ln -= 1
                        while ln >= 255:
                            out.append(255)
                            ln -= 255
                        out.append(ln)
                    i = j
                continue
            if elegir == "rle" or (elegir in ("paleta", "rlepaleta")):
                out.append(128)
                i = 0
                while i < len(t):
                    j = i + 1
                    while j < len(t) and t[j] == t[i]:
                        j += 1
                    ln = j - i - 1
                    out += cpix(pf, t[i])
                    while ln >= 255:
                        out.append(255)
                        ln -= 255
                    out.append(ln)
                    i = j
                continue
            out.append(0)
            out += b"".join(cpix(pf, c) for c in t)
    return bytes(out)


def zrle(pf, img, w, h, zs, modo="auto"):
    """Un rectángulo ZRLE con el flujo zlib continuo zs (compressobj)."""
    datos = zrle_datos(pf, img, w, h, modo)
    comp = zs.compress(datos) + zs.flush(zlib.Z_SYNC_FLUSH)
    return struct.pack(">I", len(comp)) + comp


def compacta(n):
    if n < 128:
        return bytes([n])
    if n < 16384:
        return bytes([(n & 127) | 128, n >> 7])
    return bytes([(n & 127) | 128, ((n >> 7) & 127) | 128, n >> 14])


class Tight:
    """Los cuatro flujos zlib de Tight de una conexión."""

    def __init__(self):
        self.z = [zlib.compressobj(6) for _ in range(4)]

    def reset(self, i):
        self.z[i] = zlib.compressobj(6)

    def _datos(self, flujo, crudo, ctl, resets):
        for i in range(4):
            if resets & (1 << i):
                self.reset(i)
        if len(crudo) < 12:
            return crudo
        comp = self.z[flujo].compress(crudo) + self.z[flujo].flush(zlib.Z_SYNC_FLUSH)
        return compacta(len(comp)) + comp

    def relleno(self, pf, color, resets=0):
        for i in range(4):
            if resets & (1 << i):
                self.reset(i)
        return bytes([0x80 | resets]) + tpix(pf, color)

    def copia(self, pf, img, w, h, flujo=0, resets=0, filtro_explicito=False):
        crudo = b"".join(tpix(pf, c) for c in img)
        ctl = (flujo | (4 if filtro_explicito else 0)) << 4 | resets
        cab = bytes([ctl]) + (b"\x00" if filtro_explicito else b"")
        return cab + self._datos(flujo, crudo, ctl, resets)

    def paleta(self, pf, img, w, h, flujo=1, resets=0):
        pal = []
        for c in img:
            if c not in pal:
                pal.append(c)
        assert 2 <= len(pal) <= 256
        if len(pal) == 2:
            fila = (w + 7) // 8
            crudo = bytearray(fila * h)
            for y in range(h):
                for x in range(w):
                    if pal.index(img[y * w + x]):
                        crudo[y * fila + x // 8] |= 0x80 >> (x & 7)
            crudo = bytes(crudo)
        else:
            crudo = bytes(pal.index(c) for c in img)
        cab = bytes([(flujo | 4) << 4 | resets, 1, len(pal) - 1]) + b"".join(tpix(pf, c) for c in pal)
        return cab + self._datos(flujo, crudo, 0, resets)

    def gradiente(self, pf, img, w, h, flujo=2, resets=0):
        mx = pf.max
        ant = [[0, 0, 0]] * w
        crudo = bytearray()
        for y in range(h):
            act = []
            for x in range(w):
                v = canal(pf, img[y * w + x])
                d = []
                for c in range(3):
                    izq = act[x - 1][c] if x else 0
                    arr = ant[x][c]
                    ai = ant[x - 1][c] if x else 0
                    p = max(0, min(mx[c], izq + arr - ai))
                    d.append((v[c] - p) & mx[c])
                act.append(v)
                if tpix3(pf):
                    crudo += bytes(d)
                else:
                    val = sum(d[c] << pf.desp[c] for c in range(3))
                    crudo += val.to_bytes(pf.nb, "big" if pf.big else "little")
            ant = act
        cab = bytes([(flujo | 4) << 4 | resets, 2])
        return cab + self._datos(flujo, bytes(crudo), 0, resets)

    def jpeg(self, datos, resets=0):
        for i in range(4):
            if resets & (1 << i):
                self.reset(i)
        return bytes([0x90 | resets]) + compacta(len(datos)) + datos


def rect(x, y, w, h, enc, datos=b""):
    return struct.pack(">HHHHi", x, y, w, h, enc) + datos


def actualizacion(rects):
    return struct.pack(">BBH", 0, 0, len(rects)) + b"".join(rects)


# ------------------------------------------------------------ imágenes de prueba

def imagenes_prueba():
    rnd = random.Random(253)
    out = []
    for (w, h) in ((16, 16), (37, 21), (130, 70), (200, 90)):
        out.append(("lisa", w, h, [0x336699] * (w * h)))
        out.append(("degradado", w, h, [((x * 255 // w) << 16) | ((y * 255 // h) << 8) | ((x + y) & 255) for y in range(h) for x in range(w)]))
        out.append(("ruido", w, h, [rnd.getrandbits(24) for _ in range(w * h)]))
        cols = [0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0x808080]
        img = [0xE0E0E0] * (w * h)
        for _ in range(w * h // 40):
            rw, rh, rx, ry = rnd.randrange(1, 12), rnd.randrange(1, 8), rnd.randrange(w), rnd.randrange(h)
            c = rnd.choice(cols)
            for y in range(ry, min(h, ry + rh)):
                for x in range(rx, min(w, rx + rw)):
                    img[y * w + x] = c
        out.append(("interfaz", w, h, img))
        out.append(("2 colores", w, h, [0x102030 if (x // 3 + y // 2) % 2 else 0xF0E0D0 for y in range(h) for x in range(w)]))
        pal9 = [rnd.getrandbits(24) for _ in range(9)]
        out.append(("9 colores", w, h, [pal9[(x // 5 + y // 3) % 9] for y in range(h) for x in range(w)]))
        pal20 = [rnd.getrandbits(24) for _ in range(20)]
        out.append(("20 colores", w, h, [rnd.choice(pal20) for _ in range(w * h)]))
        out.append(("franjas", w, h, [(y * 7919) & 0xFFFFFF for y in range(h) for x in range(w)]))
    return out


# ------------------------------------------------------------ unidad en el anfitrión

def compilar_unidad(sanear):
    os.makedirs(TRAB, exist_ok=True)
    exe = os.path.join(TRAB, "vnc_cliente_unidad" + ("-asan" if sanear else ""))
    fuentes = [os.path.join(RAIZ, "tests", "vnc_cliente_unidad.c"), os.path.join(RAIZ, "userland", "libsagui", "inflado_continuo.c"),
               os.path.join(RAIZ, "userland", "libsagui", "imagem_jpeg.c"), os.path.join(RAIZ, "userland", "libsagui", "imagem_idct.c"),
               os.path.join(RAIZ, "tools", "video_host.c")]
    cmd = ["gcc", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland", "inc"), "-I", os.path.join(RAIZ, "kernel", "inc"),
           "-I", os.path.join(RAIZ, "userland", "programas"), "-I", os.path.join(RAIZ, "userland", "libsagui"), "-I", os.path.join(RAIZ, "tools")]
    cmd += ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"] if sanear else ["-O2"]
    r = subprocess.run(cmd + ["-o", exe] + fuentes, capture_output=True, text=True)
    return exe, r


class Orden:
    def __init__(self, exe):
        self.p = subprocess.Popen([exe, "ordenes"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def __call__(self, linea):
        self.p.stdin.write(linea + "\n")
        self.p.stdin.flush()
        return self.p.stdout.readline().strip()

    def lineas(self, linea, n):
        self.p.stdin.write(linea + "\n")
        self.p.stdin.flush()
        return [self.p.stdout.readline().strip() for _ in range(n)]

    def cerrar(self):
        self.p.stdin.close()
        self.p.wait(timeout=30)


def fmt_orden(fw, fh, pf):
    return f"fmt {fw} {fh} {pf.bpp} {pf.big} {pf.max[0]} {pf.max[1]} {pf.max[2]} {pf.desp[0]} {pf.desp[1]} {pf.desp[2]}"


def leer_fb(o, w, h):
    s = o("fb")
    return [int(s[i * 6:i * 6 + 6], 16) for i in range(w * h)]


def unidad(sanear):
    exe, r = compilar_unidad(sanear)
    if r.returncode != 0 or "warning" in r.stderr:
        if sanear and r.returncode != 0:
            print("  AVISO: sin los sanitizadores de gcc en este anfitrión; se omiten")
            return
        raise FalhaTeste("vnc_cliente_unidad.c no compila sin avisos: " + r.stderr[:2000])
    if not sanear:
        r = subprocess.run([exe], capture_output=True, text=True, timeout=120)
        n = re.search(r"vnc_cliente_unidad: (\d+)/(\d+) comprobaciones correctas", r.stdout)
        verificar(r.returncode == 0 and n and n.group(1) == n.group(2), f"teclas (inversa de las del servidor): {r.stdout.strip()[-200:]}")
    o = Orden(exe)
    rnd = random.Random(7)

    # --- inflado continuo contra zlib
    comp = zlib.compressobj(6)
    pasos = 0
    base = bytes(rnd.getrandbits(8) for _ in range(3000))
    # el estado del programa de la orden z vive hasta el final: se usa un solo flujo
    for i in range(70):
        k = rnd.randrange(5)
        if k == 0:
            ch = bytes(rnd.randrange(4) for _ in range(rnd.randrange(1, 6000)))
        elif k == 1:
            ch = base[rnd.randrange(500):][:rnd.randrange(1, 2500)]
        elif k == 2:
            ch = bytes(rnd.getrandbits(8) for _ in range(rnd.randrange(1, 900)))
        elif k == 3:
            ch = (base[:400] + bytes(rnd.getrandbits(8) for _ in range(30))) * rnd.randrange(1, 40)
        else:
            ch = b"x" * rnd.randrange(1, 70000)
        z = comp.compress(ch) + comp.flush(zlib.Z_SYNC_FLUSH)
        got = o("z " + z.hex())
        if got != (ch.hex() if ch else "-"):
            raise FalhaTeste(f"zinflar distinto de zlib en el trozo {i} ({len(ch)} bytes): {got[:80]}")
        pasos += len(ch)
    verificar(True, f"inflado continuo igual que zlib: 70 trozos ({pasos} bytes) en un solo flujo, con el diccionario entre trozos")
    # bloques guardados (nivel 0), fijos (cadenas cortas) y dinámicos en el mismo flujo
    for nivel in (0, 1, 9):
        comp = zlib.compressobj(nivel)
        o3 = Orden(exe)
        for i in range(12):
            ch = bytes(rnd.getrandbits(8) for _ in range(rnd.randrange(1, 300))) if i % 3 == 0 else (b"abcabcabd" * rnd.randrange(1, 90))
            z = comp.compress(ch) + comp.flush(zlib.Z_SYNC_FLUSH)
            if o3("z " + z.hex()) != ch.hex():
                raise FalhaTeste(f"zinflar mal con el nivel {nivel}, trozo {i}")
        o3.cerrar()
    verificar(True, "inflado continuo: bloques guardados (nivel 0), fijos y dinámicos")
    # datos rotos: error, no cuelgue
    zr = zlib.compressobj(6)
    z = zr.compress(base * 3) + zr.flush(zlib.Z_SYNC_FLUSH)
    o4 = Orden(exe)
    bad = bytearray(z)
    for i in range(20, len(bad), 17):
        bad[i] ^= 0x5A
    r = o4("z " + bytes(bad).hex())
    verificar(r.startswith("err") or len(r) > 0, "inflado continuo: datos corruptos no cuelgan ni se salen del búfer")
    o4.cerrar()

    # --- decodificadores: imágenes x formatos x trozos
    total = 0
    for nombre_fmt, pf in FORMATOS:
        for nombre, w, h, img in imagenes_prueba():
            if (w, h) == (130, 70) and nombre not in ("interfaz", "ruido", "20 colores", "degradado"):
                continue
            esperado = [visible(pf, c) for c in img]
            casos = [("raw", 0, lambda: raw(pf, img, w, h)), ("hextile", 5, lambda: hextile(pf, img, w, h))]
            for modo in ("auto", "raw", "rle", "paleta", "rlepaleta"):
                casos.append((f"zrle-{modo}", 16, lambda modo=modo: zrle(pf, img, w, h, zlib.compressobj(6), modo)))
            casos.append(("tight-copy", 7, lambda: Tight().copia(pf, img, w, h, 0)))
            casos.append(("tight-copy-filtro", 7, lambda: Tight().copia(pf, img, w, h, 3, 0, True)))
            if len({c for c in img}) in range(2, 257):
                casos.append(("tight-paleta", 7, lambda: Tight().paleta(pf, img, w, h, 1, 0)))
            if pf.bpp != 8:
                casos.append(("tight-gradiente", 7, lambda: Tight().gradiente(pf, img, w, h, 2, 0)))
            if len(set(esperado)) == 1:
                casos.append(("tight-relleno", 7, lambda: Tight().relleno(pf, img[0], 0)))
            for tipo, enc, f in casos:
                datos = f()
                for trozo in (0, 1, 7, 100) if len(datos) < 60000 else (0, 100):
                    o(fmt_orden(w + 8, h + 8, pf))
                    r = o(f"rect {enc} 4 4 {w} {h} {trozo} {datos.hex()}")
                    if not r.startswith("ok"):
                        raise FalhaTeste(f"{tipo} {nombre} {w}x{h} {nombre_fmt} trozo {trozo}: {r}")
                    if int(r.split()[1]) != len(datos):
                        raise FalhaTeste(f"{tipo} {nombre} {w}x{h} {nombre_fmt}: consumió {r} de {len(datos)}")
                    fb = leer_fb(o, w + 8, h + 8)
                    for y in range(h):
                        fila = fb[(y + 4) * (w + 8) + 4:(y + 4) * (w + 8) + 4 + w]
                        if fila != esperado[y * w:(y + 1) * w]:
                            raise FalhaTeste(f"{tipo} {nombre} {w}x{h} {nombre_fmt} trozo {trozo}: la fila {y} no coincide")
                    if fb[0] != 0 or fb[(h + 7) * (w + 8) + w + 7] != 0:
                        raise FalhaTeste(f"{tipo} {nombre} {nombre_fmt}: escribió fuera del rectángulo")
                    total += 1
    verificar(True, f"decodificadores Raw, Hextile, ZRLE (5 modos de baldosa) y Tight (copy, palette, gradient, relleno): {total} casos, 6 formatos, trozos de 1, 7 y 100 bytes")

    # --- datos truncados: nunca dan un rectángulo completo; datos corruptos: ok o error, pero sin salirse de los búferes (ASan)
    fz = random.Random(5)
    for nombre_fmt, pf in (FORMATOS[0], FORMATOS[3], FORMATOS[5]):
        w, h = 70, 40
        img = [fz.choice([0x112233, 0xFFEEDD, 0x884422, fz.getrandbits(24)]) for _ in range(w * h)]
        for tipo, enc, f in (("raw", 0, lambda: raw(pf, img, w, h)), ("hextile", 5, lambda: hextile(pf, img, w, h)),
                             ("zrle", 16, lambda: zrle(pf, img, w, h, zlib.compressobj(6))),
                             ("tight-copy", 7, lambda: Tight().copia(pf, img, w, h, 0)), ("tight-paleta", 7, lambda: Tight().paleta(pf, img[:0] + [0x1, 0x2, 0x3] * (w * h // 3) + [0x1] * (w * h - 3 * (w * h // 3)), w, h, 1))):
            datos = f()
            o(fmt_orden(w + 8, h + 8, pf))
            r = o(f"rect {enc} 4 4 {w} {h} 0 {datos[:-1].hex()}")
            verificar(r.startswith("err"), f"{tipo} {nombre_fmt}: con un byte menos no se da por completo ({r})")
            for _ in range(40):
                mal = bytearray(datos)
                for _ in range(fz.randrange(1, 6)):
                    mal[fz.randrange(len(mal))] = fz.getrandbits(8)
                o(fmt_orden(w + 8, h + 8, pf))
                r = o(f"rect {enc} 4 4 {w} {h} {fz.choice((0, 5, 33))} {bytes(mal).hex()}")
                if not (r.startswith("ok") or r.startswith("err")):
                    raise FalhaTeste(f"{tipo} corrupto: respuesta rara {r!r}")
    verificar(True, "datos truncados y corruptos (Raw, Hextile, ZRLE, Tight): sin desbordes")

    # --- Tight: varios rectángulos seguidos en los mismos flujos, con reinicios; Hextile con el fondo que pasa de baldosa a baldosa
    pf = PF32
    o(fmt_orden(300, 200, pf))
    tg = Tight()
    esperado = [0] * (300 * 200)
    rnd = random.Random(99)
    for i in range(30):
        w, h = rnd.randrange(1, 60), rnd.randrange(1, 40)
        x, y = rnd.randrange(300 - w), rnd.randrange(200 - h)
        pal = [rnd.getrandbits(24) for _ in range(rnd.randrange(2, 7))]
        img = [pal[(a // 3 + b // 2) % len(pal)] for b in range(h) for a in range(w)] if i % 3 else [rnd.getrandbits(24) for _ in range(w * h)]
        resets = 0 if i % 5 else rnd.randrange(16)
        kind = i % 3
        if kind == 0 and len(set(img)) <= 256 and len(set(img)) >= 2:
            d = tg.paleta(pf, img, w, h, i % 4, resets)
        elif kind == 1:
            d = tg.gradiente(pf, img, w, h, i % 4, resets)
        else:
            d = tg.copia(pf, img, w, h, i % 4, resets)
        r = o(f"rect 7 {x} {y} {w} {h} 0 {d.hex()}")
        if not r.startswith("ok") or int(r.split()[1]) != len(d):
            raise FalhaTeste(f"Tight seguido {i}: {r}")
        for b in range(h):
            esperado[(y + b) * 300 + x:(y + b) * 300 + x + w] = img[b * w:(b + 1) * w]
    # la orden fmt reinicia los flujos del cliente, pero tg sigue (reinicio solo con las banderas): reiniciar los dos a la vez
    fb = leer_fb(o, 300, 200)
    verificar(fb == esperado, "Tight: 30 rectángulos seguidos en los mismos cuatro flujos, con reinicios de flujo, igual que la imagen")

    # --- JPEG
    jpg = open(JPEG, "rb").read()
    cab, lin = o.lineas("jpeg " + jpg.hex(), 2)
    verificar(cab == "ok 128 96", "el JPEG de prueba se decodifica con la libsagui (128x96)")
    jpeg_px = [int(lin[i * 6:i * 6 + 6], 16) for i in range(128 * 96)]
    o(fmt_orden(300, 200, pf))
    d = Tight().jpeg(jpg)
    r = o(f"rect 7 10 20 128 96 7 {d.hex()}")
    verificar(r.startswith("ok") and int(r.split()[1]) == len(d), "Tight con JPEG: se descodifica y consume todo (trozos de 7 bytes)")
    fb = leer_fb(o, 300, 200)
    verificar(all(fb[(20 + y) * 300 + 10 + x] == jpeg_px[y * 128 + x] for y in range(96) for x in range(128)), "el JPEG del rectángulo es el que decodifica la libsagui")
    verificar(o(f"rect 7 10 20 100 96 0 {d.hex()}").startswith("err"), "JPEG con otro tamaño que el del rectángulo: error")
    # JPEG contra la referencia (ffmpeg o el PNG guardado): diferencia media pequeña
    ref = referencia_jpeg()
    if ref is not None:
        dif = sum(abs(((a >> s) & 255) - ((b >> s) & 255)) for a, b in zip(jpeg_px, ref) for s in (16, 8, 0)) / (128 * 96 * 3)
        verificar(dif < 6, f"la libsagui y la referencia decodifican el JPEG casi igual (diferencia media {dif:.2f} de 255)")
    else:
        print("  AVISO: sin ffmpeg en el anfitrión: se omite la comparación del JPEG con la referencia")
    globals()["JPEG_PX"] = jpeg_px

    # --- CopyRect (con solape en los dos sentidos), cursor y errores
    o(fmt_orden(64, 48, pf))
    img = [(x * 4) << 16 | (y * 5) << 8 | (x ^ y) for y in range(48) for x in range(64)]
    o(f"rect 0 0 0 64 48 0 {raw(pf, img, 64, 48).hex()}")
    esperado = list(img)
    for (sx, sy, dx, dy, w, h) in ((0, 0, 5, 3, 40, 30), (10, 10, 2, 1, 50, 36), (20, 5, 20, 5, 10, 10)):
        r = o(f"rect 1 {dx} {dy} {w} {h} 0 {struct.pack('>HH', sx, sy).hex()}")
        verificar(r == "ok 4", f"CopyRect {sx},{sy} -> {dx},{dy}: {r}")
        copia = [[esperado[(sy + b) * 64 + sx + a] for a in range(w)] for b in range(h)]
        for b in range(h):
            for a in range(w):
                esperado[(dy + b) * 64 + dx + a] = copia[b][a]
    verificar(leer_fb(o, 64, 48) == esperado, "CopyRect con áreas que se solapan en los dos sentidos")
    verificar(o(f"rect 1 0 0 40 30 0 {struct.pack('>HH', 40, 30).hex()}").startswith("err"), "CopyRect con el origen fuera de la pantalla: error")
    verificar(o(f"rect 0 60 40 20 20 0 {raw(pf, [0] * 400, 20, 20).hex()}").startswith("err"), "un rectángulo que se sale de la pantalla: error")
    verificar(o("rect 9 0 0 4 4 0 00").startswith("err"), "una codificación desconocida: error")
    verificar(o("rect 7 0 0 4 4 0 a0").startswith("err"), "Tight con una orden inválida (>= 10): error")
    # cursor
    cw, ch = 11, 5
    cpx = [rnd.getrandbits(24) for _ in range(cw * ch)]
    mask = [(i * 7 % 3) != 0 for i in range(cw * ch)]
    fila = (cw + 7) // 8
    mb = bytearray(fila * ch)
    for y in range(ch):
        for x in range(cw):
            if mask[y * cw + x]:
                mb[y * fila + x // 8] |= 0x80 >> (x & 7)
    d = b"".join(pix(pf, c) for c in cpx) + bytes(mb)
    o(fmt_orden(64, 48, pf))
    cab, lpx, lmk = o.lineas(f"cursor {cw} {ch} {d.hex()}", 3)
    verificar(cab == f"ok {len(d)}", "cursor: consume la forma entera")
    verificar([int(lpx[i * 6:i * 6 + 6], 16) for i in range(cw * ch)] == [visible(pf, c) for c in cpx]
              and [int(c) for c in lmk] == [int(v) for v in mask], "cursor: píxeles y máscara")
    verificar(o.lineas(f"cursor {cw} {ch} {d[:-1].hex()}", 1)[0].startswith("err"), "cursor con datos truncados: error")
    o.cerrar()


def referencia_jpeg():
    if shutil.which("ffmpeg") is None:
        return None
    r = subprocess.run(["ffmpeg", "-v", "error", "-i", JPEG, "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], capture_output=True)
    if r.returncode != 0 or len(r.stdout) != 128 * 96 * 3:
        return None
    d = r.stdout
    return [d[i * 3] << 16 | d[i * 3 + 1] << 8 | d[i * 3 + 2] for i in range(128 * 96)]


# ------------------------------------------------------------ servidor RFB mínimo

class Servidor:
    """Un servidor RFB de un solo cliente a la vez; los mensajes del cliente quedan en self.eventos."""

    def __init__(self, version=b"003.008", seguridad="vnc", clave=CLAVE, w=400, h=300, nombre="servidor de prueba"):
        self.version, self.seguridad, self.clave, self.w, self.h, self.nombre = version, seguridad, clave, w, h, nombre
        self.ls = socket.socket()
        self.ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.ls.bind(("127.0.0.1", 0))
        self.ls.listen(4)
        self.puerto = self.ls.getsockname()[1]
        self.eventos = []
        self.lock = threading.Lock()
        self.conexiones = 0
        self.c = None
        self.pf = PF32
        self.codificaciones = []
        self.listo = threading.Event()
        self.resultado_auth = None   # True/False cuando el cliente contestó el reto
        self.parar = False
        self.hilo = threading.Thread(target=self._aceptar, daemon=True)
        self.hilo.start()

    def _aceptar(self):
        self.ls.settimeout(0.5)
        while not self.parar:
            try:
                c, _ = self.ls.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            self.conexiones += 1
            self.c = c
            self.listo.clear()
            t = threading.Thread(target=self._sesion, args=(c,), daemon=True)
            t.start()

    def _leer(self, c, n):
        b = b""
        while len(b) < n:
            d = c.recv(n - len(b))
            if not d:
                raise ConnectionError
            b += d
        return b

    def _sesion(self, c):
        try:
            c.sendall(b"RFB " + self.version + b"\n")
            v = self._leer(c, 12)
            self.version_cliente = v
            ver = int(v[8:11])
            if ver == 3:
                if self.seguridad == "vnc":
                    c.sendall(struct.pack(">I", 2))
                else:
                    c.sendall(struct.pack(">I", 1))
            else:
                c.sendall(bytes([1, 2 if self.seguridad == "vnc" else 1]))
                elegido = self._leer(c, 1)[0]
                self.seguridad_elegida = elegido
            if self.seguridad == "vnc":
                reto = os.urandom(16)
                c.sendall(reto)
                resp = self._leer(c, 16)
                ok = resp == vnc_respuesta(self.clave, reto)
                self.resultado_auth = ok
                if ok:
                    c.sendall(struct.pack(">I", 0))
                else:
                    msg = b"contrasena incorrecta"
                    c.sendall(struct.pack(">I", 1) + (struct.pack(">I", len(msg)) + msg if ver >= 8 else b""))
                    time.sleep(0.3)
                    c.close()
                    return
            elif ver >= 8:
                c.sendall(struct.pack(">I", 0))
            ci = self._leer(c, 1)
            self._anotar(("init", ci[0]))
            nombre = self.nombre.encode()
            c.sendall(struct.pack(">HH", self.w, self.h) + struct.pack(">BBBBHHHBBB3x", 32, 24, 0, 1, 255, 255, 255, 16, 8, 0)
                      + struct.pack(">I", len(nombre)) + nombre)
            while True:
                t = self._leer(c, 1)[0]
                if t == 0:
                    m = self._leer(c, 19)
                    bpp, prof, big, tc, rm, gm, bm, rs, gs, bs = struct.unpack(">BBBBHHHBBB", m[3:16])
                    self.pf = PF(bpp, big, (rm, gm, bm), (rs, gs, bs), prof)
                    self._anotar(("pf", bpp, prof, big, tc, rm, gm, bm, rs, gs, bs))
                elif t == 2:
                    m = self._leer(c, 3)
                    n = struct.unpack(">H", m[1:3])[0]
                    self.codificaciones = list(struct.unpack(">" + "i" * n, self._leer(c, 4 * n)))
                    self._anotar(("codif", tuple(self.codificaciones)))
                    self.listo.set()
                elif t == 3:
                    m = self._leer(c, 9)
                    self._anotar(("pedido", m[0], *struct.unpack(">HHHH", m[1:9])))
                elif t == 4:
                    m = self._leer(c, 7)
                    self._anotar(("tecla", m[0], struct.unpack(">I", m[3:7])[0]))
                elif t == 5:
                    m = self._leer(c, 5)
                    self._anotar(("raton", m[0], *struct.unpack(">HH", m[1:5])))
                elif t == 6:
                    m = self._leer(c, 7)
                    self._leer(c, struct.unpack(">I", m[3:7])[0])
                else:
                    self._anotar(("desconocido", t))
                    return
        except (ConnectionError, OSError):
            pass

    def _anotar(self, e):
        with self.lock:
            self.eventos.append(e)

    def tomar(self, tipo=None):
        with self.lock:
            ev = [e for e in self.eventos if tipo is None or e[0] == tipo]
            if tipo is None:
                self.eventos = []
            else:
                self.eventos = [e for e in self.eventos if e[0] != tipo]
        return ev

    def esperar_listo(self, tempo=30):
        if not self.listo.wait(tempo):
            raise FalhaTeste("el cliente no mandó SetEncodings (no llegó a conectar)")

    def enviar(self, datos):
        self.c.sendall(datos)

    def cerrar_cliente(self):
        try:
            self.c.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.c.close()

    def cerrar(self):
        self.parar = True
        try:
            self.ls.close()
        except OSError:
            pass
        if self.c:
            self.cerrar_cliente()


# ------------------------------------------------------------ QEMU

def captura(m, nombre):
    ppm = os.path.join(SAIDA, f"h253-{nombre}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    w, h, px = ler_ppm(ppm)
    return w, h, px


def pixel(cap, x, y):
    w, h, px = cap
    i = (y * w + x) * 3
    return px[i] << 16 | px[i + 1] << 8 | px[i + 2]


def ventana(m, titulo, desde=0):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[desde:])
    if not g:
        return None
    g = g[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]), cx=int(g[3]) + BORDA, cy=int(g[4]) + TITULO)


def esperar_serial(m, padron, tempo=30, desde=0):
    fin = time.time() + tempo
    while time.time() < fin:
        g = re.findall(padron, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise FalhaTeste(f"no apareció en la serial: {padron!r}")


def cerrar_ventana(m, w):
    m.clique(w["x"] + w["l"] + 2 * BORDA - BORDA - 20 + 8, w["y"] + 12)   # el X de la barra de título
    time.sleep(0.8)


def comparar(cap, w, esperada, ew, eh, ox, oy, nombre, tolerancia=0):
    """Compara el área de la ventana con la imagen esperada (ew x eh) puesta en (ox, oy) del área."""
    malos = 0
    primero = None
    for y in range(eh):
        for x in range(ew):
            v = pixel(cap, w["cx"] + ox + x, w["cy"] + oy + y)
            e = esperada[y * ew + x]
            if v != e and (tolerancia == 0 or max(abs(((v >> s) & 255) - ((e >> s) & 255)) for s in (16, 8, 0)) > tolerancia):
                malos += 1
                if primero is None:
                    primero = (x, y, v, e)
    verificar(malos == 0, f"{nombre}: la ventana pinta la imagen igual ({malos} píxeles distintos de {ew * eh}"
              + (f"; el primero en {primero[0]},{primero[1]}: {primero[2]:06x} y se esperaba {primero[3]:06x})" if primero else ")"))


def lanzar(m, opciones, srv, host="10.0.2.2", puerto=None):
    marca = len(m.serial())
    titulo = f"VNC {host}:{puerto or srv.puerto}"
    m.comando(f"vnc {opciones} {host}:{puerto or srv.puerto} &", 20)
    esperar_serial(m, r"vnc: pronto", 20, marca)
    time.sleep(0.5)
    w = ventana(m, titulo, marca)
    verificar(w is not None, f"se abre la ventana «{titulo}»")
    return w, marca


def construir_imagen():
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(TRAB, "rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    sgfs = os.path.join(TRAB, "h253.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, "h253.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--dados-mb", "16"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def principal():
    os.makedirs(TRAB, exist_ok=True)
    if not os.path.exists(os.path.join(RAIZ, "build", "userland", "bin", "vnc")):
        raise FalhaTeste("falta build/userland/bin/vnc: compilar con make antes")
    unidad(False)
    unidad(True)
    qemu_pruebas()
    print("H253: todo bien")


def qemu_pruebas():
    img = construir_imagen()
    srv = Servidor()
    srv2 = Servidor(version=b"003.003", seguridad="none", w=240, h=160)
    extras = ["-nic", "user,model=e1000"]
    jpeg_px = globals().get("JPEG_PX") or [0] * (128 * 96)
    try:
        with Maquina("h253", "ide", imagem=img, snapshot=False, extras=extras) as m:
            m.esperar_serial("sagui:/$", 150)
            time.sleep(4)
            esperar_serial(m, "10.0.2.15 \\(DHCP\\)", 40)
            m.comando("echo " + CLAVE + " > /tmp/clave")
            m.comando("echo mala > /tmp/clave-mala")

            # ===== sesión 1: RFB 3.8, VNC Authentication, todas las codificaciones
            marca0 = len(m.serial())
            w, marca = lanzar(m, "--clave-fichero /tmp/clave", srv)
            srv.esperar_listo()
            ev = srv.tomar()
            verificar(("init", 1) in ev, "ClientInit con el flag de compartir")
            pfm = [e for e in ev if e[0] == "pf"][-1]
            verificar(pfm[1:] == (32, 24, 0, 1, 255, 255, 255, 16, 8, 0), f"SetPixelFormat de 32 bpp (0x00RRGGBB): {pfm}")
            cod = [e for e in ev if e[0] == "codif"][-1][1]
            verificar(cod[:5] == (7, 16, 5, 1, 0) and -239 in cod and -223 in cod, f"SetEncodings: Tight, ZRLE, Hextile, CopyRect, Raw, Cursor y DesktopSize: {cod}")
            verificar(srv.resultado_auth is True and srv.seguridad_elegida == 2, "se contesta el reto con DES y la contraseña del fichero (VNC Authentication)")
            verificar(any(e[0] == "pedido" and e[1] == 0 and e[4:] == (400, 300) for e in ev), "primera petición de actualización completa (no incremental) de 400x300")
            esperar_serial(m, r'vnc: conectado, escritorio 400x300 "servidor de prueba" \(32 bpp, RFB 3\.8\)', 5, marca)

            W, H = 400, 300
            esperada = [0x20252A] * (W * H)

            def poner(x, y, wd, ht, im, pf=PF32):
                for b in range(ht):
                    for a in range(wd):
                        esperada[(y + b) * W + x + a] = visible(pf, im[b * wd + a])
            pf = srv.pf
            rnd = random.Random(2531)
            zs, tg = zlib.compressobj(6), Tight()
            # fondo negro de partida (relleno de toda la pantalla, Tight)
            srv.enviar(actualizacion([rect(0, 0, W, H, 7, tg.relleno(pf, 0x101820))]))
            poner(0, 0, W, H, [0x101820] * (W * H))
            rects = []
            # A: Raw (ruido)
            A = [rnd.getrandbits(24) for _ in range(200 * 100)]
            rects.append(rect(0, 0, 200, 100, 0, raw(pf, A, 200, 100))); poner(0, 0, 200, 100, A)
            # B: Hextile (interfaz)
            B = [0xE0E0E0] * (200 * 100)
            for _ in range(60):
                rw, rh, rx, ry = rnd.randrange(3, 30), rnd.randrange(2, 15), rnd.randrange(170), rnd.randrange(85)
                c = rnd.choice([0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0x808080, 0xFFCC00])
                for y in range(ry, ry + rh):
                    for x in range(rx, rx + rw):
                        B[y * 200 + x] = c
            rects.append(rect(200, 0, 200, 100, 5, hextile(pf, B, 200, 100))); poner(200, 0, 200, 100, B)
            # C: ZRLE (degradado a la izquierda, bloques de pocos colores a la derecha)
            C = [((x * 255 // 200) << 16 | (y * 255 // 100) << 8 | 0x40) if x < 100 else [0xAA0000, 0x00AA00, 0x0000AA, 0xAAAA00][(x // 10 + y // 7) % 4] for y in range(100) for x in range(200)]
            rects.append(rect(0, 100, 200, 100, 16, zrle(pf, C, 200, 100, zs))); poner(0, 100, 200, 100, C)
            # D: Tight en cuatro trozos
            D1 = [((x * 3) << 16 | (y * 4) << 8 | ((x + y) & 255)) for y in range(50) for x in range(100)]
            rects.append(rect(200, 100, 100, 50, 7, tg.copia(pf, D1, 100, 50, 0, 0, False))); poner(200, 100, 100, 50, D1)
            D2 = [[0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF][(x // 4 + y // 5) % 5] for y in range(50) for x in range(100)]
            rects.append(rect(300, 100, 100, 50, 7, tg.paleta(pf, D2, 100, 50, 1, 0))); poner(300, 100, 100, 50, D2)
            D3 = [((x * 2) << 16 | (y * 5) << 8 | (x + 2 * y) & 255) for y in range(50) for x in range(100)]
            rects.append(rect(200, 150, 100, 50, 7, tg.gradiente(pf, D3, 100, 50, 2, 0))); poner(200, 150, 100, 50, D3)
            D4 = [0xFFFFFF if (x // 5 + y // 5) % 2 else 0x203040 for y in range(50) for x in range(100)]
            rects.append(rect(300, 150, 100, 50, 7, tg.paleta(pf, D4, 100, 50, 3, 0))); poner(300, 150, 100, 50, D4)
            # banda de abajo: relleno, JPEG, copy corto (<12 bytes), palette de 8, gradient
            rects.append(rect(0, 200, 60, 100, 7, tg.relleno(pf, 0xC03020))); poner(0, 200, 60, 100, [0xC03020] * 6000)
            d = tg.jpeg(open(JPEG, "rb").read())
            rects.append(rect(62, 202, 128, 96, 7, d))
            for b in range(96):
                for a in range(128):
                    esperada[(202 + b) * W + 62 + a] = jpeg_px[b * 128 + a]
            E = [0x112233, 0x445566, 0x778899]
            rects.append(rect(200, 200, 1, 3, 7, tg.copia(pf, E, 1, 3, 0, 0, False))); poner(200, 200, 1, 3, E)
            F = [[0x101010 * (1 + (x // 6 + y // 6) % 8)][0] for y in range(50) for x in range(60)]
            rects.append(rect(210, 200, 60, 50, 7, tg.paleta(pf, F, 60, 50, 1, 0))); poner(210, 200, 60, 50, F)
            G = [((x * 2) << 16 | (255 - y * 4) << 8 | (x * y) & 255) for y in range(60) for x in range(100)]
            rects.append(rect(280, 220, 100, 60, 7, tg.gradiente(pf, G, 100, 60, 2, 0))); poner(280, 220, 100, 60, G)
            srv.enviar(actualizacion(rects))
            time.sleep(1.5)
            areaA = w["a"] - BARRA
            ox, oy = (w["l"] - W) // 2, (areaA - H) // 2
            m.mouse_para(1020, 760)
            cap = captura(m, "s1-a")
            comparar(cap, w, esperada, W, H, ox, oy, "Raw, Hextile, ZRLE y Tight (copy, palette, gradient, relleno, JPEG)")
            for nombre in ("Raw", "Hextile", "ZRLE", "Tight-copy", "Tight-paleta", "Tight-gradiente", "Tight-relleno", "Tight-JPEG"):
                verificar(f"vnc: primera vez {nombre}" in m.serial()[marca:], f"el cliente registra que descodificó {nombre}")
            # CopyRect: copiar (0,0,100,50) a (300,250) y (100,100,80,60) a (110,120) (solape)
            srv.enviar(actualizacion([rect(300, 250, 100, 50, 1, struct.pack(">HH", 0, 0))]))
            for b in range(50):
                for a in range(100):
                    esperada[(250 + b) * W + 300 + a] = esperada[b * W + a]
            time.sleep(1.0)
            cap = captura(m, "s1-b")
            comparar(cap, w, esperada, W, H, ox, oy, "CopyRect")
            verificar("vnc: primera vez CopyRect" in m.serial()[marca:], "el cliente registra CopyRect")

            # --- teclas y ratón
            srv.tomar()
            m.mouse_para(w["cx"] + ox + 60, w["cy"] + oy + 40)
            m.clique(w["cx"] + ox + 60, w["cy"] + oy + 40)
            time.sleep(0.3)
            ev = srv.tomar("raton")
            verificar(any(e[1] == 1 and abs(e[2] - 60) <= 1 and abs(e[3] - 40) <= 1 for e in ev) and any(e[1] == 0 for e in ev[-2:]),
                      f"clic izquierdo: pulsado y soltado en la posición remota (60,40): {ev[-3:]}")
            m.clique(w["cx"] + ox + 150, w["cy"] + oy + 90, botao=2)
            ev = srv.tomar("raton")
            verificar(any(e[1] == 4 and abs(e[2] - 150) <= 1 and abs(e[3] - 90) <= 1 for e in ev), f"botón derecho: máscara 4 en (150,90): {ev[-3:]}")
            srv.tomar()
            m.mouse_para(w["cx"] + ox + 100, w["cy"] + oy + 100)
            m.monitor("mouse_move 0 0 1")
            time.sleep(0.4)
            m.monitor("mouse_move 0 0 -1")
            time.sleep(0.5)
            botones = {e[1] for e in srv.tomar("raton")}
            if 8 in botones and 16 in botones:
                verificar(True, "rueda: botones 4 y 5 del RFB, con pulsar y soltar")
            else:
                print(f"  AVISO: este QEMU no entregó la rueda al sistema (botones vistos {sorted(botones)}): sin comprobar")
            srv.tomar()
            m.digitar("aB1 ", 0.15)
            m.tecla("ret", 0.15)
            m.tecla("backspace", 0.15)
            m.tecla("esc", 0.15)
            m.tecla("up", 0.15)
            m.tecla("f5", 0.15)
            m.tecla("delete", 0.15)
            m.tecla("ctrl-c", 0.2)
            time.sleep(0.5)
            tec = [(e[1], e[2]) for e in srv.tomar("tecla")]
            esperadas = [0x61, 0x42, 0x31, 0x20, 0xFF0D, 0xFF08, 0xFF1B, 0xFF52, 0xFFC2, 0xFFFF]
            presionadas = [k for d_, k in tec if d_ == 1 and k not in (0xFFE3, 0xFFE1)]
            verificar(presionadas[:len(esperadas)] == esperadas, f"teclas: a, B, 1, espacio, Intro, Retroceso, Esc, flecha arriba, F5 y Supr llegan como keysyms de X11: {[hex(k) for k in presionadas]}")
            verificar(all((0, k) in tec for k in esperadas), "cada tecla se manda pulsada y soltada")
            verificar((1, 0xFFE3) in tec and (1, ord("c")) in tec and (0, 0xFFE3) in tec, f"Ctrl+C: Control_L pulsado, 'c', y Control_L soltado: {tec[-6:]}")

            # --- cursor remoto: un cuadrado rojo de 6x6 con el punto caliente en (1,1)
            cur = [0xFF0000] * 36
            mascara = bytes([0xFC] * 6)
            srv.enviar(actualizacion([rect(1, 1, 6, 6, -239, b"".join(pix(pf, c) for c in cur) + mascara)]))
            time.sleep(0.5)
            px_, py_ = w["cx"] + ox + 330, w["cy"] + oy + 20
            m.mouse_para(px_ - 10, py_ - 10)
            m.mouse_mover(px_, py_)
            time.sleep(0.5)
            cap = captura(m, "s1-cursor")
            verificar(pixel(cap, px_ + 2, py_ + 2) == 0xFF0000,
                      "la forma del cursor que manda el servidor se dibuja en la posición del puntero")
            verificar("vnc: primera vez Cursor" in m.serial()[marca:], "el cliente registra el cursor remoto")

            # --- DesktopSize
            srv.enviar(actualizacion([rect(0, 0, 200, 150, -223)]))
            N = [(x * 255 // 200) << 16 | (y * 255 // 150) << 8 | 0x77 for y in range(150) for x in range(200)]
            srv.enviar(actualizacion([rect(0, 0, 200, 150, 0, raw(pf, N, 200, 150))]))
            time.sleep(1.0)
            m.mouse_para(1020, 760)
            cap = captura(m, "s1-tam")
            ox2, oy2 = (w["l"] - 200) // 2, (areaA - 150) // 2
            comparar(cap, w, N, 200, 150, ox2, oy2, "DesktopSize a 200x150 y una imagen nueva")
            esperar_serial(m, r"vnc: el escritorio cambia a 200x150", 5, marca)
            ev = srv.tomar("pedido")
            verificar(any(e[4:] == (200, 150) for e in ev), "después de DesktopSize se pide una actualización del tamaño nuevo")

            # --- el servidor cierra: aviso y reconexión con R
            n0 = srv.conexiones
            srv.cerrar_cliente()
            esperar_serial(m, r"vnc: desconectado: el servidor cerró la conexión", 10, marca)
            time.sleep(0.5)
            cap = captura(m, "s1-caida")
            verificar(pixel(cap, w["cx"] + 5, w["cy"] + 5) == 0x8B1A1A, "la ventana avisa con una franja roja de que se cortó la conexión")
            m.tecla("r", 0.3)
            fin = time.time() + 20
            while srv.conexiones == n0 and time.time() < fin:
                time.sleep(0.3)
            verificar(srv.conexiones == n0 + 1, "R reconecta (el servidor ve una conexión nueva)")
            srv.esperar_listo()
            esperar_serial(m, r"vnc: conectado, escritorio 400x300", 10, marca + 1)
            srv.enviar(actualizacion([rect(0, 0, 400, 300, 7, Tight().relleno(srv.pf, 0x336699))]))
            time.sleep(1.0)
            m.mouse_para(1020, 760)
            cap = captura(m, "s1-recon")
            verificar(pixel(cap, w["cx"] + ox + 5, w["cy"] + oy + 5) == 0x336699, "tras reconectar se vuelve a pintar el escritorio")
            cerrar_ventana(m, w)
            esperar_serial(m, r"vnc: cerrado", 10, marca)

            # ===== sesión 2: contraseña mala por fichero, y luego pedida en un cuadro de la ventana
            srv.resultado_auth = None
            w, marca = lanzar(m, "--clave-fichero /tmp/clave-mala", srv)
            esperar_serial(m, r"vnc: desconectado: contraseña incorrecta", 15, marca)
            verificar(srv.resultado_auth is False, "una contraseña mala: el servidor la rechaza y el cliente lo dice")
            cap = captura(m, "s2-mala")
            verificar(pixel(cap, w["cx"] + 5, w["cy"] + 5) == 0x8B1A1A, "aviso rojo de contraseña incorrecta")
            cerrar_ventana(m, w)
            esperar_serial(m, r"vnc: cerrado", 10, marca)
            srv.resultado_auth = None
            w, marca = lanzar(m, "", srv)
            esperar_serial(m, r"vnc: seguridad 2", 15, marca)
            time.sleep(1.0)
            cap = captura(m, "s2-pide")
            m.digitar("secreto", 0.12)
            m.tecla("ret", 0.3)
            srv.esperar_listo()
            verificar(srv.resultado_auth is True, "la contraseña que se escribe en la ventana (cuadro oculto) abre la sesión")
            esperar_serial(m, r"vnc: conectado, escritorio 400x300", 10, marca)
            cerrar_ventana(m, w)
            esperar_serial(m, r"vnc: cerrado", 10, marca)

            # ===== sesión 3: RFB 3.3 sin seguridad, --color 16, --solo-ver
            w, marca = lanzar(m, "--color 16 --solo-ver", srv2, puerto=srv2.puerto)
            srv2.esperar_listo()
            ev = srv2.tomar()
            verificar(srv2.version_cliente == b"RFB 003.003\n", "con un servidor 3.3 el cliente contesta 3.3")
            pfm = [e for e in ev if e[0] == "pf"][-1]
            verificar(pfm[1:] == (16, 16, 0, 1, 31, 63, 31, 11, 5, 0), f"--color 16 manda SetPixelFormat de 565: {pfm}")
            esperar_serial(m, r"vnc: conectado, escritorio 240x160 .* \(16 bpp, RFB 3\.3\)", 5, marca)
            pf = srv2.pf
            zs, tg = zlib.compressobj(6), Tight()
            W2, H2 = 240, 160
            esp2 = [0] * (W2 * H2)
            I = [((x * 255 // 120) << 16 | (y * 255 // 80) << 8 | ((x * y) & 255)) for y in range(80) for x in range(120)]
            J = [rnd.getrandbits(24) for _ in range(120 * 80)]
            rcs = [rect(0, 0, 120, 80, 7, tg.gradiente(pf, I, 120, 80, 2, 0)), rect(120, 0, 120, 80, 16, zrle(pf, J, 120, 80, zs)),
                   rect(0, 80, 120, 80, 5, hextile(pf, I, 120, 80)), rect(120, 80, 120, 80, 0, raw(pf, J, 120, 80))]
            srv2.enviar(actualizacion(rcs))
            for (x0, y0, im) in ((0, 0, I), (120, 0, J), (0, 80, I), (120, 80, J)):
                for b in range(80):
                    for a in range(120):
                        esp2[(y0 + b) * W2 + x0 + a] = visible(pf, im[b * 120 + a])
            time.sleep(1.5)
            areaA = w["a"] - BARRA
            m.mouse_para(1020, 760)
            cap = captura(m, "s3")
            comparar(cap, w, esp2, W2, H2, (w["l"] - W2) // 2, (areaA - H2) // 2, "16 bpp (565) con Tight gradient, ZRLE, Hextile y Raw")
            srv2.tomar()
            m.clique(w["cx"] + 100, w["cy"] + 80)
            m.digitar("zz", 0.15)
            time.sleep(0.5)
            verificar(not srv2.tomar("raton") and not srv2.tomar("tecla"), "--solo-ver: ni el ratón ni el teclado llegan al servidor")
            cerrar_ventana(m, w)
            esperar_serial(m, r"vnc: cerrado", 10, marca)

            # ===== sesión 4: RFB 3.7, --color 8, escritorio mayor que la ventana (escalado)
            srv3 = Servidor(version=b"003.007", seguridad="vnc", w=1200, h=900)
            try:
                w, marca = lanzar(m, "--color 8 --clave-fichero /tmp/clave", srv3)
                srv3.esperar_listo()
                verificar(srv3.version_cliente == b"RFB 003.007\n" and srv3.resultado_auth is True, "RFB 3.7 con VNC Authentication")
                pfm = [e for e in srv3.tomar() if e[0] == "pf"][-1]
                verificar(pfm[1:] == (8, 8, 0, 1, 7, 7, 3, 5, 2, 0), f"--color 8 manda SetPixelFormat de 332: {pfm}")
                pf = srv3.pf
                cuadros = [(0, 0, 600, 450, 0xFF0000), (600, 0, 600, 450, 0x00FF00), (0, 450, 600, 450, 0x0000FF), (600, 450, 600, 450, 0xFFFF00)]
                srv3.enviar(actualizacion([rect(x, y, wd, ht, 5, hextile(pf, [c] * (wd * ht), wd, ht)) for x, y, wd, ht, c in cuadros]))
                time.sleep(2.5)
                m.mouse_para(1020, 760)
                cap = captura(m, "s4")
                areaA = w["a"] - BARRA
                zl = min(w["l"] * 1000 // 1200, areaA * 1000 // 900)
                sl, sa = 1200 * zl // 1000, 900 * zl // 1000
                ox, oy = (w["l"] - sl) // 2, (areaA - sa) // 2
                ok = all(pixel(cap, w["cx"] + ox + int(fx * sl), w["cy"] + oy + int(fy * sa)) == visible(pf, c)
                         for fx, fy, c in ((0.25, 0.25, 0xFF0000), (0.75, 0.25, 0x00FF00), (0.25, 0.75, 0x0000FF), (0.75, 0.75, 0xFFFF00)))
                verificar(ok, f"escritorio de 1200x900 escalado al {zl // 10}% en la ventana, con los cuatro cuadrantes (8 bpp)")
                srv3.tomar()
                px_, py_ = w["cx"] + ox + sl * 3 // 4, w["cy"] + oy + sa // 4
                m.clique(px_, py_)
                ev = srv3.tomar("raton")
                esp = (900, 225)
                verificar(any(e[1] == 1 and abs(e[2] - esp[0]) <= 3 * 1000 // zl + 2 and abs(e[3] - esp[1]) <= 3 * 1000 // zl + 2 for e in ev),
                          f"el clic se traduce a la posición del escritorio remoto (~{esp}): {ev[:3]}")
                # el botón 1:1 enseña la imagen sin escalar
                m.clique(w["cx"] + 30, w["cy"] + w["a"] - 13)
                time.sleep(0.8)
                s = esperar_serial(m, r"vnc: escala 1:1 \(vista (-?\d+),(-?\d+)\)", 5, marca)
                m.mouse_para(1020, 760)
                cap = captura(m, "s4-uno")
                vx, vy = int(s[0]), int(s[1])
                # un punto de la vista (50, 50) cae en el remoto (vx+50, vy+50): el cuadrante sale del color que toque
                rx, ry = vx + 50, vy + 50
                c = 0xFF0000 if rx < 600 and ry < 450 else 0x00FF00 if ry < 450 else 0x0000FF if rx < 600 else 0xFFFF00
                verificar(pixel(cap, w["cx"] + 50, w["cy"] + 50) == visible(pf, c), f"1:1: la vista empieza en ({vx},{vy}) y el punto (50,50) es del color del cuadrante remoto")
                cerrar_ventana(m, w)
                esperar_serial(m, r"vnc: cerrado", 10, marca)
            finally:
                srv3.cerrar()

            # ===== sesión 5: contra el vncd de este SaguiOS
            out = m.comando("vncd --clave " + CLAVE)
            m.comando("servicio iniciar vncd", 30)
            time.sleep(2)
            marca = len(m.serial())
            m.comando("vnc --clave-fichero /tmp/clave 127.0.0.1:5900 &", 20)
            esperar_serial(m, r'vnc: conectado, escritorio (\d+)x(\d+)', 40, marca)
            time.sleep(3)
            s = m.serial()[marca:]
            verificar("vnc: primera vez ZRLE" in s or "vnc: primera vez Hextile" in s or "vnc: primera vez Raw" in s,
                      "contra el vncd de SaguiOS (127.0.0.1): llegan actualizaciones y se descodifican")
            verificar("vnc: desconectado" not in s, "la sesión con el vncd sigue abierta")
            vw = ventana(m, "VNC 127.0.0.1:5900", marca)
            cerrar_ventana(m, vw)
            esperar_serial(m, r"vnc: cerrado", 10, marca)
            m.comando("servicio parar vncd", 30)
            m.captura("H253-vnc-cliente", guardar=False)
    finally:
        srv.cerrar()
        srv2.cerrar()


if __name__ == "__main__":
    try:
        principal()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
