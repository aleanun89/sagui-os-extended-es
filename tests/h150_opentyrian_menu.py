#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H150 - el OpenTyrian desde Inicio > Juegos y la búsqueda de sus datos (D180).

Criterio:
  - la entrada «OpenTyrian» está en Inicio > Juegos (con su icono) y el menú
    sigue bien (Solitario Spider sigue ahí);
  - con el disco de datos montado en /usb, el juego abre desde el menú y
    encuentra los datos (jogos/opentyrian del disco);
  - sin datos NO falla: sale una ventana propia que explica qué falta y cómo
    conseguirlo, sin excepciones ni faltas de página;
  - con el pendrive de datos conectado pero sin montar, la ventana aparece; se
    monta a mano en OTRA ruta (/datos), se pulsa «Buscar otra vez» y el juego
    encuentra los datos allí y abre su ventana de 640x400.
Capturas en docs/capturas/H150-*.png.
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA  # noqa: E402
from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui, janela  # noqa: E402
import h136_opentyrian as h136  # noqa: E402

h136.PENDRAIVE = os.path.join(SAIDA, "h150-tyrian.img")


def extras_pen(pen):
    return ["-device", "usb-ehci,id=ehci",
            "-drive", "if=none,id=pen,format=raw,file=" + pen,
            "-device", "usb-storage,bus=ehci.0,drive=pen,removable=on,id=dev-pen"]


def terminal(m):
    esperar(m, r"sagui:/\$", 120)
    time.sleep(4)
    h136.focar_terminal(m)


def abrir_pelo_menu(m, capturar=None):
    marca = len(m.serial())
    m.clique(40, 12)
    p = painel(m, marca)
    sub = submenu_pelo_mouse(m, p, "Juegos", "OpenTyrian")
    verificar("OpenTyrian" in sub["linhas"] and "Solitario Spider" in sub["linhas"],
              "Juegos contiene OpenTyrian y Solitario Spider (%s)" % ", ".join(sub["linhas"]))
    time.sleep(0.4)
    if capturar:
        m.captura(capturar)
    marca = len(m.serial())
    clicar_aqui(m)
    esperar(m, r"inicio -> /apps/bin/opentyrian", desde=marca)
    return marca


def main():
    os.makedirs(SAIDA, exist_ok=True)
    pen = h136.preparar_pendrive()
    imagem = imagem_de_teste("", ["--vesa"])
    try:
        # ---- A: datos en /usb, abierto desde el menú
        with Maquina("h150a", "ide", imagem=imagem, extras=extras_pen(pen)) as m:
            esperar(m, r"\[DISCO\] usb0: .*TYRIAN", 60)
            terminal(m)
            desde = len(m.serial())
            m.digitar("montar usb0")
            m.tecla("ret")
            esperar(m, r"\[MONTAGEM\] usb0.*montado em /usb", 40, desde=desde)
            marca = abrir_pelo_menu(m, "H150-1-menu-juegos")
            esperar(m, r"\[OPENTYRIAN\] janela 640x400 criada", 90, desde=marca)
            s = m.serial()[marca:]
            verificar("datos en /usb/jogos/opentyrian" in s, "A: encuentra los datos en /usb/jogos/opentyrian")
            verificar("faltan los datos" not in s, "A: no sale la ventana de ayuda")
            verificar("Fallo de página" not in s, "A: sin faltas de página")

        # ---- B: sin datos
        with Maquina("h150b", "ide", imagem=imagem) as m:
            terminal(m)
            marca = abrir_pelo_menu(m)
            esperar(m, r"\[OPENTYRIAN\] faltan los datos", 60, desde=marca)
            time.sleep(2)
            j = janela(m.serial(), "OpenTyrian: faltan los datos")
            verificar(j["l"] >= 600, "B: sale la ventana explicativa (%dx%d)" % (j["l"], j["a"]))
            m.captura("H150-2-sin-datos")
            # Buscar otra vez sin datos: sigue la ventana y no se cae.
            marca = len(m.serial())
            m.clique(j["x"] + 170 + 70, j["y"] + 24 + 244 + 14)
            esperar(m, r"buscar otra vez: sin datos", 20, desde=marca)
            time.sleep(1)
            m.captura("H150-3-sin-datos-otra-vez")
            s = m.serial()
            verificar("Fallo de página" not in s and "Excepción" not in s, "B: sin excepciones ni faltas de página")
            verificar("janela 640x400 criada" not in s, "B: el juego no arranca sin datos")

        # ---- C: pendrive conectado pero sin montar; se monta en otra ruta
        with Maquina("h150c", "ide", imagem=imagem, extras=extras_pen(pen)) as m:
            esperar(m, r"\[DISCO\] usb0: .*TYRIAN", 60)
            terminal(m)
            m.digitar("sleep 45 && mount usb0 /datos &")
            m.tecla("ret")
            marca = abrir_pelo_menu(m)
            esperar(m, r"\[OPENTYRIAN\] faltan los datos", 60, desde=marca)
            verificar(True, "C: sin montar, sale la ventana de ayuda")
            esperar(m, r"\[MONTAGEM\] usb0.*montado em /datos", 90, desde=marca)
            time.sleep(1)
            j = janela(m.serial(), "OpenTyrian: faltan los datos")
            marca = len(m.serial())
            m.clique(j["x"] + 170 + 70, j["y"] + 24 + 244 + 14)
            esperar(m, r"datos en /datos/jogos/opentyrian", 30, desde=marca)
            esperar(m, r"\[OPENTYRIAN\] janela 640x400 criada", 90, desde=marca)
            verificar(True, "C: con «Buscar otra vez» encuentra los datos en /datos y abre el juego")
            s = m.serial()[marca:]
            verificar("Fallo de página" not in s, "C: sin faltas de página")
    finally:
        if os.path.exists(pen):
            os.remove(pen)
    return 0


if __name__ == "__main__":
    sys.exit(main())
