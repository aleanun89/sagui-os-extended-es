#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H130 - as transições CSS do motor da web (melhoria 085, D153).

Critério: o programa /testes/cipo-motor carrega uma página em que a janela
estreita faz entrar uma regra @media, o que muda a cor de uma caixa e a
largura de outra. A página declara transition, por isso os valores têm de
mudar aos poucos: no meio da transição (150 ms de 300 ms) a cor tem de ser
um valor intermédio (nem a cor antiga nem a nova) e a caixa tem de ter uma
largura intermédia. A mesma página sem transition salta logo para o valor
novo, e os números da pintura (nós, comandos de pintura e altura da página)
têm de ser os mesmos com e sem transition.

Há também uma caixa que usa as formas longas (transition-property,
transition-duration e transition-timing-function): a transição também tem de
acontecer.

Aqui não há OCR: o programa diz as cores que pintou e as larguras, e o teste
lê esses valores do log.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def cor(linha, chave):
    """A cor (0xRRGGBB) de um campo do log."""
    m = re.search(r"\b%s=([0-9a-f]{6})\b" % chave, linha)
    return int(m.group(1), 16) if m else None


def num(linha, chave):
    """O número de um campo do log."""
    m = re.search(r"\b%s=(\d+)\b" % chave, linha)
    return int(m.group(1)) if m else None


def canais(c):
    return ((c >> 16) & 255, (c >> 8) & 255, c & 255)


def main():
    with Maquina("h130", "ide", imagem=imagem_de_teste("/testes/cipo-motor")) as m:
        m.esperar_serial("trans:", 90)
        s = m.serial().replace("\r", "")
        linha_c = linha_f = linha_l = linha_n = ""
        for l in s.splitlines():
            if "trans:" in l:
                linha_c = l.strip()
            elif "transf:" in l:
                linha_f = l.strip()
            elif "transl:" in l:
                linha_l = l.strip()
            elif "transn:" in l:
                linha_n = l.strip()
        print("   ", linha_c)
        print("   ", linha_f)
        print("   ", linha_l)
        print("   ", linha_n)
        verificar(linha_c != "", "o motor escreveu o relatório da transição")
        verificar(linha_f != "", "o motor escreveu o relatório da forma longa")
        verificar(linha_l != "", "o motor escreveu o relatório da largura")
        verificar(linha_n != "", "o motor escreveu o relatório dos números")

        # ------------------------------------------------------ a cor
        ini = cor(linha_c, "ini")
        t0 = cor(linha_c, "t0")
        t150 = cor(linha_c, "t150")
        t400 = cor(linha_c, "t400")
        verificar(ini == 0xFF0000, "a caixa começa vermelha (%06x)" % (ini or 0))
        verificar(t0 == 0xFF0000, "no instante 0 a cor ainda é a antiga (%06x)" % (t0 or 0))
        verificar(t150 is not None and t150 != 0xFF0000 and t150 != 0x0000FF,
                  "no meio da transição a cor é outra (%06x)" % (t150 or 0))
        if t150 is not None:
            r, g, b = canais(t150)
            verificar(g == 0 and 0 < r < 255 and 0 < b < 255,
                      "no meio da transição a cor é a mistura das duas (%06x)" % t150)
            # a curva "ease" é rápida: ao meio do tempo já se passou de metade
            verificar(r < 110, "a curva de aceleração é a do CSS (%06x)" % t150)
        verificar(t400 == 0x0000FF, "no fim da transição a cor é a nova (%06x)" % (t400 or 0))

        # ------------------------------------------------------ o relógio
        verificar(num(linha_c, "tick150") == 1, "motor_tick devolveu true no meio da transição")
        verificar(num(linha_c, "tick400") == 1, "motor_tick devolveu true no fim da transição")
        verificar(num(linha_c, "tick500") == 0,
                  "a transição acabou: o motor não fica com pinturas pendentes")

        # ------------------------------------------------------ a largura
        l_ini = num(linha_l, "l_ini")
        l_t0 = num(linha_l, "l_t0")
        l_t150 = num(linha_l, "l_t150")
        l_t400 = num(linha_l, "l_t400")
        verificar(l_ini == 100, "a caixa começa com 100 px de largura (%s)" % l_ini)
        verificar(l_t0 == 100, "no instante 0 a largura ainda é a antiga (%s)" % l_t0)
        verificar(l_t150 is not None and 100 < l_t150 < 150,
                  "no meio da transição a largura é intermédia (%s)" % l_t150)
        verificar(l_t400 == 150, "no fim da transição a largura é a nova (%s)" % l_t400)

        # ------------------------------------------- as formas longas
        f_ini = cor(linha_f, "ini")
        f_t0 = cor(linha_f, "t0")
        f_t150 = cor(linha_f, "t150")
        f_t400 = cor(linha_f, "t400")
        verificar(f_ini == 0xFFFF00, "a caixa das formas longas começa amarela (%06x)" % (f_ini or 0))
        verificar(f_t0 == 0xFFFF00, "no instante 0 a cor ainda é a antiga (%06x)" % (f_t0 or 0))
        verificar(f_t150 is not None and f_t150 != 0xFFFF00 and f_t150 != 0x00FF00,
                  "no meio da transição a cor é outra (%06x)" % (f_t150 or 0))
        if f_t150 is not None:
            r, g, b = canais(f_t150)
            verificar(b == 0 and 0 < r < 255 and g == 255,
                      "no meio da transição a cor é a mistura das duas (%06x)" % f_t150)
        verificar(f_t400 == 0x00FF00, "no fim da transição a cor é a nova (%06x)" % (f_t400 or 0))

        # ------------------------------------------------------ a regressão
        nos = num(linha_n, "nos")
        cmds = num(linha_n, "cmds")
        alt = num(linha_n, "alt")
        nos2 = num(linha_n, "nos2")
        cmds2 = num(linha_n, "cmds2")
        alt2 = num(linha_n, "alt2")
        verificar(nos is not None and nos > 0, "a página tem nós")
        verificar(cmds is not None and cmds > 0, "a página tem comandos de pintura")
        verificar(nos == nos2, "a transition não muda o número de nós (%s e %s)" % (nos, nos2))
        verificar(cmds == cmds2,
                  "a transition não muda o número de comandos de pintura (%s e %s)" % (cmds, cmds2))
        verificar(alt == alt2, "a transition não muda a altura da página (%s e %s)" % (alt, alt2))

        m.captura("H130-motor-transicoes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
