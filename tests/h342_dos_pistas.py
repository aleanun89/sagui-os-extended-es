#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H342 (D339, 8.4.4) - nunca suenan dos pistas a la vez en los reproductores (Música y Sintonía).

Fallo del mantenedor: «si el reproductor está reproduciendo un CD de audio y se pincha en otra pista, no puede
reproducir 2 pistas a la vez». Causa: Archivos (y el aviso del CD) lanzan un proceso NUEVO del reproductor por cada
doble clic; cada uno abría su propio flujo de audio y el kernel mezclaba los flujos. Ahora el proceso nuevo entrega
la ruta al reproductor que ya suena (instancia única, userland/programas/instancia_unica.h) y se va, y el viejo para
la pista que sonaba antes de tocar la nueva.

QEMU no emula un CD de audio (el lector virtual no da pistas CD-DA), así que el CD se sustituye por lo que sí puede
reproducirse: dos pistas WAV de un pendrive con tonos distintos (440 y 880 Hz) que se lanzan del mismo modo que
las lanza Archivos (la orden «musica ruta», que es lo que hace executar()). El camino del CD (cdda.c) solo cambia en
que las pistas son archivos /cdda/..., lo que no influye en la lógica de instancias. LÍMITE: la lectura real de las
pistas del CD no se prueba aquí (tests de cdda existentes).

La salida de sonido se graba (-audiodev wav, 48 kHz) y se analiza con Goertzel:
  A. Música: suena a.wav (440 Hz); se lanza «musica b.wav» (880 Hz): el proceso nuevo se entrega y se va (una sola
     ventana, «entregado a la instancia viva») y desde ese momento solo suena 880 Hz, sin 440 Hz;
  B. Música: con la tecla P (pista anterior, dentro del mismo proceso) vuelve a a.wav y solo suena 440 Hz;
  C. Sintonía: lo mismo que A.
Uso: h342_dos_pistas.py [musica] [sintonia]     (con KVM: sg kvm -c "SAGUI_KVM=1 python3 ...")
"""
import array
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, opcoes_apps, criar_pendrive, pendrive_qemu, SAIDA, FalhaTeste  # noqa: E402
import h70_sonido as h70  # noqa: E402

TAXA = 48000
BORDA, TITULO = 2, 24


def esperar(m, padrao, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.15)
    raise FalhaTeste(f"tiempo agotado esperando {padrao!r}")


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{titulo}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO)


def pos_wav(m):
    """Muestra (por canal) a la que ha llegado la grabación ahora mismo."""
    return max(0, (os.path.getsize(m.wav) - 44) // 4)


def energia(x, f):
    # potencia relativa de la frecuencia f en x (Goertzel normalizado)
    return h70.goertzel(x, f) / (len(x) ** 2) if len(x) else 0.0


def bandas(esq, desde, hasta):
    """Energía de 440 y 880 Hz en [desde, hasta) (muestras), por ventanas de 0,1 s."""
    e440 = e880 = 0.0
    n = 0
    for i in range(desde, hasta - 4800, 4800):
        w = esq[i:i + 4800]
        e440 += energia(w, 440)
        e880 += energia(w, 880)
        n += 1
    return (e440 / n, e880 / n) if n else (0.0, 0.0)


def prueba(nombre_prog, titulo_ventana, con_p):
    arqs = {"a.wav": h70.gerar_wav(44100, 16, 2, 40.0, [440, 440], 0.4),
            "b.wav": h70.gerar_wav(44100, 16, 2, 40.0, [880, 880], 0.4)}
    pen = criar_pendrive(f"h342-{nombre_prog}", rotulo="PISTAS", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    img = imagem_de_teste("", ["--vesa"] + opcoes_apps())
    with Maquina(f"h342-{nombre_prog}", "ide", imagem=img, extras=extras, som="hda", memoria="256M") as m:
        m.esperar_serial('"Terminal"', 180)
        m.esperar_serial("sagui:/$", 60)
        m.comando("montar usb0p1")
        ini = len(m.serial())
        m.digitar(f"{nombre_prog} /usb/a.wav", 0.04)
        m.tecla("ret")
        esperar(m, rf"{nombre_prog}: reproduciendo a.wav", 60, ini)
        time.sleep(3.5)
        p1 = pos_wav(m)
        jm = janela(m.serial()[ini:], titulo_ventana)
        # el terminal pierde el foco con la ventana nueva: se le devuelve con un clic en su parte visible
        m.clique(900, 300)
        time.sleep(0.5)
        ini2 = len(m.serial())
        m.digitar(f"{nombre_prog} /usb/b.wav", 0.04)
        m.tecla("ret")
        esperar(m, rf"{nombre_prog}: entregado a la instancia viva", 30, ini2)
        esperar(m, rf"{nombre_prog}: reproduciendo b.wav", 30, ini2)
        ph = pos_wav(m)   # instante del cambio (el sonido ya encolado, unos 0,4 s, aún sale)
        verificar(True, f"[{nombre_prog}] la segunda orden se entrega a la instancia viva y toca b.wav")
        time.sleep(4.0)
        p2 = pos_wav(m)
        s = m.serial()
        verificar(len(re.findall(rf"{nombre_prog}: pronto", s)) == 1 and len(re.findall(rf'criada por \d+: "{titulo_ventana}"', s)) == 1,
                  f"[{nombre_prog}] hay una sola ventana del reproductor")
        m.captura(f"h342-{nombre_prog}")
        p3 = None
        if con_p:
            m.clique(350, 12)   # el botón de la barra de tareas trae la ventana del reproductor al frente y le da el foco
            time.sleep(0.5)
            m.tecla("p")
            esperar(m, r"musica: reproduciendo a.wav", 30, ini2)
            pp = pos_wav(m)
            time.sleep(4.0)
            p3 = pos_wav(m)
        time.sleep(0.5)
        wav = m.wav
    esq, _ = h70.ler_gravacao(wav)
    a440, a880 = bandas(esq, p1 - 2 * TAXA, p1)
    verificar(a440 > 1e5 * max(a880, 1e-9) and a440 > 1000, f"[{nombre_prog}] antes del cambio solo suena a.wav (440 Hz: {a440:.0f}, 880 Hz: {a880:.2f})")
    b440, b880 = bandas(esq, ph + int(1.0 * TAXA), p2)
    verificar(b880 > 1000 and b440 < 0.01 * b880, f"[{nombre_prog}] después de pinchar b.wav solo suena 880 Hz (440 Hz: {b440:.2f}, 880 Hz: {b880:.0f})")
    if con_p:
        c440, c880 = bandas(esq, pp + int(1.0 * TAXA), p3)
        verificar(c440 > 1000 and c880 < 0.01 * c440, f"[{nombre_prog}] con la tecla P vuelve a a.wav y solo suena 440 Hz (440: {c440:.0f}, 880: {c880:.2f})")


def main():
    quais = sys.argv[1:] or ["musica", "sintonia"]
    if "musica" in quais:
        prueba("musica", "M.sica", True)
    if "sintonia" in quais:
        prueba("sintonia", "Sinton.a", False)
    return 0


if __name__ == "__main__":
    sys.exit(main())
