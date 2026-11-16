#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H329 (D324, 8.4.3r2) - aviso del escritorio cuando falta /apps en una imagen «base».

La imagen base lleva /etc/apps-fuera en el ramdisk (las aplicaciones van en la partición SAGUI-APPS).
Criterios:
  A. Imagen base SIN la partición /apps (el PC no ve el disco): el escritorio avisa una vez y el
     registro lleva «falta /apps».
  B. Imagen base CON /apps: no hay aviso.
"""
import os
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

AVISO = "No se encontró la partición de aplicaciones"


def arrancar(nombre, imagem):
    with Maquina(nombre, "ide", imagem=imagem, memoria="64M") as m:
        m.esperar_serial('"Terminal"', 180)
        m.esperar_serial("sagui:/$", 60)
        time.sleep(3)
        return m.serial()


def main():
    s = arrancar("h329-a", imagem_de_teste("", ["--vesa"], apps=False))
    verificar("notificacao: " + AVISO in s, "A: sin /apps, el escritorio muestra el aviso")
    verificar("falta /apps" in s, "A: el registro lleva «falta /apps»")
    verificar(s.count("notificacao: " + AVISO) == 1, "A: el aviso sale una sola vez")
    s = arrancar("h329-b", imagem_de_teste("", ["--vesa"]))
    verificar(AVISO not in s and "falta /apps" not in s, "B: con /apps no hay aviso")


if __name__ == "__main__":
    main()
