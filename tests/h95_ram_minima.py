#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H95 - RAM mínima: SaguiOS arranca con 32 MB (requisito del mantenedor, D122).

Criterio: con 32 MB de RAM la imagen normal llega al escritorio (el Terminal se abre) y
la de consola al shell; `mem` dice cuánta RAM queda libre, y debe quedar al menos
MIN_LIBRE_KB para que quepa un programa. Con 48 MB, lo mismo con más margen. Si la
imagen crece hasta comerse la RAM de un PC de 32 MB, esta prueba avisa antes de
publicar.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402

MIN_LIBRE_KB = 4 * 1024   # lo justo para abrir un programa mediano tras arrancar


def libre_kb(out):
    m = re.search(r"Libre:\s+.*\((\d+) KB\)", out)
    return int(m.group(1)) if m else -1


def main():
    for mem in ("32M", "48M"):
        with Maquina("h95c-" + mem, "ide", imagem=imagem_console(), memoria=mem) as m:
            m.esperar_serial("sagui:/$", 120)
            out = m.comando("mem", 30)
            kb = libre_kb(out)
            verificar(kb >= MIN_LIBRE_KB, f"[{mem}, consola] arranca y quedan {kb // 1024} MB libres (mínimo {MIN_LIBRE_KB // 1024} MB)")
            out = m.comando("ls bin | conta", 30)
            verificar("codigo 0" in out or re.search(r"\d+", out), f"[{mem}, consola] los programas funcionan")

        with Maquina("h95g-" + mem, "ide", memoria=mem) as m:
            m.esperar_serial("sagui:/$", 120)
            verificar("\"Terminal\"" in m.serial(), f"[{mem}, escritorio] el escritorio abre el Terminal")
            m.captura(f"H95-escritorio-{mem}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
