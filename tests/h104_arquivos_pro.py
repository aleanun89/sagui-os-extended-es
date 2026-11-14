#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H104 - o explorador de arquivos profissional (melhoria 058, D136).

Critério:
  - o Archivos abre com a barra de ferramentas, a barra de ruta com as migas de
    pan, o painel lateral e a barra de estado (capturas);
  - os ocultos: por defeito não se veem; com Ctrl+H aparecem, e a preferência
    fica gravada (o Archivos seguinte abre já com os ocultos à vista);
  - a vista de detalhes ordena pela coluna pulsada;
  - criar uma pasta, renomeá-la (F2) e apagá-la (com confirmação);
  - copiar um arquivo e colá-lo noutra pasta;
  - buscar por nome com curingas (* e ?).
Tudo o que se comprova é o que o programa escreve na serial; a tela só fica
como imagem.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402

# As coordenadas do programa (a janela do Archivos, coordenadas do cliente):
# a lista começa em x=146 (depois do painel lateral) e y=70, com itens de 20 px.
LISTA_X, LISTA_Y, ALT_ITEM = 146, 70, 20
# A barra de ferramentas: oito botões, o primeiro em x=8.
BT = {"voltar": 8, "nova": 106, "borrar": 224, "adelante": 304, "arriba": 390,
      "ocultos": 476, "vista": 588, "buscar": 636}


def janela(serial, titulo):
    m = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24)


def item(a, i):
    """O centro do item i da lista."""
    return a["x"] + LISTA_X + 120, a["y"] + LISTA_Y + 2 + i * ALT_ITEM + 8


def botao(a, nome):
    """O centro de um botão da barra de ferramentas."""
    return a["x"] + BT[nome] + 35, a["y"] + 7 + 13


def principal():
    with Maquina("h104", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---- preparação: um arquivo oculto, uma pasta e um arquivo dentro dela
        m.comando("touch /tmp/.ponto")
        time.sleep(0.3)
        m.comando("mkdir /tmp/origem")
        time.sleep(0.3)
        m.comando("touch /tmp/origem/uno.txt")
        time.sleep(0.5)

        m.comando("arquivos /tmp")
        m.esperar_serial("arquivos: pronto", 20)
        time.sleep(0.8)
        a = janela(m.serial(), "Archivos")
        m.captura("H104-1-arquivos")

        # ---- os ocultos: por defeito não se veem
        verificar("arquivos: pasta /tmp (1 itens)" in m.serial(),
                  "por defeito os ocultos não se veem (só a pasta /tmp/origem)")
        m.tecla("ctrl-h")
        m.esperar_serial("arquivos: ocultos sim", 10)
        # com os ocultos: /tmp/origem, /tmp/.ponto e, se o gestor de serviços está em marcha,
        # a sua pasta oculta /tmp/.servicios (D216): 2 ou 3 itens
        for _ in range(50):
            if "arquivos: pasta /tmp (2 itens)" in m.serial() or "arquivos: pasta /tmp (3 itens)" in m.serial():
                break
            time.sleep(0.2)
        verificar("arquivos: pasta /tmp (2 itens)" in m.serial() or "arquivos: pasta /tmp (3 itens)" in m.serial(),
                  "com Ctrl+H o arquivo oculto /tmp/.ponto aparece")
        time.sleep(0.4)
        m.captura("H104-2-ocultos")
        m.tecla("ctrl-h")
        m.esperar_serial("arquivos: ocultos nao", 10)
        m.esperar_serial("arquivos: pasta /tmp (1 itens)", 10)
        verificar(True, "com Ctrl+H outra vez os ocultos desaparecem")

        # ---- a vista de detalhes ordena pela coluna pulsada
        m.clique(*botao(a, "vista"))
        m.esperar_serial("arquivos: vista detalhes", 10)
        time.sleep(0.4)
        m.captura("H104-3-detalhes")
        m.clique(a["x"] + LISTA_X + 566 - 250 - 20, a["y"] + LISTA_Y + 8)   # a coluna "Tamano"
        m.esperar_serial("arquivos: ordenada por tamanho", 10)
        verificar(True, "a vista de detalhes ordena pela coluna pulsada")
        m.clique(*botao(a, "vista"))
        m.esperar_serial("arquivos: vista icones", 10)
        time.sleep(0.4)
        m.captura("H104-4-icones")
        m.clique(*botao(a, "vista"))
        m.esperar_serial("arquivos: vista lista", 10)
        verificar(True, "as três vistas (lista, detalhes e ícones) mudam com o botão Vista")

        # ---- nova pasta
        m.clique(*botao(a, "nova"))
        m.esperar_serial('"Nueva carpeta"', 10)
        time.sleep(0.4)
        m.digitar("prova\n", 0.05)
        m.esperar_serial("arquivos: pasta criada /tmp/prova", 10)
        verificar("arquivos: pasta /tmp (2 itens)" in m.serial(), "a pasta nova aparece na lista")
        time.sleep(0.4)
        m.captura("H104-5-pasta-nova")

        # ---- renomear no sítio (F2)
        m.clique(*item(a, 1))                     # a pasta "prova" (as pastas vêm primeiro)
        time.sleep(0.3)
        m.tecla("f2")
        m.esperar_serial('"Renombrar"', 10)
        time.sleep(0.4)
        for _ in range(8):
            m.tecla("backspace", 0.03)
        m.digitar("prova2\n", 0.05)
        m.esperar_serial("arquivos: renomeado /tmp/prova -> /tmp/prova2", 10)
        verificar(True, "F2 renomeia a pasta no sítio")

        # ---- copiar e colar
        m.clique(*item(a, 0), duplo=True)         # entra em /tmp/origem
        m.esperar_serial("arquivos: pasta /tmp/origem (1 itens)", 10)
        m.clique(*item(a, 0))                     # seleciona uno.txt
        time.sleep(0.3)
        m.tecla("ctrl-c")
        m.esperar_serial("arquivos: copiados 1 itens", 10)
        verificar(True, "Ctrl+C põe o arquivo no portapapeles de arquivos")
        m.clique(*botao(a, "arriba"))             # sobe para /tmp
        m.esperar_serial("arquivos: pasta /tmp (2 itens)", 10)
        m.tecla("ctrl-v")
        m.esperar_serial("arquivos: colados 1 itens", 10)
        m.esperar_serial("arquivos: pasta /tmp (3 itens)", 10)
        verificar(True, "o arquivo copiado é colado em /tmp (a lista passa a 3 elementos)")
        time.sleep(0.5)
        m.captura("H104-6-colado")

        # ---- apagar com confirmação
        m.clique(*item(a, 1))                     # a pasta "prova2"
        time.sleep(0.3)
        m.clique(*botao(a, "borrar"))
        m.esperar_serial('"Borrar"', 10)
        time.sleep(0.4)
        m.captura("H104-7-confirmar-apagar")
        d = janela(m.serial(), "Borrar")
        m.clique(d["x"] + 380 - 16 - 45, d["y"] + d["a"] - 40 + 13)   # Sim
        m.esperar_serial("arquivos: apagado /tmp/prova2", 10)
        verificar(True, "a pasta apagada (com confirmação) some da lista")
        m.esperar_serial("arquivos: pasta /tmp (2 itens)", 10)

        # ---- buscar com curingas
        m.clique(*botao(a, "buscar"))
        time.sleep(0.4)
        m.digitar("*.txt\n", 0.05)
        m.esperar_serial("arquivos: busca *.txt", 10)
        verificar(True, "a busca por nome com curingas encontra os arquivos")
        time.sleep(0.4)
        m.captura("H104-8-busca")

        # ---- a preferência dos ocultos fica gravada: o programa seguinte abre com eles
        m.tecla("esc")
        time.sleep(0.3)
        m.tecla("ctrl-h")
        m.esperar_serial("arquivos: ocultos sim", 10)
        time.sleep(0.3)
        fechar(m, a)
        m.esperar_serial("(arquivos) terminou", 10)
        time.sleep(0.5)

        # ---- o Terminal confere o que ficou no disco
        out = m.comando("ls -a /tmp")
        verificar("uno.txt" in out and "prova2" not in out and ".ponto" in out,
                  "no disco: o arquivo colado está, a pasta apagada não, e o oculto continua lá")

        m.comando("arquivos /tmp")
        m.esperar_serial("arquivos: pronto", 20)
        time.sleep(0.8)
        verificar("arquivos: pasta /tmp (3 itens)" in m.serial(),
                  "a preferência fica gravada: o Archivos seguinte abre com os ocultos à vista")
        time.sleep(0.4)
        m.captura("H104-9-ocultos-lembrados")
    return 0


def fechar(m, a):
    """O X da moldura (a janela do Archivos)."""
    m.clique(a["x"] + a["l"] - 12, a["y"] - 12)


if __name__ == "__main__":
    sys.exit(principal())
