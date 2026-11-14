#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""elf32.py - comprueba que un fichero ELF es de 32 bits (i386).

O kernel e os programas do utilizador de SaguiOS sao ELF de 32 bits. Se usa
depois de ligar para que um compilador que perdesse o -m32 nao passe
despercebido (o erro do gcc e craro, mas um linker que escolhesse o
arquitecto errado nao daria erro nenhum).

Uso:  python3 tools/elf32.py ficheiro [ficheiro...]
      (sem argumentos: diz so o modo de saida)
"""
import os
import sys


def classe_elf(caminho):
    """Devolve '32', '64' o None se nao for um ELF."""
    try:
        with open(caminho, "rb") as f:
            cab = f.read(5)
    except OSError:
        return None
    if len(cab) < 5 or cab[:4] != b"\x7fELF":
        return None
    return {1: "32", 2: "64"}.get(cab[4])


def main():
    if len(sys.argv) < 2:
        print("uso: elf32.py ficheiro [ficheiro...]", file=sys.stderr)
        return 2
    maus = []
    for caminho in sys.argv[1:]:
        c = classe_elf(caminho)
        if c is None:
            print(f"{caminho}: nao e um ELF", file=sys.stderr)
            maus.append(caminho)
        elif c != "32":
            print(f"{caminho}: ELF de {c} bits (devia ser de 32)", file=sys.stderr)
            maus.append(caminho)
    if maus:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
