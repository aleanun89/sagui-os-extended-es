# SPDX-License-Identifier: GPL-2.0-or-later
# flotante.py - calculos con coma flotante (double de 64 bits, por SSE2).
# Ejecuta con: python /ejemplos/flotante.py
import math
import time

print("1/3 =", 1 / 3)
print("2**0.5 =", 2 ** 0.5)
print("pi = %.15f, e = %.15f" % (math.pi, math.e))

# pi por series
def leibniz(n):
    s = 0.0
    signo = 1.0
    for k in range(n):
        s += signo / (2 * k + 1)
        signo = -signo
    return 4 * s

def machin():
    return 4 * (4 * math.atan(1 / 5) - math.atan(1 / 239))

t = time.ticks_ms()
p = leibniz(100000)
ms = time.ticks_diff(time.ticks_ms(), t)
print("pi (Leibniz, 100000 terminos) = %.10f  error %.1e  (%d ms)" % (p, abs(p - math.pi), ms))
print("pi (Machin)                   = %.15f" % machin())

# raices: Newton contra math.sqrt
def raiz(x):
    r = x if x > 1 else 1.0
    for _ in range(30):
        r = (r + x / r) / 2
    return r

for x in (2, 10, 12345.678):
    print("raiz(%s): Newton %.12f  math.sqrt %.12f" % (x, raiz(x), math.sqrt(x)))

# trigonometria
print("grados   sin       cos       tan")
for g in range(0, 91, 15):
    r = math.radians(g)
    tan = "inf" if g == 90 else "%9.4f" % math.tan(r)
    print("%5d %9.4f %9.4f %s" % (g, math.sin(r), math.cos(r), tan))
print("atan2(1, -1) = %.6f (3*pi/4)" % math.atan2(1, -1))
print("exp(1) = %.12f, log(10) = %.12f, log10(1000) = %g" % (math.exp(1), math.log(10), math.log10(1000)))
print("floor(-2.5) = %d, ceil(-2.5) = %d, round(3.14159, 2) = %s" % (math.floor(-2.5), math.ceil(-2.5), round(3.14159, 2)))
print("fmod(7.5, 2) = %s, fabs(-3.25) = %s, pow(2, 10) = %s" % (math.fmod(7.5, 2), math.fabs(-3.25), math.pow(2, 10)))

# formato
v = 1234.56789
print("f-string: {:.2f} | {:10.3f} | {:e} | {}".format(v, v, v, v))
print(f"f-string con expresion: {v / 7:.4f}")
print("int(3.99) = %d, float('2.5') * 2 = %s" % (int(3.99), float("2.5") * 2))

# rendimiento: un bucle de multiplicaciones y sumas
t = time.ticks_ms()
x = 0.0
for i in range(100000):
    x = x * 0.999 + 1.5
ms = time.ticks_diff(time.ticks_ms(), t)
print("Bucle de 100000 operaciones float: %d ms (x = %.6f)" % (ms, x))
