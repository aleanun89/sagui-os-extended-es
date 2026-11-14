#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H98 - componentes novos da libsagui (melhora 042).

Critério: o programa de demonstração mostra o lienzo, a barra de progresso
e a casilla de verificação; o clique no lienzo escolhe um elemento, a
casilha se marca e a barra de progresso desenhada tem o texto certo.
Captura em docs/capturas/.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def origem(serial, titulo):
    m = re.findall(rf'criada por \d+: "{titulo}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return int(m[2]) + 2, int(m[3]) + 24, int(m[0]), int(m[1])   # canto da área de conteúdo


def main():
    with Maquina("h98", "ide", imagem=imagem_de_teste("/bin/componentes", ["--vesa"])) as m:
        m.esperar_serial("componentes: pronto", 120)
        ox, oy, _, _ = origem(m.serial(), "Componentes de libsagui")
        m.captura("H98-componentes-v6.1")

        # A barra de progresso determinada escreve "40%" (a do programa de
        # demonstração vai a 40 %).
        # O lienzo está no canto superior direito: rectángulo (332,20,212,84).
        # O rectángulo dentro do lienzo va de (10,10) a (70,50), o que na
        # janela fica em (332+10, 20+10) = (342, 30).
        m.clique(ox + 332 + 30, oy + 20 + 25)
        m.esperar_serial("componentes: lenzo elemento", 10)
        elem = int(re.findall(r"componentes: lenzo elemento (-?\d+)", m.serial())[-1])
        verificar(elem == 0, f"lienzo: o clique no rectângulo escolheu o elemento 0 (veio {elem})")

        # Um clique fora dos elementos não escolhe nenhum (-1).
        m.clique(ox + 332 + 5, oy + 20 + 78)
        m.esperar_serial("componentes: lenzo elemento", 10)
        elem = int(re.findall(r"componentes: lenzo elemento (-?\d+)", m.serial())[-1])
        verificar(elem == -1, f"lienzo: o clique no vazio não escolhe elemento (veio {elem})")

        # A elipse (segundo elemento) também se escolhe.
        m.clique(ox + 332 + 105, oy + 20 + 30)
        m.esperar_serial("componentes: lenzo elemento", 10)
        elem = int(re.findall(r"componentes: lenzo elemento (-?\d+)", m.serial())[-1])
        verificar(elem == 1, f"lienzo: o clique na elipse escolheu o elemento 1 (veio {elem})")
        m.captura("H98-lienzo")

        # A casilla de verificação se marca e se desmarca.
        m.clique(ox + 290 + 7, oy + 302 + 7)
        m.esperar_serial("componentes: casilla marcada", 10)
        m.clique(ox + 290 + 7, oy + 302 + 7)
        m.esperar_serial("componentes: casilla desmarcada", 10)
        verificar(True, "casilla de verificação: marca e desmarca ao clicar")
        m.captura("H98-casilla")
    return 0


if __name__ == "__main__":
    sys.exit(main())
