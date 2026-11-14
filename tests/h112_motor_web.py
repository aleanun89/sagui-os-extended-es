#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H112 - o motor da web do Cipo (melhoria 081).

Critério: o programa /testes/cipo-motor carrega um HTML, faz o layout, pinta a
página com as fontes do sistema e diz no log quantos nós, comandos de pintura e
pixels pintou. Se o motor pintou texto de verdade, o motor funciona dentro do
SaguiOS: as fontes da web, o layout CSS e a pintura estão no sitio.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def main():
    with Maquina("h112", "ide", imagem=imagem_de_teste("/testes/cipo-motor")) as m:
        m.esperar_serial("motor:", 90)
        s = m.serial().replace("\r", "")
        linha = ""
        for l in s.splitlines():
            if "motor:" in l:
                linha = l.strip()
        print("   ", linha)
        verificar(linha != "", "o motor escreveu o seu relatório no log")

        # nos=12 cmds=NN imgs=0 alt=NN links=1 pintados=NN
        d = dict(re.findall(r"(\w+)=(\d+)", linha))
        verificar("nos" in d, "o log tem os contadores do motor")
        verificar(int(d.get("nos", "0")) > 0, "a árvore do HTML tem nós")
        verificar(int(d.get("cmds", "0")) > 0, "o motor gerou comandos de pintura")
        verificar(int(d.get("alt", "0")) > 0, "o layout deu uma altura de página")
        verificar(int(d.get("links", "0")) >= 1, "o motor viu o link da página")
        verificar(int(d.get("pintados", "0")) > 100,
                  "a página pintou pixels de verdade (%s)" % d.get("pintados", "0"))
        m.captura("H112-motor-web")
    return 0


if __name__ == "__main__":
    sys.exit(main())
