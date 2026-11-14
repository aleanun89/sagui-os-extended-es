#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H110 - la hoja de cálculo Macaco (mejora 078, D140).

Criterio:
  - el Macaco abre con la rejilla, la barra de fórmulas y los separadores de
    hoja (capturas);
  - escribir valores y fórmulas: la suma, el promedio, el mínimo, el máximo,
    el contar, el si, el redondear, el absoluto y el producto dan el resultado
    correcto (se lee en la serial, que es lo que el programa escribe);
  - guardar el fichero .mxs y volverlo a cargar sin cambios (con el modo -t
    del programa, que vuelca las celdas en la serial);
  - un gráfico de barras y otro de tarta a partir de un rango;
  - un fichero con dos hojas y una fórmula que se refiera a la otra.

Todo lo que se comprueba es lo que el programa escribe en la serial; la
pantalla sólo queda como imagen.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402

# La geometría de la hoja (la del programa): la rejilla empieza en x=46 (la
# franja de los números de fila) y en y=78 (barra + barra de fórmulas +
# cabecera de columnas); las columnas miden 72 px y las filas 20 px.
GRID_X, GRID_Y = 46, 78
COL_W, ROW_H = 72, 20

# La barra de herramientas: los botones van uno detrás de otro desde x=8, y
# cada uno mide (letras * 8 + 20) de ancho, con 4 px de separación.
BOTOES = ("Nuevo", "Abrir", "Guardar", "Deshacer", "Gráfico", "Opciones")


def janela(serial, titulo):
    """La posición de la ventana del programa (coordenadas del cliente)."""
    achada = re.findall(rf'criada por \d+: "{re.escape(titulo)}[^"]*" (\d+)x(\d+) em \((\d+),(\d+)\)',
                        serial)
    m = achada[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24)


def celula(a, l, c):
    """El centro de la celda (l, c), contando desde cero."""
    return (a["x"] + GRID_X + c * COL_W + COL_W // 2,
            a["y"] + GRID_Y + l * ROW_H + ROW_H // 2)


def botao(a, nome):
    """El centro de un botón de la barra de herramientas."""
    x = 8
    for t in BOTOES:
        larg = len(t) * 8 + 20
        if t == nome:
            return a["x"] + x + larg // 2, a["y"] + 6 + 11
        x += larg + 4
    raise AssertionError(f"no hay botón {nome!r}")


def mais_folha(a):
    """El botón de hoja nueva, en la barra de abajo (a la derecha de las
    pestañas)."""
    x = 4
    for i in range(1):          # sólo está la primera hoja al principio
        x += len("Hoja1") * 8 + 20 + 2
    return a["x"] + x + 12, a["y"] + a["a"] - 11


def escrever(m, texto):
    """Escribe en la celda activa y confirma con Enter."""
    m.digitar(texto, 0.05)
    m.tecla("ret", 0.2)


def escolher_menu(m, texto):
    """Elige una línea del menú contextual con el teclado (el menú abierto
    se maneja con las flechas y el Enter)."""
    m.esperar_serial("menu linhas:", 10)
    linhas = re.findall(r"menu linhas: (.*)", m.serial())[-1].strip()
    itens = linhas.split("|")
    alvo = -1
    for i, t in enumerate(itens):
        if t.lstrip("~").strip() == texto:
            alvo = i
            break
    if alvo < 0:
        raise AssertionError(f"la línea {texto!r} no está en el menú: {itens}")
    # Las flechas sólo se paran en las líneas que se pueden elegir (no en
    # los separadores ni en las desactivadas).
    baixo = 0
    for t in itens[:alvo + 1]:
        if not t.startswith("-") and not t.startswith("~"):
            baixo += 1
    for _ in range(baixo):
        m.tecla("down", 0.06)
    m.tecla("ret", 0.3)


def principal():
    with Maquina("h110", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---- abrir el programa
        m.digitar("macaco\n", 0.05)
        m.esperar_serial("macaco: pronto", 30)
        time.sleep(1.5)
        a = janela(m.serial(), "Macaco")
        verificar(a["l"] >= 700, "la ventana del Macaco tiene su tamaño")
        m.captura("H110-1-hoja")

        # ---- escribir los datos: las etiquetas en la columna A y los
        # valores en la B
        marca = len(m.serial())
        escrever(m, "Norte")
        escrever(m, "Sur")
        escrever(m, "Este")
        for _ in range(3):
            m.tecla("up", 0.1)          # de A4 a A1
        m.tecla("tab", 0.1)             # a B1
        escrever(m, "42")
        escrever(m, "17")
        escrever(m, "25")
        novo = m.serial()[marca:]
        for ref, val in (("A1", "Norte"), ("A2", "Sur"), ("A3", "Este"),
                         ("B1", "42"), ("B2", "17"), ("B3", "25")):
            verificar(f"celula {ref} = {val}" in novo,
                      f"la celda {ref} vale {val}")

        # ---- las fórmulas, en la columna C
        marca = len(m.serial())
        for _ in range(3):
            m.tecla("up", 0.1)          # de B4 a B1
        m.tecla("tab", 0.1)             # a C1
        escrever(m, "=SUM(B1:B3)")
        escrever(m, "=AVERAGE(B1:B3)")
        escrever(m, "=MIN(B1:B3)")
        escrever(m, "=MAX(B1:B3)")
        escrever(m, "=COUNT(B1:B3)")
        escrever(m, "=IF(B1>40,1,0)")
        escrever(m, "=ROUND(B2/3,0)")
        escrever(m, "=ABS(0-42)")
        escrever(m, "=PRODUCT(B1:B3)")
        novo = m.serial()[marca:]
        esperado = (("C1", "84"), ("C2", "28"), ("C3", "17"), ("C4", "42"),
                    ("C5", "3"), ("C6", "1"), ("C7", "6"), ("C8", "42"),
                    ("C9", "17850"))
        for ref, val in esperado:
            verificar(f"celula {ref} = {val} " in novo,
                      f"la fórmula de {ref} da {val}")
        time.sleep(0.4)
        m.captura("H110-2-formulas")

        # ---- el gráfico de barras, con el rango A1:B3
        marca = len(m.serial())
        m.arrastar(*celula(a, 0, 0), *celula(a, 2, 1))
        time.sleep(0.3)
        m.clique(*botao(a, "Gráfico"))
        m.esperar_serial("macaco: gráfico Gráfico de barras con 3 series", 20)
        verificar("macaco: gráfico Gráfico de barras con 3 series" in m.serial()[marca:],
                  "el gráfico de barras sale del rango A1:B3")
        time.sleep(1.0)
        m.captura("H110-3-grafico-barras")

        # ---- la ventana del gráfico se cierra, para poder usar la rejilla
        g = janela(m.serial(), "Macaco - Gráfico")
        m.clique(g["x"] + g["l"] - 12, g["y"] - 12)      # la X de la moldura
        m.esperar_serial("macaco: cerrada la ventana del gráfico", 20)
        time.sleep(0.5)

        # ---- el gráfico de tarta, con el menú contextual
        marca = len(m.serial())
        m.clique(*celula(a, 1, 1), botao=2)
        m.esperar_serial("menu linhas:", 15)
        escolher_menu(m, "Gráfico de tarta")
        m.esperar_serial("macaco: gráfico Gráfico de tarta con 3 series", 20)
        verificar("macaco: gráfico Gráfico de tarta con 3 series" in m.serial()[marca:],
                  "el gráfico de tarta sale del mismo rango")
        time.sleep(1.0)
        m.captura("H110-4-grafico-tarta")

        # ---- se cierra la ventana del gráfico y vuelve a quedar la hoja
        g = janela(m.serial(), "Macaco - Gráfico")
        m.clique(g["x"] + g["l"] - 12, g["y"] - 12)      # la X de la moldura
        m.esperar_serial("macaco: cerrada la ventana del gráfico", 20)
        time.sleep(0.5)

        # ---- guardar el fichero .mxs
        marca = len(m.serial())
        m.tecla("ctrl-s", 0.5)
        m.esperar_serial('"Guardar como"', 15)
        time.sleep(0.5)
        for _ in range(30):             # vacía la caja del diálogo
            m.tecla("backspace", 0.03)
        m.digitar("/tmp/livro.mxs", 0.05)
        m.tecla("ret", 0.5)
        m.esperar_serial("macaco: guardado /tmp/livro.mxs", 20)
        verificar("macaco: guardado /tmp/livro.mxs" in m.serial()[marca:],
                  "el fichero se guarda en /tmp/livro.mxs")
        time.sleep(0.5)
        m.captura("H110-5-guardado")

        # ---- una segunda hoja, con una fórmula que se refiera a la otra
        marca = len(m.serial())
        m.clique(*mais_folha(a))
        m.esperar_serial("macaco: nueva hoja Hoja2", 15)
        escrever(m, "=Hoja1!C1")
        m.esperar_serial("celula A1 = 84 (=Hoja1!C1)", 15)
        verificar("celula A1 = 84 (=Hoja1!C1)" in m.serial()[marca:],
                  "la fórmula de Hoja2 ve el valor de Hoja1")
        marca = len(m.serial())
        m.tecla("ctrl-s", 0.5)
        m.esperar_serial("macaco: guardado /tmp/livro.mxs", 20)
        verificar("macaco: guardado /tmp/livro.mxs" in m.serial()[marca:],
                  "el fichero con dos hojas se vuelve a guardar")
        time.sleep(0.5)
        m.captura("H110-6-dos-hojas")

        # ---- cerrar el programa
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)      # la X de la moldura
        m.esperar_serial("macaco: cerrado", 20)
        time.sleep(0.8)

        # ---- el fichero en el disco
        out = m.comando("cat /tmp/livro.mxs")
        verificar("MXS 1" in out, "el fichero .mxs empieza por la versión")
        verificar('hoja "Hoja1"' in out, "el fichero declara la hoja Hoja1")
        verificar('hoja "Hoja2"' in out, "el fichero declara la hoja Hoja2")
        verificar('celda A1 "Norte"' in out, "el fichero guarda el texto de A1")
        verificar("celda B1 42" in out, "el fichero guarda el número de B1")
        verificar("celda C1 =SUM(B1:B3)" in out, "el fichero guarda la fórmula")
        verificar("celda A1 =Hoja1!C1" in out, "el fichero guarda la fórmula entre hojas")

        # ---- el programa vuelve a cargarlo y recalcula
        out = m.comando("macaco -t /tmp/livro.mxs")
        verificar("celula Hoja1!A1 = Norte" in out, "al cargar, A1 de Hoja1 sigue siendo Norte")
        verificar("celula Hoja1!B1 = 42" in out, "al cargar, B1 de Hoja1 sigue siendo 42")
        verificar("celula Hoja1!C1 = 84" in out, "al cargar, la fórmula de C1 vuelve a dar 84")
        verificar("celula Hoja2!A1 = 84" in out, "al cargar, la fórmula entre hojas vuelve a dar 84")
        verificar("Hoja2" in out, "al cargar, el fichero conserva las dos hojas")
    return 0


if __name__ == "__main__":
    sys.exit(principal())
