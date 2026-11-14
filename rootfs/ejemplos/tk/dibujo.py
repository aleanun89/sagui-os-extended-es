# SPDX-License-Identifier: GPL-2.0-or-later
# dibujo.py - dibujar en un Canvas con el ratón.
# Ejecuta con: python /ejemplos/tk/dibujo.py
# Arrastra con el botón izquierdo para dibujar; el derecho abre un menú
# para cambiar el color o el grosor; la rueda cambia el grosor. "Guardar"
# guarda el dibujo en /tmp/dibujo.bmp.
import tkinter as tk

COLORES = ["black", "red", "green", "blue", "orange", "purple"]

raiz = tk.Tk()
raiz.title("Dibujo")
color = tk.StringVar(value="black")
grosor = tk.IntVar(value=3)

barra = tk.Frame(raiz, padx=4, pady=4)
barra.pack(fill="x")
for c in COLORES:
    tk.Radiobutton(barra, text=c, variable=color, value=c, fg=c).pack(side="left")
tk.Scale(barra, from_=1, to=12, orient="horizontal", variable=grosor, label="Grosor", length=110).pack(side="left", padx=6)
tk.Button(barra, text="Borrar", command=lambda: lienzo.delete("trazo")).pack(side="right")

lienzo = tk.Canvas(raiz, width=480, height=320, bg="white", relief="sunken", bd=1)
lienzo.pack(fill="both", expand=True, padx=4, pady=(0, 4))
info = tk.Label(raiz, text="Arrastra para dibujar", anchor="w")
info.pack(fill="x")
ultimo = [None]
trazos = [0]


def pulsar(ev):
    ultimo[0] = (ev.x, ev.y)
    trazos[0] += 1
    lienzo.create_oval(ev.x - 1, ev.y - 1, ev.x + 1, ev.y + 1, fill=color.get(), outline="", tags="trazo")


def mover(ev):
    if ultimo[0]:
        x0, y0 = ultimo[0]
        lienzo.create_line(x0, y0, ev.x, ev.y, fill=color.get(), width=grosor.get(), tags="trazo")
    ultimo[0] = (ev.x, ev.y)
    info.config(text="(%d, %d)  %d objetos" % (ev.x, ev.y, len(lienzo.find_all())))


def soltar(ev):
    ultimo[0] = None
    print("dibujo: trazo %d terminado en (%d, %d) con %s, %d objetos" % (trazos[0], ev.x, ev.y, color.get(),
                                                                        len(lienzo.find_withtag("trazo"))))


menu = tk.Menu(raiz, tearoff=0)
sub = tk.Menu(menu, tearoff=0)
for c in COLORES:
    sub.add_radiobutton(label=c, variable=color, value=c)
menu.add_cascade(label="Color", menu=sub)
menu.add_command(label="Más grueso", command=lambda: grosor.set(min(12, grosor.get() + 2)))
menu.add_command(label="Más fino", command=lambda: grosor.set(max(1, grosor.get() - 2)))
menu.add_separator()
menu.add_command(label="Borrar todo", command=lambda: lienzo.delete("trazo"))

lienzo.bind("<Button-1>", pulsar)
lienzo.bind("<B1-Motion>", mover)
lienzo.bind("<ButtonRelease-1>", soltar)
lienzo.bind("<Button-3>", lambda ev: menu.tk_popup(ev.x_root, ev.y_root))
lienzo.bind("<MouseWheel>", lambda ev: grosor.set(max(1, min(12, grosor.get() + (1 if ev.delta > 0 else -1)))))
raiz.mainloop()
