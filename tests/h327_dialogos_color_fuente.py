#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H327 (D327) - diálogos comunes de color y fuente y combo_fuentes.

Desde Python (sagui.dialogo tipos 3 y 4, con la ventana real) y desde Pluma:
  color    elegir un básico, definir uno personalizado con RGB (y su HSL), agregarlo, que persista en
           /home/.config/colores.conf y se pueda elegir en otra apertura;
  fuente   filtrar escribiendo, elegir estilo y tamaño, y la tupla devuelta;
  combo    el combo de fuentes de Pluma: abrir, buscar escribiendo y elegir.
Capturas docs/capturas/H327-*.png.
"""
import os
import re
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu  # noqa: E402
from h291_pluma import esperar_desde, janela, lanzar  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def dato(n):
    return open(os.path.join(RAIZ, "tests", "datos", n), "rb").read()


def res(m, desde, clave, t=30):
    fin = time.time() + t
    while time.time() < fin:
        g = re.findall(r"H327-RES %s (.+)" % clave, m.serial()[desde:].replace("\r", ""))
        if g:
            return g[-1].strip()
        time.sleep(0.3)
    verificar(False, "esperaba el resultado %s" % clave)


def ultimo(m, desde, patron):
    g = re.findall(patron, m.serial()[desde:].replace("\r", ""))
    return g[-1] if g else None


def main():
    pen = criar_pendrive("h327-pen", rotulo="DLGCF", arquivos={"c3.py": dato("h327_color.py"), "c4.py": dato("h327_fuente.py")})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h327", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        verificar("Pendrive montado en /usb" in m.comando("montar usb0"), "pendrive montado")
        m.comando("mkdir /home")
        m.comando("mkdir /home/.config")
        color(m)
        fuente(m)
        combo(m)
    print("H327 OK")


def color(m):
    # ---- un básico
    k = len(m.serial())
    m.digitar("python /usb/c3.py\n", 0.04)
    esperar_desde(m, "dialogo: color abierto inicial=000000", k, 60)
    time.sleep(1)
    w = janela(m, "Color", k)
    m.captura("H327-1-color-basico")
    m.clique(w["x"] + 12 + 17, w["y"] + 28 + 22 + 11)       # fila 2, columna 1 = FF0000
    time.sleep(0.5)
    verificar(ultimo(m, k, r"dialogo: color rgb=(\w+)") == "FF0000", "elegir el básico FF0000")
    m.clique(w["x"] + 12 + 66, w["y"] + 276 + 13)           # Aceptar
    verificar(res(m, k, "color") == "16711680", "Python recibe 0xFF0000 = 16711680")
    time.sleep(1)

    # ---- personalizado con RGB
    k = len(m.serial())
    m.digitar("python /usb/c3.py\n", 0.04)
    esperar_desde(m, "dialogo: color abierto", k, 60)
    time.sleep(1)
    w = janela(m, "Color", k)
    m.clique(w["x"] + 12 + 136, w["y"] + 240 + 13)          # Definir colores personalizados >>
    esperar_desde(m, "color definir personalizados", k, 20)
    time.sleep(1)
    for dx, dy, t in ((568, 180, "12"), (568, 208, "34"), (568, 236, "56")):
        m.clique(w["x"] + dx + 20, w["y"] + dy + 12)
        m.digitar(t, 0.08)
        time.sleep(0.3)
    time.sleep(0.5)
    m.captura("H327-2-color-definir")
    ult = ultimo(m, k, r"dialogo: color rgb=(\w+) hsl=([\d,]+)")
    verificar(ult == ("0C2238", "140,155,32"), "RGB 12,34,56 -> 0C2238 con HSL 140,155,32 (%s)" % (ult,))
    m.clique(w["x"] + 312 + 136, w["y"] + 276 + 13)         # Agregar
    esperar_desde(m, "color personalizado 0 = 0C2238", k, 20)
    time.sleep(0.4)
    m.clique(w["x"] + 12 + 66, w["y"] + 276 + 13)           # Aceptar
    verificar(res(m, k, "color") == str(0x0C2238), "Python recibe 0x0C2238")
    time.sleep(1)
    out = m.comando("cat /home/.config/colores.conf")
    verificar("0C2238" in out, "el color personalizado quedó en colores.conf: %r" % out[-200:])

    # ---- persiste: otra apertura lo ofrece
    k = len(m.serial())
    m.digitar("python /usb/c3.py\n", 0.04)
    esperar_desde(m, "dialogo: color abierto", k, 60)
    time.sleep(1)
    w = janela(m, "Color", k)
    m.clique(w["x"] + 12 + 17, w["y"] + 188 + 11)           # personalizado 0
    time.sleep(0.4)
    m.captura("H327-3-color-persistente")
    m.clique(w["x"] + 12 + 66, w["y"] + 276 + 13)
    verificar(res(m, k, "color") == str(0x0C2238), "tras reabrir, el personalizado 0 sigue siendo 0C2238")
    time.sleep(1)


def fuente(m):
    k = len(m.serial())
    m.digitar("python /usb/c4.py\n", 0.04)
    esperar_desde(m, "dialogo: fuentes", k, 60)
    time.sleep(1)
    w = janela(m, "Fuente", k)
    m.captura("H327-4-fuente-inicial")
    for _ in range(14):
        m.tecla("backspace", 0.03)
    m.digitar("mono", 0.1)
    time.sleep(0.6)
    f = ultimo(m, k, r'dialogo: fuente filtro "mono" (\d+) coincidencias')
    verificar(f is not None and int(f) >= 1, "escribir «mono» filtra la lista (%s coincidencias)" % f)
    verificar(ultimo(m, k, r"dialogo: fuente (DejaVu Sans Mono) est=") == "DejaVu Sans Mono", "la primera coincidencia pasa a ser la familia")
    m.captura("H327-5-fuente-filtrada")
    m.clique(w["x"] + 214 + 60, w["y"] + 60 + 2 + 20 + 10)   # segundo estilo
    time.sleep(0.5)
    est = ultimo(m, k, r"dialogo: fuente DejaVu Sans Mono est=(\d+) tam=")
    verificar(est not in (None, "0"), "elegir un estilo cambia est (%s)" % est)
    m.clique(w["x"] + 356 + 30, w["y"] + 28 + 12)           # cuadro de tamaño
    for _ in range(3):
        m.tecla("backspace", 0.03)
    m.digitar("24", 0.1)
    time.sleep(0.6)
    verificar(ultimo(m, k, r"est=\d+ tam=(\d+)") == "24", "escribir 24 en el tamaño")
    m.captura("H327-6-fuente-estilo-tamano")
    m.clique(w["x"] + 230 + 46, w["y"] + 304 + 13)          # Aceptar
    r = res(m, k, "fuente")
    verificar(r.startswith("('DejaVu Sans Mono', %s, 24, 0, 0)" % est), "Python recibe la tupla de la fuente: %s" % r)
    time.sleep(1)


def combo(m):
    pos = lanzar(m, "pluma --clasico")
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    w = janela(m, "Documento1 - Pluma", pos)
    k = len(m.serial())
    m.clique(w["x"] + 100, w["y"] + 51 + 10)
    esperar_desde(m, "dialogo: fuentes", k, 20)
    time.sleep(0.6)
    m.captura("H327-7-combo-fuentes")
    m.digitar("serif", 0.1)
    time.sleep(0.6)
    n = ultimo(m, k, r'dialogo: fuentes filtro "serif" (\d+) coincidencias')
    verificar(n is not None and int(n) >= 1, "el buscador del combo filtra (%s coincidencias)" % n)
    m.captura("H327-8-combo-buscar")
    m.tecla("ret")
    esperar_desde(m, "pluma: fuente aplicada DejaVu Serif", k, 20)
    time.sleep(0.5)
    m.captura("H327-9-combo-aplicada")
    cerrar = janela(m, "Documento1 - Pluma", pos)
    m.clique(cerrar["x"] + cerrar["l"] + 4 - 2 - 20 + 8 - 2, cerrar["y"] - 12)
    time.sleep(1)


if __name__ == "__main__":
    main()
