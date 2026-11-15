#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H272 - Diálogos comunes de abrir, guardar y elegir carpeta (mejora 103, D272).

Editor: Abrir (vista de lista y de detalles, desplegable «Buscar en», patrón escrito,
entrar y subir con el teclado, abrir un archivo), Guardar como (confirmación al
sobrescribir y extensión según el tipo). Mico: «Abrir carpeta» con el árbol.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402


def janela(serial, titulo):
    m = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24)


def esperar(m, patron, tempo=15, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        r = re.findall(patron, m.serial().replace("\r", "")[desde:])
        if r:
            return r[-1]
        time.sleep(0.3)
    verificar(False, f"esperaba {patron!r}")


def marca(m):
    return len(m.serial().replace("\r", ""))


def borrar(m, n=16):
    for _ in range(n):
        m.tecla("backspace", 0.03)


def main():
    with Maquina("h272", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        for c in ("mkdir /tmp/dlg", "mkdir /tmp/dlg/sub", "echo hola > /tmp/dlg/a.txt",
                  "echo nota > /tmp/dlg/b.md", "echo x > /tmp/dlg/c.png", "echo y > /tmp/dlg/sub/d.txt"):
            m.comando(c)
        m.comando("editor /tmp/dlg/a.txt")
        m.esperar_serial("editor: aberto /tmp/dlg/a.txt", 20)
        time.sleep(0.5)
        ed = janela(m.serial(), "Editor - sin nombre")

        # ---- Abrir: lista, detalles, «Buscar en»
        k = marca(m)
        m.clique(ed["x"] + 136, ed["y"] + 20)               # botón «Abrir»
        n = esperar(m, r"dialogo: (\d+) elementos en /tmp/dlg\n", 15, k)
        verificar(n == "2", f"Abrir: filtro de texto muestra la carpeta y a.txt ({n} elementos)")
        time.sleep(0.5)
        dl = janela(m.serial(), "Abrir")
        m.captura("H272-abrir-lista")
        m.clique(dl["x"] + 472 + 39, dl["y"] + 20)          # «Detalles»
        time.sleep(0.5)
        m.captura("H272-abrir-detalles")
        m.clique(dl["x"] + 84 + 100, dl["y"] + 20)          # desplegable «Buscar en»
        time.sleep(0.5)
        m.captura("H272-abrir-buscar-en")
        m.tecla("esc")
        time.sleep(0.4)

        # ---- patrón escrito, entrar en la carpeta y subir con el teclado
        m.clique(dl["x"] + 160 + 200, dl["y"] + 278 + 12)    # foco al nombre de archivo
        k = marca(m)
        borrar(m)
        m.digitar("*\n", 0.05)
        n = esperar(m, r"dialogo: (\d+) elementos en /tmp/dlg\n", 10, k)
        verificar(n == "4", f"un patrón «*» escrito en el nombre filtra la lista ({n} elementos)")
        k = marca(m)
        m.digitar("sub\n", 0.05)
        n = esperar(m, r"dialogo: (\d+) elementos en /tmp/dlg/sub\n", 10, k)
        verificar(n == "1", f"escribir una carpeta y Intro entra en ella ({n} elementos)")
        time.sleep(0.4)
        m.captura("H272-abrir-subcarpeta")
        k = marca(m)
        m.tecla("tab")                                       # a la lista
        m.tecla("backspace")                                 # sube un nivel
        esperar(m, r"dialogo: (\d+) elementos en /tmp/dlg\n", 10, k)
        verificar(True, "Tab a la lista y Retroceso suben un nivel")
        k = marca(m)
        m.tecla("shift-tab")
        m.digitar("b.md\n", 0.05)
        esperar(m, r"editor: aberto /tmp/dlg/b.md", 15, k)
        verificar(True, "una ruta escrita en «Nombre de archivo» abre el archivo")

        # ---- Guardar como: sobrescribir y extensión
        time.sleep(0.5)
        k = marca(m)
        m.clique(ed["x"] + 272 + 55, ed["y"] + 20)          # «Guardar como»
        esperar(m, r"dialogo: guardar en /tmp/dlg", 15, k)
        time.sleep(0.5)
        m.captura("H272-guardar")
        k = marca(m)
        m.tecla("ret")                                       # b.md ya existe
        esperar(m, r'criada por \d+: "Guardar como" \d+x\d+ em', 10, k)
        time.sleep(0.5)
        m.captura("H272-sobrescribir")
        m.tecla("s")                                         # «Sí» (el predeterminado es «No», como en Windows 95)
        esperar(m, r"editor: salvo /tmp/dlg/b.md", 15, k)
        verificar(True, "Guardar sobre un archivo existente pide confirmación y guarda")
        time.sleep(0.5)
        k = marca(m)
        m.clique(ed["x"] + 272 + 55, ed["y"] + 20)
        esperar(m, r"dialogo: guardar en /tmp/dlg", 15, k)
        time.sleep(0.4)
        borrar(m)
        m.digitar("nuevo\n", 0.05)
        esperar(m, r"editor: salvo /tmp/dlg/nuevo.txt", 15, k)
        verificar(True, "Guardar añade la extensión del tipo elegido (nuevo -> nuevo.txt)")
        # cancelar con Esc no abre nada
        time.sleep(0.5)
        k = marca(m)
        m.clique(ed["x"] + 136, ed["y"] + 20)
        esperar(m, r"dialogo: abrir en", 15, k)
        time.sleep(0.4)
        m.tecla("esc")
        time.sleep(0.8)
        verificar("editor: aberto" not in m.serial().replace("\r", "")[k:], "Esc cancela sin abrir nada")
        # cerrar el editor
        m.clique(ed["x"] + ed["l"] + 4 - 2 - 20 + 8 - 2, ed["y"] - 12)
        m.esperar_serial("editor: fechado", 10)
        time.sleep(0.5)
        out = m.comando("ls /tmp/dlg")
        verificar("nuevo.txt" in out, "ls /tmp/dlg muestra nuevo.txt")

        # ---- Mico: elegir carpeta (árbol)
        k = marca(m)
        m.comando("mico /tmp/dlg/a.txt")
        esperar(m, r"mico: pronto", 40, k)
        time.sleep(1)
        mi = janela(m.serial(), "Mico")
        m.tecla("ctrl-o")                                    # Abrir con los filtros por lenguaje
        esperar(m, r"dialogo: abrir en", 15, k)
        time.sleep(0.5)
        m.captura("H272-mico-abrir")
        m.tecla("esc")
        time.sleep(0.6)
        k = marca(m)
        m.tecla("ctrl-shift-s")                              # Guardar como: tipo del archivo actual
        esperar(m, r"dialogo: guardar en", 15, k)
        time.sleep(0.5)
        m.captura("H272-mico-guardar")
        m.tecla("esc")
        time.sleep(0.6)
        k = marca(m)
        m.clique(mi["x"] + 20, mi["y"] + 12)                 # menú Archivo
        time.sleep(0.6)
        m.tecla("home")
        m.tecla("down")
        m.tecla("down")
        m.tecla("ret")
        esperar(m, r"dialogo: carpeta en", 15, k)
        time.sleep(0.5)
        m.captura("H272-carpeta-arbol")
        k = marca(m)
        m.tecla("down")                                      # baja por el árbol
        m.tecla("right")                                     # despliega
        time.sleep(0.4)
        m.captura("H272-carpeta-desplegada")
        m.tecla("ret")
        esperar(m, r"mico: carpeta (/\S*) \(", 15, k)
        verificar(True, "elegir una carpeta del árbol con el teclado abre esa carpeta en Mico")
    return 0


if __name__ == "__main__":
    sys.exit(main())
