# SPDX-License-Identifier: GPL-2.0-or-later
# hilos.py - la misma cuenta en varios hilos de Python (D170).
#
#     python /ejemplos/hilos.py 4
#
# Es el ejemplo de /ejemplos/hilos.c en Python. El MicroPython tiene un GIL:
# un solo hilo ejecuta Python a la vez, asi que el tiempo con 4 hilos es el
# de la cuenta 4 veces, aun con varias CPU. Los hilos sirven para que uno
# espere (time.sleep, la red) mientras otro trabaja: la segunda parte lo
# muestra.
import sys
import time
import threading

total = 0
cerrojo = threading.Lock()


def cuenta(hasta):
    s = 0
    for i in range(hasta):
        s += i % 7
    return s


def hilo(n):
    global total
    r = cuenta(200000)
    with cerrojo:               # el total es de todos: con el cerrojo
        total += r
        print("hilo %d: %d" % (n, r))


n = int(sys.argv[1]) if len(sys.argv) > 1 else 4
n = max(1, min(n, 8))
t0 = time.ticks_ms()
hilos = [threading.Thread(target=hilo, args=(i,)) for i in range(n)]
for h in hilos:
    h.start()
for h in hilos:
    h.join()
print("total %d con %d hilos en %d ms" % (total, n, time.ticks_diff(time.ticks_ms(), t0)))

# Un hilo espera (time.sleep suelta el GIL) mientras el principal calcula.
pasos = []


def reloj():
    for i in range(5):
        time.sleep_ms(20)
        pasos.append(i)


r = threading.Thread(target=reloj)
r.start()
c = 0
while r.is_alive():
    c += cuenta(1000)
r.join()
print("el reloj dio %d pasos mientras el principal calculaba" % len(pasos))
