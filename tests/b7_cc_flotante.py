#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""B7 - float y double en el compilador de C (cc) dentro de SaguiOS (D039).

Criterio: dentro de SaguiOS, cc compila programas con float/double
(aritmética, conversiones, printf %f/%e/%g, math.h, structs con double,
vectores de float, funciones que reciben y devuelven double, varargs) y su
salida es idéntica a la del mismo fuente compilado con GCC en el ordenador
de desarrollo, tanto con SSE2 (por defecto) como con cc -x87; el ejemplo
mandelbrot.c dibuja lo mismo que con GCC; dos programas de coma flotante a
la vez (uno con &) dan los resultados correctos (estado de la FPU por
proceso, D032); un error de tipos con float muestra archivo:línea; y un
programa sin float queda pequeño (sin la biblioteca no usada)."""
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, RAIZ  # noqa: E402

ROOTFS = os.path.join(RAIZ, "rootfs")
# D172: con KVM la CPU acaba 20 M de vueltas (2 s) antes de que el shell reciba el segundo
# comando (no se solapan): 50 veces más.
VUELTAS_ESTRES = 1000000000 if os.environ.get("SAGUI_KVM") == "1" else 20000000


def salida_gcc(fuente, *args):
    """Compila el fuente del rootfs con GCC (en el ordenador de desarrollo)
    y devuelve su salida."""
    with tempfile.TemporaryDirectory() as d:
        exe = os.path.join(d, "prog")
        subprocess.run(["gcc", "-w", "-O0", "-o", exe, os.path.join(ROOTFS, fuente), "-lm"], check=True)
        return subprocess.run([exe, *args], capture_output=True, text=True, check=True).stdout


def salida_programa(out):
    """Las líneas que escribió el programa: sin el eco del comando, los
    avisos del kernel ni el prompt."""
    lineas = out.split("\n")[1:]
    return [l for l in lineas if l and not l.startswith("[SAGUI]") and not l.startswith("sagui:")]


def comparar(m, comando, fuente, nombre, *args):
    out = m.comando(comando, tempo=150)
    nuestra = salida_programa(out)
    esperada = [l for l in salida_gcc(fuente, *args).splitlines() if l]
    if nuestra != esperada:
        for a, b in zip(nuestra + [""] * len(esperada), esperada + [""] * len(nuestra)):
            if a != b:
                print(f"    SaguiOS: {a!r}\n    GCC:     {b!r}")
                break
    verificar(nuestra == esperada, nombre)
    return out


def main():
    with Maquina("b7", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("cd /tmp")

        out = m.comando("cc /testes/cc_flotante.c -o flot", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc compila un programa con float/double (SSE2)")
        comparar(m, "./flot", "testes/cc_flotante.c",
                 "aritmética, conversiones, NaN, printf %f/%e/%g, math.h, structs, vectores y funciones = GCC (SSE2)")
        m.captura("B7-flotante")

        out = m.comando("cc -x87 /testes/cc_flotante.c -o flot87", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc -x87 compila el mismo programa")
        comparar(m, "./flot87", "testes/cc_flotante.c", "con la FPU x87 (cc -x87) la salida también es la de GCC")

        out = m.comando("cc /ejemplos/mandelbrot.c", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc compila /ejemplos/mandelbrot.c")
        comparar(m, "./mandelbrot", "ejemplos/mandelbrot.c", "mandelbrot.c dibuja lo mismo que con GCC")
        m.captura("B7-mandelbrot")

        out = m.comando("cc /ejemplos/flotante.c", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc compila /ejemplos/flotante.c")
        comparar(m, "./flotante", "ejemplos/flotante.c", "flotante.c (pi, raíces, trigonometría, %e/%g) = GCC")

        # Rendimiento (informativo): las iteraciones deben ser las de GCC.
        out = m.comando("cc /ejemplos/rendimiento.c", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc compila /ejemplos/rendimiento.c")
        out = m.comando("./rendimiento 10", tempo=170)
        esperada = {l.split(":")[0]: l.split(",")[0] for l in salida_gcc("ejemplos/rendimiento.c", "10").splitlines()}
        nuestra = salida_programa(out)
        print("    " + "\n    ".join(nuestra))
        verificar(len(nuestra) == 3 and all(l.split(",")[0] == esperada[l.split(":")[0]] for l in nuestra),
                  "rendimiento.c: double, float y punto fijo cuentan las mismas iteraciones que con GCC")

        # Dos programas de coma flotante a la vez: cada uno con su estado
        # de la FPU (XMM0-XMM6 / pila x87) guardado por el kernel.
        out = m.comando("cc /testes/cc_estres.c -o estres", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc compila cc_estres.c")
        out = m.comando("cc -x87 /testes/cc_estres.c -o estres87", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc -x87 compila cc_estres.c")
        n = str(VUELTAS_ESTRES)
        for bg, fg, sem_bg, sem_fg in (("./estres", "./estres87", "1", "2"), ("./estres", "./estres", "3", "4")):
            antes = len(m.serial())
            m.comando(f"{bg} {sem_bg} {n} &")
            m.digitar(f"{fg} {sem_fg} {n}\n", 0.04)
            for sem in (sem_bg, sem_fg):
                m.esperar_serial(f"estres {sem}: ", 170)
            m.esperar_serial(f"({fg[2:]}) terminou", 170)
            serial = m.serial()[antes:].replace("\r", "")
            ok = True
            for sem in (sem_bg, sem_fg):
                esperada = salida_gcc("testes/cc_estres.c", sem, n).strip()
                if esperada + "\n" not in serial:
                    print(f"    falta {esperada!r}")
                    ok = False
            verificar(ok, f"{bg} {sem_bg} & y {fg} {sem_fg} en paralelo dan el resultado de GCC")
            # Se solaparon: el segundo empezó antes de que el primero acabara.
            # Depois do executar do primeiro plano (o sagsh, D037, anuncia o fundo como "[1] pid").
            inicio_bg = serial.find(f"executar /tmp/{bg[2:]}")
            inicio_fg = serial.find(f"executar /tmp/{fg[2:]}", inicio_bg + 1)
            fin_bg = serial.find(f"estres {sem_bg}: ")
            verificar(0 <= inicio_fg < fin_bg, "los dos programas corrieron a la vez")
        m.comando("echo listo")
        # Error de tipos con float.
        m.comando("echo 'int main() { double d = 1; return d % 2; }' > /tmp/malf.c")
        out = m.comando("cc malf.c")
        verificar("malf.c:1: error: operador invalido con float/double" in out, "error con float: archivo:línea y mensaje")
        verificar("(cc) terminou com codigo 1" in out, "cc termina con código 1")

        # Un programa sin float no lleva la biblioteca no usada.
        out = m.comando("cc /ejemplos/hola.c", tempo=120)
        out = m.comando("ls")
        tam = re.search(r"^hola\s+(\d+) bytes", out, re.M)
        print(f"    hola: {tam.group(1) if tam else '?'} bytes")
        verificar(tam and int(tam.group(1)) < 8000, "hola.c ocupa menos de 8 KB (antes ~13 KB)")
        out = m.comando("./hola")
        verificar("Hola, mundo!" in out, "hola sigue funcionando")
    return 0


if __name__ == "__main__":
    sys.exit(main())
