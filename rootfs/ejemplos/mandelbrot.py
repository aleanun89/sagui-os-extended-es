# SPDX-License-Identifier: GPL-2.0-or-later
# mandelbrot.py - el conjunto de Mandelbrot en ASCII, con coma flotante.
# Ejecuta con: python /ejemplos/mandelbrot.py
import time

ANCHO = 78
ALTO = 24
MAX_ITER = 60
CARACTERES = " .:-=+*#%@"

def iteraciones(cr, ci):
    zr = zi = 0.0
    for n in range(MAX_ITER):
        zr2 = zr * zr
        zi2 = zi * zi
        if zr2 + zi2 > 4.0:
            return n
        zi = 2.0 * zr * zi + ci
        zr = zr2 - zi2 + cr
    return MAX_ITER

inicio = time.ticks_ms()
for fila in range(ALTO):
    ci = -1.2 + 2.4 * fila / (ALTO - 1)
    linea = ""
    for col in range(ANCHO):
        cr = -2.1 + 2.8 * col / (ANCHO - 1)
        n = iteraciones(cr, ci)
        if n == MAX_ITER:
            linea += "@"
        else:
            linea += CARACTERES[n * (len(CARACTERES) - 1) // MAX_ITER]
    print(linea)
ms = time.ticks_diff(time.ticks_ms(), inicio)
print("Mandelbrot %dx%d, %d iteraciones como maximo: %d ms" % (ANCHO, ALTO, MAX_ITER, ms))
