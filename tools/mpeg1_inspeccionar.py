#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""mpeg1_inspeccionar.py - mira por dentro de un MPEG-PS (o de su flujo elemental).

Herramienta de depuración: dice qué paquetes hay, qué lleva el flujo de vídeo
(MPEG-1) y, sobre todo, el **orden de presentación** de los cuadros (los
cuadros B se decodifican en otro orden que en el que se ven).

    mpeg1_inspeccionar.py video.mpg
"""
import sys

TIPO = {1: "I", 2: "P", 3: "B", 4: "D", 5: "P5"}
FPS = {1: 24000 / 1001, 2: 24.0, 3: 25.0, 4: 30000 / 1001, 5: 30.0,
       6: 50.0, 7: 60000 / 1001, 8: 60.0}


class Leitor:
    """Lee bits de un búfer (el lector de bits del MPEG-1)."""

    def __init__(self, dados, pos=0):
        self.d = dados
        self.p = pos
        self.bits = 0

    def bit(self):
        if self.p >= len(self.d):
            raise EOFError("se acabó el flujo")
        b = (self.d[self.p] >> (7 - self.bits)) & 1
        self.bits += 1
        if self.bits == 8:
            self.bits = 0
            self.p += 1
        return b

    def bitsn(self, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | self.bit()
        return v


def ler_pes(dados, i):
    """Lee un paquete PES en la posición i.

    Devuelve (fim, stream, carga_i, carga_fim); carga_i es None si el paquete
    no lleva carga (cabecera de pack, de sistema o de secuencia).
    """
    if i + 4 > len(dados):
        return None
    if dados[i] != 0 or dados[i + 1] != 0 or dados[i + 2] != 1:
        return None
    sid = dados[i + 3]
    if sid == 0xBA:                      # cabecera de pack: longitud variable
        # (MPEG-1: 12 bytes; MPEG-2: 14, más relleno), así que se busca el
        # siguiente código de inicio.
        j = i + 4
        while j + 3 < len(dados) and not (dados[j] == 0 and dados[j + 1] == 0 and dados[j + 2] == 1):
            j += 1
        return (j, sid, None, None)
    if sid == 0xB9:                      # fin del programa
        return (None, sid, None, None)
    if sid in (0xBB, 0xB8):              # cabecera del sistema o del grupo
        if i + 6 > len(dados):
            return None
        tam = (dados[i + 4] << 8) | dados[i + 5]
        return (i + 6 + tam, sid, None, None)
    if sid == 0xB3:                      # cabecera de secuencia
        return (None, sid, None, None)
    if i + 6 > len(dados):
        return None
    tam = (dados[i + 4] << 8) | dados[i + 5]
    # La cabecera del PES: su longitud depende del estilo (MPEG-1 o MPEG-2) y
    # el byte de flags es ambiguo; lo fiable es que la carga del paquete
    # empieza en el primer código de inicio (00 00 01) que viene después.
    cab = 0
    for cab in range(6, 34):
        if (dados[i + cab] == 0 and dados[i + cab + 1] == 0 and dados[i + cab + 2] == 1):
            break
    p = i + cab
    if p > len(dados):
        return None
    fim = i + 6 + tam
    if tam == 0:
        # Longitud 0: el paquete llega hasta el siguiente código de inicio.
        j = p
        while j + 3 < len(dados) and not (dados[j] == 0 and dados[j + 1] == 0 and dados[j + 2] == 1):
            j += 1
        fim = j
    return (fim, sid, p, fim)


def fluxo_video(dados):
    """Los bytes del flujo de vídeo MPEG-1 (el contenido de los PES 0xE0)."""
    i = 0
    partes = []
    n_pes = 0
    while i < len(dados) - 4:
        if dados[i] == 0 and dados[i + 1] == 0 and dados[i + 2] == 1:
            sid = dados[i + 3]
            r = ler_pes(dados, i)
            if r is None:
                i += 1
                continue
            fim, s, pi, pf = r
            if sid in (0xE0, 0xE1, 0xE2, 0xE3) and pi is not None:
                partes.append(dados[pi:pf])
                n_pes += 1
            if fim is None:
                break
            i = fim
        else:
            i += 1
    return b"".join(partes), n_pes


def cabeceras(fluxo):
    """Recorre el flujo y saca las cabeceras de secuencia y de cuadro."""
    seqs, pics = [], []
    i = 0
    while i < len(fluxo) - 4:
        if fluxo[i] == 0 and fluxo[i + 1] == 0 and fluxo[i + 2] == 1:
            cod = fluxo[i + 3]
            if cod == 0xB3:          # cabecera de secuencia
                r = Leitor(fluxo, i + 4)
                larg = r.bitsn(12)
                alt = r.bitsn(12)
                aspecto = r.bitsn(4)
                cod_fps = r.bitsn(4)
                seqs.append((larg, alt, aspecto, cod_fps))
            elif cod == 0x00:        # cabecera de cuadro
                r = Leitor(fluxo, i + 4)
                tr = r.bitsn(10)
                tipo = r.bitsn(3)
                r.bitsn(16)           # vbv_delay
                pics.append((tipo, tr))
            i += 1
        else:
            i += 1
    return seqs, pics


def principal():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    dados = open(sys.argv[1], "rb").read()
    fluxo, n_pes = fluxo_video(dados)
    print(f"archivo: {sys.argv[1]} ({len(dados)} bytes), {n_pes} PES de vídeo, "
          f"flujo de vídeo: {len(fluxo)} bytes")
    seqs, pics = cabeceras(fluxo)
    for s in seqs[:3]:
        print(f"secuencia: {s[0]}x{s[1]}, aspecto {s[2]}, fps {FPS.get(s[3], s[3])}")
    print(f"cuadros: {len(pics)}")
    for n, (tipo, tr) in enumerate(pics[:40]):
        print(f"  {n:3d}  {TIPO.get(tipo, tipo)}  TR={tr}")
    return 0


if __name__ == "__main__":
    sys.exit(principal())
