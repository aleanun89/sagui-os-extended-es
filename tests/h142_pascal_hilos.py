#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H142 - hilos en el Pascal del sistema (D171).

Criterio, con una CPU, con cuatro CPU sin --smp y con cuatro CPU y --smp:
  - `pascal` compila /testes/pascal/hilos1.pas (uses Hilos): crear y esperar
    hilos (con el codigo de SalirHilo), contador con mutex exacto,
    productor/consumidor con semaforo, New/Dispose desde varios hilos y
    WriteLn desde varios hilos (las lineas no se mezclan);
  - `pascal` compila /ejemplos/hilos.pas y con cuatro hilos da el total
    exacto (cada hilo suma i mod 7 de 0 a 19 999 999: 59 999 997);
  - ninguna excepcion del kernel.

SAGUI_KVM=1 usa KVM (si no, TCG: mas lento).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 240 if KVM else 900


def caso(nome, extras, nucleos):
    imagem = imagem_de_teste("", ["--vesa", "--console"] + extras)
    with Maquina(nome, "ide", imagem=imagem, nucleos=nucleos) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("pascal /testes/pascal/hilos1.pas -o /tmp/hilos1", tempo=TEMPO)
        verificar("terminou com codigo 0" in out, "%s: pascal compila hilos1.pas" % nome)
        out = m.comando("/tmp/hilos1", tempo=TEMPO).replace("\r", "")
        for l in out.split("\n"):
            if l.startswith("OK") or l.startswith("FALLA") or l.startswith("RESULTADO"):
                print("    %s" % l.strip())
        for p in ("OK crear y esperar", "OK contador 80000", "OK semaforo 2001000", "OK memoria"):
            verificar(p in out, "%s: %s" % (nome, p))
        verificar("RESULTADO hilos pascal: 0 fallos" in out, "%s: hilos1 sin fallos" % nome)
        lineas = [l.strip() for l in out.split("\n") if l.startswith("LINEA ")]
        buenas = [l for l in lineas if re.fullmatch(r"LINEA hilo \d vuelta \d fin", l)]
        verificar(len(lineas) == 12 and len(buenas) == 12, "%s: las 12 lineas de los hilos salen enteras" % nome)

        out = m.comando("pascal /ejemplos/hilos.pas -o /tmp/hilos", tempo=TEMPO)
        verificar("terminou com codigo 0" in out, "%s: pascal compila /ejemplos/hilos.pas" % nome)
        out = m.comando("/tmp/hilos 4", tempo=TEMPO).replace("\r", "")
        for l in out.split("\n"):
            if "hilo" in l or "total" in l:
                print("    %s" % l.strip())
        verificar("total 239999988 con 4 hilos" in out, "%s: el ejemplo da el total exacto" % nome)
        verificar(all(("hilo %d: 59999997" % i) in out for i in range(4)), "%s: cada hilo hace su cuenta" % nome)
        verificar("EXCECAO" not in m.serial(), "%s: sin excepciones del kernel" % nome)


def main():
    caso("h142-1", [], 1)
    caso("h142-sem", [], 4)
    caso("h142-smp", ["--smp"], 4)
    return 0


if __name__ == "__main__":
    sys.exit(main())
