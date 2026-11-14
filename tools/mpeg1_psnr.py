#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""mpeg1_psnr.py - compara la salida del decodificador con la de ffmpeg.

    mpeg1_psnr.py mio.rgb ref.rgb

Los dos archivos son RGB24 en crudo; el primero lleva la cabecera de texto
("SAGUIV1 <ancho> <alto>"). Dice el PSNR de cada cuadro y la media.
"""
import math
import sys


def ler_cabecera(f):
    """Lee la cabecera de texto y devuelve (ancho, alto, pos)."""
    linha = b""
    while not linha.endswith(b"\n"):
        c = f.read(1)
        if not c:
            raise EOFError("no hay cabecera")
        linha += c
    partes = linha.split()
    if partes[0] != b"SAGUIV1":
        raise ValueError("cabecera desconocida: %s" % linha)
    return int(partes[1]), int(partes[2]), f.tell()


def psnr(a, b):
    """El PSNR entre dos listas de bytes (la misma longitud)."""
    n = len(a)
    if n != len(b):
        raise ValueError("longitudes distintas: %d y %d" % (n, len(b)))
    total = 0
    for x, y in zip(a, b):
        d = x - y
        total += d * d
    mse = total / n
    if mse == 0:
        return 99.0
    return 10.0 * math.log10(255.0 * 255.0 / mse)


def principal():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    with open(sys.argv[1], "rb") as f:
        larg, alt, pos = ler_cabecera(f)
        meu = f.read()
    with open(sys.argv[2], "rb") as f:
        ref = f.read()
    tam = larg * alt * 3
    print("cuadro %dx%d, mio %d cuadros, ref %d cuadros" %
          (larg, alt, len(meu) // tam, len(ref) // tam))
    n = min(len(meu), len(ref)) // tam
    soma = 0.0
    pior = 99.0
    for i in range(n):
        a = meu[i * tam:(i + 1) * tam]
        b = ref[i * tam:(i + 1) * tam]
        p = psnr(a, b)
        soma += p
        if p < pior:
            pior = p
        print("  quadro %2d: %6.2f dB" % (i, p))
    if n:
        print("media: %6.2f dB   pior: %6.2f dB" % (soma / n, pior))
    return 0


if __name__ == "__main__":
    sys.exit(principal())
