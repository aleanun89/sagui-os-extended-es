#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""EMU 86Box - driver NE2000 (D329) en p2-ne2000 (PCI, RTL8029) y p2-ne2000-isa (ISA). A PETICIÓN, nunca en la batería.

Arranca la imagen consola, espera el prompt y comprueba por el serie: que el kernel detecta la NE2000,
que DHCP da IP (slirp de 86Box: 10.0.2.15) y que el ping a la puerta de enlace contesta.

    tools/turno.sh python3 tests/emu_86box_ne2000.py [--irq N] [maquina ...]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu86box  # noqa: E402
from emu86box import Maquina86Box, verificar  # noqa: E402
from qemu import imagem_de_teste  # noqa: E402


def main():
    args = sys.argv[1:]
    extras = []
    if "--irq" in args:   # fuerza la IRQ de la ISA por la cabecera (--ne2k-irq N)
        i = args.index("--irq")
        extras = ["--ne2k-irq", args[i + 1]]
        del args[i:i + 2]
    maquinas = args or ["p2-ne2000", "p2-ne2000-isa"]
    verificar(emu86box.instalado(), "86Box instalado (bash tools/86box_instalar.sh)")
    imagen = imagem_de_teste("", ["--vesa", "--console", *extras])
    for nombre in maquinas:
        print(f"== {nombre}", flush=True)
        if nombre.endswith("-isa"):   # el sondeo ISA va apagado por defecto
            imagen = imagem_de_teste("", ["--vesa", "--console", "--ne2k-isa", *extras])
        with Maquina86Box(nombre, imagen, nome=f"{nombre}-red") as m:
            m.pasar_bios(tempo=60)
            m.esperar_serial("[SAGUI] boot ok", 150)
            m.esperar_serial("sagui:", 90)
            s = m.serial().replace("\x00", "")
            print("  " + "\n  ".join(l for l in s.splitlines() if "NE2000" in l or "[REDE]" in l or "Red:" in l))
            verificar("[NE2000]" in s, f"{nombre}: el kernel detecta la NE2000")
            verificar("[ ok ] Red: ne2000" in s and "(DHCP)" in s, f"{nombre}: DHCP da una IP")
            out = m.comando("ifconfig", 30)
            print("  " + out.strip().replace("\n", "\n  "))
            out = m.comando("ping -c 3 10.0.2.2", 40)
            verificar("3 recibidos" in out, f"{nombre}: ping -c 3 10.0.2.2")
            m.captura(f"{nombre}-red", guardar=False)
    return 0


if __name__ == "__main__":
    sys.exit(main())
