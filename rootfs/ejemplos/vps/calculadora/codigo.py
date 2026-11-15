# SPDX-License-Identifier: GPL-2.0-or-later
# Calculadora sencilla: dos números, una operación y el botón Calcular.

def Boton1_click():
    try:
        a = float(CuadroTexto1.texto)
        b = float(CuadroTexto2.texto)
    except ValueError:
        msgbox("Escribe dos números.", "Calculadora", icono="aviso")
        return
    op = Combo1.texto
    if op == "+":
        r = a + b
    elif op == "-":
        r = a - b
    elif op == "*":
        r = a * b
    else:
        r = a / b
    Etiqueta4.texto = "Resultado: %g" % r
