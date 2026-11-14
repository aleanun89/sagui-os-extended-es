#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H241 - captura del ratón (D242).

Con /testes/captura-raton (una ventana que pide la captura con JAN_CAPTURA):
  (a) vmmouse: al capturar, el dispositivo pasa a modo relativo; movimientos relativos de
      QMP mucho mayores que la pantalla llegan sin recortar (suma > ancho de pantalla); el
      cursor queda fijo (al soltar está donde se fijó, en el centro de la ventana);
  (b) Ctrl+Alt suelta la captura, el programa recibe EV_CAPTURA_SOLTA, y el ratón vuelve a
      ser absoluto (un clic absoluto de QMP cae donde toca);
  (c) perder el foco (Alt+Tab) la suelta;
  (d) PS/2 relativo (vmport=off): lo mismo, deltas crudos sin recortar.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, SAIDA  # noqa: E402
from h36_vmmouse import Qmp, ABS_MAX  # noqa: E402
import h228_openjazz as h228  # noqa: E402
from h60_barra_tareas import esperar  # noqa: E402


def acumulado(serial, desde):
    g = re.findall(r"captura-raton: delta -?\d+ -?\d+ acum (-?\d+) (-?\d+)", serial[desde:])
    return (int(g[-1][0]), int(g[-1][1])) if g else (0, 0)


def rel(q, dx, dy):
    q.comando("input-send-event", events=[{"type": "rel", "data": {"axis": "x", "value": dx}},
                                          {"type": "rel", "data": {"axis": "y", "value": dy}}])


def abrir(m):
    h228.terminal(m)
    marca = len(m.serial())
    m.digitar("/testes/captura-raton\n", 0.04)
    esperar(m, r"captura-raton: listo", 30, desde=marca)
    time.sleep(1)


def parte(vm):
    qmp = os.path.join(SAIDA, "h241.qmp")
    if os.path.exists(qmp):
        os.remove(qmp)
    extras = ["-qmp", f"unix:{qmp},server,nowait"]
    from qemu import imagem_de_teste
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h241", "ide", imagem=img, vmmouse=vm, extras=extras) as m:
        abrir(m)
        q = Qmp(qmp)
        marca = len(m.serial())
        m.tecla("c")
        esperar(m, r"captura-raton: pedida r=0", 10, desde=marca)
        g = esperar(m, r"\[MOUSE\] captura ligada.*?cursor em \((\d+),(\d+)\)", 10, desde=marca)
        fijo = (int(g[0]), int(g[1]))
        if vm:
            verificar("[vmmouse relativo]" in m.serial()[marca:], "con vmmouse, el dispositivo pasa a modo relativo")
        for _ in range(12):   # 12 x 120 = 1440 px a la derecha, 12 x 70 = 840 hacia abajo: más que la pantalla
            rel(q, 120, 70)
            time.sleep(0.12)
        time.sleep(1)
        ax, ay = acumulado(m.serial(), marca)
        print("    acumulado:", ax, ay)
        verificar(ax >= 1300 and ay >= 700, f"deltas crudos sin recortar al borde de la pantalla ({ax},{ay})")
        verificar("mover normal" not in m.serial()[marca:], "durante la captura no llegan movimientos normales")
        # Ctrl+Alt la suelta
        marca2 = len(m.serial())
        m.monitor("sendkey ctrl-alt")
        g = esperar(m, r"\[MOUSE\] captura solta, cursor em \((\d+),(\d+)\)", 10, desde=marca2)
        verificar((int(g[0]), int(g[1])) == fijo, f"el cursor no se movió durante la captura (sigue en {fijo})")
        esperar(m, r"captura-raton: el sistema solto la captura", 10, desde=marca2)
        verificar("(Ctrl+Alt)" in m.serial()[marca2:], "la suelta Ctrl+Alt")
        time.sleep(0.5)
        if vm:   # vuelve a ser absoluto: un clic absoluto cae donde toca
            q.clicar_abs(0.25, 0.5)
            esperar(m, r"\[MOUSE\] botao esquerdo apertado em \((\d+),(\d+)\)", 10, desde=marca2)
            verificar("[vmmouse absoluto]" in m.serial()[marca2:], "el vmmouse vuelve a modo absoluto")
        # Otra captura y se pierde el foco
        if vm:
            m.tecla("esc")   # por si algún menú
        time.sleep(0.5)
        # el clic de arriba pudo sacar el foco: se vuelve a la ventana con Alt+Tab
        marca3 = len(m.serial())
        m.tecla("c")
        ok = re.search(r"captura-raton: pedida r=0", m.serial()[marca3:]) is not None
        if not ok:
            m.monitor("sendkey alt-tab")
            time.sleep(1.5)
            marca3 = len(m.serial())
            m.tecla("c")
        esperar(m, r"captura-raton: pedida r=0", 10, desde=marca3)
        m.monitor("sendkey alt-tab")
        esperar(m, r"\[GUI\] captura do mouse solta \((perdio|menu)", 10, desde=marca3)
        verificar(True, "al perder el foco (Alt+Tab) se suelta la captura")


def main():
    print("-- vmmouse")
    parte(True)
    print("-- PS/2 relativo")
    parte(False)
    return 0


if __name__ == "__main__":
    sys.exit(main())
