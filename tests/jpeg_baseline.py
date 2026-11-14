#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""jpeg_baseline.py - um JPEG baseline minúsculo, feito à mão (para as provas).

As fotografias do repositório são JPEG *progressivos* (SOF2) e o
descodificador do SaguiOS só sabe ler os *baseline* (SOF0); e as que são
baseline são de 12 megapixels, lentas demais na emulação. Por isso esta
fábrica: um JPEG baseline de uma cor só, 4:4:4, com as tabelas de Huffman da
norma (anexo K). Só precisa de ser válido: a cor não é exacta, fica a
perto de um tom.
"""
import struct

# Tabela de Huffman da luminância (anexo K da norma JPEG).
BITS_DC = [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
VALS_DC = list(range(12))
BITS_AC = [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D]
VALS_AC = [
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA,
]


def _codigos(bits, vals):
    """(código, comprimento) de cada símbolo, a partir da tabela da norma."""
    tabela = {}
    codigo = 0
    k = 0
    for comprimento in range(1, 17):
        for _ in range(bits[comprimento - 1]):
            tabela[vals[k]] = (codigo, comprimento)
            codigo += 1
            k += 1
        codigo <<= 1
    return tabela


class _Bits:
    """Escritor de bits do fluxo de Huffman (MSB primeiro, com o 0x00 de escape)."""

    def __init__(self):
        self.out = bytearray()
        self.cur = 0
        self.n = 0

    def escrever(self, valor, nbits):
        for i in range(nbits - 1, -1, -1):
            self.cur = (self.cur << 1) | ((valor >> i) & 1)
            self.n += 1
            if self.n == 8:
                self.out.append(self.cur)
                if self.cur == 0xFF:
                    self.out.append(0)       # escape: depois de FF vai um 00
                self.cur = 0
                self.n = 0

    def acabar(self):
        while self.n:
            self.escrever(1, 1)              # o padding é de uns
        return bytes(self.out)


def _categoria(valor):
    """(categoria, bits extra) de um valor com sinal, como na norma.

    A categoria s é o número de bits que o valor ocupa (o alcance é
    [-2^s+1, 2^s-1]). Um valor negativo escreve-se somando 2^s-1; o
    descodificador volta atrás quando o bit mais alto é zero.
    """
    if valor == 0:
        return 0, 0
    n = abs(valor).bit_length()
    extra = valor if valor > 0 else valor + (1 << n) - 1
    return n, extra & ((1 << n) - 1)


def jpeg_solido(larg, alt, r, g, b):
    """Um JPEG baseline de uma cor só, com `larg` x `alt` píxeis."""
    y = 0.299 * r + 0.587 * g + 0.114 * b
    cb = 128 - 0.168736 * r - 0.331264 * g + 0.5 * b
    cr = 128 + 0.5 * r - 0.418688 * g - 0.081312 * b
    dcs = [round((y - 128) / 2), round((cb - 128) / 2), round((cr - 128) / 2)]

    dc_cod = _codigos(BITS_DC, VALS_DC)
    ac_cod = _codigos(BITS_AC, VALS_AC)

    saida = bytearray(b"\xff\xd8")                       # SOI
    saida += b"\xff\xdb" + struct.pack(">H", 2 + 1 + 64) + b"\x00" + bytes([16] * 64)   # DQT
    corpo = struct.pack(">BHHB", 8, alt, larg, 3)      # SOF0 (baseline)
    for cid in (1, 2, 3):
        corpo += bytes([cid, 0x11, 0])                  # amostragem 1x1, tabela 0
    saida += b"\xff\xc0" + struct.pack(">H", 2 + len(corpo)) + corpo
    saida += (b"\xff\xc4" + struct.pack(">H", 2 + 1 + 16 + len(VALS_DC)) + b"\x00" +
              bytes(BITS_DC) + bytes(VALS_DC))          # DHT da luminância (DC)
    saida += (b"\xff\xc4" + struct.pack(">H", 2 + 1 + 16 + len(VALS_AC)) + b"\x10" +
              bytes(BITS_AC) + bytes(VALS_AC))          # DHT da luminância (AC)
    sos = bytes([3, 1, 0x00, 2, 0x00, 3, 0x00, 0, 63, 0])
    saida += b"\xff\xda" + struct.pack(">H", 2 + len(sos)) + sos

    bits = _Bits()
    mcus = ((larg + 7) // 8) * ((alt + 7) // 8)
    for mcu in range(mcus):
        for ci, dc in enumerate(dcs):
            # O preditor de DC e por componente: o primeiro bloco de cada
            # componente leva o valor inteiro; os seguintes levam a diferenca
            # (que aqui e sempre zero, porque a imagem e de uma cor so).
            diff = dc if mcu == 0 else 0
            cat, extra = _categoria(diff)
            cod, n = dc_cod[cat]
            bits.escrever(cod, n)
            if cat:
                bits.escrever(extra, cat)
            cod, n = ac_cod[0x00]                        # EOB: não há coeficientes AC
            bits.escrever(cod, n)
    saida += bits.acabar()
    saida += b"\xff\xd9"                                # EOI
    return bytes(saida)
