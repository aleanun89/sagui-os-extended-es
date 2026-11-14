#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H154 - pilas de usuario bajo demanda con guarda (D186).

Criterio (/testes/pila, con una CPU y con cuatro CPU y --smp):
  - una recursion de 1 MB y otra de 6 MB de pila funcionan, y el kernel dice
    que el proceso ocupa esa pila (se asigno al tocarla);
  - la recursion infinita termina el proceso con "Desbordamiento de pila" y el
    sistema sigue;
  - un hilo usa 512 KB de pila;
  - un hilo que se desborda muere en su guarda ("Desbordamiento de pila ...
    (hilo N)") despues de usar casi todo su hueco, con el centinela del heap
    intacto;
  - el kernel escribe (read) en una parte de la pila aun sin tocar;
  - 10 procesos pequenos ocupan como mucho 16 KB de pila cada uno y devuelven
    su memoria;
  - cuatro hilos hacen crecer a la vez sus pilas y la misma pagina nueva de la
    pila del principal (con --smp, en varias CPU);
  - ninguna excepcion del kernel.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 240 if KVM else 600


def correr(nome, extras, nucleos):
    with Maquina(nome, "ide", imagem=imagem_de_teste("/testes/pila", extras), nucleos=nucleos) as m:
        m.esperar_serial("RESULTADO pila:", TEMPO)
        s = m.serial().replace("\r", "")
        for l in s.split("\n"):
            if l.startswith("PILA ") and "centinela intacto" not in l or "[FALHA]" in l or "Desbordamiento" in l:
                print("    %s" % l.strip())
        verificar("RESULTADO pila: SUCESSO (0 falhas)" in s, "%s: /testes/pila paso todas sus pruebas" % nome)
        verificar("EXCECAO" not in s, "%s: sin excepciones del kernel" % nome)

        r6 = re.search(r"PILA recursao 6144 KB: usado (\d+) KB, pila del proceso (\d+) KB", s)
        verificar(r6 is not None and int(r6.group(2)) >= 6144,
                  "%s: los 6 MB de la recursion se asignaron al tocarlos (INFO_PILHAS)" % nome)
        verificar(re.search(r"finalizado: Desbordamiento de pila: escritura 0x[0-9a-f]+\n", s) is not None,
                  "%s: la recursion infinita muere con 'Desbordamiento de pila'" % nome)
        verificar(re.search(r"finalizado: Desbordamiento de pila: escritura 0x[0-9a-f]+ \(hilo \d+\)", s) is not None,
                  "%s: el hilo que se desborda muere con 'Desbordamiento de pila' (hilo N)" % nome)
        kb = [int(x) for x in re.findall(r"PILA hilo-desborda: (\d+) KB, centinela intacto", s)]
        verificar(kb and max(kb) >= 800, "%s: el hilo bajo hasta %s KB con el centinela del heap intacto"
                  % (nome, max(kb) if kb else "?"))
        verificar("CENTINELA ROTO" not in s, "%s: el desbordamiento no toco el heap" % nome)
        return s


def main():
    correr("h154-1", [], 1)
    s = correr("h154-smp", ["--smp"], 4)
    verificar("CPU 3 arrancou" in s, "con --smp arrancan los secundarios")
    return 0


if __name__ == "__main__":
    sys.exit(main())
