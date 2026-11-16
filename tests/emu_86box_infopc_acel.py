#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""EMU 86Box - infopc con la Voodoo3 (issue #43). A PETICIÓN, nunca en la batería.

  * `infopc -t gpu`: la pestaña GPU da el nombre (tabla del acelerador del kernel) y la familia, no «?» ni «otra».
  * `infopc --bench-gpu` (como administrador): «Camino GPU: tdfx ...» y las tres primeras pruebas con GPU medida.
  * La ventana: pestañas GPU y Benchmark 2D, con capturas docs/capturas/H43-*.png.

    SAGUI_TURNOS=6 tools/turno.sh python3 tests/emu_86box_infopc_acel.py [maquina]
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu86box  # noqa: E402
from emu86box import Maquina86Box, verificar, FalhaTeste, SALIDA  # noqa: E402
from qemu import imagem_de_teste  # noqa: E402


def main():
    nombre = sys.argv[1] if len(sys.argv) > 1 else "p2-450-voodoo3500"
    verificar(emu86box.instalado(), "86Box instalado")
    img = imagem_de_teste("", ["--vesa"])
    os.makedirs(SALIDA, exist_ok=True)
    with Maquina86Box(nombre, img, nome=f"{nombre}-infopc-acel") as m:
        try:
            m.pasar_bios(tempo=60)
            m.esperar_serial("area de trabalho no ar", 240)
            m.esperar_serial("sagui:", 120)
            time.sleep(3)
            out = m.comando("infopc -t gpu", 120)
            g = re.search(r"^GPU:\s+121a:0005\s+(.*)$", out, re.M)
            verificar(g and "Voodoo3" in g.group(1) and "?" not in g.group(1), f"GPU con nombre ({g.group(0) if g else '?'})")
            f = re.search(r"^\s+Familia:\s+(.*)$", out, re.M)
            verificar(f and f.group(1).strip() not in ("otra", "desconocida"), f"familia con nombre ({f.group(1) if f else '?'})")
            out = m.comando("infopc --bench-gpu", 900)
            c = re.search(r"^Camino GPU: (.*)$", out, re.M)
            print("   ", c.group(0) if c else out[-500:])
            verificar(c and c.group(1).startswith("tdfx"), "Camino GPU: el acelerador tdfx")
            verificar("FALL" not in out, "no se declara fallida la autoprueba")
            for p in ("Rellenos de rect", "Copias de bloques", "Copias solapadas"):
                r = re.search(r"^%s.*$" % p, out, re.M)
                verificar(r and "sin acelerador" not in r.group(0) and re.search(r"\sOK\s", r.group(0)), f"{p}: medida con GPU ({r.group(0).strip() if r else '?'})")
            m.digitar("infopc\n")
            m.esperar_serial("infopc: janela aberta", 60)
            m.digitar("9", 0.1)
            m.esperar_serial("infopc: secao gpu", 30)
            time.sleep(2)
            m.captura("H43-infopc-gpu", guardar=True)
            m.digitar("m", 0.1)
            m.esperar_serial("infopc: gpu bench total", 900)
            time.sleep(20)   # la ventana de la medida se cierra y la pestaña se repinta
            m.captura("H43-infopc-benchmark", guardar=True)
            verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones")
        except Exception:
            m.captura("H43-fallo")
            print("serie:", m.serial().replace("\x00", "")[-1500:])
            raise
    print("EMU infopc acel: BIEN")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
