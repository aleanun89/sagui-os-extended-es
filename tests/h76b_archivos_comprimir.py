#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H76b - Archivos: «Comprimir en .zip», «Comprimir en .tar.gz» y «Extraer» (mejora 056, D103).

Critério: com uma pasta /tmp/c1 (com um arquivo), o botão direito sobre ela no Archivos
cria c1.zip e c1.tar.gz (os comandos zip e tar rodam na pasta atual do Archivos); sobre
c1.zip, «Extraer en carpeta...» cria a pasta ex1; os códigos de saída chegam ao log
e a lista é atualizada. Captura do menu.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402
from h55_menu_contextual import janela, esperar, abrir_menu, linha, item_arquivos  # noqa: E402


def main():
    with Maquina("h76b", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("mkdir /tmp/c1")
        m.comando("echo hola mundo > /tmp/c1/a.txt")
        marca = len(m.serial())
        m.digitar("arquivos /tmp\n", 0.04)
        esperar(m, r"arquivos: pasta /tmp \(1 itens\)", 30, desde=marca)
        time.sleep(0.8)
        a = janela(m.serial(), "Archivos")

        marca = len(m.serial())
        menu = abrir_menu(m, *item_arquivos(a, 0))
        esperar(m, r"arquivos: menu do item c1", desde=marca)
        verificar(menu[4] == 10, "o menu de um item tem as 10 linhas (com Comprimir e Extraer)")
        time.sleep(0.3)
        m.captura("H76-menu-archivos-comprimir")
        marca = len(m.serial())
        m.clique(*linha(menu, 6, 2))
        esperar(m, r"arquivos: comprimir c1 -> c1.zip \(codigo 0\)", 60, desde=marca)
        esperar(m, r"arquivos: pasta /tmp \(2 itens\)", desde=marca)
        verificar(True, "«Comprimir en .zip» cria c1.zip (código 0) e a lista passa a 2 itens")
        time.sleep(0.5)

        marca = len(m.serial())
        menu = abrir_menu(m, *item_arquivos(a, 0))
        m.clique(*linha(menu, 7, 2))
        esperar(m, r"arquivos: comprimir c1 -> c1.tar.gz \(codigo 0\)", 60, desde=marca)
        esperar(m, r"arquivos: pasta /tmp \(3 itens\)", desde=marca)
        verificar(True, "«Comprimir en .tar.gz» cria c1.tar.gz (código 0) e a lista passa a 3 itens")
        time.sleep(0.5)
        m.captura("H76-archivos-comprimidos")

        # c1 (pasta), c1.tar.gz, c1.zip
        marca = len(m.serial())
        menu = abrir_menu(m, *item_arquivos(a, 2))
        esperar(m, r"arquivos: menu do item c1.zip", desde=marca)
        m.clique(*linha(menu, 9, 2))
        m.esperar_serial('"Extraer en carpeta"', 10)
        time.sleep(0.4)
        for _ in range(4):
            m.tecla("backspace", 0.04)
        m.digitar("ex1\n", 0.05)
        esperar(m, r"arquivos: extrair c1.zip em ex1 \(codigo 0\)", 60, desde=marca)
        esperar(m, r"arquivos: pasta /tmp \(4 itens\)", desde=marca)
        verificar(True, "«Extraer en carpeta...» extrai c1.zip para ex1 (código 0) e a lista passa a 4 itens")
        time.sleep(0.4)
        m.captura("H76-archivos-extraido")
    print("H76b ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
