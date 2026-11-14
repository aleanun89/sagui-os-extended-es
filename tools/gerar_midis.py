#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Genera los archivos MIDI de ejemplo de SaguiOS (D190) en rootfs/ejemplos/musica/.

Todo es propio o de dominio público; no se copia ningún archivo de nadie:
  escala_la.mid  formato 0. Escala de La mayor de dos octavas (La 440 Hz, nota por nota), con piano;
                 sirve también a la prueba h157 (el espectro debe tener las notas de la escala).
  alegria.kar    formato 1, karaoke. Tema de la «Oda a la alegría» (Beethoven, 1824: dominio público)
                 para flauta, con acompañamiento de piano, bajo y percusión (canal 10), un cambio de
                 tempo (ritardando) y una letra PROPIA en español en metaeventos de texto.
  elisa.mid      formato 0. Comienzo de «Para Elisa» (Beethoven, c. 1810: dominio público) para piano.
  gm_canales.mid formato 1. Un acorde de Do mayor con varios instrumentos de General MIDI, uno por canal,
                 y percusión: muestra la vista de canales.

Uso: tools/gerar_midis.py [carpeta]   (por defecto rootfs/ejemplos/musica)
"""
import os
import struct
import sys


def vlq(n):
    b = [n & 0x7F]
    n >>= 7
    while n:
        b.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(b))


class Pista:
    def __init__(self):
        self.ev = []   # (tick, orden, bytes)

    def add(self, t, datos):
        self.ev.append((t, len(self.ev), bytes(datos)))

    def meta(self, t, tipo, datos):
        self.add(t, bytes([0xFF, tipo]) + vlq(len(datos)) + datos)

    def nota(self, t, dur, ch, n, vel=90):
        self.add(t, [0x90 | ch, n, vel])
        self.add(t + dur, [0x80 | ch, n, 0])

    def prog(self, t, ch, p):
        self.add(t, [0xC0 | ch, p])

    def cc(self, t, ch, c, v):
        self.add(t, [0xB0 | ch, c, v])

    def bytes(self):
        self.ev.sort(key=lambda e: (e[0], e[1]))
        out = b""
        ant = 0
        for t, _, d in self.ev:
            out += vlq(t - ant) + d
            ant = t
        out += vlq(0) + b"\xFF\x2F\x00"
        return b"MTrk" + struct.pack(">I", len(out)) + out


def escribir(ruta, formato, ppqn, pistas):
    datos = b"MThd" + struct.pack(">IHHH", 6, formato, len(pistas), ppqn)
    for p in pistas:
        datos += p.bytes()
    with open(ruta, "wb") as f:
        f.write(datos)
    print(f"{ruta}: {len(datos)} bytes")


def tempo(bpm):
    return struct.pack(">I", int(60000000 / bpm))[1:]


NOTAS = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def n(s):
    """'C4' = 60, 'D#5', 'Bb3'."""
    v = NOTAS[s[0]]
    i = 1
    while s[i] in "#b":
        v += 1 if s[i] == "#" else -1
        i += 1
    return 12 * (int(s[i:]) + 1) + v


def escala_la():
    p = Pista()
    p.meta(0, 3, b"Escala de La mayor")
    p.meta(0, 2, b"Generado para SaguiOS (dominio publico)")
    p.meta(0, 0x51, tempo(120))
    p.prog(0, 0, 0)
    t = 0
    q = 480
    sube = [n(x) for x in ("A4", "B4", "C#5", "D5", "E5", "F#5", "G#5", "A5")]
    for x in sube:
        p.nota(t, q - 40, 0, x, 96)
        t += q
    for x in reversed(sube[:-1]):
        p.nota(t, q - 40, 0, x, 96)
        t += q
    escribir("escala_la.mid", 0, 480, [p])


ODA = [  # (nota, duracion en negras)
    ("E4", 1), ("E4", 1), ("F4", 1), ("G4", 1), ("G4", 1), ("F4", 1), ("E4", 1), ("D4", 1),
    ("C4", 1), ("C4", 1), ("D4", 1), ("E4", 1), ("E4", 1.5), ("D4", .5), ("D4", 2),
    ("E4", 1), ("E4", 1), ("F4", 1), ("G4", 1), ("G4", 1), ("F4", 1), ("E4", 1), ("D4", 1),
    ("C4", 1), ("C4", 1), ("D4", 1), ("E4", 1), ("D4", 1.5), ("C4", .5), ("C4", 2),
    ("D4", 1), ("D4", 1), ("E4", 1), ("C4", 1), ("D4", 1), ("E4", .5), ("F4", .5), ("E4", 1), ("C4", 1),
    ("D4", 1), ("E4", .5), ("F4", .5), ("E4", 1), ("D4", 1), ("C4", 1), ("D4", 1), ("G3", 2),
    ("E4", 1), ("E4", 1), ("F4", 1), ("G4", 1), ("G4", 1), ("F4", 1), ("E4", 1), ("D4", 1),
    ("C4", 1), ("C4", 1), ("D4", 1), ("E4", 1), ("D4", 1.5), ("C4", .5), ("C4", 2),
]
# letra propia: una sílaba por nota (las dos últimas frases repiten la música de las primeras)
_PALABRAS = ("Un-a can-ción de paz pa-ra to-dos los ni-ños hoy "
             "yo can-ta-ré con vos-o-tros al-zad las vo-ces ya "
             "el sol sa-le a ca-len-tar la tie-rra y el mar "
             "can-ta-mos al fin con a-le-gría hoy "
             "un-a can-ción de paz pa-ra to-dos los ni-ños hoy").split()
# en un .kar el espacio entre palabras va al principio de la primera sílaba de cada palabra
# (los saltos de línea van antes de «yo», «el», «can» y «un», donde empieza cada frase de la letra)
SILABAS = [("" if k else " ") + sil for pal in _PALABRAS for k, sil in enumerate(pal.split("-"))]


def alegria():
    q = 480
    t0 = Pista()
    t0.meta(0, 3, b"Himno a la alegria")
    t0.meta(0, 2, b"Beethoven (dominio publico). Arreglo y letra: SaguiOS")
    t0.meta(0, 0x58, bytes([4, 2, 24, 8]))
    t0.meta(0, 0x51, tempo(112))
    t0.meta(14 * 4 * q, 0x51, tempo(96))      # ritardando en los dos últimos compases
    t0.meta(15 * 4 * q, 0x51, tempo(80))
    pal = Pista()
    pal.meta(0, 3, b"Words")
    pal.meta(0, 1, b"@KMIDI KARAOKE FILE")
    pal.meta(0, 1, b"@TAlegria")
    pal.meta(0, 1, b"@TBeethoven / SaguiOS")
    mel = Pista()
    mel.meta(0, 3, b"Melodia")
    mel.prog(0, 0, 73)
    mel.cc(0, 0, 7, 110)
    t = 0.0
    k = 0
    nuevo = {0: b"\\", 14: b"/", 28: b"/", 42: b"/", 52: b"/"}
    for i, (nombre, dur) in enumerate(ODA):
        tick = int(t * q)
        mel.nota(tick, int(dur * q * 0.92), 0, n(nombre), 100)
        sil = SILABAS[i % len(SILABAS)].encode("latin-1")
        mark = nuevo.get(i, b"")
        pal.meta(tick, 1, mark + (sil.lstrip() if mark else sil))
        t += dur
    fin = int(t * q)
    # acompañamiento: acordes de piano (un acorde por compás), bajo y percusión
    pia = Pista()
    pia.meta(0, 3, b"Piano")
    pia.prog(0, 1, 0)
    pia.cc(0, 1, 7, 85)
    pia.cc(0, 1, 10, 40)
    baj = Pista()
    baj.meta(0, 3, b"Bajo")
    baj.prog(0, 2, 33)
    baj.cc(0, 2, 7, 105)
    baj.cc(0, 2, 10, 80)
    bat = Pista()
    bat.meta(0, 3, b"Bateria")
    acordes = {"C": (("C4", "E4", "G4"), "C2"), "G": (("B3", "D4", "G4"), "G2"), "F": (("A3", "C4", "F4"), "F2")}
    prog = "C C C G C C C G G G G C C C C G".split()
    for c in range(16):
        ac, raiz = acordes[prog[c]]
        t = c * 4 * q
        for x in ac:
            pia.nota(t, 2 * q - 30, 1, n(x) - 12 if False else n(x), 70)
            pia.nota(t + 2 * q, 2 * q - 30, 1, n(x), 62)
        baj.nota(t, q * 2 - 20, 2, n(raiz), 100)
        baj.nota(t + 2 * q, q * 2 - 20, 2, n(raiz) + (7 if prog[c] == "C" else 0), 90)
        for b in range(8):
            bat.nota(t + b * q // 2, q // 4, 9, 42, 70 if b % 2 else 90)
        bat.nota(t, q // 2, 9, 36, 110)
        bat.nota(t + 2 * q, q // 2, 9, 36, 100)
        bat.nota(t + q, q // 2, 9, 38, 95)
        bat.nota(t + 3 * q, q // 2, 9, 38, 95)
    # compás final
    t = 16 * 4 * q
    for x in ("C4", "E4", "G4", "C5"):
        pia.nota(t, 3 * q, 1, n(x), 90)
    baj.nota(t, 3 * q, 2, n("C2"), 105)
    mel.nota(t, 3 * q, 0, n("C5"), 100)
    bat.nota(t, 4 * q, 9, 49, 100)
    bat.nota(t, q, 9, 36, 110)
    escribir("alegria.kar", 1, q, [t0, pal, mel, pia, baj, bat])


def elisa():
    u = 120  # una semicorchea (ppqn = 480)
    p = Pista()
    p.meta(0, 3, b"Para Elisa (comienzo)")
    p.meta(0, 2, b"Beethoven (dominio publico). Arreglo: SaguiOS")
    p.meta(0, 0x51, tempo(72 * 2))
    p.prog(0, 0, 0)
    p.cc(0, 0, 91, 50)
    frase1 = [("E5", 1), ("D#5", 1), ("E5", 1), ("D#5", 1), ("E5", 1), ("B4", 1), ("D5", 1), ("C5", 1), ("A4", 3), (None, 1),
              ("C4", 1), ("E4", 1), ("A4", 1), ("B4", 3), (None, 1), ("E4", 1), ("G#4", 1), ("B4", 1), ("C5", 3), (None, 1)]
    frase2 = [("E4", 1), ("E5", 1), ("D#5", 1), ("E5", 1), ("D#5", 1), ("E5", 1), ("B4", 1), ("D5", 1), ("C5", 1),
              ("A4", 3), (None, 1), ("C4", 1), ("E4", 1), ("A4", 1), ("B4", 3), (None, 1), ("E4", 1), ("C5", 1), ("B4", 1), ("A4", 6)]
    t = 0
    for fr in (frase1, frase2):
        for nota, d in fr:
            if nota:
                p.nota(t * u, int(d * u * 0.9), 0, n(nota), 88)
            t += d
    # mano izquierda: arpegios en La menor y Mi mayor
    t = 8
    for raiz, quinta, octava in (("A2", "E3", "A3"), ("E2", "E3", "G#3"), ("A2", "E3", "A3")):
        for k, x in enumerate((raiz, quinta, octava)):
            p.nota((t + k * 2) * u, 2 * u - 10, 0, n(x), 62)
        t += 6
    t = 30
    for raiz, quinta, octava in (("A2", "E3", "A3"), ("E2", "E3", "G#3"), ("A2", "E3", "A3")):
        for k, x in enumerate((raiz, quinta, octava)):
            p.nota((t + k * 2) * u, 2 * u - 10, 0, n(x), 62)
        t += 6
    p.nota(46 * u, 8 * u, 0, n("A2"), 70)
    p.nota(46 * u, 8 * u, 0, n("A3"), 66)
    escribir("elisa.mid", 0, 480, [p])


def gm_canales():
    q = 480
    t0 = Pista()
    t0.meta(0, 3, b"Demo de canales General MIDI")
    t0.meta(0, 2, b"Generado para SaguiOS (dominio publico)")
    t0.meta(0, 0x51, tempo(100))
    instr = [(0, 0, "Piano"), (1, 11, "Vibrafono"), (2, 19, "Organo"), (3, 24, "Guitarra"), (4, 33, "Bajo"), (5, 40, "Violin"),
             (6, 48, "Cuerdas"), (7, 56, "Trompeta"), (8, 68, "Oboe"), (10, 73, "Flauta"), (11, 81, "Lead"), (12, 89, "Pad"),
             (13, 104, "Sitar"), (14, 4, "Piano electrico"), (15, 14, "Campanas tubulares")]
    pistas = [t0]
    acordes = [("C", 0, 4, 7), ("A", -3, 0, 4), ("F", -7, -3, 0), ("G", -5, -1, 2)]
    for ch, prog, nombre in instr:
        p = Pista()
        p.meta(0, 3, nombre.encode())
        p.prog(0, ch, prog)
        p.cc(0, ch, 10, 10 + ch * 7)
        base = {4: 36, 3: 48}.get(ch, 60 if ch not in (5, 8, 10, 13) else 67)
        for c, (nom, a, b, d) in enumerate(acordes):
            t = c * 4 * q
            for k, x in enumerate((a, b, d, b)):
                p.nota(t + k * q + ch * 30, q - 20, ch, base + x, 80 + (k == 0) * 20)
        pistas.append(p)
    bat = Pista()
    bat.meta(0, 3, b"Bateria")
    for c in range(4):
        t = c * 4 * q
        for b in range(8):
            bat.nota(t + b * q // 2, q // 4, 9, 42, 70)
        for b, nota in enumerate((36, 38, 36, 38)):
            bat.nota(t + b * q, q // 2, 9, nota, 100)
    bat.nota(0, q, 9, 49, 90)
    pistas.append(bat)
    escribir("gm_canales.mid", 1, q, pistas)


if __name__ == "__main__":
    destino = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "rootfs", "ejemplos", "musica")
    os.makedirs(destino, exist_ok=True)
    os.chdir(destino)
    escala_la()
    alegria()
    elisa()
    gm_canales()
