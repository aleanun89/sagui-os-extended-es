#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H144 - los planificadores seleccionables (D174).

Criterio (con --smp y cuatro CPU, para cada planificador: rr, afinidad,
equilibrio y prioridad):
  - el cambio en caliente (SIS_PLANIFICADOR) funciona y el log lo dice;
  - bench 4 c, bench 4 h y bench 4 s terminan y dan una ganancia;
  - las migraciones de 4 y 6 procesos de cálculo se cuentan;
  - la latencia de despertar a correr de un proceso que duerme 10 ms con
    cuatro procesos calculando se mide;
  - un proceso con máscara de una sola CPU no sale de ella, y una máscara
    sin ninguna CPU que trabaje se rechaza;
  - las órdenes "planificador" y "afinidad" funcionan;
  - sin excepciones.
Además:
  - --planificador NOMBRE en la imagen lo elige al arrancar;
  - sin --smp, los cuatro funcionan con una CPU (cambiados en caliente);
  - con "-smp 4,cores=2,threads=2" el kernel ve dos núcleos con dos hilos, y
    afinidad y equilibrio ponen dos procesos de cálculo en núcleos distintos.
Solo se exige lo robusto (que todo acabe, sin fallos, con las cuentas
coherentes); con KVM, además, que afinidad migre menos que rr y que
prioridad dé menos latencia que rr. Las cifras salen en una tabla.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
PLANIFICADORES = ["rr", "afinidad", "equilibrio", "prioridad"]


def ganancias(s):
    """Las tres ganancias (c, h, s) en el orden en que corrieron."""
    r = {}
    for modo in ("c", "h", "s"):
        m = re.search(r"planif: bench %s\n(.*?)bench: ganho (\d+)\.(\d+)x" % modo, s, re.S)
        r[modo] = int(m.group(2)) * 100 + int(m.group(3)) if m else None
    return r


def correr(nome, teste, extras, nucleos, espera="planif: fin", tempo=400, qemu=None):
    with Maquina(nome, "ide", imagem=imagem_de_teste(teste, extras), nucleos=nucleos, extras=qemu) as m:
        m.esperar_serial(espera, tempo)
        return m.serial().replace("\r", "")


def main():
    tabela = {}
    for p in PLANIFICADORES:
        s = correr("h144-" + p, "/testes/planif %s bmlao" % p, ["--smp"], 4)
        verificar("EXCECAO" not in s, "%s: sin excepciones" % p)
        verificar("planif: cambio en caliente a %s: ok" % p in s, "%s: cambio en caliente" % p)
        verificar("[SAGUI] planificador: %s" % p in s, "%s: el log dice el planificador activo" % p)
        g = ganancias(s)
        for modo in ("c", "h", "s"):
            verificar(g[modo] is not None and g[modo] > 0, "%s: bench 4 %s da una ganancia" % (p, modo))
        mig = {int(n): (int(t), int(mx), int(e)) for n, t, mx, e in
               re.findall(r"planif: migraciones (\d+) procesos: total (\d+) \(max (\d+) por proceso\), ejecuciones (\d+)", s)}
        verificar(4 in mig and 6 in mig and mig[4][2] > 0 and mig[6][2] > 0, "%s: migraciones medidas" % p)
        lat = re.search(r"planif: latencia con 4 calculando: media (\d+) us, max (\d+) us \((\d+) despertares\)", s)
        lat0 = re.search(r"planif: latencia sin carga: media (\d+) us", s)
        verificar(lat is not None and int(lat.group(3)) >= 100, "%s: latencia medida" % p)
        af = re.search(r"planif: afinidad fija CPU (\d+): (\S+) .*mascara imposible (\S+)\)", s)
        verificar(af is not None and af.group(2) == "respetada", "%s: la afinidad fija se respeta" % p)
        verificar(af is not None and af.group(3) == "rechazada", "%s: una máscara imposible se rechaza" % p)
        verificar("planif: ordenes: ok" in s, "%s: las órdenes planificador y afinidad" % p)
        tabela[p] = (g, mig, lat and (int(lat.group(1)), int(lat.group(2))), lat0 and int(lat0.group(1)))

    # ---- la tabla ----
    print("    %-11s %7s %7s %7s %13s %13s %16s %10s" % ("plan.", "4 c", "4 h", "4 s", "migr. 4 proc", "migr. 6 proc",
                                                       "latencia med/max", "sin carga"))
    for p in PLANIFICADORES:
        g, mig, lat, lat0 = tabela[p]
        fg = lambda v: "%d.%02dx" % (v // 100, v % 100) if v else "?"
        print("    %-11s %7s %7s %7s %13s %13s %16s %10s" % (
            p, fg(g["c"]), fg(g["h"]), fg(g["s"]),
            "%d (%d ej.)" % (mig[4][0], mig[4][2]) if 4 in mig else "?",
            "%d (%d ej.)" % (mig[6][0], mig[6][2]) if 6 in mig else "?",
            "%d/%d us" % lat if lat else "?", "%d us" % lat0 if lat0 is not None else "?"))
    if KVM:
        # Lo robusto con KVM: la afinidad migra bastante menos que rr con 6
        # procesos en 4 CPU, y la prioridad responde antes que rr.
        verificar(tabela["afinidad"][1][6][0] * 2 <= tabela["rr"][1][6][0] + 4,
                  "con KVM, afinidad migra al menos la mitad menos que rr (6 procesos)")
        verificar(tabela["prioridad"][2][0] < tabela["rr"][2][0],
                  "con KVM, prioridad da menos latencia media que rr con 4 procesos calculando")

    # ---- elegido al arrancar ----
    s = correr("h144-arranque", "/testes/planif - o", ["--smp", "--planificador", "equilibrio"], 4)
    verificar("[SAGUI] planificador: equilibrio (opcion de arranque)" in s, "--planificador lo elige al arrancar")
    verificar("planif: activo equilibrio" in s, "el programa ve el planificador del arranque")
    verificar("EXCECAO" not in s, "sin excepciones con el planificador del arranque")
    s = correr("h144-defecto", "/testes/planif - o", ["--smp"], 4)
    verificar("[SAGUI] planificador: rr (por defecto)" in s, "sin la opción, rr por defecto")

    # ---- sin --smp: una CPU ----
    s = correr("h144-1cpu", "/testes/planif todos", [], 1)
    verificar("EXCECAO" not in s, "sin --smp: sin excepciones")
    for p in PLANIFICADORES:
        verificar("planif: con %s" % p in s, "sin --smp: %s en caliente" % p)
    verificar(len(re.findall(r"bench: ganho", s)) == 4, "sin --smp: el bench acaba con los cuatro")
    verificar(len(re.findall(r"planif: latencia con 4 calculando", s)) == 4, "sin --smp: la latencia con los cuatro")

    # ---- topología: dos núcleos con dos hilos ----
    # El qemu32 no publica la hoja 0xB (nivel 4) y con KVM lleva el
    # fabricante del anfitrión: sin más, el kernel no puede saber qué CPU son
    # hermanas y cada una es su núcleo. Se sube el nivel a 0xB (Intel y AMD
    # modernos) y, en equilibrio, se prueba la hoja 0x8000001E de AMD (con
    # KVM en un AMD; con TCG el fabricante es Intel y sale por las hojas 1 y 4).
    cpus = {"rr": "qemu32,level=11", "afinidad": "qemu32,level=11",
            "equilibrio": "qemu32,+topoext,xlevel=0x8000001e"}
    for p in ("rr", "afinidad", "equilibrio"):
        s = correr("h144-smt-" + p, "/testes/planif %s t" % p, ["--smp"], "4,cores=2,threads=2",
                   qemu=["-cpu", cpus[p]])
        nuc = re.findall(r"topologia \(([^)]*)\): CPU (\d) nucleo (\d) hilo (\d)", s)
        print("    %s (-cpu %s, %s): %s" % (p, cpus[p], nuc[0][0] if nuc else "?",
                                             "; ".join("CPU %s n%s h%s" % (c, n, h) for _, c, n, h in nuc)))
        verificar(len(nuc) == 4 and len({n for _, _, n, _ in nuc}) == 2 and {h for _, _, _, h in nuc} == {"0", "1"},
                  "%s: el kernel ve dos núcleos con dos hilos" % p)
        t = re.search(r"planif: topologia 2 procesos: (\d+) muestras en nucleos distintos, (\d+) en el mismo", s)
        verificar(t is not None, "%s: muestras de la topología" % p)
        d, m_ = int(t.group(1)), int(t.group(2))
        print("    %s: 2 procesos en nucleos distintos %d de %d muestras" % (p, d, d + m_))
        if p != "rr":
            verificar(d >= 4 * m_ and d > 0, "%s: los dos procesos de cálculo van en núcleos distintos" % p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
