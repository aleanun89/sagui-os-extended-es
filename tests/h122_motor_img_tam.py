#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H122 - o tamanho do <img> com os atributos width/height (v6.3).

Critério: o programa /testes/cipo-motor pinta quatro imagens (uma PNG 4x2
em data:) e mede a caixa que cada uma ocupou: duas com os atributos
width/height, uma só com a largura, uma só com a altura e uma com o CSS a
mandar. O texto alt de uma imagem que não chegou tem de ficar dentro da
caixa e aparecer uma vez só.

Sem OCR: o teste lê do log as caixas que o motor pintou.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def num(linha, chave):
    m = re.search(r"\b%s=(\d+)\b" % chave, linha)
    return int(m.group(1)) if m else None


def main():
    with Maquina("h122", "ide", imagem=imagem_de_teste("/testes/cipo-motor")) as m:
        m.esperar_serial("img:", 90)
        s = m.serial().replace("\r", "")
        linha = ""
        for l in s.splitlines():
            if "img:" in l:
                linha = l.strip()
        print("   ", linha)
        verificar(linha != "", "o motor escreveu o relatório das imagens")

        # width e height: o tamanho pedido é o dos atributos
        verificar(num(linha, "imgl") == 120 and num(linha, "imga") == 60,
                  "a imagem com width=120 height=60 ficou com 120x60 (%s x %s)"
                  % (num(linha, "imgl"), num(linha, "imga")))
        # só a largura: a altura sai da proporção natural (a PNG é 4x2)
        verificar(num(linha, "img2l") == 80 and num(linha, "img2a") == 40,
                  "a imagem com width=80 ficou com 80x40 (%s x %s)"
                  % (num(linha, "img2l"), num(linha, "img2a")))
        # só a altura: a largura sai da proporção natural
        verificar(num(linha, "img3l") == 60 and num(linha, "img3a") == 30,
                  "a imagem com height=30 ficou com 60x30 (%s x %s)"
                  % (num(linha, "img3l"), num(linha, "img3a")))
        # o CSS manda sobre o atributo
        verificar(num(linha, "img4l") == 200 and num(linha, "img4a") == 60,
                  "o CSS width=200px mandou sobre o atributo (%s x %s)"
                  % (num(linha, "img4l"), num(linha, "img4a")))
        # o texto alt: uma vez só e dentro da caixa (a caixa tem 100x20)
        altl = num(linha, "altl")
        alta = num(linha, "alta")
        verificar(altl is not None and 0 < altl <= 100,
                  "o texto alt cabe na largura da caixa (%s de 100)" % altl)
        verificar(alta is not None and 0 < alta <= 20,
                  "o texto alt cabe na altura da caixa (%s de 20)" % alta)
    return 0


if __name__ == "__main__":
    sys.exit(main())
