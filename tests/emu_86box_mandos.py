#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""EMU 86Box - el joystick del puerto de juegos (D307). A PETICIÓN, nunca en la batería.

86Box emula el puerto de juegos (0x201) y los joysticks analógicos («2-axis, 2-button», «4-axis,
4-button»...), con la entrada desde un mando del anfitrión. Aquí, sin mando en el anfitrión, 86Box da
los ejes en reposo: se comprueba la detección (cuántos ejes), que la lectura a petición da tiempos
razonables y estables, la calibración a mano y la automática, y que sin joystick configurado no hay
mando y nada se cuelga.

    tools/turno.sh python3 tests/emu_86box_mandos.py [maquina]      (p2-voodoo3 por defecto)

No lo recogen rodar_todos.py ni rodar_paralelo.py (solo buscan h<dos dígitos>*.py y b<dígito>_*.py).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu86box  # noqa: E402
from emu86box import Maquina86Box, verificar, SALIDA, MAQUINAS  # noqa: E402
from qemu import imagem_de_teste  # noqa: E402


def cfg_con_joystick(maquina, tipo):
    """Una copia de la máquina con joystick_type = tipo en [Input devices]."""
    with open(os.path.join(MAQUINAS, maquina + ".cfg"), encoding="utf-8") as f:
        t = f.read()
    # joystick_0_nr = 1: el joystick 1 del PC emulado va al primer mando del anfitrión. Sin mando en
    # el anfitrión, 86Box lee sus ejes a 0 (el centro); con joystick_0_nr = 0 los ejes «no están» y
    # el puerto se comporta como sin joystick enchufado.
    linhas = "joystick_type = %s\njoystick_0_nr = 1\n" % tipo
    for k in range(4):
        linhas += "joystick_0_axis_%d = %d\njoystick_0_button_%d = %d\n" % (k, k, k, k)
    if "[Input devices]" in t:
        t = t.replace("[Input devices]\n", "[Input devices]\n" + linhas, 1)
    else:
        t += "\n[Input devices]\n" + linhas
    os.makedirs(SALIDA, exist_ok=True)
    destino = os.path.join(SALIDA, "%s-joy-%s.cfg" % (maquina, tipo))
    with open(destino, "w", encoding="utf-8") as f:
        f.write(t)
    return destino


def tiempos(texto):
    """Los tiempos de «mandos --estado» (us por eje; None si no hay eje)."""
    m = re.findall(r"tiempos ([0-9.\- ]+) us", texto)
    if not m:
        return None
    return [None if v == "-" else float(v) for v in m[-1].split()]


def con_joystick(maquina, imagen, tipo, ejes):
    nombre = f"{maquina}-{tipo}"
    with Maquina86Box(cfg_con_joystick(maquina, tipo), imagen, nome=f"{nombre}-mandos") as m:
        m.pasar_bios(tempo=60)
        m.esperar_serial("sagui:", 240)
        s = m.serial().replace("\x00", "")
        det = re.findall(r"\[MANDOS\] puerto de juegos: [^\n]*", s)
        print("   ", "\n    ".join(det))
        verificar(any("ejes" in d for d in det), f"{nombre}: el kernel detecta el joystick del puerto de juegos")
        out = m.comando("mandos --lista", 30)
        verificar("Joystick analógico (%d ejes" % ejes in out or "Joystick anal" in out, f"{nombre}: mandos --lista ({out.strip()[-120:]})")
        lecturas = []
        for _ in range(4):
            lecturas.append(tiempos(m.comando("mandos --estado 0", 30)))
        print("    tiempos (us):", lecturas)
        verificar(all(lecturas), f"{nombre}: la lectura a petición da tiempos")
        x = [t[0] for t in lecturas]
        verificar(all(v is not None and 15 <= v <= 3000 for v in x), f"{nombre}: el eje X entre 15 us y 3 ms ({x})")
        verificar(max(x) - min(x) <= max(30, 0.1 * max(x)), f"{nombre}: lecturas estables en reposo ({x})")
        if ejes == 2:
            verificar(lecturas[0][2] is None and lecturas[0][3] is None, f"{nombre}: sin los ejes 3 y 4")
        out = m.comando("mandos --estado 0", 30)
        verificar(re.search(r"ejes -?\d+ -?\d+", out) is not None, f"{nombre}: estado normalizado ({out.strip()[-100:]})")
        out = m.comando("mandos --calibracion 100,600,1200/100,600,1200/0,0,0/0,0,0", 30)
        verificar("a mano" in out, f"{nombre}: calibración a mano")
        out = m.comando("mandos --calibracion auto", 30)
        verificar("automática" in out, f"{nombre}: vuelve a la automática")
        out = m.comando("mandos --detectar", 60)
        verificar("Hay un joystick" in out, f"{nombre}: --detectar lo vuelve a encontrar")
        m.captura(f"{nombre}-mandos", guardar=False)


def sin_joystick(maquina, imagen):
    with Maquina86Box(maquina, imagen, nome=f"{maquina}-sin-joy") as m:
        m.pasar_bios(tempo=60)
        m.esperar_serial("sagui:", 240)
        s = m.serial().replace("\x00", "")
        verificar("[MANDOS] puerto de juegos: no hay joystick" in s, f"{maquina} sin joystick: no hay mando")
        out = m.comando("mandos --detectar", 60)
        verificar("No hay joystick" in out, f"{maquina} sin joystick: --detectar no se cuelga")


def main():
    maquina = sys.argv[1] if len(sys.argv) > 1 else "p2-voodoo3"
    verificar(emu86box.instalado(), "86Box instalado (bash tools/86box_instalar.sh)")
    imagen = imagem_de_teste("", ["--vesa", "--console"])
    con_joystick(maquina, imagen, "2axis_2button", 2)
    con_joystick(maquina, imagen, "4axis_4button", 4)
    sin_joystick(maquina, imagen)
    print("EMU 86Box mandos OK")


if __name__ == "__main__":
    main()
