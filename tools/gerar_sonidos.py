#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""gerar_sonidos.py - el esquema de sonidos propio «SaguiOS» (D421), sintetizado por código.

Hace diez WAV deterministas (mono, 22050 Hz, 16 bits) con acordes y campanas: sumas de senos con envolventes
exponenciales, sin ninguna muestra de terceros ni aleatoriedad (el mismo guion da siempre los mismos bytes).
Es el esquema libre de serie: «generado por SaguiOS-Extended, GPL v3».

Uso: gerar_sonidos.py DESTINO     (crea DESTINO/esquema.conf, CREDITOS.txt y los .wav)
"""
import math
import os
import struct
import sys

TAXA = 22050
# Notas (Hz), afinación igual con La = 440
C4, D4, E4, F4, G4, A4, B4 = 261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88
C5, D5, E5, G5, A5, C6 = 523.25, 587.33, 659.25, 783.99, 880.00, 1046.50


def campana(f, dur, ganancia=1.0, decai=5.0):
    """Una campana: fundamental y parciales inarmónicos que se apagan antes cuanto más agudos."""
    n = int(TAXA * dur)
    parciales = [(1.0, 1.0), (2.0, 0.45), (2.76, 0.25), (5.4, 0.10)]
    out = [0.0] * n
    for k, (mult, amp) in enumerate(parciales):
        w = 2 * math.pi * f * mult / TAXA
        d = decai * (1 + k * 0.9)
        for i in range(n):
            t = i / TAXA
            out[i] += ganancia * amp * math.exp(-d * t) * math.sin(w * i)
    # ataque de 3 ms para evitar el clic
    for i in range(min(n, int(TAXA * 0.003))):
        out[i] *= i / (TAXA * 0.003)
    return out


def tono(f, dur, ganancia=1.0, decai=6.0):
    """Un tono suave (seno con un armónico), para avisos más secos que la campana."""
    n = int(TAXA * dur)
    out = []
    for i in range(n):
        t = i / TAXA
        a = min(1.0, i / (TAXA * 0.004)) * math.exp(-decai * t)
        out.append(ganancia * a * (math.sin(2 * math.pi * f * t) + 0.3 * math.sin(4 * math.pi * f * t)))
    return out


def poner(mezcla, nota, inicio):
    i0 = int(TAXA * inicio)
    if len(mezcla) < i0 + len(nota):
        mezcla.extend([0.0] * (i0 + len(nota) - len(mezcla)))
    for i, v in enumerate(nota):
        mezcla[i0 + i] += v


def acorde(freqs, dur, escalonado=0.0, ganancia=0.6, decai=3.5):
    m = []
    for k, f in enumerate(freqs):
        poner(m, campana(f, dur, ganancia / math.sqrt(len(freqs)), decai), k * escalonado)
    return m


def secuencia(notas, paso, dur, fn=campana, ganancia=0.7, decai=5.0):
    m = []
    for k, f in enumerate(notas):
        poner(m, fn(f, dur, ganancia, decai), k * paso)
    return m


def sonidos():
    s = {}
    # Arranque: acorde de Do mayor con séptima mayor, que sube arpegiado y se queda sonando.
    s["arranque"] = acorde([C4, E4, G4, C5, E5], 1.8, 0.14, 0.9, 2.6)
    # Apagado: el mismo acorde, bajando y más corto.
    m = []
    for k, f in enumerate([E5, C5, G4, E4, C4]):
        poner(m, campana(f, 1.3, 0.5, 3.4), k * 0.13)
    s["apagar"] = m
    # Reinicio: baja y vuelve a subir (dos terceras).
    s["reiniciar"] = secuencia([G4, E4, C4, E4, G4], 0.12, 0.7, campana, 0.6, 4.5)
    # Aviso: una campana limpia.
    s["aviso"] = campana(A5, 0.9, 0.8, 5.0)
    # Error: dos tonos graves y disonantes (un tritono).
    m = []
    poner(m, tono(F4, 0.30, 0.7, 5.0), 0.0)
    poner(m, tono(185.0, 0.45, 0.7, 4.5), 0.18)
    s["error"] = m
    # Pregunta: dos notas que suben y se quedan en el aire (quinta).
    s["pregunta"] = secuencia([D5, A5], 0.17, 0.55, campana, 0.7, 6.0)
    # USB conectado: dos notas que suben; desconectado: las mismas bajando.
    s["usb_conectar"] = secuencia([E5, A5], 0.11, 0.35, tono, 0.7, 9.0)
    s["usb_desconectar"] = secuencia([A5, E5], 0.11, 0.35, tono, 0.7, 9.0)
    # Batería baja: tres pitidos iguales.
    s["bateria"] = secuencia([A5, A5, A5], 0.22, 0.16, tono, 0.8, 14.0)
    # Papelera: un barrido corto que cae (como arrugar un papel, sin ruido: seno que baja de frecuencia).
    n = int(TAXA * 0.35)
    fase, m = 0.0, []
    for i in range(n):
        t = i / TAXA
        f = 1400 * math.exp(-5.0 * t) + 200
        fase += 2 * math.pi * f / TAXA
        m.append(0.7 * min(1.0, i / (TAXA * 0.004)) * math.exp(-6.0 * t) * math.sin(fase))
    s["papelera"] = m
    return s


def escribir_wav(ruta, muestras):
    pico = max(abs(v) for v in muestras) or 1.0
    esc = 0.8 * 32767 / pico   # normalizados al 80 %: todos suenan parecido
    datos = b"".join(struct.pack("<h", int(round(v * esc))) for v in muestras)
    cab = b"RIFF" + struct.pack("<I", 36 + len(datos)) + b"WAVE"
    cab += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, TAXA, TAXA * 2, 2, 16)
    with open(ruta, "wb") as f:
        f.write(cab + b"data" + struct.pack("<I", len(datos)) + datos)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    dest = sys.argv[1]
    os.makedirs(dest, exist_ok=True)
    conf = ["# esquema.conf del esquema «SaguiOS» (D421): evento=archivo", "nombre=SaguiOS",
            "creditos=Sonidos generados por SaguiOS-Extended, GPL v3"]
    for ev, m in sonidos().items():
        escribir_wav(os.path.join(dest, ev + ".wav"), m)
        conf.append(f"{ev}={ev}.wav")
    with open(os.path.join(dest, "esquema.conf"), "w", encoding="utf-8") as f:
        f.write("\n".join(conf) + "\n")
    with open(os.path.join(dest, "CREDITOS.txt"), "w", encoding="utf-8") as f:
        f.write("Esquema «SaguiOS»: sonidos generados por SaguiOS-Extended (tools/gerar_sonidos.py), GPL v3.\n"
                "Son síntesis pura (senos con envolventes): no llevan muestras de terceros.\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
