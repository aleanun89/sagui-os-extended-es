#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H395 - Configuración > Pantalla > «Aceleración gráfica»: bloque fijo de información de la tarjeta (solo lectura).

QEMU con la VGA estándar y sin driver de aceleración: la página enseña el modelo PCI de la tarjeta, la memoria de
vídeo (la del VBE), el 2D por la CPU («sin aceleración»), el 3D por software y el volcado por la GPU «no
disponible». Se comprueba por el registro («configuracion: tarjeta [...]»), que sale al abrir la página, y con
una captura. Mirar no enciende nada: el 3D sigue sin driver y el 2D en la CPU tras abrir la página.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
import h323_conf_aceleracion as h  # noqa: E402


def main():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--dados-mb", "16"])
    with Maquina("h395", "ide", imagem=img, snapshot=False) as m:
        h.arrancar(m)
        v, n0 = h.abrir_ventana(m)
        m.captura("H395-bloque-tarjeta")
        g = re.search(r"configuracion: tarjeta \[([^\]]*)\] vram_kb=(\d+) 2d=(\S+) 3d=(\S*) volcado=(\d+)", m.serial()[n0:])
        verificar(g, "la página registra el bloque de la tarjeta")
        modelo, vram, d2, d3, volc = g.groups()
        verificar("1234:1111" in modelo, f"modelo PCI de la VGA estándar de QEMU ({modelo!r})")
        verificar(int(vram) >= 1024, f"memoria de vídeo del VBE ({vram} KB)")
        verificar(d2 == "cpu", f"2D por la CPU, sin aceleración ({d2})")
        verificar(volc == "0", f"volcado por la GPU no disponible ({volc})")
        h.cerrar_ventana(m, v)
        out = m.comando("acel2d")
        verificar("Aceleracion 2D: CPU" in out, f"mirar no enciende el 2D ({out[:200]!r})")
        out = m.comando("gpu3d")
        verificar("3D por GPU: activo" not in out, f"mirar no enciende el 3D ({out[:200]!r})")
    print("H395 OK")


if __name__ == "__main__":
    main()
