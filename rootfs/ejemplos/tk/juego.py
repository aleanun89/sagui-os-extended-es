# SPDX-License-Identifier: GPL-2.0-or-later
# juego.py - "Lluvia de bananas": un pequeño juego en un Canvas.
# Ejecuta con: python /ejemplos/tk/juego.py
# El sagui recoge las bananas que caen; las piedras quitan una vida.
# Muévelo con las flechas o con el ratón. Espacio: pausa. Intro: empezar otra vez.
import random
import tkinter as tk

ANCHO, ALTO = 400, 300
raiz = tk.Tk()
raiz.title("Lluvia de bananas")
raiz.resizable(False, False)
marcador = tk.Label(raiz, text="", anchor="w", padx=6)
marcador.pack(fill="x")
c = tk.Canvas(raiz, width=ANCHO, height=ALTO, bg="#7EC8E3", highlightthickness=0)
c.pack()
c.create_rectangle(0, ALTO - 20, ANCHO, ALTO, fill="#3CDC6E", outline="")
for x in range(20, ANCHO, 90):   # nubes
    c.create_oval(x, 20 + x % 40, x + 60, 45 + x % 40, fill="white", outline="")

estado = {}


def sagui(x):
    """El sagui: cuerpo, cabeza, orejas blancas y la cola."""
    y = ALTO - 22
    partes = [c.create_oval(x - 18, y - 22, x + 18, y, fill="#6E553C", outline="#141210"),
              c.create_oval(x - 11, y - 38, x + 11, y - 16, fill="#AA7864", outline="#141210"),
              c.create_oval(x - 20, y - 40, x - 9, y - 29, fill="#F5F5F0", outline=""),
              c.create_oval(x + 9, y - 40, x + 20, y - 29, fill="#F5F5F0", outline=""),
              c.create_line(x + 16, y - 6, x + 30, y - 18, x + 26, y - 30, fill="#3C2D23", width=3, smooth=1)]
    for p in partes:
        c.addtag_withtag("sagui", p)


def empezar(evento=None):
    c.delete("cosa")
    c.delete("sagui")
    c.delete("aviso")
    estado.update(x=ANCHO // 2, puntos=0, vidas=3, pausa=False, fin=False, ritmo=1400, cosas=[])
    sagui(ANCHO // 2)
    actualizar_marcador()


def actualizar_marcador():
    marcador.config(text="Bananas: %d    Vidas: %d" % (estado["puntos"], estado["vidas"]))


def soltar_cosa():
    if estado["fin"] or estado["pausa"]:
        return
    x = random.randint(20, ANCHO - 20)
    if random.random() < 0.25:
        i = c.create_oval(x - 9, -18, x + 9, 0, fill="#5A564E", outline="#141210", tags=("cosa", "piedra"))
    else:
        i = c.create_arc(x - 14, -22, x + 14, 6, start=200, extent=140, style="chord", fill="#FFD52E",
                         outline="#D9A400", width=2, tags=("cosa", "banana"))
    estado["cosas"].append([i, random.uniform(2.5, 4.5) + estado["puntos"] * 0.08])


def paso():
    if not estado["fin"] and not estado["pausa"]:
        sx = estado["x"]
        for cosa in list(estado["cosas"]):
            i, v = cosa
            c.move(i, 0, v)
            x0, y0, x1, y1 = c.bbox(i)
            if y1 >= ALTO - 60 and abs((x0 + x1) / 2 - sx) < 26 and y0 < ALTO - 40:
                estado["cosas"].remove(cosa)
                piedra = "piedra" in c.gettags(i)
                c.delete(i)
                if piedra:
                    perder()
                else:
                    estado["puntos"] += 1
                    estado["ritmo"] = max(500, estado["ritmo"] - 40)
                    actualizar_marcador()
            elif y0 > ALTO - 20:
                estado["cosas"].remove(cosa)
                if "banana" in c.gettags(i):
                    perder()
                c.delete(i)
    raiz.after(40, paso)


def perder():
    estado["vidas"] -= 1
    actualizar_marcador()
    if estado["vidas"] <= 0:
        estado["fin"] = True
        c.create_text(ANCHO // 2, ALTO // 2, text="¡Fin! %d bananas\nIntro para jugar otra vez" % estado["puntos"],
                      fill="#141210", justify="center", tags="aviso")
        print("juego: fin con %d bananas" % estado["puntos"])


def lluvia():
    soltar_cosa()
    raiz.after(estado["ritmo"], lluvia)


def mover_a(x):
    x = max(20, min(ANCHO - 20, x))
    c.move("sagui", x - estado["x"], 0)
    estado["x"] = x


def tecla(ev):
    if ev.keysym == "Left":
        mover_a(estado["x"] - 18)
    elif ev.keysym == "Right":
        mover_a(estado["x"] + 18)
    elif ev.keysym == "space":
        estado["pausa"] = not estado["pausa"]
    elif ev.keysym == "Return" and estado["fin"]:
        empezar()


raiz.bind("<Key>", tecla)
c.bind("<Motion>", lambda ev: mover_a(ev.x) if not estado["fin"] else None)
c.tag_bind("banana", "<Button-1>", lambda ev: print("juego: ¡eso es una banana!"))
empezar()
paso()
lluvia()
raiz.mainloop()
