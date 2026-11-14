# SPDX-License-Identifier: GPL-2.0-or-later
# formulario.py - Checkbutton, Radiobutton, Scale, Spinbox, OptionMenu y messagebox.
# Ejecuta con: python /ejemplos/tk/formulario.py
import tkinter as tk
from tkinter import messagebox

raiz = tk.Tk()
raiz.title("Formulario")
raiz.columnconfigure(1, weight=1)

tk.Label(raiz, text="Nombre:").grid(row=0, column=0, sticky="w", padx=8, pady=4)
nombre = tk.Entry(raiz, width=24)
nombre.grid(row=0, column=1, sticky="ew", padx=8, pady=4)

tk.Label(raiz, text="Edad:").grid(row=1, column=0, sticky="w", padx=8, pady=4)
edad = tk.Spinbox(raiz, from_=1, to=120, width=5)
edad.grid(row=1, column=1, sticky="w", padx=8, pady=4)

grupo = tk.LabelFrame(raiz, text="Lenguaje favorito", padx=8, pady=4)
grupo.grid(row=2, column=0, columnspan=2, sticky="ew", padx=8, pady=4)
lenguaje = tk.StringVar(value="Python")
for i, l in enumerate(("Python", "C", "sagsh")):
    tk.Radiobutton(grupo, text=l, variable=lenguaje, value=l).grid(row=0, column=i, sticky="w", padx=4)

boletin = tk.BooleanVar()
tk.Checkbutton(raiz, text="Recibir el boletín", variable=boletin).grid(row=3, column=0, columnspan=2, sticky="w", padx=8)
terminos = tk.IntVar()
tk.Checkbutton(raiz, text="Acepto las condiciones", variable=terminos).grid(row=4, column=0, columnspan=2, sticky="w", padx=8)

tk.Label(raiz, text="Nivel:").grid(row=5, column=0, sticky="w", padx=8)
nivel = tk.Scale(raiz, from_=0, to=10, orient="horizontal", length=200)
nivel.set(5)
nivel.grid(row=5, column=1, sticky="ew", padx=8)

tk.Label(raiz, text="Sistema:").grid(row=6, column=0, sticky="w", padx=8, pady=4)
sistema = tk.StringVar(value="SaguiOS")
tk.OptionMenu(raiz, sistema, "SaguiOS", "Linux", "Otro").grid(row=6, column=1, sticky="w", padx=8, pady=4)


def enviar():
    if not nombre.get().strip():
        messagebox.showerror("Formulario", "Falta el nombre.")
        return
    if not terminos.get():
        if not messagebox.askyesno("Formulario", "No has aceptado las condiciones.\n¿Aceptarlas ahora?"):
            return
        terminos.set(1)
    resumen = "%s, %s años\nLenguaje: %s\nBoletín: %s\nNivel: %d\nSistema: %s" % (
        nombre.get(), edad.get(), lenguaje.get(), "sí" if boletin.get() else "no", nivel.get(), sistema.get())
    print("formulario:", resumen.replace("\n", "; "))
    messagebox.showinfo("Enviado", resumen)


botones = tk.Frame(raiz, pady=8)
botones.grid(row=7, column=0, columnspan=2)
tk.Button(botones, text="Enviar", width=10, command=enviar).pack(side="left", padx=4)
tk.Button(botones, text="Salir", width=10, command=raiz.destroy).pack(side="left", padx=4)
nombre.focus_set()
raiz.mainloop()
