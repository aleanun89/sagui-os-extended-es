#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H80 - optimizaciones del generador común de cc y Pascal (D108).

Criterio: dentro de SaguiOS, cc y pascal compilan un conjunto de programas
(los ejemplos y los de /testes/otim) con -O0 (sin optimizar) y con -O1 (por
omisión: mirilla, inmediatos, comparar y saltar, reducción de fuerza). Las
salidas deben ser idénticas, el programa de enteros de C también coincide con
GCC en el ordenador de desarrollo, cada opción -fno-... deja la misma salida,
y el ejecutable optimizado nunca es mayor. Muestra una tabla de tamaño del
ejecutable y, en los dos programas de cálculo, tiempo: el optimizado no debe
ser más lento."""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROGRAMAS = [
    ("cc", "/ejemplos/hola.c"),
    ("cc", "/ejemplos/primos.c"),
    ("cc", "/ejemplos/mandelbrot.c"),
    ("cc", "/ejemplos/flotante.c"),
    ("cc", "/testes/otim/enteros.c"),
    ("cc", "/testes/otim/bucle.c"),
    ("pascal", "/ejemplos/pascal/hola.pas"),
    ("pascal", "/ejemplos/pascal/primos.pas"),
    ("pascal", "/ejemplos/pascal/fibonacci.pas"),
    ("pascal", "/ejemplos/pascal/ordenar.pas"),
    ("pascal", "/ejemplos/pascal/lista.pas"),
    ("pascal", "/ejemplos/pascal/cadenas.pas"),
    ("pascal", "/testes/pascal/t1.pas"),
    ("pascal", "/testes/pascal/m1.pas"),
    ("pascal", "/testes/pascal/r1.pas"),
    ("pascal", "/testes/otim/enteros.pas"),
    ("pascal", "/testes/otim/bucle.pas"),
]
CALCULO = {"/testes/otim/bucle.c", "/testes/otim/bucle.pas"}
VUELTAS = 3   # el tiempo es el mínimo de varias ejecuciones


def lineas(out):
    """Lo que escribió el programa: sin el eco del comando, avisos ni prompt;
    las líneas 'ms=' (tiempo) no cuentan para la comparación."""
    r = []
    for l in out.replace("\r", "").split("\n")[1:]:
        l = l.rstrip()
        if not l or l.startswith("[SAGUI]") or l.startswith("sagui:") or "terminou com codigo" in l:
            continue
        r.append(l)
    return r


def sin_tiempo(ls):
    return [l for l in ls if not l.startswith("ms=")]


def tiempo(ls):
    for l in ls:
        if l.startswith("ms="):
            return int(l[3:])
    return None


def tamano(m, nombre):
    out = m.comando("ls")
    t = re.search(rf"^{nombre}\s+(\d+) bytes", out, re.M)
    return int(t.group(1)) if t else None


def compilar(m, comp, fuente, opcion, nombre):
    out = m.comando(f"{comp} {opcion} {fuente} -o {nombre}", tempo=300)
    if f"({comp}) terminou com codigo 0" not in out:
        print(out)
        return None
    return tamano(m, nombre)


def main():
    filas = []
    with Maquina("h80", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("cd /tmp")

        for comp, fuente in PROGRAMAS:
            t0 = compilar(m, comp, fuente, "-O0", "p0")
            t1 = compilar(m, comp, fuente, "-O1", "p1")
            verificar(t0 and t1, f"{comp} {fuente} compila con -O0 y con -O1")
            o0 = lineas(m.comando("./p0", tempo=300))
            o1 = lineas(m.comando("./p1", tempo=300))
            verificar(sin_tiempo(o0) == sin_tiempo(o1) and len(sin_tiempo(o0)) > 0,
                      f"{fuente}: la salida con -O1 es idéntica a la de -O0 ({len(sin_tiempo(o0))} líneas)")
            verificar(t1 <= t0, f"{fuente}: el ejecutable optimizado no es mayor ({t0} -> {t1} bytes)")
            ms0 = ms1 = None
            if fuente in CALCULO:
                tm0, tm1 = [tiempo(o0)], [tiempo(o1)]
                for _ in range(VUELTAS - 1):
                    tm0.append(tiempo(lineas(m.comando("./p0", tempo=300))))
                    tm1.append(tiempo(lineas(m.comando("./p1", tempo=300))))
                ms0, ms1 = min(tm0), min(tm1)
            filas.append((fuente, t0, t1, ms0, ms1))
            if fuente == "/testes/otim/enteros.c":
                esperada = subprocess.run(["bash", "-c", f"gcc -w -O0 -o /tmp/h80_gcc {RAIZ}/rootfs{fuente} && /tmp/h80_gcc"],
                                          capture_output=True, text=True).stdout.rstrip("\n").split("\n")
                verificar(sin_tiempo(o1) == esperada, f"{fuente}: la salida de -O1 coincide con la de GCC ({len(esperada)} líneas)")
            if fuente.endswith("/otim/enteros.c") or fuente.endswith("/otim/enteros.pas"):
                for op in ("mirilla", "inmediatos", "saltos", "reduccion"):
                    tf = compilar(m, comp, fuente, f"-fno-{op}", "p2")
                    verificar(tf is not None, f"{comp} -fno-{op} compila")
                    o2 = lineas(m.comando("./p2", tempo=300))
                    verificar(sin_tiempo(o2) == sin_tiempo(o1), f"{fuente} con -fno-{op}: la misma salida")
                    print(f"    -fno-{op}: {tf} bytes (-O0 {t0}, -O1 {t1})")
                    verificar(t1 <= tf <= t0, f"-fno-{op} queda entre -O1 y -O0 en tamaño")

        # Las opciones se aceptan en cualquier orden y -O0 desactiva todo.
        out = m.comando("cc -fno-saltos -O1 /ejemplos/hola.c -o p3", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc acepta -fno-saltos y -O1 juntos")

    print()
    print(f"    {'programa':34} {'-O0':>8} {'-O1':>8} {'tamaño':>8}   tiempo -O0 / -O1 (ms)")
    s0 = s1 = 0
    for fuente, t0, t1, ms0, ms1 in filas:
        s0 += t0
        s1 += t1
        extra = f"{ms0} / {ms1}  ({100 * (ms0 - ms1) / ms0:.0f}% menos)" if ms0 else ""
        print(f"    {fuente:34} {t0:8} {t1:8} {100 * (t1 - t0) / t0:7.1f}%   {extra}")
    print(f"    {'TOTAL':34} {s0:8} {s1:8} {100 * (s1 - s0) / s0:7.1f}%")
    verificar(s1 < s0 * 0.95, f"en total el ejecutable es al menos un 5% menor ({s0} -> {s1})")
    for fuente, t0, t1, ms0, ms1 in filas:
        if ms0:
            verificar(ms1 <= ms0 * 1.15, f"{fuente}: el código optimizado no es más lento ({ms0} -> {ms1} ms)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
