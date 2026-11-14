# SPDX-License-Identifier: GPL-2.0-or-later
# hola.py - ejemplo de MicroPython en SaguiOS.
# Ejecuta con: python /ejemplos/hola.py

def fib(n):
    a, b = 0, 1
    for _ in range(n):
        a, b = b, a + b
    return a

print("Hola desde MicroPython en SaguiOS!")
print("Fibonacci:", [fib(i) for i in range(12)])
print("2**100 =", 2 ** 100)

class Sagui:
    def __init__(self, nombre):
        self.nombre = nombre
    def saludar(self):
        return "El sagui " + self.nombre + " come bananas"

print(Sagui("Tito").saludar())
cuenta = {}
for p in "el sagui sube y el sagui baja".split():
    cuenta[p] = cuenta.get(p, 0) + 1
print("Palabras:", sorted(cuenta.items()))
