# SPDX-License-Identifier: GPL-2.0-or-later
# calculadora.py - una calculadora con tkinter y grid.
# Ejecuta con: python /ejemplos/tk/calculadora.py
import tkinter as tk

raiz = tk.Tk()
raiz.title("Calculadora tk")
raiz.resizable(False, False)

pantalla = tk.StringVar(value="0")
estado = {"acumulado": None, "operador": None, "nuevo": True}


def escribir(cifra):
    if estado["nuevo"] or pantalla.get() == "0":
        pantalla.set("" if cifra != "." else "0")
        estado["nuevo"] = False
    if cifra == "." and "." in pantalla.get():
        return
    pantalla.set(pantalla.get() + cifra)


def calcular(a, op, b):
    if op == "+":
        return a + b
    if op == "-":
        return a - b
    if op == "×":
        return a * b
    if b == 0:
        raise ZeroDivisionError
    return a / b


def mostrar(x):
    if x == int(x) and abs(x) < 1e15:
        pantalla.set(str(int(x)))
    else:
        pantalla.set("%.10g" % x)


def operar(op):
    valor = float(pantalla.get())
    try:
        if estado["operador"] and not estado["nuevo"]:
            valor = calcular(estado["acumulado"], estado["operador"], valor)
            mostrar(valor)
    except ZeroDivisionError:
        pantalla.set("Error")
        estado.update(acumulado=None, operador=None, nuevo=True)
        return
    estado.update(acumulado=valor, operador=None if op == "=" else op, nuevo=True)
    print("calculadora:", pantalla.get())


def borrar():
    pantalla.set("0")
    estado.update(acumulado=None, operador=None, nuevo=True)


tk.Label(raiz, textvariable=pantalla, anchor="e", bg="white", relief="sunken", bd=2,
         padx=8, pady=6).grid(row=0, column=0, columnspan=4, sticky="ew", padx=6, pady=6)
teclas = [("7", "8", "9", "÷"), ("4", "5", "6", "×"), ("1", "2", "3", "-"), ("0", ".", "=", "+")]
for f, fila in enumerate(teclas, start=1):
    for c, t in enumerate(fila):
        if t in "0123456789.":
            cmd = lambda t=t: escribir(t)
        else:
            cmd = lambda t=t: operar(t)
        tk.Button(raiz, text=t, width=4, command=cmd).grid(row=f, column=c, sticky="nsew", padx=2, pady=2)
tk.Button(raiz, text="C", command=borrar, fg="#D83A30").grid(row=5, column=0, columnspan=4, sticky="ew",
                                                            padx=6, pady=(4, 6))
for c in range(4):
    raiz.columnconfigure(c, weight=1)


def tecla(ev):
    if ev.char and ev.char in "0123456789.":
        escribir(ev.char)
    elif ev.char in ("+", "-"):
        operar(ev.char)
    elif ev.char == "*":
        operar("×")
    elif ev.char == "/":
        operar("÷")
    elif ev.keysym in ("Return", "equal"):
        operar("=")
    elif ev.keysym in ("Escape", "BackSpace"):
        borrar()


raiz.bind("<Key>", tecla)
raiz.mainloop()
