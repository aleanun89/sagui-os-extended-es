#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H140 - hilos de usuario (D167).

Criterio:
  - /testes/hilos pasa todas sus pruebas (crear y esperar hilos, contador
    con mutex exacto, productor/consumidor con semaforos, un hilo que pide
    memoria mientras otros la leen, muchos hilos que ceden el turno, y
    procesos que acaban con hilos vivos sin fugas) con una CPU, con cuatro
    CPU sin --smp y con cuatro CPU y --smp;
  - el calculo repartido en cuatro hilos de un proceso (bench 4 h) gana, con
    cuatro CPU, --smp y KVM, al menos 1,5 veces;
  - el cc del sistema compila /ejemplos/hilos.c (hilos y mutex de su propia
    biblioteca) y el programa da el total exacto con cuatro hilos y --smp;
  - ninguna excepcion del kernel en ningun caso.

SAGUI_KVM=1 usa KVM; sin KVM (TCG) no se exige la ganancia.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 240 if KVM else 600


def correr_hilos(nome, extras, nucleos):
    with Maquina(nome, "ide", imagem=imagem_de_teste("/testes/hilos", extras), nucleos=nucleos) as m:
        m.esperar_serial("RESULTADO hilos:", TEMPO)
        s = m.serial().replace("\r", "")
        for l in s.split("\n"):
            if l.startswith("HILOS ") or "[FALHA]" in l:
                print("    %s" % l.strip())
        verificar("RESULTADO hilos: SUCESSO (0 falhas)" in s, "%s: /testes/hilos paso todas sus pruebas" % nome)
        verificar("EXCECAO" not in s, "%s: sin excepciones del kernel" % nome)
        return s


def main():
    # ---- una CPU ----
    correr_hilos("h140-1", [], 1)

    # ---- cuatro CPU sin --smp: un procesador, como siempre ----
    s = correr_hilos("h140-sem", [], 4)
    verificar("SMP desactivado" in s, "sin --smp el kernel usa un procesador")

    # ---- cuatro CPU con --smp: los hilos corren a la vez en varias CPU ----
    s = correr_hilos("h140-smp", ["--smp"], 4)
    verificar("CPU 3 arrancou" in s, "con --smp arrancan los secundarios")
    n = re.search(r"HILOS filho \d+: (\d+) hilos", s)
    verificar(n is not None and int(n.group(1)) == 5, "el kernel cuenta los cinco hilos del proceso (INFO_PROCESSOS)")

    # ---- la ganancia: el calculo en cuatro hilos con cuatro CPU ----
    with Maquina("h140-bench", "ide", imagem=imagem_de_teste("/testes/bench 4 h", ["--smp"]), nucleos=4) as m:
        m.esperar_serial("bench: ganho", TEMPO)
        s = m.serial().replace("\r", "")
        verificar("EXCECAO" not in s, "sin excepciones con el calculo en cuatro hilos")
        for l in s.split("\n"):
            if "bench:" in l:
                print("    %s" % l.strip())
        g = re.search(r"bench: ganho (\d+)\.(\d+)x", s)
        verificar(g is not None, "el calculo en cuatro hilos dio una ganancia")
        verificar("0 fallos" in s, "los cuatro hilos del calculo acabaron bien")
        ganho = int(g.group(1)) * 100 + int(g.group(2))
        print("    4 CPU, 4 hilos de calculo: ganancia %d.%02dx" % (ganho // 100, ganho % 100))
        if KVM:
            verificar(ganho >= 150, "con KVM, cuatro hilos de calculo van al menos 1,5 veces mas rapido")

    # ---- el ejemplo en C, compilado con el cc del propio sistema ----
    imagem = imagem_de_teste("", ["--vesa", "--console", "--smp"])
    with Maquina("h140-cc", "ide", imagem=imagem, nucleos=4) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("cc /ejemplos/hilos.c -o /tmp/hilos", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc compila /ejemplos/hilos.c")
        out = m.comando("/tmp/hilos 4", tempo=120)
        for l in out.replace("\r", "").split("\n"):
            if "hilo" in l or "total" in l:
                print("    %s" % l.strip())
        # cada hilo suma i % 7 de 0 a 19 999 999: 59 999 997
        verificar("total 239999988 con 4 hilos" in out, "el ejemplo de hilos da el total exacto con cuatro hilos")
        verificar(all(("hilo %d: 59999997" % i) in out for i in range(4)), "cada hilo del ejemplo hace su cuenta")
        verificar("EXCECAO" not in m.serial(), "sin excepciones con el ejemplo compilado por cc")
    return 0


if __name__ == "__main__":
    sys.exit(main())
