#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H163 - con varias CPU, todas tienen la misma PAT que el principal (D198).

El framebuffer se mapea en write-combining por la entrada 1 de la PAT (D057). Cada
procesador tiene su propia PAT: si solo la programa el principal, los secundarios
escriben en la pantalla sin combinar (en el Amilo T2400, el escritorio con --smp
iba lentísimo). QEMU no aplica tipos de memoria, pero sí guarda el MSR de cada
CPU: la prueba comprueba que cada secundario anuncia la misma PAT que el principal.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def main():
    with Maquina("h163", "ide", imagem=imagem_de_teste("/testes/ctors", ["--smp"]), nucleos=4) as m:
        m.esperar_serial("RESULTADO ctors:", 240)   # un programa cualquiera: ya arrancaron las CPU
        s = m.serial().replace("\r", "")
        principal = re.search(r"\[VIDEO\] PAT [0-9a-f]+:[0-9a-f]+ -> ([0-9a-f]+:[0-9a-f]+)", s)
        verificar(principal is not None, "el principal programa la PAT con la entrada WC")
        secundarios = re.findall(r"\[VIDEO\] CPU (\d+): PAT ([0-9a-f]+:[0-9a-f]+)", s)
        print("    principal:", principal.group(1) if principal else None, " secundarios:", secundarios)
        verificar(len(secundarios) == 3, "las tres CPU secundarias anuncian su PAT (%d)" % len(secundarios))
        verificar(principal is not None and all(p == principal.group(1) for _, p in secundarios),
                  "todas las CPU tienen la misma PAT que el principal")
        verificar("EXCECAO" not in s, "sin excepciones")
    return 0


if __name__ == "__main__":
    sys.exit(main())
