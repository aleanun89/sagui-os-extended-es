#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H200 (D201) - ecualizador de 5 bandas.

  unidad  en el PC (tests/ecualizador_unidad.c, con el ecualizador.c de la libsagui y el eq_fixo.h
          del kernel): la respuesta del filtro en punto fijo frente al de coma flotante, plano =
          identidad exacta, sin saturación con todas las bandas a +12 dB;
  os      en SaguiOS con Intel HDA grabando la salida: `ecualizador agudos` escribe [AUDIO]
          ecualizador en el log y la clave ecualizador= en /home/.config/sagui.conf; un WAV con
          250 Hz + 4 kHz suena con la banda de 4 kHz unos dB más fuerte que con `plano`; `plano`
          deja el sonido como estaba; los argumentos erróneos se rechazan; infopc muestra el estado.

Uso: h200_ecualizador.py [unidad] [os] [gui]
"""
import math
import os
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_pendrive, pendrive_qemu, RAIZ, SAIDA  # noqa: E402
import h70_sonido as h70  # noqa: E402
import h83_configuracion as h83  # noqa: E402

DIR = os.path.join(SAIDA, "h200")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "ecualizador_unidad")
    r = subprocess.run(["gcc", "-O2", "-DEQ_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe,
                        os.path.join(RAIZ, "tests", "ecualizador_unidad.c"),
                        os.path.join(RAIZ, "userland", "libsagui", "ecualizador.c"), "-lm"],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print(r.stdout)
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "respuesta en frecuencia, identidad y margen en punto fijo")


def wav_dos_tonos():
    n = 48000 * 2
    d = bytearray()
    for i in range(n):
        v = 0.2 * math.sin(2 * math.pi * 250 * i / 48000) + 0.2 * math.sin(2 * math.pi * 4000 * i / 48000)
        d += struct.pack("<hh", int(v * 32767), int(v * 32767))
    cab = b"RIFF" + struct.pack("<I", 36 + len(d)) + b"WAVE"
    cab += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 2, 48000, 48000 * 4, 4, 16)
    return cab + b"data" + struct.pack("<I", len(d)) + bytes(d)


def provar_os():
    pen = criar_pendrive("h200-pen", rotulo="ECUA", arquivos={"dos.wav": wav_dos_tonos()})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    img = h83.imagem_persistente("h200-persistente", False)
    with Maquina("h200", "ide", imagem=img, snapshot=False, extras=extras, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("ecualizador --ayuda")
        verificar("ecualizador [plano | graves | voz | agudos" in out, "--ayuda explica el uso")
        out = m.comando("ecualizador")
        verificar("Ecualizador: plano" in out, "arranca plano")
        m.comando("montar usb0p1")
        verificar("montado em /home, leitura e escrita" in m.serial(), "/home persistente (rw)")
        m.comando("mkdir /tmp/s")
        m.comando("cp /usb/dos.wav /tmp/s/dos.wav", 120)
        for malo in ("60=+13", "5k=3", "voz graves", "60"):
            out = m.comando(f"ecualizador {malo}")
            verificar("ecualizador:" in out and "Guardado" not in out, f"rechaza \"{malo}\"")
        m.comando("reproducir /tmp/s/dos.wav", 60)         # 1.a: plano
        time.sleep(0.5)
        out = m.comando("ecualizador agudos")
        verificar("Ecualizador: agudos" in out and "Guardado en /home/.config/sagui.conf" in out, "agudos se aplica y se guarda")
        verificar("[AUDIO] ecualizador activo: 0 0 0 4 6 dB" in m.serial(), "el kernel registra [AUDIO] ecualizador activo")
        conf = m.comando("cat /home/.config/sagui.conf")
        verificar("ecualizador=0,0,0,4,6" in conf, "sagui.conf guarda ecualizador=0,0,0,4,6")
        out = m.comando("infopc -t sistema")
        verificar("Ecualizador:" in out and "+4" in out and "+6" in out, "infopc muestra los valores")
        m.comando("reproducir /tmp/s/dos.wav", 60)         # 2.a: agudos
        time.sleep(0.5)
        out = m.comando("ecualizador 60=+3 1k=-2")
        verificar("personalizado" in out, "bandas sueltas dan un ajuste personalizado")
        conf = m.comando("cat /home/.config/sagui.conf")
        verificar("ecualizador=3,0,-2,4,6" in conf and conf.count("ecualizador=") == 1, "la clave se reemplaza, no se duplica")
        out = m.comando("ecualizador plano")
        verificar("Ecualizador: plano" in out, "plano")
        verificar("[AUDIO] ecualizador plano" in m.serial(), "el kernel registra [AUDIO] ecualizador plano")
        out = m.comando("infopc -t sistema")
        verificar("Ecualizador:     plano" in out, "infopc: plano")
        m.comando("reproducir /tmp/s/dos.wav", 60)         # 3.a: otra vez plano
        time.sleep(0.5)
        out = m.comando("ecualizador voz")
        verificar("Guardado" in out, "voz se guarda")
        h83.reiniciar(m)
    # Segundo arranque: "configuracion --aplicar" restablece el ecualizador guardado.
    with Maquina("h200b", "ide", imagem=img, snapshot=False, som="hda") as m2:
        m2.esperar_serial("sagui:/$", 120)
        s = m2.serial()
        verificar("configuracion: ecualizador -3,-1,3,3,-2 -> 0" in s, "el arranque aplica el ecualizador guardado")
        verificar("[AUDIO] ecualizador activo: -3 -1 3 3 -2 dB" in s, "el kernel lo registra al arrancar")
        out = m2.comando("ecualizador")
        verificar("Ecualizador: voz" in out, "tras reiniciar sigue en voz")
        m2.comando("ecualizador plano")
    esq, dir_ = h70.ler_gravacao(m.wav)
    tr = h70.trechos(esq, dir_)
    print("  trechos:", [(a / 48000, b / 48000) for a, b in tr])
    tr = [(a, b) for a, b in tr if b - a > 48000]
    verificar(len(tr) == 3, f"tres reproducciones largas en la grabación (hay {len(tr)})")

    def razon(a, b):
        x = esq[a + 12000:b - 12000]
        return 20 * math.log10(h70.goertzel(x, 4000) ** 0.5 / h70.goertzel(x, 250) ** 0.5)
    r = [razon(a, b) for a, b in tr]
    print(f"  4 kHz sobre 250 Hz: plano {r[0]:.2f} dB, agudos {r[1]:.2f} dB, plano de nuevo {r[2]:.2f} dB")
    verificar(abs(r[0]) < 0.3, f"plano: las dos bandas igual de fuertes ({r[0]:.2f} dB)")
    verificar(3.0 <= r[1] - r[0] <= 5.5, f"agudos sube 4 kHz sobre 250 Hz unos 4 dB ({r[1] - r[0]:.2f} dB)")
    verificar(abs(r[2] - r[0]) < 0.2, f"plano otra vez: igual que al principio ({r[2] - r[0]:.2f} dB)")


def provar_gui():
    """Configuración > Sonido: un clic en el deslizador de 1 kHz, los botones de preajuste y la captura."""
    img = h83.imagem_persistente("h200-grafica", True)
    with Maquina("h200g", "ide", imagem=img, snapshot=False, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("configuracion &")
        fim = time.time() + 20
        while time.time() < fim and '"Configuración"' not in m.serial():
            time.sleep(0.3)
        time.sleep(1.0)
        j = h83.janela(m.serial(), "Configuración")
        ox, oy = j["x"] + 2, j["y"] + 24
        pos = h83.pos_paginas()
        m.clique(ox + pos[3][0] + pos[3][1] // 2, oy + 24)
        time.sleep(0.6)
        m.clique(ox + 60 + 2 * 84, oy + 50 + 250 + 24)        # 1 kHz, a un cuarto del carril: +6 dB
        time.sleep(0.6)
        s = m.serial()
        verificar("[AUDIO] ecualizador activo: 0 0 6 0 0 dB" in s, "clic en el deslizador de 1 kHz: +6 dB en el kernel")
        m.captura("H200-1-sonido-1k")
        m.clique(150, 12)                                      # el botón del Terminal en la barra de tareas
        time.sleep(0.6)
        out = m.comando("cat /home/.config/sagui.conf")
        verificar("ecualizador=0,0,6,0,0" in out, "el deslizador guarda ecualizador=0,0,6,0,0")
        m.clique(300, 12)                                      # y vuelve a Configuración
        m.clique(ox + 500 + 75, oy + 50 + 250 + 46 + 13)       # Voz
        time.sleep(0.6)
        verificar("[AUDIO] ecualizador activo: -3 -1 3 3 -2 dB" in m.serial(), "el botón Voz aplica el preajuste")
        m.captura("H200-2-sonido-voz")
        m.clique(ox + 500 + 75, oy + 50 + 250 - 14 + 13)       # Plano
        time.sleep(0.6)
        verificar("[AUDIO] ecualizador plano" in m.serial(), "el botón Plano vuelve a la derivación total")
        m.clique(150, 12)
        time.sleep(0.6)
        out = m.comando("ecualizador")
        verificar("Ecualizador: plano" in out, "el comando ve el estado")


def main():
    quais = sys.argv[1:] or ["unidad", "os", "gui"]
    if "gui" in quais:
        provar_gui()
    if "unidad" in quais:
        unidad()
    if "os" in quais:
        provar_os()
    print("H200: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
