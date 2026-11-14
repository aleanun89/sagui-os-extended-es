#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H160 - constructores globales y división de 64 bits en todos los programas (mejora 094, puntos 9 y 10).

  - inicio.asm llama a los constructores (.init_array) antes de principal, una vez y en orden de prioridad;
  - la libsagui da __udivdi3/__umoddi3/__divdi3/__moddi3 (débiles) sin que cada programa lleve su copia.
El programa de prueba es /testes/ctors (userland/testes/ctors.c).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

TEMPO = 120 if os.environ.get("SAGUI_KVM") == "1" else 300


def main():
    with Maquina("h160", "ide", imagem=imagem_de_teste("/testes/ctors", [])) as m:
        m.esperar_serial("RESULTADO ctors:", TEMPO)
        s = m.serial().replace("\r", "")
        for l in s.split("\n"):
            if l.startswith("CTORS "):
                print("    %s" % l.strip())
        verificar("RESULTADO ctors: SUCESSO (0 falhas)" in s, "/testes/ctors pasa todas sus comprobaciones")
        verificar("EXCECAO" not in s, "sin excepciones del kernel")
    return 0


if __name__ == "__main__":
    sys.exit(main())
