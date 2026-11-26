# SPDX-License-Identifier: GPL-2.0-or-later
# estadistica.py - ulab (el «NumPy» de MicroPython): estadística, álgebra lineal y FFT (D479).
# Ejecuta con: python /ejemplos/datos/estadistica.py
import math
from ulab import numpy as np
from ulab import utils

datos = np.array([2, 4, 4, 4, 5, 5, 7, 9], dtype=np.float)
print("media", np.mean(datos), "mediana", np.median(datos), "desviación", np.std(datos))

# sistema de ecuaciones  2x + y = 5,  x + 3y = 10
A = np.array([[2.0, 1.0], [1.0, 3.0]])
b = np.array([5.0, 10.0])
x = np.dot(np.linalg.inv(A), b)
print("x =", x[0], " y =", x[1], " det(A) =", np.linalg.det(A))

# la FFT de una señal con 5 ciclos y la mitad de amplitud en 12
N = 64
t = np.arange(N) / N
senal = np.sin(2 * math.pi * 5 * t) + 0.5 * np.sin(2 * math.pi * 12 * t)
mag = abs(np.fft.fft(senal))
for k in range(N // 2):
    if mag[k] > 1:
        print("frecuencia", k, "amplitud", mag[k] * 2 / N)

# ajuste de una recta por mínimos cuadrados
c = np.polyfit(np.array([0.0, 1.0, 2.0, 3.0]), np.array([1.1, 2.9, 5.2, 6.8]), 1)
print("recta: y = %.3f x + %.3f" % (c[0], c[1]))
