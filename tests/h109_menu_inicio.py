#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H109 - o menu Inicio com icones em todos os programas (D139).

Critério:
  - os programas que estavam sem icone no menu Inicio (Recortes, Iconos,
    Musica e Fecha y hora) ja desenham o seu icone;
  - os programas novos (Visor de PDF, Reprodutor de video e IA) estao no
    menu e tambem tem icone;
  - um programa sem icone (Apagar...) continua sem icone: a zona do icone
    fica limpa, para a comprobacao nao dar um falso positivo.

A posicao dos paineis e das linhas sai do proprio kernel (o log da serial
diz onde se abriu cada menu e que linhas tem); o que se comprova e a tela.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA  # noqa: E402

LINHA_A, SEP_A = 22, 9    # kernel/gui/menu.c
FUNDO_MENU = (0xF4, 0xF4, 0xF0)    # COR_FUNDO_MENU
INICIO_X, BARRA = 2, 24             # INICIO_X e BARRA_SUPERIOR (kernel/gui/barra.c)


def esperar(m, padrao, tempo=15, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def linhas_do_log(m, desde):
    """As linhas do painel que o kernel registou (sem os prefixos do log)."""
    g = esperar(m, r"\[GUI\] menu linhas: (.*)", 10, desde=desde)
    linhas = []
    for l in g.replace("\r", "").split("|"):
        l = l.strip()
        if not l:
            continue
        linhas.append(l.lstrip(">*~"))
    return linhas


def y_das_linhas(panel_y, linhas):
    """A y de cada linha (as separadoras ocupam menos)."""
    ys, y = [], panel_y + 2
    for l in linhas:
        ys.append(y)
        y += SEP_A if l == "-" else LINHA_A
    return ys


def zona_do_icone(px, larg, alt, x, y):
    """Os pixels de uma zona de 16x16 (o sitio onde o kernel desenharia o icone)."""
    fora = 0
    for j in range(y, min(y + 16, alt)):
        for i in range(x, min(x + 16, larg)):
            k = (j * larg + i) * 3
            if (px[k], px[k + 1], px[k + 2]) != FUNDO_MENU:
                fora += 1
    return fora


def painel_do_log(m, desde, sub=False):
    """(x, y, l, a) do painel aberto, do log do kernel."""
    if sub:
        g = esperar(m, r"\[GUI\] submenu \d+ \([^)]*\) aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas",
                    10, desde=desde)
    else:
        g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", 10, desde=desde)
    return tuple(int(v) for v in g[:4])


def principal():
    with Maquina("h109", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        ppm = os.path.join(SAIDA, "h109-menu.ppm")

        # ---- o menu Inicio
        marca = len(m.serial())
        m.clique(40, 12)     # o botao Inicio da barra de tarefas
        esperar(m, r"\[GUI\] menu Inicio aberto \(mouse\)", 10, desde=marca)
        px0, py0, pl, pa = painel_do_log(m, marca)
        linhas = linhas_do_log(m, marca)
        ys = y_das_linhas(py0, linhas)
        time.sleep(0.5)
        m.captura("H109-1-menu-inicio")

        # ---- o submenu "Accesorios"
        i = linhas.index("Accesorios")
        marca2 = len(m.serial())
        m.clique(px0 + 34 + 30, ys[i] + LINHA_A // 2)
        sx, sy, sl, sa = painel_do_log(m, marca2, sub=True)
        sub_linhas = linhas_do_log(m, marca2)
        m.monitor(f"screendump {ppm}")
        for _ in range(50):
            if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
                break
            time.sleep(0.1)
        time.sleep(0.3)
        larg, alt, px = ler_ppm(ppm)

        ys_sub = y_das_linhas(sy, sub_linhas)
        for programa in ("Recortes", "Iconos", "Música", "Visor de PDF", "IA (chat)", "Chat IA"):
            if programa not in sub_linhas:
                verificar(False, f"o programa {programa} devia estar no menu Inicio")
                continue
            k = sub_linhas.index(programa)
            fora = zona_do_icone(px, larg, alt, sx + 12, ys_sub[k] + 3)
            verificar(fora > 0, f"o programa {programa} tem icone no menu Inicio ({fora} pixels)")

        # ---- o submenu "Sistema": o "Fecha y hora"
        marca3 = len(m.serial())
        m.clique(px0 + 34 + 30, ys[linhas.index("Sistema")] + LINHA_A // 2)
        sx, sy, sl, sa = painel_do_log(m, marca3, sub=True)
        sub_linhas = linhas_do_log(m, marca3)
        m.monitor(f"screendump {ppm}")
        time.sleep(0.5)
        larg, alt, px = ler_ppm(ppm)
        ys_sub = y_das_linhas(sy, sub_linhas)
        k = sub_linhas.index("Fecha y hora")
        fora = zona_do_icone(px, larg, alt, sx + 12, ys_sub[k] + 3)
        verificar(fora > 0, f"o programa Fecha y hora tem icone no menu Inicio ({fora} pixels)")

        # ---- um programa sem icone: a zona tem de ficar limpa (a prova de que
        # a comprobacao acima nao e um falso positivo)
        marca4 = len(m.serial())
        m.clique(40, 12)     # fecha o menu (o mesmo botao)
        time.sleep(0.5)
        m.clique(40, 12)     # abre outra vez
        esperar(m, r"\[GUI\] menu Inicio aberto \(mouse\)", 10, desde=marca4)
        px0, py0, pl, pa = painel_do_log(m, marca4)
        linhas = linhas_do_log(m, marca4)
        ys = y_das_linhas(py0, linhas)
        m.monitor(f"screendump {ppm}")
        time.sleep(0.5)
        larg, alt, px = ler_ppm(ppm)
        k = linhas.index("Apagar...")
        fora = zona_do_icone(px, larg, alt, px0 + 12, ys[k] + 3)
        verificar(fora == 0, f"um programa sem icone (Apagar...) deixa a zona vazia ({fora} pixels)")

        # ---- os programas novos abrem de verdade. Com o teclado: o rato, ao
        # passar por cima das outras linhas, abre outros submenus.
        marca5 = len(m.serial())
        m.clique(px0 + 34 + 30, ys[linhas.index("Accesorios")] + LINHA_A // 2)
        esperar(m, r"\[GUI\] submenu \d+ \(Accesorios\) aberto", 10, desde=marca5)
        # O kernel registra as linhas de cada painel (menu linhas), tambem as do
        # submenu. Calcula a posicao do "Visor de PDF" em vez de fixar um
        # numero: assim a prova aguenta que se acrescente um programa ao grupo.
        sub = []
        for l in m.serial()[marca5:].replace("\r", "").splitlines():
            if "[GUI] menu linhas:" in l:
                sub = l.split("[GUI] menu linhas:")[1].split("|")
        # "-": separador, "~": desactivada; nenhuma das duas se escolhe.
        escolhiveis = [x for x in sub if x != "-" and not x.startswith("~")]
        pos = [i for i, x in enumerate(escolhiveis) if "Visor de PDF" in x]
        verificar(len(pos) == 1, "o log diz onde esta o Visor de PDF no submenu")
        m.tecla("home")
        for _ in range(pos[0]):
            m.tecla("down")
        m.tecla("ret")
        m.esperar_serial("inicio -> /apps/bin/visorpdf", 25)
        verificar("/apps/bin/visorpdf" in m.serial()[marca5:], "o Visor de PDF abre desde o menu Inicio")

    return 0


if __name__ == "__main__":
    sys.exit(principal())
