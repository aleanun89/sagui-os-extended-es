#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H328 (issue #43) - infopc sin ser administrador: el benchmark 2D no declara fallida la autoprueba.

La autoprueba del acelerador (INFO_ACEL2D_PROVA) solo la deja el kernel al administrador. Antes infopc no miraba el
resultado de la llamada: sin permisos la estructura quedaba a cero y salía «el acelerador FALLÓ la autoprueba ()».
Ahora, con la sesión como sagui y sin escritorio (no hay diálogo de permisos):
  * `infopc --bench-gpu` dice que no se midió con el acelerador y por qué (nunca un motivo vacío) y no dice «FALLÓ»;
  * las pruebas siguen midiéndose por software, con la suma correcta.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402


def main():
    img = imagem_de_teste("", ["--sesion-sagui"])
    with Maquina("h332", "ide", imagem=img) as m:
        m.esperar_serial("sagui:", 120)
        time.sleep(1)
        verificar("[PERMISOS] sesion como sagui" in m.serial(), "la sesión es sagui")
        out = m.comando("infopc --bench-gpu", 600).replace("\r", "")
        g = re.search(r"^Camino GPU: (.*)$", out, re.M)
        verificar(g, "sale la línea «Camino GPU»")
        print("  ", g.group(0))
        verificar("FALL" not in g.group(0), "no declara fallida la autoprueba")
        verificar(not re.search(r"\(\)", g.group(0)), "nunca un motivo vacío")
        verificar(re.search(r"administrador|no hay acelerador", g.group(0)), "dice por qué (permisos o sin acelerador)")
        filas = re.findall(r"^(?:Rellenos de rect|Copias de bloques|Copias solapadas|Líneas|Texto|Ventana).*\sOK\s+\d+$", out, re.M)
        verificar(len(filas) == 5, f"las cinco pruebas sin ventana con suma correcta ({len(filas)}; la de ventana pide escritorio)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
