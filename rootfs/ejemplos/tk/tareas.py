# SPDX-License-Identifier: GPL-2.0-or-later
# tareas.py - una lista de tareas con Entry, Listbox, Scrollbar y botones.
# Ejecuta con: python /ejemplos/tk/tareas.py
# Escribe una tarea y pulsa Intro (o "Añadir"); selecciona una y pulsa
# "Hecha" para tacharla o "Borrar". La lista se guarda en /tmp/tareas.txt.
# El menú Archivo vacía la lista.
import tkinter as tk
from tkinter import messagebox

ARCHIVO = "/tmp/tareas.txt"

raiz = tk.Tk()
raiz.title("Tareas")

arriba = tk.Frame(raiz, padx=8, pady=8)
arriba.pack(fill="x")
texto = tk.StringVar()
entrada = tk.Entry(arriba, textvariable=texto, width=30)
entrada.pack(side="left", fill="x", expand=True)
tk.Button(arriba, text="Añadir", command=lambda: anadir()).pack(side="left", padx=(6, 0))

medio = tk.Frame(raiz, padx=8)
medio.pack(fill="both", expand=True)
lista = tk.Listbox(medio, height=10, selectmode="browse")
barra = tk.Scrollbar(medio, command=lista.yview)
lista.config(yscrollcommand=barra.set)
barra.pack(side="right", fill="y")
lista.pack(side="left", fill="both", expand=True)

abajo = tk.Frame(raiz, padx=8, pady=8)
abajo.pack(fill="x")
estado = tk.Label(abajo, text="", anchor="w")
estado.pack(side="left", fill="x", expand=True)
tk.Button(abajo, text="Borrar", command=lambda: borrar()).pack(side="right", padx=(6, 0))
tk.Button(abajo, text="Hecha", command=lambda: hecha()).pack(side="right")


def contar():
    n = lista.size()
    hechas = sum(1 for i in range(n) if lista.get(i).startswith("[x]"))
    estado.config(text="%d tareas, %d hechas" % (n, hechas))


def guardar():
    with open(ARCHIVO, "w") as f:
        for t in lista.get(0, "end"):
            f.write(t + "\n")
    contar()


def anadir(evento=None):
    t = texto.get().strip()
    if not t:
        raiz.bell()
        return
    lista.insert("end", "[ ] " + t)
    lista.see("end")
    texto.set("")
    print("tareas: añadida", t)
    guardar()


def seleccion():
    s = lista.curselection()
    if not s:
        messagebox.showwarning("Tareas", "Primero selecciona una tarea.")
        return None
    return s[0]


def hecha():
    i = seleccion()
    if i is None:
        return
    t = lista.get(i)
    t = "[ ]" + t[3:] if t.startswith("[x]") else "[x]" + t[3:]
    lista.delete(i)
    lista.insert(i, t)
    lista.selection_set(i)
    print("tareas: marcada", t)
    guardar()


def borrar():
    i = seleccion()
    if i is None:
        return
    if messagebox.askyesno("Borrar", "¿Borrar «%s»?" % lista.get(i)[4:]):
        print("tareas: borrada", lista.get(i)[4:])
        lista.delete(i)
        guardar()


def vaciar():
    if lista.size() and messagebox.askokcancel("Vaciar", "¿Borrar todas las tareas?"):
        lista.delete(0, "end")
        print("tareas: lista vaciada")
        guardar()


barra_menu = tk.Menu(raiz)
archivo = tk.Menu(barra_menu, tearoff=0)
archivo.add_command(label="Vaciar la lista", command=vaciar)
archivo.add_separator()
archivo.add_command(label="Salir", command=raiz.destroy, accelerator="Ctrl+Q")
barra_menu.add_cascade(label="Archivo", menu=archivo)
ayuda = tk.Menu(barra_menu, tearoff=0)
ayuda.add_command(label="Acerca de", command=lambda: messagebox.showinfo(
    "Tareas", "Lista de tareas hecha con tkinter\nen MicroPython sobre SaguiOS."))
barra_menu.add_cascade(label="Ayuda", menu=ayuda)
raiz.config(menu=barra_menu)
raiz.bind("<Control-q>", lambda e: raiz.destroy())

try:
    for linea in open(ARCHIVO):
        if linea.strip():
            lista.insert("end", linea.rstrip("\n"))
except OSError:
    pass
entrada.bind("<Return>", anadir)
lista.bind("<Double-Button-1>", lambda e: hecha())
entrada.focus_set()
contar()
raiz.mainloop()
