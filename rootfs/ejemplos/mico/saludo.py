# SPDX-License-Identifier: GPL-2.0-or-later
# saludo.py - F5 lo ejecuta con python
def saludar(nombre, veces=2):
    """Saluda varias veces."""
    for i in range(veces):
        print("Hola,", nombre, i + 1)

saludar("sagui")
print(len("Mico") * 10)
