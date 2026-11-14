#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H24 - Terminal em janela.

Critério: captura com o terminal em janela rodando `ps` e `ls bin | conta`.
Também: o sistema abre o Terminal sozinho ao ligar, a edição com setas
funciona dentro dele, os comandos dos programas gráficos são reconhecidos
e "sair" fecha a janela.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, BUILD  # noqa: E402

# La lista real de /bin, tal como entra en la imagen (build/rootfs/bin): los programas de
# userland/programas, los de carpeta propia, python y python-x87 (si hay MicroPython) y los
# scripts como /bin/lmk (lite-monkey, D117). Así un programa nuevo no obliga a tocar la prueba.
PROGRAMAS = sorted(f for f in os.listdir(os.path.join(BUILD, "rootfs", "bin")) if not f.startswith("."))   # sin .gitkeep (ls no lo muestra)


def main():
    with Maquina("h24", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar('"Terminal"' in s and "executar /bin/sagsh" in s, "ao ligar, o sistema abre o Terminal com o sagsh dentro")
        time.sleep(1)
        out = m.comando("ps")
        verificar(re.search(r"terminal", out) and "sagsh" in out and "janelas (kernel)" in out,
                  "ps mostra o terminal, o sagsh e a tarefa das janelas")
        out = m.comando("ls bin | conta")
        verificar(f"{len(PROGRAMAS)} líneas" in out, f"ls bin | conta = {len(PROGRAMAS)}")
        m.captura("H24-terminal-ps-conta")
        out = m.comando("echo abc", teclas_finais=("left", "left", "shift-x", "ret"))
        verificar("aXbc" in out, "edição com setas dentro do Terminal em janela")
        out = m.comando("ajuda")
        for c in ("arquivos", "editor", "calculadora", "banana"):
            verificar(c in out, f"ajuda lista o comando gráfico {c}")
        m.comando("limpar")
        out = m.comando("echo depois de limpar")
        verificar("depois de limpar" in out, "limpar funciona na janela")
        m.digitar("sair\n", 0.04)
        m.esperar_serial("(terminal) terminou", 20)
        verificar(True, "sair fecha o shell e o Terminal termina")
    return 0


if __name__ == "__main__":
    sys.exit(main())
