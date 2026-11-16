#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Configuración cabe entera a 800x600 y sigue bien a 1024x768 (issue #43).

Arranca con --resolucao 800x600 y con 1024x768, abre la ventana, comprueba que cabe en la pantalla y
captura la pestaña Mandos (la penúltima; la última es Usuarios, D340) y Pantalla > Aceleración gráfica (las filas largas de botones).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

BORDA, TITULO, LARG, Y_PAG = 2, 24, 788, 50
TITULOS = ("Teclado", "Fecha y hora", "Pantalla", "Sonido", "Escritorio", "Capturas", "IA", "Fuentes", "Red",
           "Escritorio remoto", "Mandos", "Usuarios")


def pestana(nombre):
    """Centro de la pestaña, con el mismo reparto que traducir_rotulos() de configuracion.c."""
    suma = sum(len(t) * 8 for t in TITULOS)
    relleno = 10
    while relleno > 1 and 10 + suma + len(TITULOS) * (relleno + 1) > LARG - 4:
        relleno -= 1
    x = 4 if 10 + suma + len(TITULOS) * (relleno + 1) > LARG - 4 else 10   # D340: con Usuarios
    for t in TITULOS:
        l = len(t) * 8 + relleno
        if t == nombre:
            return (x + l // 2, 24)
        x += l + 1


def esperar(m, patron, tempo, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-1500:])


def probar(res):
    L, A = (int(v) for v in res.split("x"))
    img = imagem_de_teste("", ["--vesa", "--resolucao", res])
    with Maquina("hconf" + res, "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        n = len(m.serial())
        m.comando("configuracion --ventana &", 20)
        esperar(m, r'"Configuraci.n"', 30, n)
        time.sleep(1.5)
        g = re.findall(r'janela (\d+) criada por \d+: "Configuraci.n" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)', m.serial()[n:])
        verificar(g, "ventana creada")
        l, a, x, y = (int(v) for v in g[-1][1:])
        verificar(l == LARG, f"la ventana mide {LARG} de ancho ({l})")
        verificar(x >= 0 and x + l + 2 * BORDA <= L, f"cabe a lo ancho en {res} (x={x}, l={l})")
        verificar(y >= 0 and y + a + TITULO + BORDA <= A, f"cabe a lo alto en {res} (y={y}, a={a})")
        ox, oy = x + BORDA, y + TITULO
        px, py = pestana("Mandos")
        m.clique(ox + px, oy + py)
        time.sleep(1)
        m.captura(f"H-conf-800-{res}-mandos")
        px, py = pestana("Pantalla")
        m.clique(ox + px, oy + py)
        time.sleep(1)
        m.clique(ox + 592 + 95, oy + Y_PAG + 2 + 13)
        time.sleep(1.5)
        m.captura(f"H-conf-800-{res}-aceleracion")


def main():
    for res in ("800x600", "1024x768"):
        probar(res)
    print("H-CONF-800: OK")


if __name__ == "__main__":
    main()
