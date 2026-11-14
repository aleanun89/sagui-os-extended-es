#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H85 - bandeja de la barra de tareas: volumen, red y temperatura (D114, mejora 058).

Critério:
  - a bandeja fica à esquerda do relógio com 2 ícones no QEMU (volume e rede;
    a temperatura não aparece: o hipervisor não emula os sensores) e os ícones
    estão desenhados na captura (alto-falante claro, barras verdes da rede);
  - com a placa AC'97: ao passar o mouse aparece a dica "Volumen: 100 %" e, na
    rede, o IP 10.0.2.15 e a placa; o clique no volume abre o painel com o
    controle deslizante; clicar no trilho muda o volume e o comando `volumen`
    mostra o mesmo valor; a roda sobre o ícone sobe/baixa de 5 em 5;
    "Silenciar" cala (volumen mostra "silencio") e o ícone fica tachado;
  - o clique na rede abre o painel com IP, porta de enlace e DNS e o botão
    "Renovar DHCP" dispara a renovação sem derrubar o IP; Esc fecha o painel;
  - INFO_BANDEJA-sem-sensores: nada quebra (log "temp -1");
  - sem placa de som: o ícone fica cinza e a dica diz "Sin sonido"; o clique
    só mostra um aviso (nenhum painel).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel  # noqa: E402

IC_Y = 3
CLARO = (0xD8, 0xDE, 0xE4)
CINZA_APAGADO = (0x4A, 0x56, 0x62)
VERDE = (0x6F, 0xD9, 0x8F)
VERMELHO = (0xE0, 0x52, 0x4A)


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def icones(m):
    g = re.findall(r"\[GUI\] bandeja (\d+)\+(\d+):([^\r\n]*)", m.serial())[-1]
    return {n: int(x) for n, x in re.findall(r"(\w+)@(\d+)", g[2])}, int(g[0]), int(g[1])


def roda(m, entalhes):
    """Gira a roda: + para baixo (no monitor, dz > 0 é para cima)."""
    for _ in range(abs(entalhes)):
        m.monitor(f"mouse_move 0 0 {-1 if entalhes > 0 else 1}")
        time.sleep(0.15)
    time.sleep(0.5)


def volumen(m):
    out = m.comando("volumen")
    g = re.search(r"Volumen: (\d+) %(?: \((silencio)\))?", out)
    return int(g[1]), bool(g[2])


def painel(m, desde, nome):
    g = esperar(m, rf"bandeja: painel {nome} aberto em \((\d+),(\d+)\) (\d+)x(\d+)", desde=desde)
    return tuple(int(v) for v in g)


def com_som():
    with Maquina("h85", "ide", som="ac97") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1.5)
        esperar(m, r"\[GUI\] bandeja: som 1 volume 100, rede 2 ip 10\.0\.2\.15, temp -1", 30)
        verificar(True, "a bandeja lê som (AC'97), rede (10.0.2.15) e temperatura n/d sem quebrar nada")
        ic, bx, bl = icones(m)
        verificar(set(ic) == {"vol", "red"}, f"sem sensores a temperatura fica oculta ({ic})")
        relogio = int(re.findall(r"relogio (\d+)", m.serial())[-1])
        verificar(bx + bl < relogio - 6, f"a bandeja ({bx}+{bl}) fica à esquerda do relógio ({relogio})")
        png = m.captura("H85-1-bandeja")
        verificar(cor_do_pixel(png, ic["vol"] + 2, IC_Y + 7) == CLARO, "o ícone do volume (alto-falante) está desenhado")
        verificar(cor_do_pixel(png, ic["vol"] + 9, IC_Y + 7) == CLARO, "com 100 % o ícone tem as ondas")
        verificar(cor_do_pixel(png, ic["red"] + 14, IC_Y + 8) == VERDE, "o ícone da rede está verde (conectado)")

        # ---- dicas ao passar o mouse
        marca = len(m.serial())
        m.mouse_para(ic["vol"] + 8, 12)
        esperar(m, r'bandeja: dica "Volumen: 100 %"', desde=marca)
        verificar(True, 'sobre o volume: dica "Volumen: 100 %"')
        marca = len(m.serial())
        m.mouse_mover(ic["red"] + 8, 12)
        g = esperar(m, r'bandeja: dica "(Red: [^"]*)"', desde=marca)
        verificar("10.0.2.15" in g and "eth0" in g, f"sobre a rede: a dica mostra o IP e a placa ({g})")
        time.sleep(0.5)
        m.captura("H85-2-dica-rede")

        # ---- painel do volume: o trilho muda o volume
        marca = len(m.serial())
        m.clique(ic["vol"] + 8, 12)
        px, py, pl, pa = painel(m, marca, "volume")
        sl = pl - 24
        m.captura("H85-3-painel-volume")
        m.mouse_mover(px + 12 + sl // 2, py + 38)
        m.monitor("mouse_button 1")
        time.sleep(0.1)
        m.monitor("mouse_button 0")
        time.sleep(0.6)
        esperar(m, r"\[AUDIO\] bandeja: volume 50", 10)
        m.captura("H85-4-volume-50")
        # Esc fecha o painel; o terminal ainda tem o foco para o comando
        m.tecla("esc")
        time.sleep(0.5)
        v, calado = volumen(m)
        verificar(v == 50 and not calado, f"o clique no trilho fixou o volume em 50 e `volumen` concorda ({v} %)")

        # ---- a roda sobre o ícone
        m.mouse_para(ic["vol"] + 8, 12)
        roda(m, -2)   # para cima: +10
        v, _ = volumen(m)
        verificar(v == 60, f"a roda para cima sobe 5 por entalhe ({v} %)")
        m.mouse_para(ic["vol"] + 8, 12)
        roda(m, 3)    # para baixo: -15
        v, _ = volumen(m)
        verificar(v == 45, f"a roda para baixo desce 5 por entalhe ({v} %)")

        # ---- silenciar
        marca = len(m.serial())
        m.clique(ic["vol"] + 8, 12)
        px, py, pl, pa = painel(m, marca, "volume")
        m.mouse_para(px + 60, py + 62)
        m.monitor("mouse_button 1")
        time.sleep(0.1)
        m.monitor("mouse_button 0")
        time.sleep(0.8)
        esperar(m, r"bandeja: silencio ligado", 10)
        png = m.captura("H85-5-silencio")
        verificar(cor_do_pixel(png, ic["vol"] + 10, IC_Y + 6) == VERMELHO, "no silêncio o ícone fica tachado (cruz vermelha)")
        m.tecla("esc")
        time.sleep(0.5)
        v, calado = volumen(m)
        verificar(calado and v == 45, f"`volumen` mostra silencio ({v} %)")
        m.comando("volumen activar")

        # ---- painel da rede
        marca = len(m.serial())
        m.clique(ic["red"] + 8, 12)
        px, py, pl, pa = painel(m, marca, "rede")
        time.sleep(0.5)
        m.captura("H85-6-painel-red")
        marca = len(m.serial())
        m.mouse_para(px + 12 + 40, py + pa - 20)
        m.monitor("mouse_button 1")
        time.sleep(0.1)
        m.monitor("mouse_button 0")
        esperar(m, r"bandeja: renovar DHCP", 10, desde=marca)
        esperar(m, r"\[REDE\] DHCP: renovacao pedida", 10, desde=marca)
        time.sleep(3)
        verificar("rede 2 ip 10.0.2.15" in m.serial()[marca:] or "bandeja: som" not in m.serial()[marca:],
                  "o botão Renovar DHCP renova e o IP continua 10.0.2.15")
        m.tecla("esc")
        time.sleep(0.5)
        png = m.captura("H85-7-fechado")
        verificar(cor_do_pixel(png, px + 100, py + 60) != (0x1C, 0x26, 0x30), "Esc fechou o painel")
    return 0


def sem_som():
    with Maquina("h85-sem", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1.5)
        esperar(m, r"\[GUI\] bandeja: som 0 ", 30)
        ic, bx, bl = icones(m)
        png = m.captura("H85-8-sem-som")
        verificar(cor_do_pixel(png, ic["vol"] + 2, IC_Y + 7) == CINZA_APAGADO, "sem placa de som o ícone é cinza")
        marca = len(m.serial())
        m.mouse_para(ic["vol"] + 8, 12)
        esperar(m, r'bandeja: dica "Sin sonido"', desde=marca)
        verificar(True, 'sem placa de som: dica "Sin sonido"')
        marca = len(m.serial())
        m.clique(ic["vol"] + 8, 12)
        time.sleep(1)
        verificar("bandeja: painel volume" not in m.serial()[marca:], "sem placa o clique não abre painel")
    return 0


def main():
    return com_som() or sem_som()


if __name__ == "__main__":
    sys.exit(main())
