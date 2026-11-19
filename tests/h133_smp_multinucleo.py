#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H133 - o multinucleo: os processadores secundarios arrancam e trabalham (D157).

Criterio:
  - con una CPU el kernel dice que hay un procesador y el sistema funciona como
    siempre;
  - con cuatro CPU pero SIN la opción de arranque --smp (D164) el kernel usa
    uno solo y lo dice;
  - con --smp (D164) y cuatro CPU arrancan los tres secundarios;
    (el log dice "CPU N arrancou"), el kernel dice nucleos=4 y el infopc
    muestra los cuatro procesadores con su uso;
  - com duas CPUs um processador secundario arranca;
  - o benchmark com quatro processos continua a correr com quatro CPUs e mede
    o ganho: e o teste que diz se os nucleos sao usados de verdade;
  - con muchas llamadas al sistema (bench s, D165) cuatro CPU también ganan:
    cada CPU atiende sus llamadas con el cerrojo general del kernel;
  - con --trava-turnos el cerrojo general va por turnos (D169) y el mismo
    benchmark termina sin excepciones.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def nucleos_do_log(s):
    m = re.search(r"\[SAGUI\] nucleos=(\d+)", s)
    return int(m.group(1)) if m else 0


def main():
    # ---- uma CPU: nada muda ----
    with Maquina("h133-1", "ide", imagem=imagem_de_teste("/testes/bench 4")) as m:
        m.esperar_serial("bench: ganho", 180)
        s = m.serial().replace("\r", "")
        verificar(nucleos_do_log(s) == 1, "com uma CPU o kernel diz nucleos=1")
        verificar("bench: ganho" in s, "o benchmark correu com uma CPU")
        verificar("EXCECAO" not in s, "sem excecoes com uma CPU")
        g1 = re.search(r"bench: ganho (\d+)\.(\d+)x", s)
        if g1:
            print("    1 CPU: ganho %s" % g1.group(0))

    # ---- cuatro CPU sin --smp: uno solo (D164) ----
    with Maquina("h133-sem", "ide", imagem=imagem_de_teste("/testes/bench 4"), nucleos=4) as m:
        m.esperar_serial("bench: ganho", 180)
        s = m.serial().replace("\r", "")
        verificar("SMP desactivado" in s, "sin --smp el kernel dice que el SMP esta desactivado")
        verificar(nucleos_do_log(s) == 1, "sin --smp y con cuatro CPU el kernel dice nucleos=1")
        verificar("EXCECAO" not in s, "sin excepciones sin --smp")

    # ---- quatro CPUs: os tres secundarios arrancam ----
    with Maquina("h133-4", "ide", imagem=imagem_de_teste("/testes/bench 4", ["--smp"]), nucleos=4) as m:
        m.esperar_serial("bench: ganho", 180)
        s = m.serial().replace("\r", "")
        verificar(nucleos_do_log(s) == 4, "com quatro CPUs o kernel diz nucleos=4")
        for i in (1, 2, 3):
            verificar("CPU %d arrancou" % i in s, "o processador secundario %d arrancou" % i)
        verificar("EXCECAO" not in s, "sem excecoes com quatro CPUs")
        verificar("bench: ganho" in s, "o benchmark correu com quatro CPUs")
        m1 = re.search(r"bench: 1 processo: (\d+) ms", s)
        mn = re.search(r"bench: (\d+) processos: (\d+) ms", s)
        g = re.search(r"bench: ganho (\d+)\.(\d+)x", s)
        if m1 and mn:
            print("    4 CPUs: 1 processo %s ms | %s processos %s ms | ganho %s" %
                  (m1.group(1), mn.group(1), mn.group(2), g.group(0) if g else "?"))
        if g:
            ganho = int(g.group(1)) * 10 + int(g.group(2))
            verificar(ganho > 0, "o ganho com quatro CPUs foi medido")

    # ---- cuatro CPU con cálculo puro: el SMP tiene que ganar (D164) ----
    # La carga normal (copiar memoria) mide la caché, que los núcleos
    # comparten, y no escala; el cálculo sí.
    with Maquina("h133-calc", "ide", imagem=imagem_de_teste("/testes/bench 4 c", ["--smp"]), nucleos=4) as m:
        m.esperar_serial("bench: ganho", 300)
        s = m.serial().replace("\r", "")
        verificar("EXCECAO" not in s, "sin excepciones con el calculo en cuatro CPU")
        g = re.search(r"bench: ganho (\d+)\.(\d+)x", s)
        verificar(g is not None, "el calculo en cuatro CPU dio una ganancia")
        ganho = int(g.group(1)) * 100 + int(g.group(2))
        print("    4 CPU, calculo: ganancia %d.%02dx" % (ganho // 100, ganho % 100))
        if os.environ.get("SAGUI_KVM") == "1":
            verificar(ganho >= 150, "con KVM, cuatro procesos de calculo van al menos 1,5 veces mas rapido")

    # ---- cuatro CPU con muchas llamadas al sistema (D165) ----
    # Con el modelo maestro/esclavo (D164) cada llamada volvía a la CPU 0 y un
    # solo proceso tardaba más de 200 veces lo que con una CPU; con el cerrojo
    # general del kernel cada CPU atiende las suyas.
    with Maquina("h133-chamadas", "ide", imagem=imagem_de_teste("/testes/bench 4 s", ["--smp"]), nucleos=4) as m:
        m.esperar_serial("bench: ganho", 300)
        s = m.serial().replace("\r", "")
        verificar("EXCECAO" not in s, "sin excepciones con las llamadas al sistema en cuatro CPU")
        verificar("cerrojo general: sin cola" in s, "bajo un hipervisor el cerrojo general va sin cola (D169)")
        g = re.search(r"bench: ganho (\d+)\.(\d+)x", s)
        verificar(g is not None, "las llamadas al sistema en cuatro CPU dieron una ganancia")
        ganho = int(g.group(1)) * 100 + int(g.group(2))
        print("    4 CPU, llamadas al sistema: ganancia %d.%02dx" % (ganho // 100, ganho % 100))
        if os.environ.get("SAGUI_KVM") == "1":
            verificar(ganho >= 150, "con KVM, cuatro procesos con muchas llamadas al sistema van al menos 1,5 veces mas rapido")

    # ---- el cerrojo general por turnos (D169) ----
    # Bajo un hipervisor (QEMU) el cerrojo se toma sin cola, como en la D165;
    # --trava-turnos lo fuerza por turnos, que es lo que usa una máquina de
    # verdad: tiene que funcionar igual (la ganancia aquí depende de que el
    # anfitrión tenga núcleos libres para las cuatro CPU virtuales).
    with Maquina("h133-turnos", "ide", imagem=imagem_de_teste("/testes/bench 4 s", ["--smp", "--trava-turnos"]), nucleos=4) as m:
        m.esperar_serial("bench: ganho", 300)
        s = m.serial().replace("\r", "")
        verificar("cerrojo general: por turnos" in s, "con --trava-turnos el cerrojo general va por turnos")
        verificar("EXCECAO" not in s, "sin excepciones con el cerrojo por turnos")
        g = re.search(r"bench: ganho (\d+)\.(\d+)x", s)
        verificar(g is not None, "las llamadas al sistema con el cerrojo por turnos dieron una ganancia")
        ganho = int(g.group(1)) * 100 + int(g.group(2))
        print("    4 CPU, llamadas al sistema, cerrojo por turnos: ganancia %d.%02dx" % (ganho // 100, ganho % 100))

    # ---- duas CPUs ----
    with Maquina("h133-2", "ide", imagem=imagem_de_teste("/testes/bench 4", ["--smp"]), nucleos=2) as m:
        m.esperar_serial("bench: ganho", 180)
        s = m.serial().replace("\r", "")
        verificar(nucleos_do_log(s) == 2, "com duas CPUs o kernel diz nucleos=2")
        verificar("CPU 1 arrancou" in s, "o processador secundario arrancou com duas CPUs")
        verificar("EXCECAO" not in s, "sem excecoes com duas CPUs")

    # ---- o infopc mostra os processadores ----
    with Maquina("h133-info", "ide", imagem=imagem_de_teste("/bin/infopc -t sistema", ["--smp"]), nucleos=4) as m:
        m.esperar_serial("Processadores:", 120)
        try:   # las líneas de cada CPU llegan después de «Processadores:» (leer enseguida las cortaba)
            m.esperar_serial("  CPU 3: ", 30)
        except Exception:
            pass
        s = m.serial().replace("\r", "")
        verificar("Processadores:     4" in s, "o infopc mostra os quatro processadores")
        verificar("CPU 0:" in s and "CPU 3:" in s, "o infopc lista cada processador")
        for l in s.split("\n"):
            if "CPU " in l and "arrancada" in l:
                print("    %s" % l.strip())
    return 0


if __name__ == "__main__":
    sys.exit(main())
