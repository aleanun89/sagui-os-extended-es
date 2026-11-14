#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H245 - revisión del driver R7xx (D246): «acel2d probar --detalle».

Con la VMware (la Radeon no se emula) comprueba que la orden de diagnóstico vuelca al log [ACEL2D]
las copias sueltas con su tiempo (GPU y CPU) y que el sistema sigue respondiendo. La lógica de
paquetes (bloques de filas en ancho completo) la cubre h223 (unidad en el PC).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402
from h161_acel2d import arrancar, VMWARE  # noqa: E402


def main():
    img = imagem_de_teste("", ["--vesa", "--acel2d=vmware"])
    with Maquina("h245", "ide", imagem=img, extras=VMWARE) as m:
        arrancar(m)
        out = m.comando("acel2d probar --detalle", 180).replace("\r", "")
        verificar("detalle volcado al log" in out, "«acel2d probar --detalle» avisa de que el detalle va al log")
        s = m.serial()
        verificar("[ACEL2D] detalle: vmware" in s, "el log trae la cabecera del detalle")
        verificar(s.count("[ACEL2D] detalle: copia") >= 6, "el log trae el tiempo de varias copias sueltas")
        verificar("ancho completo" in s, "hay copias de ancho completo")
        out = m.comando("echo vivo245", 20)
        verificar("vivo245" in out, "el sistema sigue respondiendo")
    print("H245 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
