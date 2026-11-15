#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""EMU 86Box - arranque de SaguiOS en máquinas antiguas emuladas (D301). A PETICIÓN, nunca en la batería.

Arranca la imagen (BIOS, consola) en p2-voodoo3 y p1-s3 (o las máquinas que se pasen),
espera el prompt por el serie, saca una captura y registra lo que detectó el kernel
(CPU, RAM, PCI, sonido, red, IDE) en build/86box/informe-<máquina>.txt.

    tools/turno.sh python3 tests/emu_86box_arranque.py [maquina ...]

No lo recogen rodar_todos.py ni rodar_paralelo.py: solo buscan h<dos dígitos>*.py y b<dígito>_*.py.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu86box  # noqa: E402
from emu86box import Maquina86Box, verificar, SALIDA  # noqa: E402
from qemu import imagem_de_teste  # noqa: E402

DETECCION = [("cpu", r"\[CPU\][^\n]*|\[RAPIDO\][^\n]*"),
             ("ram", r"memoria_mb=[^\n]*|\[SAGUI\] memoria fisica[^\n]*"),
             ("video", r"\[VIDEO\] \d+x\d+[^\n]*"),
             ("ide", r"\[ATA\][^\n]*|Discos internos[^\n]*"),
             ("usb", r"\[(?:USB|UHCI|OHCI|EHCI)\][^\n]*"),
             ("sonido", r"\[(?:SB|SB16|AC97|HDA|SONIDO|AUDIO|ES1371)[^\]]*\][^\n]*"),
             ("red", r"\[(?:NE2K|NE2000|RTL|RED|NET|ETH)[^\]]*\][^\n]*"),
             ("pci", r"\[PCI\][^\n]*")]


def main():
    maquinas = sys.argv[1:] or ["p2-voodoo3", "p1-s3"]
    verificar(emu86box.instalado(), "86Box instalado (bash tools/86box_instalar.sh)")
    imagen = imagem_de_teste("", ["--vesa", "--console"])
    os.makedirs(SALIDA, exist_ok=True)
    fallos = []
    for nombre in maquinas:
        print(f"== {nombre}", flush=True)
        lineas = [f"# Detección del kernel en {nombre}"]
        try:
            with Maquina86Box(nombre, imagen, nome=f"{nombre}-arranque") as m:
                try:
                    m.pasar_bios(tempo=60)
                    m.esperar_serial("[SAGUI] boot ok", 120)
                    verificar(True, f"{nombre}: el kernel llegó a «boot ok»")
                    m.esperar_serial("sagui:", 90)
                    verificar(True, f"{nombre}: el terminal da el prompt")
                    m.captura(f"{nombre}-consola", guardar=False)
                    cuerpo = m.serial().replace("\x00", "")
                    for clave, rx in DETECCION:
                        for h in re.findall(rx, cuerpo):
                            lineas.append(f"{clave}: {h.strip()}")
                    # lspci e ifconfig por el terminal (el resultado sale también por el serie)
                    for orden in ("lspci", "ifconfig"):
                        lineas.append(f"$ {orden}")
                        lineas.append(m.comando(orden, 30).strip())
                    m.captura(f"{nombre}-lspci", guardar=False)
                except Exception as e:  # noqa: BLE001 - se informa y se sigue con la máquina siguiente
                    m.captura(f"{nombre}-fallo", guardar=False)
                    lineas.append(f"FALLO: {e}")
                    lineas.append("serie: " + m.serial().replace("\x00", "")[-600:])
                    fallos.append(nombre)
                    print(f"  FALLO en {nombre}: {e}", flush=True)
        finally:
            ruta = os.path.join(SALIDA, f"informe-{nombre}.txt")
            with open(ruta, "w", encoding="utf-8") as f:
                f.write("\n".join(lineas) + "\n")
            print(f"  informe: {ruta}", flush=True)
    if fallos:
        print("máquinas donde no llegó al prompt:", ", ".join(fallos))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
