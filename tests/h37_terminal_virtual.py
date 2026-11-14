#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H37 - terminal virtual no Terminal em janela (D036).

Critério: dentro do Terminal em janela, um programa que lê no modo linha
mostra o que se digita (eco), com Backspace funcionando: o REPL do python,
o input() e o cat sem arquivo (que termina com Ctrl+D). Ctrl+C encerra o
programa em primeiro plano (código 130) e o shell continua; Ctrl+C no
próprio shell só descarta a linha.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402

TEM_PYTHON = True   # o MicroPython é opcional (make MICROPYTHON=0)


def main():
    global TEM_PYTHON
    with Maquina("h37", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        TEM_PYTHON = "python" in m.comando("ls /bin")

        # cat sem arquivo: o eco aparece enquanto se digita; Ctrl+D termina.
        n = m.serial().count("sagui:")
        m.digitar("cat\n", 0.04)
        m.esperar_serial("executar /bin/cat", 10)
        m.digitar("hola mundoo", 0.04)
        m.tecla("backspace")
        m.tecla("ret")
        m.tecla("ctrl-d")
        m.esperar_serial("(cat) terminou com codigo 0", 10)
        s = m.serial().replace("\r", "")
        verificar("hola mundoo\b\nhola mundo\n" in s, "cat: eco do que se digita, Backspace e a linha devolvida")
        verificar(s.count("sagui:") > n, "Ctrl+D numa linha vazia termina o cat e o shell volta")

        # Ctrl+C encerra o programa em primeiro plano.
        m.digitar("contador 60\n", 0.04)
        m.esperar_serial("executar /bin/contador", 10)
        m.tecla("ctrl-c")
        m.esperar_serial("(contador) terminou com codigo 130", 10)
        verificar("^C" in m.serial(), "Ctrl+C encerra o programa em primeiro plano (código 130)")
        out = m.comando("echo sigo vivo")
        verificar("sigo vivo" in out, "depois do Ctrl+C o shell continua")

        # Um programa em segundo plano escrevendo com o shell esperando um
        # comando: o shell redesenha o prompt e continua vivo (antes, a leitura
        # de 1 byte recebia 0 = "fim do arquivo" e o sagsh terminava).
        m.comando("contador 2 &")
        m.esperar_serial("(contador) terminou", 15)
        out = m.comando("echo sigo aqui")
        verificar("sigo aqui" in out and "(sagsh) terminou" not in m.serial(),
                  "texto em segundo plano não derruba o shell")

        # Ctrl+C no shell: só descarta a linha.
        out = m.comando("echo nada", teclas_finais=("ctrl-c", "ret"))
        verificar("nada" not in out.split("^C")[-1], "Ctrl+C no shell descarta a linha")

        if TEM_PYTHON:
            m.digitar("python\n", 0.04)
            m.esperar_serial(">>> ", 20)
            m.digitar("print(6*7)\n", 0.04)
            m.esperar_serial("42", 10)
            verificar(">>> print(6*7)" in m.serial(), "python: o REPL mostra o que se digita")
            m.digitar("nome = input('Nombre: ')\n", 0.04)
            m.esperar_serial("Nombre: ", 10)
            m.digitar("Tito\n", 0.04)
            m.digitar("print('Hola', nome)\n", 0.04)
            m.esperar_serial("Hola Tito", 10)
            verificar("Nombre: Tito" in m.serial(), "python: input() com eco")
            m.captura("H37-python-no-terminal")
            m.tecla("ctrl-d")
            m.esperar_serial("(python) terminou", 10)
            verificar(True, "Ctrl+D sai do python")
    return 0


if __name__ == "__main__":
    sys.exit(main())
