#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Genera los sonidos y las músicas de los ejemplos de Capibara (obra propia, sintetizados; licencia MIT/CC0).

Uso: generar_sonidos.py carpeta_plataformas carpeta_disparos   (escribe audio/*.wav de 22050 Hz, mono, 16 bits)
Determinista: el mismo resultado en cada ejecución (semilla fija)."""
import math, os, random, struct, sys, wave

TASA = 22050

def guardar(ruta, muestras, vol=0.8):
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    with wave.open(ruta, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(TASA)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, m * vol)) * 32767)) for m in muestras))

def onda(tipo, fase):
    f = fase % 1.0
    if tipo == "cuadrada": return 1.0 if f < 0.5 else -1.0
    if tipo == "triangulo": return 4 * abs(f - 0.5) - 1
    if tipo == "sierra": return 2 * f - 1
    return math.sin(2 * math.pi * f)

def tono(f0, f1, dur, tipo="cuadrada", ataque=0.005, caida=None):
    n = int(dur * TASA); out = []; fase = 0.0
    for i in range(n):
        t = i / n; f = f0 + (f1 - f0) * t; fase += f / TASA
        env = min(1.0, i / (ataque * TASA + 1)) * (1 - t) ** (1.0 if caida is None else caida)
        out.append(onda(tipo, fase) * env)
    return out

def ruido(dur, caida=2.0, filtro=0.5, semilla=1):
    r = random.Random(semilla); n = int(dur * TASA); out = []; y = 0.0
    for i in range(n):
        y += (r.uniform(-1, 1) - y) * filtro
        out.append(y * (1 - i / n) ** caida)
    return out

def mezclar(*pistas):
    n = max(len(p) for p in pistas); out = [0.0] * n
    for p in pistas:
        for i, v in enumerate(p): out[i] += v
    return out

def notas(secuencia, bpm, tipo="triangulo", vol=1.0):
    """secuencia: lista de (nota MIDI o None, duración en pulsos)."""
    out = []; paso = 60.0 / bpm
    for nota, dur in secuencia:
        n = int(dur * paso * TASA)
        if nota is None: out += [0.0] * n; continue
        f = 440 * 2 ** ((nota - 69) / 12); fase = 0.0
        for i in range(n):
            t = i / n; fase += f / TASA
            env = min(1.0, i / 200.0) * (1 - 0.6 * t)
            out.append(onda(tipo, fase) * env * vol)
    return out

def musica_plataformas():
    # 16 compases de do mayor, ritmo alegre; melodía de onda cuadrada suave y bajo triangular
    m = [(72, .5), (76, .5), (79, .5), (76, .5), (74, .5), (77, .5), (81, .5), (77, .5), (76, .5), (79, .5), (84, .5), (79, .5), (74, .5), (79, .5), (77, .5), (74, .5)]
    m += [(72, .5), (76, .5), (79, .5), (84, .5), (83, .5), (79, .5), (77, .5), (74, .5), (72, 1), (None, .5), (67, .5), (72, 2)]
    b = []
    for raiz in (48, 53, 55, 48):
        b += [(raiz, 1), (raiz + 7, 1), (raiz, 1), (raiz + 7, 1)]
    mel = notas(m, 150, "cuadrada", 0.22); bajo = notas(b, 150, "triangulo", 0.5)
    n = max(len(mel), len(bajo)); mel += [0.0] * (n - len(mel)); bajo += [0.0] * (n - len(bajo))
    return mezclar(mel, bajo)

def musica_disparos():
    m = [(69, .5), (72, .5), (76, .5), (72, .5), (69, .5), (72, .5), (77, .5), (72, .5), (67, .5), (71, .5), (74, .5), (71, .5), (64, .5), (67, .5), (71, .5), (67, .5)]
    m += [(69, .5), (72, .5), (76, .5), (81, .5), (80, .5), (76, .5), (72, .5), (69, .5), (68, 1), (64, 1), (69, 2)]
    b = []
    for raiz in (45, 41, 43, 40):
        b += [(raiz, .5), (raiz, .5), (raiz + 12, .5), (raiz, .5)] * 2
    mel = notas(m, 168, "sierra", 0.16); bajo = notas(b, 168, "cuadrada", 0.2)
    n = max(len(mel), len(bajo)); mel += [0.0] * (n - len(mel)); bajo += [0.0] * (n - len(bajo))
    return mezclar(mel, bajo)

def main(pl, di):
    guardar(os.path.join(pl, "audio/salto.wav"), tono(260, 640, 0.18), 0.5)
    guardar(os.path.join(pl, "audio/moneda.wav"), mezclar(tono(988, 988, 0.07, caida=0.3) + tono(1319, 1319, 0.22, caida=1.0), []), 0.45)
    guardar(os.path.join(pl, "audio/golpe.wav"), mezclar(ruido(0.22, 1.5, 0.3, 4), tono(180, 60, 0.22, "triangulo")), 0.7)
    guardar(os.path.join(pl, "audio/pisoton.wav"), mezclar(tono(520, 140, 0.12, "cuadrada"), ruido(0.1, 2, 0.4, 5)), 0.55)
    guardar(os.path.join(pl, "audio/victoria.wav"), notas([(72, .25), (76, .25), (79, .25), (84, .75)], 160, "cuadrada", 0.5), 0.6)
    guardar(os.path.join(pl, "audio/musica.wav"), musica_plataformas(), 0.55)
    guardar(os.path.join(di, "audio/disparo.wav"), tono(1100, 280, 0.09, "cuadrada"), 0.32)
    guardar(os.path.join(di, "audio/explosion.wav"), mezclar(ruido(0.5, 1.2, 0.18, 7), tono(110, 35, 0.5, "triangulo")), 0.8)
    guardar(os.path.join(di, "audio/impacto.wav"), mezclar(ruido(0.12, 1.5, 0.5, 9), tono(300, 120, 0.12)), 0.5)
    guardar(os.path.join(di, "audio/mejora.wav"), notas([(76, .15), (81, .15), (88, .4)], 200, "triangulo", 1.0), 0.6)
    guardar(os.path.join(di, "audio/musica.wav"), musica_disparos(), 0.5)

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
