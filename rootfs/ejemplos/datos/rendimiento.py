# SPDX-License-Identifier: GPL-2.0-or-later
# rendimiento.py - @micropython.native y @micropython.viper en x86 (D479): el mismo bucle, tres veces.
# Ejecuta con: python /ejemplos/datos/rendimiento.py
import time
import micropython

N = 20000000


def suma_python(n):
    s = 0
    i = 0
    while i < n:
        s = (s + i * 3 + 1) & 0xFFFFF
        i += 1
    return s


@micropython.native
def suma_native(n: int):
    s = 0
    i = 0
    while i < n:
        s = (s + i * 3 + 1) & 0xFFFFF
        i += 1
    return s


@micropython.viper
def suma_viper(n: int) -> int:
    s = 0
    i = 0
    while i < n:
        s = (s + i * 3 + 1) & 0xFFFFF
        i += 1
    return s


for nombre, f in (("Python", suma_python), ("native", suma_native), ("viper", suma_viper)):
    t0 = time.time_ns()
    r = f(N)
    print("%-7s resultado %d en %d ms" % (nombre, r, (time.time_ns() - t0) // 1000000))
