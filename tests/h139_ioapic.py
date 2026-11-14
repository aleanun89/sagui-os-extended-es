#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H139 - las IRQ de los dispositivos por el IOAPIC (D166).

Criterio:
  1. sin la opción de arranque --ioapic todo sigue por el PIC 8259: el log
     dice que el IOAPIC está desactivado y el sistema llega a la shell;
  2. con --ioapic (una CPU, i440fx): el log dice el IOAPIC detectado (versión,
     entradas), la override de la IRQ 0 al GSI 2 y "activo"; sin vuelta
     atrás al PIC ni excepciones; y funcionan, por el IOAPIC:
       - el tic del PIT (sleep 3 tarda unos 3 s de verdad, uptime avanza),
       - el teclado (IRQ 1: la shell responde),
       - el ratón PS/2 (IRQ 12: los clics llegan con su posición),
       - la red e1000 (IRQ PCI por nivel: ping y contador de interrupciones),
       - el sonido AC'97 (reproducir y las interrupciones atendidas en infopc),
       - el USB EHCI (el pendrive se lee) y el disco IDE de arranque (discos);
  3. con --ioapic --smp y cuatro CPU: los secundarios arrancan, el IOAPIC
     lleva las IRQ a la CPU 0 y la red y el teclado funcionan;
  4. con --ioapic en una máquina q35 (ICH9): el IOAPIC se activa y la red
     funciona.

Uso: h139_ioapic.py [sem] [ioapic] [smp] [q35]   (sin argumentos, todo)
"""
import math
import os
import re
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, criar_pendrive, pendrive_qemu  # noqa: E402


def imagem(*opcoes):
    return imagem_de_teste("", ["--vesa", "--console", *opcoes])


def wav_tono(segundos=1.0, freq=440, taxa=48000):
    n = int(taxa * segundos)
    dados = b"".join(struct.pack("<h", int(12000 * math.sin(2 * math.pi * freq * i / taxa))) for i in range(n))
    cab = b"RIFF" + struct.pack("<I", 36 + len(dados)) + b"WAVE"
    cab += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, taxa, taxa * 2, 2, 16)
    return cab + b"data" + struct.pack("<I", len(dados)) + dados


def comum_ioapic(m, rotulo):
    """Lo que vale para cualquier arranque con --ioapic."""
    s = m.serial().replace("\r", "")
    verificar(re.search(r"\[IOAPIC\] IOAPIC 0: id \d+, fisico fec00000, version \w+, \d+ entradas", s),
              f"[{rotulo}] el log dice el IOAPIC detectado (direccion, version, entradas)")
    verificar("[IOAPIC] IRQ 0 -> GSI 2" in s, f"[{rotulo}] la IRQ 0 del PIT va por la override al GSI 2")
    verificar("[IOAPIC] activo" in s, f"[{rotulo}] el IOAPIC queda activo")
    verificar("vuelta al PIC" not in s, f"[{rotulo}] sin vuelta atras al PIC")
    verificar("EXCECAO" not in s, f"[{rotulo}] sin excepciones")


def tic(m, rotulo):
    """El tic del PIT llega por el IOAPIC: sleep 3 tarda unos 3 s."""
    t0 = time.time()
    m.comando("sleep 3", 30)
    dt = time.time() - t0
    verificar(2.5 <= dt <= 12, f"[{rotulo}] sleep 3 tarda {dt:.1f} s: el tic del PIT llega")


def rede(m, rotulo):
    s = m.serial()
    verificar("com interrupcao" in s and "10.0.2.15" in s, f"[{rotulo}] la red e1000 con interrupcion y DHCP")
    out = m.comando("ping -c 3 10.0.2.2", tempo=30)
    verificar("3 paquetes enviados, 3 recibidos, 0% perdidos" in out, f"[{rotulo}] ping -c 3 10.0.2.2: 3 respuestas")
    out = m.comando("ifconfig")
    n = re.search(r"interrupciones: (\d+)", out)
    verificar(n and int(n.group(1)) > 0, f"[{rotulo}] la e1000 atendio interrupciones ({n.group(1) if n else '?'})")


def sem_opcao():
    with Maquina("h139-sem", "ide", imagem=imagem()) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[IOAPIC] desactivado" in s, "sin --ioapic el log dice que el IOAPIC esta desactivado")
        verificar("[IOAPIC] activo" not in s, "sin --ioapic no se activa")
        verificar("EXCECAO" not in s, "sin excepciones sin --ioapic")
        out = m.comando("echo teclado ok")
        verificar("teclado ok" in out, "sin --ioapic el teclado funciona")


def com_ioapic():
    pen = criar_pendrive("h139-pen", arquivos={"tono.wav": wav_tono()})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h139-ioapic", "ide", imagem=imagem("--ioapic"), extras=extras, som="ac97") as m:
        m.esperar_serial("sagui:/$", 120)
        comum_ioapic(m, "ioapic")
        out = m.comando("echo teclado ok")
        verificar("teclado ok" in out, "[ioapic] el teclado (IRQ 1) funciona")
        tic(m, "ioapic")

        # Ratón PS/2 (IRQ 12): un clic en el centro y otro tras moverlo.
        verificar("[MOUSE] mouse PS/2 ligado" in m.serial(), "[ioapic] el raton PS/2 se enciende")
        m.monitor("mouse_button 1"); time.sleep(0.3); m.monitor("mouse_button 0"); time.sleep(0.3)
        for _ in range(4):
            m.monitor("mouse_move 40 30")
            time.sleep(0.1)
        time.sleep(0.5)
        m.monitor("mouse_button 1"); time.sleep(0.3); m.monitor("mouse_button 0"); time.sleep(0.5)
        cl = [(int(x), int(y)) for x, y in re.findall(r"\[MOUSE\] botao esquerdo apertado em \((\d+),(\d+)\)", m.serial())]
        verificar(len(cl) >= 2 and cl[-1][0] > cl[0][0] and cl[-1][1] > cl[0][1],
                  f"[ioapic] el raton (IRQ 12) mueve el cursor y hace clic: {cl}")

        rede(m, "ioapic")

        # USB EHCI: el pendrive se lee.
        out = m.comando("usb")
        verificar("puerto 0: dispositivo conectado" in out, "[ioapic] el EHCI ve el pendrive")
        m.comando("montar usb0p1")
        out = m.comando("ls /usb")
        verificar("tono.wav" in out.lower(), "[ioapic] el pendrive USB se lee")

        # Disco IDE de arranque.
        out = m.comando("discos")
        verificar("Discos internos" in out and "QEMU" in out, "[ioapic] discos lista el disco IDE de arranque")

        # Sonido AC'97: reproducir y las interrupciones atendidas.
        m.comando("cp /usb/tono.wav /tmp/tono.wav", 60)
        out = m.comando("reproducir /tmp/tono.wav", 60)
        verificar("48000 Hz" in out, "[ioapic] reproducir toca el WAV por la AC'97")
        out = m.comando("infopc", 60)
        irq = re.search(r"Interrupci.n:\s+IRQ (\d+) \((\d+) atendidas\)", out)
        verificar(irq and int(irq.group(2)) > 0,
                  f"[ioapic] la AC'97 atendio interrupciones ({irq.group(0) if irq else 'sin linea de IRQ'})")
        verificar("EXCECAO" not in m.serial(), "[ioapic] sin excepciones al final")


def com_smp():
    with Maquina("h139-smp", "ide", imagem=imagem("--ioapic", "--smp"), nucleos=4) as m:
        m.esperar_serial("sagui:/$", 120)
        comum_ioapic(m, "smp")
        s = m.serial()
        verificar("[SAGUI] nucleos=4" in s, "[smp] con --ioapic --smp arrancan las cuatro CPU")
        out = m.comando("echo teclado ok")
        verificar("teclado ok" in out, "[smp] el teclado funciona")
        tic(m, "smp")
        rede(m, "smp")
        # D168: con --smp las IRQ de los dispositivos se reparten entre los
        # secundarios; el teclado y la red de arriba ya pasaron por ellos.
        s = m.serial()
        verificar("[IOAPIC] reparto:" in s, "[smp] las IRQ se reparten entre las CPU secundarias")
        atendidas = re.findall(r"\[IOAPIC\] CPU ([1-9]) atiende la IRQ (\d+)", s)
        verificar(len(atendidas) > 0,
                  f"[smp] una CPU secundaria atendio una IRQ de dispositivo: {atendidas}")
        verificar("excecao" not in s.lower() and "EXCEPCION" not in s,
                  "[smp] sin excepciones con las IRQ repartidas")


def com_q35():
    with Maquina("h139-q35", "ide", imagem=imagem("--ioapic"), extras=["-machine", "q35", "-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 120)
        comum_ioapic(m, "q35")
        out = m.comando("echo teclado ok")
        verificar("teclado ok" in out, "[q35] el teclado funciona")
        tic(m, "q35")
        rede(m, "q35")


def main():
    partes = sys.argv[1:] or ["sem", "ioapic", "smp", "q35"]
    if "sem" in partes:
        sem_opcao()
    if "ioapic" in partes:
        com_ioapic()
    if "smp" in partes:
        com_smp()
    if "q35" in partes:
        com_q35()
    return 0


if __name__ == "__main__":
    sys.exit(main())
