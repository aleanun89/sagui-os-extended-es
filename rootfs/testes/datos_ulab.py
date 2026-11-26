# SPDX-License-Identifier: GPL-3.0-or-later
# datos_ulab.py - pruebas de ulab (el «NumPy» de MicroPython) y de los compiladores
# @micropython.native / @micropython.viper (D479, tests/h479_python_datos.py).
#
#     python /testes/datos_ulab.py
#
# Cada prueba escribe "DATOS ok <nombre>" o "DATOS FALLO <nombre>: ..."; al final
# "RESULTADO datos_ulab: <n> fallos". Las medidas salen como "MEDIDA <nombre> <ms>".
import math
import time
import micropython
from ulab import numpy as np
from ulab import scipy as spy
from ulab import utils as ut

fallos = 0


def ok(nombre, cond, extra=""):
    global fallos
    if cond:
        print("DATOS ok", nombre)
    else:
        fallos += 1
        print("DATOS FALLO %s: %s" % (nombre, extra))


def cerca(a, b, tol=1e-6):
    return abs(a - b) <= tol * max(1.0, abs(b))


# ---------------------------------------------------------------- estadística
a = np.array([2, 4, 4, 4, 5, 5, 7, 9], dtype=np.float)
ok("media", cerca(np.mean(a), 5.0), np.mean(a))
ok("mediana", cerca(np.median(a), 4.5), np.median(a))
ok("desviacion", cerca(np.std(a), 2.0), np.std(a))
ok("suma", cerca(np.sum(a), 40.0))
ok("max-min", np.max(a) == 9 and np.min(a) == 2)
ok("argmax", int(np.argmax(a)) == 7)
ok("ordenar", list(np.sort(np.array([3, 1, 2]))) == [1, 2, 3])
ok("arange-linspace", len(np.arange(10)) == 10 and cerca(np.linspace(0, 1, 11)[5], 0.5))

# ---------------------------------------------------------------- álgebra lineal
m = np.array([[4.0, 3.0], [6.0, 3.0]])
ok("det", cerca(np.linalg.det(m), -6.0), np.linalg.det(m))
inv = np.linalg.inv(m)
ok("inversa", cerca(inv[0, 0], -0.5) and cerca(inv[0, 1], 0.5) and cerca(inv[1, 0], 1.0) and cerca(inv[1, 1], -2.0 / 3.0), inv)
p = np.dot(m, inv)
ok("producto-identidad", cerca(p[0, 0], 1.0) and abs(p[0, 1]) < 1e-9 and abs(p[1, 0]) < 1e-9 and cerca(p[1, 1], 1.0), p)
# sistema 2x+y=5, x+3y=10  ->  x=1, y=3
A = np.array([[2.0, 1.0], [1.0, 3.0]])
sol = np.dot(np.linalg.inv(A), np.array([5.0, 10.0]))
ok("sistema-lineal", cerca(sol[0], 1.0) and cerca(sol[1], 3.0), sol)
w, v = np.linalg.eig(np.array([[2.0, 0.0], [0.0, 3.0]]))
ok("autovalores", sorted([float(w[0]), float(w[1])]) == [2.0, 3.0], w)
ok("traspuesta", np.array([[1, 2, 3], [4, 5, 6]]).transpose().shape == (3, 2))

# ---------------------------------------------------------------- FFT de una señal conocida
N = 64
t = np.arange(N) / N
senal = np.sin(2 * math.pi * 5 * t)               # 5 ciclos en la ventana
z = np.fft.fft(senal)                             # con complejos, devuelve un ndarray complejo
mag = abs(z)
pico = int(np.argmax(mag[:N // 2]))
ok("fft-pico-en-5", pico == 5, pico)
ok("fft-amplitud", cerca(mag[5], N / 2.0, 1e-6), mag[5])
ok("fft-resto-casi-cero", mag[3] < 1e-9 and mag[10] < 1e-9, (mag[3], mag[10]))
back = np.real(np.fft.ifft(z))
ok("fft-ida-y-vuelta", cerca(back[3], senal[3], 1e-6), (back[3], senal[3]))
ok("fft-espectro", len(ut.spectrogram(senal)) == N)

# ---------------------------------------------------------------- polinomios, interpolación, integración
c = np.polyfit(np.array([0.0, 1.0, 2.0, 3.0]), np.array([1.0, 3.0, 5.0, 7.0]), 1)
ok("polyfit", cerca(c[0], 2.0) and cerca(c[1], 1.0), c)
ok("polyval", cerca(np.polyval(c, np.array([10.0]))[0], 21.0))
ok("interp", cerca(np.interp(2.5, np.array([0.0, 5.0]), np.array([0.0, 10.0])), 5.0))
ok("trapz", cerca(np.trapz(np.array([0.0, 1.0, 2.0])), 2.0))
ok("vector-sin", cerca(np.sin(np.array([math.pi / 2]))[0], 1.0))
ok("complejos", cerca(abs(np.array([3 + 4j], dtype=np.complex))[0], 5.0))
ok("comparar-where", list(np.where(np.array([1, 5, 2]) > 1, 1, 0)) == [0, 1, 1])

# ---------------------------------------------------------------- native y viper
@micropython.native
def suma_nativa(n: int):
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


def suma_python(n):
    s = 0
    i = 0
    while i < n:
        s = (s + i * 3 + 1) & 0xFFFFF
        i += 1
    return s


@micropython.viper
def llenar(buf: ptr8, n: int, valor: int):
    i = 0
    while i < n:
        buf[i] = (valor + i) & 255
        i += 1


N = 30000000                  # el bucle de la medida (cuenta con máscara: sin enteros grandes)
ok("native-resultado", suma_nativa(1000) == (1000 * 999 // 2 * 3 + 1000) & 0xFFFFF)
ok("viper-resultado", suma_viper(1000) == (1000 * 999 // 2 * 3 + 1000) & 0xFFFFF)
b = bytearray(16)
llenar(b, 16, 250)
ok("viper-ptr8", list(b) == [(250 + i) & 255 for i in range(16)], list(b))


def medir(f):
    t0 = time.time_ns()
    r = f(N)
    return r, (time.time_ns() - t0) // 1000000


rp, tp = medir(suma_python)
rn, tn = medir(suma_nativa)
rv, tv = medir(suma_viper)
print("MEDIDA python %d" % tp)
print("MEDIDA native %d" % tn)
print("MEDIDA viper %d" % tv)
ok("viper-mismo-valor", rv == rp, (rv, rp))
ok("native-mismo-valor", rn == rp, (rn, rp))
ok("native-mas-rapido", tn * 1.5 < tp, (tn, tp))
ok("viper-mas-rapido", tv * 3 < tp, (tv, tp))

print("RESULTADO datos_ulab: %d fallos" % fallos)
