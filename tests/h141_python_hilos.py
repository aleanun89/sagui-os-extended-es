#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H141 - hilos en el Python del sistema (D170).

Criterio, con una CPU y con cuatro CPU y --smp:
  - /testes/python_hilos.py pasa todas sus pruebas: _thread.start_new_thread
    (con argumentos y kwargs), threading.Thread + join, un contador con Lock
    exacto, un hilo que duerme (time.sleep suelta el GIL) mientras el
    principal calcula y los dos avanzan, dos hilos que solo calculan y los
    dos avanzan (el GIL va y viene), el recolector con cuatro hilos creando
    objetos sin corrupcion, una excepcion en un hilo y 48 hilos en tandas;
  - lo mismo con un heap de 512 KB (el recolector corre muchas veces y en
    cualquiera de los hilos);
  - RLock, Event y current_thread del threading minimo; una recursion sin
    fin en un hilo da RuntimeError y el programa sigue;
  - el programa sale con hilos vivos (dormidos o calculando) sin colgarse y
    con codigo 0;
  - /ejemplos/hilos.py da el total exacto con cuatro hilos;
  - (una CPU) un hilo espera una linea del terminal mientras el principal
    sigue contando;
  - ninguna excepcion del kernel.

SAGUI_KVM=1 usa KVM.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 240 if KVM else 900

PRUEBAS = ["start_new_thread", "Thread+join", "contador con Lock", "sleep mientras otro calcula",
           "dos hilos que calculan avanzan los dos", "recolector con varios hilos creando objetos",
           "una excepcion en un hilo no tumba el programa", "48 hilos en tandas de 8",
           "RLock, Event y current_thread", "recursion sin fin en un hilo"]


def bateria(m, nome, cmd):
    out = m.comando(cmd, tempo=TEMPO)
    for l in out.split("\n"):
        if l.startswith("    ") or "FALLO" in l:
            print("    %s" % l.strip())
    verificar("RESULTADO python_hilos: 0 fallos" in out, "%s: %s pasa todas sus pruebas" % (nome, cmd))
    for p in PRUEBAS:
        verificar(("HILOSPY ok " + p) in out, "%s: %s" % (nome, p))
    verificar("terminou com codigo 0" in out, "%s: python acaba con codigo 0" % nome)


def entrada(m, nome):
    antes = m.serial()
    n = antes.count("sagui:")
    m.digitar("python /testes/python_hilos.py entrada", 0.04)
    m.tecla("ret", 0.04)
    fim = time.time() + TEMPO
    while m.serial()[len(antes):].count("HILOSPY tic") < 3:
        if time.time() > fim:
            raise FalhaTeste("el principal no conto mientras el hilo esperaba la entrada")
        time.sleep(0.2)
    m.digitar("hola\n", 0.04)
    while m.serial().count("sagui:") <= n:
        if time.time() > fim:
            raise FalhaTeste("python no acabo tras leer la linea")
        time.sleep(0.2)
    out = m.serial()[len(antes):].replace("\r", "")
    r = re.search(r"HILOSPY leido: (\S+) tras (\d+) tics", out)
    verificar(r is not None and r.group(1) == "hola" and int(r.group(2)) >= 3,
              "%s: un hilo espera una linea del terminal mientras el principal cuenta" % nome)


def maquina(nome, extras, nucleos, con_entrada):
    imagem = imagem_de_teste("", ["--vesa", "--console"] + extras)
    with Maquina(nome, "ide", imagem=imagem, nucleos=nucleos) as m:
        m.esperar_serial("sagui:/$", 120)
        bateria(m, nome, "python /testes/python_hilos.py")
        bateria(m, nome, "python -X heap=512K /testes/python_hilos.py")
        for modo in ("salir-sleep", "salir-calculo"):
            out = m.comando("python /testes/python_hilos.py " + modo, tempo=TEMPO)
            verificar("HILOSPY saliendo con hilos vivos" in out and "terminou com codigo 0" in out,
                      "%s: sale con hilos vivos (%s) sin colgarse" % (nome, modo))
        out = m.comando("python /ejemplos/hilos.py 4", tempo=TEMPO)
        for l in out.split("\n"):
            if l.startswith("total") or l.startswith("el reloj"):
                print("    %s" % l.strip())
        verificar("total 2399976 con 4 hilos" in out, "%s: /ejemplos/hilos.py da el total exacto" % nome)
        verificar(all(("hilo %d: 599994" % i) in out for i in range(4)), "%s: cada hilo del ejemplo hace su cuenta" % nome)
        verificar("el reloj dio 5 pasos" in out, "%s: el reloj del ejemplo avanza mientras se calcula" % nome)
        if con_entrada:
            entrada(m, nome)
        verificar("EXCECAO" not in m.serial(), "%s: sin excepciones del kernel" % nome)


def main():
    maquina("h141-1", [], 1, True)
    maquina("h141-smp", ["--smp"], 4, False)
    return 0


if __name__ == "__main__":
    sys.exit(main())
