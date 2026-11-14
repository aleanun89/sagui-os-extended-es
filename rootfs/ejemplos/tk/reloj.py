# SPDX-License-Identifier: GPL-2.0-or-later
# reloj.py - un reloj analógico y digital con after().
# Ejecuta con: python /ejemplos/tk/reloj.py
# El programa duerme entre segundo y segundo: no gasta CPU.
import math
import time
import tkinter as tk

raiz = tk.Tk()
raiz.title("Reloj")
lienzo = tk.Canvas(raiz, width=220, height=220, bg="#F4F4F0", highlightthickness=0)
lienzo.pack(padx=8, pady=8)
digital = tk.Label(raiz, text="--:--:--", padx=10)
digital.pack(pady=(0, 8))
C = 110
lienzo.create_oval(10, 10, 210, 210, fill="white", outline="#1E2328", width=3)
for i in range(60):
    a = math.radians(i * 6)
    r0 = 88 if i % 5 else 80
    lienzo.create_line(C + r0 * math.sin(a), C - r0 * math.cos(a), C + 96 * math.sin(a), C - 96 * math.cos(a),
                       fill="#1E2328", width=1 if i % 5 else 3)
for h in range(1, 13):
    a = math.radians(h * 30)
    lienzo.create_text(C + 68 * math.sin(a), C - 68 * math.cos(a), text=str(h), fill="#0B7A3E")
agujas = {"h": lienzo.create_line(C, C, C, C, width=5, fill="#1E2328"),
          "m": lienzo.create_line(C, C, C, C, width=3, fill="#1E2328"),
          "s": lienzo.create_line(C, C, C, C, width=1, fill="#D83A30")}
lienzo.create_oval(C - 5, C - 5, C + 5, C + 5, fill="#D83A30", outline="")


def aguja(nombre, fraccion, largo):
    a = fraccion * 2 * math.pi
    lienzo.coords(agujas[nombre], C, C, C + largo * math.sin(a), C - largo * math.cos(a))


def tic():
    t = time.localtime()
    h, m, s = t[3], t[4], t[5]
    aguja("h", ((h % 12) + m / 60) / 12, 50)
    aguja("m", (m + s / 60) / 60, 75)
    aguja("s", s / 60, 90)
    digital.config(text="%02d:%02d:%02d" % (h, m, s))
    print("reloj: %02d:%02d:%02d" % (h, m, s))
    raiz.after(1000 - int(time.time() * 1000) % 1000, tic)


tic()
raiz.mainloop()
