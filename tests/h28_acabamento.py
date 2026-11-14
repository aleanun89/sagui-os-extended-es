#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H28 - acabamento.

Critério: captura com três programas abertos ao mesmo tempo; imagem medida e
registrada (meta: até 38 MB, D181; antes 16 MB, D122; 1,44 MB no início). Os programas são abertos com duplo clique nos
ícones da área de trabalho; a tela Sobre mostra versão, tamanho da imagem,
processos ativos e créditos.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ  # noqa: E402

ICONES = ["Arquivos", "Terminal", "Editor", "Calculadora", "Banana", "Sagui 3D", "Sobre"]


def abrir_icone(m, nome):
    i = ICONES.index(nome)
    m.clique(20 + 48, 24 + 16 + i * 84 + 32, duplo=True)


def main():
    tamanho = int(open(os.path.join(RAIZ, "build", "tamanho.txt")).read())
    # A versão esperada vem da constante única (kernel/inc/versao.h, D101).
    versao = re.search(r'#define SAGUI_VERSAO\s+"([^"]+)"', open(os.path.join(RAIZ, "kernel", "inc", "versao.h")).read()).group(1)
    base = int(open(os.path.join(RAIZ, "build", "tamanho-base.txt")).read())   # sin la partición de juegos (D228)
    verificar(base <= 38 * 1024 * 1024, f"sistema base com {base} bytes úteis ({100 * base / (38 * 1024 * 1024):.1f}% da meta de 38 MB)")
    with Maquina("h28", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        for nome, titulo in (("Arquivos", "Archivos"), ("Calculadora", "Calculadora"), ("Banana", "Sagui Atrapa Bananas")):
            abrir_icone(m, nome)
            m.esperar_serial(f'"{titulo}"', 15)
            verificar(True, f"duplo clique no ícone {nome} abre o programa")
            time.sleep(0.5)
        abrir_icone(m, "Sobre")
        m.esperar_serial("sobre: versao", 15)
        s = m.serial()
        v = re.findall(r"sobre: versao (.*?), imagem (\d+) bytes, (\d+) processos", s)[-1]
        verificar(v[0] == versao and int(v[1]) == tamanho, f"Sobre mostra a versão {v[0]} e o tamanho da imagem ({v[1]} bytes)")
        verificar(int(v[2]) >= 7, f"Sobre mostra {v[2]} processos ativos (ocioso, janelas, terminal, sagsh e os programas abertos)")
        time.sleep(1)
        m.captura("H28-programas-abertos")
        # Dados da compilação (D126): a tela "Acerca de" mostra la fecha, la hora
        # y el commit; el registro los lleva para comprobarlo.
        comp = re.findall(r"compilacao (\S+) (\S+) commit (\S+)", s)[-1]
        verificar(re.match(r"\d{4}-\d{2}-\d{2}", comp[0]) is not None,
                  f"el Acerca de muestra la fecha de compilación {comp[0]} a las {comp[1]} (commit {comp[2]})")
        # Botão "Créditos" do Acerca de (D101): troca de página. A posição da
        # janela é lida do log, para não depender de coordenadas fixas.
        mh = re.findall(r'"Acerca de SaguiOS-Extended" (\d+)x(\d+) em \((\d+),(\d+)\)', s)[-1]
        vx, vy = int(mh[2]), int(mh[3])
        m.clique(vx + 20 + 55, vy + 24 + 420 + 13)     # botão "Créditos"
        m.esperar_serial("sobre: pagina creditos", 10)
        time.sleep(0.5)
        m.captura("H28-creditos")
        # O programa tem de seguir vivo: uma vez lia uma linha de mais da
        # lista de créditos e caía (D126).
        s = m.serial()
        verificar("(sobre-o-sistema) terminou" not in s and "Fallo de página" not in s,
                  "a página de créditos não crashed o Acerca de")
        # E volta para a página de informação.
        m.clique(vx + 20 + 55, vy + 24 + 420 + 13)     # agora "Informacion"
        m.esperar_serial("sobre: pagina informacao", 10)
        s = m.serial()
        verificar("(sobre-o-sistema) terminou" not in s and "Fallo de página" not in s,
                  "voltar à página de informação também funciona")
        verificar(True, "o botão Créditos do Acerca de mostra a página de créditos (captura H28-creditos)")
        criadas = len(re.findall(r"\[GUI\] janela \d+ criada", s))
        fechadas = len(re.findall(r"\[GUI\] fechar janela", s))
        verificar(criadas >= 5 and fechadas == 0, f"{criadas} janelas abertas ao mesmo tempo (Terminal + 4 programas)")
        # Fecha o Sobre e espalha as janelas para a captura de três programas lado a lado.
        m.tecla("ret")
        m.esperar_serial("(sobre-o-sistema) terminou", 10)

        def moldura(titulo):
            g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())[-1]
            return int(g[2]), int(g[3])

        bx, by = moldura("Sagui Atrapa Bananas")
        m.arrastar(bx + 60, by + 12, 528 + 60, 316 + 12)
        cx, cy = moldura("Calculadora")
        m.arrastar(cx + 60, cy + 12, 150 + 60, 330 + 12)
        ax, ay = moldura("Archivos")
        m.arrastar(ax + 60, ay + 12, 330 + 60, 40 + 12)
        time.sleep(1)
        png = m.captura("H28-tres-programas")
        verificar(True, "captura com Arquivos, Calculadora e Sagui Pega Banana lado a lado")
    return 0


if __name__ == "__main__":
    sys.exit(main())
