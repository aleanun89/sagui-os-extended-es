#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H96 (D124) - r8169: arranque de Linux para la familia RTL8168E/8168E-VL.

El QEMU no emula la RTL8168, así que solo se prueba en el anfitrión:
compila tests/recursos/r8169_8168e_anfitriao.c con el MISMO
kernel/rede/r8169_logica.h y kernel/terceros/linux/r8169/*.h y comprueba la
elección del arranque por versión (32..34 -> 8168e; 25/26 del HP dv7 siguen
en el clásico; 40..48 en el moderno), RxConfig/TxConfig/CPlusCmd y lo que
escriben rtl_hw_start_8168e_1/_2 en una tarjeta simulada (EPHY, ERI, MISC,
Config2..5, jumbo, MAC en el ERI de la 34).

Uso: h96_realtek_8168e.py
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, FalhaTeste, verificar  # noqa: E402


def main():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "r8169_8168e_anfitriao")
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-o", exe,
                    os.path.join(RAIZ, "tests", "recursos", "r8169_8168e_anfitriao.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    linhas = r.stdout.splitlines()
    for l in linhas:
        if l.startswith("FALHA"):
            print("  " + l)
    verificar(r.returncode == 0 and linhas and linhas[-1] == "TUDO OK",
              f"arranque de las 8168e en el anfitrión ({sum(l.startswith('ok') for l in linhas)} comprobaciones)")
    print("H96 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        sys.exit(1)
