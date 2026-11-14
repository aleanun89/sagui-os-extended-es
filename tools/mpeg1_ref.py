#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""mpeg1_ref.py - decodificador de referencia (en Python) de los bloques de un
slice MPEG-1, para comparar con el decodificador de C.

    mpeg1_ref.py video.mpg [macrobloco]

Sirve sólo para depurar: imprime los niveles de los coeficientes de cada bloque.
"""
import math
import sys

sys.path.insert(0, __file__.rsplit('/', 1)[0])
from mpeg1_inspeccionar import fluxo_video  # noqa: E402

# Las tablas de la norma (extraídas del FFmpeg, sólo para referencia).
AC_BITS = [2, 4, 5, 7, 8, 8, 10, 12, 12, 12, 12, 13,
           13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14,
           14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15,
           15, 15, 15, 15, 3, 6, 8, 10, 12, 13, 13, 15,
           15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 4, 7,
           10, 12, 13, 5, 8, 12, 13, 5, 10, 12, 6, 10,
           13, 6, 12, 16, 6, 12, 7, 12, 7, 13, 8, 13,
           8, 16, 8, 16, 8, 16, 10, 16, 10, 16, 10, 16,
           12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 16, 16,
           16, 16, 16, 6, 2]
AC_CODES = [0x3, 0x4, 0x5, 0x6, 0x26, 0x21, 0xa, 0x1d, 0x18, 0x13, 0x10, 0x1a,
            0x19, 0x18, 0x17, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x18, 0x17,
            0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x18, 0x17, 0x16, 0x15, 0x14,
            0x13, 0x12, 0x11, 0x10, 0x3, 0x6, 0x25, 0xc, 0x1b, 0x16, 0x15, 0x1f,
            0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x13, 0x12, 0x11, 0x10, 0x5, 0x4,
            0xb, 0x14, 0x14, 0x7, 0x24, 0x1c, 0x13, 0x6, 0xf, 0x12, 0x7, 0x9,
            0x12, 0x5, 0x1e, 0x14, 0x4, 0x15, 0x7, 0x11, 0x5, 0x11, 0x27, 0x10,
            0x23, 0x1a, 0x22, 0x19, 0x20, 0x18, 0xe, 0x17, 0xd, 0x16, 0x8, 0x15,
            0x1f, 0x1a, 0x19, 0x17, 0x16, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1f, 0x1e,
            0x1d, 0x1c, 0x1b, 0x1, 0x2]
RUN = [0] * 40 + [1] * 17 + [2] * 5 + [3] * 4 + [4] * 3 + [5] * 2 + [6] * 2 + \
      [7] * 2 + [8] * 2 + [9] * 2 + [10] * 2 + [11] * 2 + [12] * 2 + [13] * 2 + \
      [14] * 2 + [15] * 2 + [16] * 2 + [17] * 1 + [18] * 1 + [19] * 1 + [20] * 1 + \
      [21] * 1 + [22] * 1 + [23] * 1 + [24] * 1 + [25] * 1 + [26] * 1 + [27] * 1 + \
      [28] * 1 + [29] * 1 + [30] * 1 + [31]
LEVEL = list(range(1, 41)) + list(range(1, 19)) + [1, 2, 3, 4, 5] + \
        [1, 2, 3, 4, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 1, 2] + \
        [1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2] + \
        [1] * 19
DCL_BITS = [3, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 9]
DCL_CODES = [0x4, 0x0, 0x1, 0x5, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe, 0x1ff]
DCC_BITS = [2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10]
DCC_CODES = [0x0, 0x1, 0x2, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe, 0x3fe, 0x3ff]
INCR_BITS = [1, 3, 3, 4, 4, 5, 5, 7, 7, 8, 8, 8,
             8, 8, 8, 10, 10, 10, 10, 10, 10, 11, 11, 11,
             11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 8]
INCR_CODES = [0x1, 0x3, 0x2, 0x3, 0x2, 0x3, 0x2, 0x7, 0x6, 0xb, 0xa, 0x9,
              0x8, 0x7, 0x6, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x23, 0x22, 0x21,
              0x20, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x18, 0x8, 0xf, 0x0]
ZIGZAG = [0, 1, 8, 16, 9, 2, 3, 10,
          17, 24, 32, 25, 18, 11, 4, 5,
          12, 19, 26, 33, 40, 48, 41, 34,
          27, 20, 13, 6, 7, 14, 21, 28,
          35, 42, 49, 56, 57, 50, 43, 36,
          29, 22, 15, 23, 30, 37, 44, 51,
          58, 59, 52, 45, 38, 31, 39, 46,
          53, 60, 61, 54, 47, 55, 62, 63]
MAT_INTRA = [8, 16, 19, 22, 26, 27, 29, 34,
             16, 16, 22, 24, 27, 29, 34, 37,
             19, 22, 26, 27, 29, 34, 34, 38,
             22, 22, 26, 27, 29, 34, 37, 40,
             22, 26, 27, 29, 32, 35, 40, 48,
             26, 27, 29, 32, 35, 40, 48, 58,
             26, 27, 29, 34, 38, 46, 56, 69,
             27, 29, 35, 38, 46, 56, 69, 83]


class Bits:
    def __init__(self, dados, pos=0):
        self.d = dados
        self.p = pos
        self.b = 0
        self.n = 0

    def bit(self):
        if self.n == 0:
            self.b = self.d[self.p] if self.p < len(self.d) else 0
            self.p += 1
            self.n = 8
        self.n -= 1
        return (self.b >> self.n) & 1

    def bits(self, k):
        v = 0
        for _ in range(k):
            v = (v << 1) | self.bit()
        return v

    def signed(self, k):
        v = self.bits(k)
        if v & (1 << (k - 1)):
            v -= 1 << k
        return v


def vlc(bits, bits_tab, codes_tab, n):
    """Un símbolo de la tabla (-1 si no hay código)."""
    p0, b0, n0 = bits.p, bits.b, bits.n
    v = 0
    for _ in range(16):
        v = (v << 1) | bits.bit()
    bits.p, bits.b, bits.n = p0, b0, n0
    melhor = -1
    melhor_len = 0
    for i in range(n):
        nb = bits_tab[i]
        if nb > 16:
            continue
        if (v >> (16 - nb)) == codes_tab[i] and nb > melhor_len:
            melhor = i
            melhor_len = nb
    if melhor < 0:
        return -1
    bits.bits(melhor_len)
    return melhor


def main():
    global ALVO_LINHA, ALVO_COL
    dados = open(sys.argv[1], 'rb').read()
    if len(sys.argv) > 3:
        ALVO_LINHA = int(sys.argv[2])
        ALVO_COL = int(sys.argv[3])
    fluxo, _ = fluxo_video(dados)
    # el primer cuadro
    i = fluxo.find(b'\x00\x00\x01\x00')
    fim = fluxo.find(b'\x00\x00\x01\x00', i + 4)
    quad = fluxo[i:fim]
    # los slices
    p = 4
    mb_larg = 27
    mb_y = 0
    while p + 4 <= len(quad):
        if quad[p:p + 3] != b'\x00\x00\x01':
            p += 1
            continue
        sid = quad[p + 3]
        if sid < 1 or sid > 0xAF:
            break
        mb_y = sid - 1
        ini = p + 4
        f = ini
        while f + 3 <= len(quad) and quad[f:f + 3] != b'\x00\x00\x01':
            f += 1
        dados_slice = quad[ini:f]
        print("slice %d (linha %d), %d bytes" % (sid, mb_y, len(dados_slice)))
        descodifica_slice(dados_slice, mb_y, mb_larg)
        p = f


ALVO_LINHA = 6
ALVO_COL = 12


def descodifica_slice(dados, mb_y, mb_larg):
    b = Bits(dados)
    qscale = b.bits(5)
    while b.bit():
        b.bits(8)
    print("  qscale %d" % qscale)
    mb_x = 0
    ultimo_dc = [128, 128, 128]
    while mb_y < 15:
        # el incremento de la dirección
        pulo = 0
        while True:
            c = vlc(b, INCR_BITS, INCR_CODES, 36)
            if c == 33:
                pulo += 33
                continue
            if c == 34:
                continue
            if c == 35:
                return
            pulo += c
            break
        for _ in range(pulo):
            mb_x += 1
            if mb_x >= mb_larg:
                mb_x = 0
                mb_y += 1
        if mb_y == ALVO_LINHA and mb_x == ALVO_COL:
            print("    MB (%d,%d) comeca nos bits %d, qscale %d" % (mb_x, mb_y, b.p * 8 - b.n, qscale))
            descodifica_mb(b, ultimo_dc, qscale, verbose=True)
        else:
            descodifica_mb(b, ultimo_dc, qscale, verbose=False)
        mb_x += 1
        if mb_x >= mb_larg:
            mb_x = 0
            mb_y += 1


def descodifica_mb(b, ultimo_dc, qscale, verbose):
    if b.bit() == 0:
        if b.bit() == 0:
            raise SystemExit("macrobloco invalido")
        q = b.bits(5)
    tipo = 1
    for bloco in range(6):
        comp = 0 if bloco <= 3 else bloco - 3
        bits_tab, codes_tab = (DCL_BITS, DCL_CODES) if comp == 0 else (DCC_BITS, DCC_CODES)
        tam = vlc(b, bits_tab, codes_tab, 12)
        diff = 0
        if tam > 0:
            v = b.bits(tam)
            if v < (1 << (tam - 1)):
                diff = v - ((1 << tam) - 1)
            else:
                diff = v
        ultimo_dc[comp] += diff
        dc = ultimo_dc[comp]
        coef = [0] * 64
        coef[0] = dc * MAT_INTRA[0]
        pos = 0
        while True:
            idx = vlc(b, AC_BITS, AC_CODES, 113)
            if idx == 112:
                break
            if idx == 111:
                pos_bit = b.p * 8 - b.n
                run = b.bits(6) + 1
                if verbose:
                    print("    fuga na pos de bits %d, run %d" % (pos_bit, run))
                v = b.bits(8)
                if v == 0x80:
                    v = b.bits(8) - 256
                elif v == 0:
                    v = b.bits(8)
                elif v > 127:
                    v -= 256
                nivel = v
                fuga = True
            else:
                run = RUN[idx] + 1
                nivel = LEVEL[idx]
                if b.bit():
                    nivel = -nivel
                fuga = False
            pos += run
            j = ZIGZAG[pos]
            l = abs(nivel)
            l = (l * qscale * MAT_INTRA[j]) >> 4
            l = (l - 1) | 1
            if nivel < 0:
                l = -l
            coef[j] = l
            if verbose:
                print("    bloque %d pos %d nivel %d %s coef %d" %
                      (bloco, pos, nivel, "(fuga)" if fuga else "", coef[j]))


if __name__ == "__main__":
    main()
