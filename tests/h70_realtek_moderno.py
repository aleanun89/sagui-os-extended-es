#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H70 (D091) - driver Realtek r8169: revisión del chip, inicio por versión y
diagnóstico.

El QEMU no emula la RTL8168, así que el chip real solo se prueba en hardware
(docs/compatibilidad.md). Aquí:
  1. (anfitrión) compila tests/recursos/r8169_anfitriao.c con el MISMO
     kernel/rede/r8169_logica.h y kernel/terceros/linux/r8169/*.h: tabla de
     versiones por el XID, perfil (moderno solo para 40..48), RxConfig de cada
     versión, las secuencias del perfil moderno contra una tarjeta simulada
     (sale del modo fuera de banda, desbloquea la recepción, FIFO, filtro, MAC
     del eFuse, ASPM) y la decodificación del descriptor de recepción.
  2. (QEMU, RTL8139) el diagnóstico solo aparece con el driver experimental:
     el "ifconfig" de una tarjeta que funciona no cambia.

Uso: h70_realtek_moderno.py [anfitriao] [qemu]
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, Maquina, FalhaTeste, imagem_console, verificar  # noqa: E402


def parte_anfitriao():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "r8169_anfitriao")
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-o", exe,
                    os.path.join(RAIZ, "tests", "recursos", "r8169_anfitriao.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    linhas = r.stdout.splitlines()
    for l in linhas:
        if l.startswith("FALHA"):
            print("  " + l)
    verificar(r.returncode == 0 and linhas and linhas[-1] == "TUDO OK",
              f"lógica del r8169 en el anfitrión ({sum(l.startswith('ok') for l in linhas)} comprobaciones)")


def parte_qemu():
    with Maquina("h70-rtl8139", "ide", imagem=imagem_console(), extras=["-nic", "user,model=rtl8139"]) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("ifconfig")
        verificar("driver rtl8139" in out, "ifconfig muestra la RTL8139")
        verificar("Descartes en la pila" not in out and "Chip:" not in out,
                  "sin diagnóstico para un driver que no es experimental")
        out = m.comando("ip")
        verificar("Descartes en la pila" not in out, "ip (alias) tampoco lo muestra")


def main():
    partes = sys.argv[1:] or ["anfitriao", "qemu"]
    if "anfitriao" in partes:
        parte_anfitriao()
    if "qemu" in partes:
        parte_qemu()
    print("H70 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        sys.exit(1)
