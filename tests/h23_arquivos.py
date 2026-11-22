#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H23 - Arquivos (e o fim do critério da H25).

Critério: teste navega até historia/, abre linux.txt no Editor, cria uma
pasta em tmp/ e a apaga. Também: a nota criada no Editor é reaberta pelo
Arquivos e o conteúdo confere (critério da H25).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, raiz_en_disco, IMAGEM  # noqa: E402

TEXTO = "Olá, SaguiOS! Ação e coração."
# Pastas da raiz em ordem (o Arquivos mostra as pastas primeiro): os índices
# saem do rootfs/, para o teste não quebrar quando se acrescenta uma pasta.
_ROOTFS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "rootfs")
# D194: el punto de montaje /apps (SAGUI-APPS) también está en la raíz.
# D255: /sistema (con /sistema/conf, SAGUI-CONF) también aparece en la raíz.
# D371: /root también. D379: con la raíz en disco (ext2) hay además las carpetas que crea mkimagem.py.
_EXTRA = {"apps", "sistema", "root"}
if raiz_en_disco(IMAGEM):
    _EXTRA |= {"home", "tmp", "jogos", "logs", "initrd", "lost+found"}
PASTAS = sorted(set(d for d in os.listdir(_ROOTFS) if os.path.isdir(os.path.join(_ROOTFS, d))) | _EXTRA)
ITENS_RAIZ = len(set(os.listdir(_ROOTFS)) | _EXTRA)


def janela(serial, titulo):
    m = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24, fx=int(m[2]), fy=int(m[3]))


def fechar(m, j):
    m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)


def item(a, i):
    return a["x"] + 146 + 120, a["y"] + 98 + 2 + i * 20 + 8   # a lista começa depois da lateral (H34)


def main():
    with Maquina("h23", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        # Nota criada no Editor (H25), para reabrir pelo Arquivos.
        m.comando("editor")
        m.esperar_serial("editor: pronto", 20)
        time.sleep(0.4)
        m.digitar(TEXTO, 0.05)
        m.tecla("ctrl-s")
        m.esperar_serial('"Guardar como"', 10)
        time.sleep(0.3)
        m.digitar("nota.txt\n", 0.05)
        m.esperar_serial("editor: salvo /tmp/nota.txt", 10)
        fechar(m, janela(m.serial(), "Editor - sin nombre"))
        m.esperar_serial("editor: fechado", 10)
        time.sleep(0.5)

        m.comando("arquivos")
        m.esperar_serial("arquivos: pronto", 20)
        time.sleep(0.5)
        a = janela(m.serial(), "Archivos")
        verificar(f"arquivos: pasta / ({ITENS_RAIZ} itens)" in m.serial(), f"Arquivos abre na raiz: {len(PASTAS)} pastas e {ITENS_RAIZ - len(PASTAS)} arquivos")
        m.captura("H23-arquivos-raiz")

        m.clique(*item(a, PASTAS.index("historia")), duplo=True)   # historia/
        m.esperar_serial("arquivos: pasta /historia (2 itens)", 10)
        verificar(True, "duplo clique na pasta historia entra nela")
        m.clique(*item(a, 0), duplo=True)                         # linux.txt
        m.esperar_serial("editor: aberto /historia/linux.txt", 10)
        verificar(True, "duplo clique em linux.txt abre no Editor")
        time.sleep(0.5)
        m.captura("H23-linux-no-editor")
        fechar(m, janela(m.serial(), "Editor - sin nombre"))
        m.esperar_serial("editor: fechado", 10)
        time.sleep(0.4)

        m.clique(a["x"] + 22, a["y"] + 20)                       # Voltar (botón plano de icono, D423)
        m.esperar_serial(f"arquivos: pasta / ({ITENS_RAIZ} itens)", 10)
        # D379: con la raiz en disco /tmp queda fuera de la vista: se va por la barra de ruta (Ctrl+L)
        m.tecla("ctrl-l")
        time.sleep(0.4)
        m.digitar("/tmp\n", 0.05)
        m.esperar_serial("arquivos: pasta /tmp (1 itens)", 10)
        m.clique(*item(a, 0), duplo=True)                         # nota.txt
        m.esperar_serial("editor: aberto /tmp/nota.txt (29 bytes)", 10)
        verificar(True, "H25: a nota do Editor reabre pelo Arquivos com o mesmo tamanho (29 bytes)")
        time.sleep(0.5)
        png = m.captura("H23-nota-reaberta")
        fechar(m, janela(m.serial(), "Editor - sin nombre"))
        m.esperar_serial("editor: fechado", 10)
        time.sleep(0.4)

        m.clique(a["x"] + 185, a["y"] + 20)                      # Nova pasta (112-258)
        m.esperar_serial('"Nueva carpeta"', 10)
        time.sleep(0.3)
        m.digitar("teste\n", 0.05)
        m.esperar_serial("arquivos: pasta criada /tmp/teste", 10)
        verificar("arquivos: pasta /tmp (2 itens)" in m.serial(), "pasta /tmp/teste criada e aparece na lista")
        m.clique(*item(a, 0))                                      # seleciona teste/
        m.clique(a["x"] + 307, a["y"] + 20)                      # Apagar («Borrar», 262-352)
        m.esperar_serial('"Borrar"', 10)
        time.sleep(0.4)
        m.captura("H23-confirmar-apagar")
        d = janela(m.serial(), "Borrar")
        m.clique(d["x"] + 380 - 16 - 45, d["y"] + d["a"] - 40 + 13)   # Sim
        m.esperar_serial("arquivos: apagado /tmp/teste", 10)
        verificar(m.serial().count("arquivos: pasta /tmp (1 itens)") >= 2, "pasta apagada (com confirmação) e some da lista")
        fechar(m, a)                                               # o foco volta para o Terminal
        m.esperar_serial("(arquivos) terminou", 10)
        time.sleep(0.4)
        out = m.comando("ls /tmp")
        verificar("teste" not in out and "nota.txt" in out, "ls /tmp no Terminal confere")
    return 0


if __name__ == "__main__":
    sys.exit(main())
