#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""EMU 86Box - la Sound Blaster 16 en una máquina de la época (D302). A PETICIÓN, nunca en la batería.

Arranca la imagen consola en p2-voodoo3 (o las máquinas que se pasen; todas llevan SB16 en 220h, IRQ 5,
DMA 1 y 5) y comprueba por el serie:
  - la detección ([SB16] 220h: DSP 4.xx, IRQ 5, DMA 1 y 5) y el OPL3 que 86Box sí emula;
  - que la reproducción mueve el DMA y llega la IRQ de medio búfer de 16 bits (sonido --tono);
  - que la grabación trae cuadros (86Box emula el ADC, a diferencia de QEMU) y que la salida
    vuelve después (semidúplex).
86Box no graba su salida de audio a un fichero: la forma del sonido solo se comprueba en QEMU (h302).

    tools/turno.sh python3 tests/emu_86box_sb16.py [maquina ...]
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu86box  # noqa: E402
from emu86box import Maquina86Box, verificar  # noqa: E402
from qemu import imagem_de_teste  # noqa: E402


def irqs16(m):
    out = m.comando("sonido", 30)
    r = re.search(r"de 16 bits (\d+)", out)
    return int(r.group(1)) if r else -1


def main():
    maquinas = sys.argv[1:] or ["p2-voodoo3"]
    verificar(emu86box.instalado(), "86Box instalado (bash tools/86box_instalar.sh)")
    imagen = imagem_de_teste("", ["--vesa", "--console"])
    for nombre in maquinas:
        print(f"== {nombre}", flush=True)
        with Maquina86Box(nombre, imagen, nome=f"{nombre}-sb16") as m:
            m.pasar_bios(tempo=60)
            m.esperar_serial("[SAGUI] boot ok", 150)
            m.esperar_serial("sagui:", 90)
            s = m.serial()
            verificar(re.search(r"\[SB16\] 220h: DSP 4\.\d\d, IRQ 5, DMA 1 y 5", s) is not None,
                      f"{nombre}: detecta la SB16 en 220h, IRQ 5, DMA 1 y 5")
            print("  " + (re.search(r"\[SB16\][^\n]*", s).group(0)), flush=True)
            antes = irqs16(m)
            m.comando("sonido --tono", 90)
            depois = irqs16(m)
            verificar(depois > antes, f"{nombre}: reproducir mueve el DMA y llegan IRQ de 16 bits ({antes} -> {depois})")
            m.comando("grabar -t 1500 /tmp/g.wav", 60)
            cap = re.findall(r"\[CAPTURA\] fechada; (\d+) cuadros", m.serial())
            verificar(cap and int(cap[-1]) > 0, f"{nombre}: la grabación trae cuadros ({cap[-1] if cap else 0})")
            antes = irqs16(m)
            m.comando("sonido --tono", 90)
            depois = irqs16(m)
            verificar(depois > antes, f"{nombre}: después de grabar la salida vuelve ({antes} -> {depois})")
            out = m.comando("infopc -t sistema", 60)
            c = re.search(r"(\d+) cortes", out)
            print(f"  cortes del mezclador: {c.group(1) if c else '?'}", flush=True)
    print("EMU SB16 OK")


if __name__ == "__main__":
    main()
