#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H146 - benchmark monoNúcleo y multiNúcleo (D176).

Criterio:
  - `benchmark --rapido` con una CPU: las ocho cargas dan la suma de comprobación correcta
    (estado=1, errores=0), puntuaciones > 0 con un núcleo, con procesos y con hilos, y con una CPU
    la escala es ~1 (los "N" trabajadores son uno);
  - con --smp y cuatro CPU (KVM): lo mismo, el kernel dice nucleos=4 y la puntuación multinúcleo
    supera claramente a la de un núcleo en las cargas de cálculo (entero y coma flotante >= 1,5x)
    y en el total, con procesos y con hilos;
  - estabilidad: dos ejecuciones iguales dan una puntuación monoNúcleo total parecida (+-40 %);
  - en el escritorio: Información del sistema > pestaña Rendimiento, "Prueba de un núcleo" y
    "Prueba multinúcleo" dan resultados (barra de progreso, cargas, total, escala y comparación con
    la ejecución anterior), con capturas H146-*.png y sin excepciones del kernel;
  - el historial /home/.benchmark (o /tmp) recibe una línea por ejecución.
Hace falta KVM (SAGUI_KVM=1): con TCG el trabajo fijo tarda minutos.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, cor_do_pixel  # noqa: E402

CARGAS = ["hash", "criba", "mandel", "ncuerpos", "copia", "aleat", "sha256", "llamadas"]


def leer(s):
    """Los resultados del registro: {carga: {estado, mono, procesos, hilos}} y el total."""
    cargas = {}
    for m in re.finditer(r"benchmark: carga (\S+) estado=(\d+) mono=(\d+) var=([\d.]+) procesos=(\d+) hilos=(\d+)", s):
        cargas[m.group(1)] = dict(estado=int(m.group(2)), mono=int(m.group(3)), var=float(m.group(4)),
                                  procesos=int(m.group(5)), hilos=int(m.group(6)))
    t = re.findall(r"benchmark: total mono=(\d+) procesos=(\d+) hilos=(\d+) escala=(\d+)\.(\d+)x eficiencia=(\d+)% "
                   r"nucleos=(\d+) errores=(\d+)", s)
    total = None
    if t:
        g = t[-1]
        total = dict(mono=int(g[0]), procesos=int(g[1]), hilos=int(g[2]), escala=int(g[3]) + int(g[4]) / 100,
                     efi=int(g[5]), nucleos=int(g[6]), errores=int(g[7]))
    return cargas, total


def correr(nombre, nucleos, smp, args="--rapido"):
    extras = ["--smp"] if smp else []
    with Maquina(nombre, "ide", imagem=imagem_de_teste("/bin/benchmark " + args, extras), nucleos=nucleos) as m:
        m.esperar_serial("benchmark: total", 170)
        time.sleep(1.0)
        s = m.serial().replace("\r", "")
    verificar("EXCECAO" not in s, f"{nombre}: sin excepciones del kernel")
    return leer(s)


def comprobar_basico(nombre, cargas, total, nucleos):
    verificar(total is not None, f"{nombre}: el programa escribe la línea final")
    verificar(total["errores"] == 0, f"{nombre}: ninguna carga con error de comprobación")
    verificar(total["nucleos"] == nucleos, f"{nombre}: el programa ve {nucleos} núcleo(s)")
    for c in CARGAS:
        v = cargas.get(c)
        verificar(v is not None and v["estado"] == 1 and v["mono"] > 0 and v["procesos"] > 0 and v["hilos"] > 0,
                  f"{nombre}: la carga {c} da resultado correcto y puntos > 0 ({v})")
    verificar(total["mono"] > 0 and total["procesos"] > 0 and total["hilos"] > 0, f"{nombre}: totales > 0 ({total})")


def main():
    if os.environ.get("SAGUI_KVM") != "1":
        print("  SKIP: h146 necesita KVM (SAGUI_KVM=1); con TCG el trabajo fijo tarda minutos")
        return 0

    # ---- una CPU
    (c1, t1) = correr("h146-1", 1, False)
    comprobar_basico("1 CPU", c1, t1, 1)
    verificar(0.5 <= t1["escala"] <= 1.6, f"1 CPU: la escala es ~1 ({t1['escala']})")
    print("    1 CPU: mono %d, procesos %d, hilos %d, escala %.2f" % (t1["mono"], t1["procesos"], t1["hilos"], t1["escala"]))

    # ---- cuatro CPU con --smp, dos veces
    (c4, t4) = correr("h146-4a", 4, True)
    comprobar_basico("4 CPU", c4, t4, 4)
    for c in ("hash", "criba", "mandel", "ncuerpos"):
        for var in ("procesos", "hilos"):
            r = c4[c][var] / max(1, c4[c]["mono"])
            verificar(r >= 1.5, f"4 CPU: {c} con {var} escala {r:.2f}x (>= 1,5x)")
    verificar(t4["procesos"] >= 1.5 * t4["mono"] and t4["hilos"] >= 1.5 * t4["mono"],
              f"4 CPU: el total multinúcleo supera 1,5x al monoNúcleo ({t4['mono']} -> {t4['procesos']} / {t4['hilos']})")
    verificar(t4["procesos"] > 1.5 * t1["procesos"] and t4["hilos"] > 1.5 * t1["hilos"],
              "4 CPU: el multinúcleo con 4 CPU supera claramente al de 1 CPU")
    verificar(t4["escala"] >= 1.5, f"4 CPU: factor de escala {t4['escala']:.2f}x, eficiencia {t4['efi']}%")
    print("    4 CPU: mono %d, procesos %d, hilos %d, escala %.2f, eficiencia %d%%" %
          (t4["mono"], t4["procesos"], t4["hilos"], t4["escala"], t4["efi"]))
    for c in CARGAS:
        print("      %-9s mono %5d  procesos %5d  hilos %5d  (+-%.1f%%)" % (c, c4[c]["mono"], c4[c]["procesos"], c4[c]["hilos"], c4[c]["var"]))

    (c4b, t4b) = correr("h146-4b", 4, True)
    comprobar_basico("4 CPU (2)", c4b, t4b, 4)
    for clave in ("mono", "procesos", "hilos"):
        a, b = t4[clave], t4b[clave]
        d = abs(a - b) / max(a, b)
        verificar(d <= 0.40, f"estabilidad: total {clave} {a} vs {b} (variación {d * 100:.0f}%, máximo 40%)")
    d = abs(t1["mono"] - t4["mono"]) / max(t1["mono"], t4["mono"])
    verificar(d <= 0.40, f"estabilidad: monoNúcleo con 1 y con 4 CPU {t1['mono']} vs {t4['mono']} ({d * 100:.0f}%)")

    # ---- escritorio: Información del sistema > Rendimiento
    with Maquina("h146-esc", "ide", imagem=imagem_de_teste("", ["--vesa", "--smp"]), nucleos=4) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.digitar("infopc\n", 0.04)
        g = None
        for _ in range(100):
            r = re.findall(r'criada por \d+: "Información del PC" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())
            if r:
                g = r[-1]
                break
            time.sleep(0.3)
        verificar(g is not None, "infopc abre su ventana en el escritorio")
        l, a, fx, fy = map(int, g)
        x0, y0 = fx + 2, fy + 24
        time.sleep(1.0)
        # la pestaña Rendimiento (la octava): con el ratón
        m.clique(x0 + 10 + 7 * 98 + 46, y0 + 22)
        marca = len(m.serial())
        for _ in range(50):
            if "infopc: secao rendimiento" in m.serial()[marca - 4000 if marca > 4000 else 0:]:
                break
            time.sleep(0.2)
        verificar("infopc: secao rendimiento" in m.serial(), "la pestaña Rendimiento se abre")
        time.sleep(0.5)
        m.captura("H146-1-pestana-vacia")
        # Prueba de un núcleo: el botón, en la franja de abajo
        marca = len(m.serial())
        m.clique(x0 + 10 + 95, y0 + (560 - 42) + 6 + 14)
        t0 = time.time()
        visto = False
        while time.time() - t0 < 25:
            if re.search(r"infopc: rendimiento inicio un nucleo", m.serial()[marca:]):
                visto = True
                break
            time.sleep(0.2)
        verificar(visto, "el botón «Prueba de un núcleo» lanza la medida")
        time.sleep(3.0)
        m.captura("H146-2-midiendo-un-nucleo")
        patron = r"infopc: rendimiento total mono (\d+) procesos (\d+) hilos (\d+) escala (\d+) eficiencia (\d+) errores (\d+)"
        for _ in range(300):
            if re.search(patron, m.serial()[marca:]):
                break
            time.sleep(0.5)
        r = re.findall(patron, m.serial()[marca:])
        verificar(r and int(r[-1][0]) > 0 and int(r[-1][5]) == 0, f"la prueba de un núcleo da puntos y ningún error ({r[-1:] })")
        time.sleep(1.0)
        png = m.captura("H146-3-un-nucleo")
        # Prueba multinúcleo
        marca = len(m.serial())
        m.clique(x0 + 10 + 200 + 95, y0 + (560 - 42) + 6 + 14)
        for _ in range(60):
            if re.search(r"infopc: rendimiento inicio multinucleo", m.serial()[marca:]):
                break
            time.sleep(0.2)
        time.sleep(4.0)
        m.captura("H146-4-midiendo-multinucleo")
        for _ in range(400):
            if re.search(patron, m.serial()[marca:]):
                break
            time.sleep(0.5)
        r = re.findall(patron, m.serial()[marca:])
        verificar(r, "la prueba multinúcleo termina")
        mono, mp, mh, esc, efi, err = map(int, r[-1])
        verificar(mono > 0 and mp > 0 and mh > 0 and err == 0, f"multinúcleo: puntos de los tres modos y sin errores ({r[-1]})")
        verificar(esc >= 150, f"multinúcleo en el escritorio: escala {esc / 100:.2f}x (>= 1,5x)")
        time.sleep(1.0)
        m.captura("H146-5-multinucleo")
        verificar(cor_do_pixel(png, x0 + 5, y0 + 5) is not None, "la captura se hizo")
        # otra prueba de un núcleo con el teclado (U): la ventana la compara con la anterior guardada
        marca = len(m.serial())
        m.tecla("u", 0.3)
        for _ in range(300):
            if re.search(patron, m.serial()[marca:]):
                break
            time.sleep(0.5)
        r = re.findall(patron, m.serial()[marca:])
        verificar(r and int(r[-1][0]) > 0 and int(r[-1][5]) == 0, "la segunda prueba de un núcleo (tecla U) termina bien")
        ant = re.findall(r"infopc: rendimiento anterior (\S+ \S+) mono (\d+)", m.serial()[marca:])
        verificar(ant and int(ant[-1][1]) > 0, f"la ventana compara con la ejecución anterior guardada ({ant[-1:]})")
        time.sleep(1.0)
        m.captura("H146-6-comparacion")
        # el historial: una línea por ejecución (tres)
        verificar(len(re.findall(r"benchmark: historial guardado en /\S+", m.serial())) >= 3, "cada ejecución guarda su línea en el historial")
        s = m.serial()
        verificar("EXCECAO" not in s, "el escritorio: sin excepciones del kernel")
    return 0


if __name__ == "__main__":
    sys.exit(main())
