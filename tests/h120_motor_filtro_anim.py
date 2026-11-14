#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H120 - o filtro CSS e as animações do motor da web (v6.3).

Critério: o programa /testes/cipo-motor pinta uma página com caixas de
fundo com filtro (grayscale, brightness, opacity, blur), com texto dentro de
um contentor filtrado e com uma animação de @keyframes. O programa diz no
log as cores dos pixels que pintou e o que motor_tick devolveu.

Aqui não há OCR (a tela não deixa ler os rótulos): o teste compara as cores
que o motor pintou de verdade com o que se espera.
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
    with Maquina("h120", "ide", imagem=imagem_de_teste("/testes/cipo-motor")) as m:
        m.esperar_serial("anim:", 90)
        s = m.serial().replace("\r", "")
        linha_f = linha_t = linha_a = ""
        for l in s.splitlines():
            if "filtro:" in l:
                linha_f = l.strip()
            elif "texto:" in l:
                linha_t = l.strip()
            elif "anim:" in l:
                linha_a = l.strip()
        print("   ", linha_f)
        print("   ", linha_t)
        print("   ", linha_a)
        verificar(linha_f != "", "o motor escreveu o relatório do filtro")
        verificar(linha_t != "", "o motor escreveu o relatório do texto")
        verificar(linha_a != "", "o motor escreveu o relatório da animação")

        # ------------------------------------------------------ o filtro
        cinza = cor(linha_f, "cinza")
        verificar(cinza is not None, "o log tem a cor com cinza")
        r, g, b = canais(cinza)
        verificar(r == g == b, "o grayscale deixou a cor cinzenta (%06x)" % cinza)
        verificar(cinza != 0xFF0000, "o grayscale mudou mesmo a cor")

        brilho = cor(linha_f, "brilho")
        verificar(brilho is not None, "o log tem a cor com brilho")
        r, g, b = canais(brilho)
        verificar(r > 200 and 120 <= g <= 136 and b == 0,
                  "o brightness dobrou a cor (%06x)" % brilho)

        opac = cor(linha_f, "opac")
        verificar(opac is not None, "o log tem a cor com opacidade")
        r, g, b = canais(opac)
        verificar(g > 200 and 120 <= r <= 136 and r == b,
                  "o opacity deixou a cor meio transparente (%06x)" % opac)

        canto = cor(linha_f, "canto")
        verificar(canto is not None, "o log tem a cor do canto desfocado")
        verificar(canto != 0xFF0000, "o blur desfez a borda da caixa (%06x)" % canto)
        vermelhos = num(linha_f, "vermelhos")
        verificar(vermelhos is not None and vermelhos < 800,
                  "o blur espalhou o vermelho da caixa (%s de 800)" % vermelhos)

        # o texto de dentro herda o filtro do contentor
        texto = cor(linha_f, "texto")
        verificar(texto is not None, "o log tem a cor do texto")
        r, g, b = canais(texto)
        verificar(r == g == b and texto != 0xFFFFFF,
                  "o texto saiu cinzento, com o filtro do contentor (%06x)" % texto)
        verificar(num(linha_t, "nvermelho") == 0, "não sobrou texto vermelho")
        verificar(num(linha_t, "npreto") == 0, "não sobrou texto preto")
        verificar(num(linha_t, "ncinza") > 0, "o texto cinzento foi pintado")

        # ------------------------------------------------------ a animação
        base = cor(linha_a, "base")
        t0 = cor(linha_a, "t0")
        t500 = cor(linha_a, "t500")
        verificar(base == 0x00FF00, "sem o tick a cor é a da cascata (%06x)" % (base or 0))
        verificar(t0 == 0xFF0000, "no instante 0 a animação está no primeiro quadro (%06x)" % (t0 or 0))
        verificar(t500 is not None and t500 != t0 and t500 != base,
                  "no instante 500 a cor é outra (%06x)" % (t500 or 0))
        if t500 is not None:
            r, g, b = canais(t500)
            verificar(g == 0 and r == b and 100 <= r <= 140,
                      "no instante 500 a cor é a mistura dos dois quadros (%06x)" % t500)
        verificar(num(linha_a, "tick0") == 1, "motor_tick devolveu true quando a animação mudou")
        verificar(num(linha_a, "tick500") == 1, "motor_tick devolveu true no instante seguinte")
        verificar(num(linha_a, "tick500b") == 0, "motor_tick devolveu false quando nada mudou")
        verificar(num(linha_a, "tick1000") == 1, "motor_tick devolveu true quando a animação acabou")
        m.captura("H120-motor-filtro-anim")
    return 0


if __name__ == "__main__":
    sys.exit(main())
