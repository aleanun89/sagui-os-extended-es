#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""psf2fonte.py - convierte fuentes PSF 8x16 (con tabla Unicode) a un cabecero C con los 256 códigos Latin-1.

Uso: psf2fonte.py SALIDA.h nombre=ruta.psf.gz ...
Cada fuente sale como `static const unsigned char fonte_sis_<nombre>[256][16]` (un byte por línea, bit 7 = columna
izquierda, igual que fonte_sagui). Los códigos Latin-1 sin glifo en la fuente usan el de la fuente de SaguiOS (se avisa)."""
import gzip
import struct
import sys


def leer_psf(ruta):
    d = gzip.open(ruta).read() if ruta.endswith(".gz") else open(ruta, "rb").read()
    if d[:2] == b"\x36\x04":                       # PSF1
        modo, alto = d[2], d[3]
        n = 512 if modo & 1 else 256
        ini, ancho = 4, 8
        tabla = modo & 6
        unip = None
        if tabla:
            pos = ini + n * alto
            unip = []
            for _ in range(n):
                us = []
                while True:
                    u = struct.unpack_from("<H", d, pos)[0]
                    pos += 2
                    if u == 0xFFFF:
                        break
                    if u != 0xFFFE:
                        us.append(u)
                unip.append(us)
    elif d[:4] == b"\x72\xb5\x4a\x86":             # PSF2
        _v, hs, flags, n, bpg, alto, ancho = struct.unpack_from("<7I", d, 4)
        ini = hs
        unip = None
        if flags & 1:
            pos = ini + n * bpg
            unip = []
            for _ in range(n):
                us = []
                while True:
                    b = d[pos]
                    if b == 0xFF:
                        pos += 1
                        break
                    if b == 0xFE:
                        pos += 1
                        continue
                    # UTF-8
                    l = 1 if b < 0x80 else 2 if b < 0xE0 else 3 if b < 0xF0 else 4
                    us.append(ord(d[pos:pos + l].decode("utf-8")))
                    pos += l
                unip.append(us)
        alto = bpg // ((ancho + 7) // 8)
    else:
        sys.exit("psf2fonte: formato desconocido " + ruta)
    if ancho != 8 or alto != 16:
        sys.exit(f"psf2fonte: {ruta} no es 8x16 ({ancho}x{alto})")
    bpg = alto
    glifos = [d[ini + i * bpg: ini + (i + 1) * bpg] for i in range(n)]
    return glifos, unip


def main():
    salida = sys.argv[1]
    out = ["/* SPDX-License-Identifier: OFL-1.1 AND LicenseRef-Dominio-publico */\n/* Datos de terceros (docs/terceros.md): Terminus (SIL OFL 1.1) y Fixed (X11 misc-fixed, dominio público), convertidos de las\n * fuentes de consola PSF Uni3-Terminus16, Uni3-TerminusBold16 y Uni3-Fixed16 (paquete console-setup). */\n/* Generado por tools/psf2fonte.py: fuentes de mapa de bits 8x16 con los 256 códigos Latin-1 (D428). No editar. */\n"]
    for arg in sys.argv[2:]:
        nombre, ruta = arg.split("=", 1)
        glifos, unip = leer_psf(ruta)
        por_u = {}
        for i, us in enumerate(unip or []):
            for u in us:
                por_u.setdefault(u, i)
        out.append(f"static const unsigned char fonte_sis_{nombre}[256][16] = {{\n")
        faltan = []
        for c in range(256):
            i = por_u.get(c, c if not unip and c < len(glifos) else None)
            if i is None:
                faltan.append(c)
                g = bytes(16)
            else:
                g = glifos[i]
            out.append("    {" + ",".join("0x%02X" % b for b in g) + "},\n")
        out.append("};\n")
        sys.stderr.write(f"{nombre}: sin glifo {[hex(c) for c in faltan if c >= 32 and not 0x7f <= c < 0xa0]}\n")
    open(salida, "w", encoding="utf-8").write("".join(out))


main()
