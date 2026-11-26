#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H482 (D482) - cursores del ratón: tres temas y tres tamaños, desde Configuración (clics) y por orden.

Se comprueba: la orden `configuracion cursor`, la página «Cursor del ratón» con clics reales, el tamaño real de la flecha
en pantalla (caja de los píxeles que cambian con y sin el cursor: 12x18 por la escala), el color de cada tema y que
tema y tamaño siguen tras reiniciar (sagui.conf en /home persistente). Capturas docs/capturas/H482-*.png."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, cor_do_pixel, SAIDA  # noqa: E402
import conf_arbol as ca  # noqa: E402
import estilos_png as pnglib  # noqa: E402

P = (700, 520)      # donde se pone el cursor para medirlo (escritorio libre)
LEJOS = (60, 700)


def cargar(png):
    """(ancho, alto, bytes RGB) de una captura (lector PNG propio: aquí `convert` no decodifica)."""
    return pnglib.leer(png)


def caja(a, b, x0, y0, w, h):
    """Caja (ancho, alto) de los píxeles que difieren entre dos capturas dentro de la zona dada."""
    W, _, da = cargar(a)
    db = cargar(b)[2]
    xs, ys = [], []
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            o = (y * W + x) * 3
            if da[o:o + 3] != db[o:o + 3]:
                xs.append(x); ys.append(y)
    if not xs:
        return (0, 0, None)
    return (max(xs) - min(xs) + 1, max(ys) - min(ys) + 1, (min(xs), min(ys)))


def medir(m, nombre, esperado_escala):
    m.mouse_para(*LEJOS)
    time.sleep(0.6)
    sin = m.captura("H482-sin-" + nombre, guardar=False)
    m.mouse_para(*P)
    time.sleep(0.8)
    con = m.captura("H482-" + nombre)
    w, h, ori = caja(con, sin, P[0] - 4, P[1] - 4, 80, 80)
    verificar((w, h) == (12 * esperado_escala, 18 * esperado_escala),
              f"{nombre}: la flecha mide {w}x{h} px (esperado {12 * esperado_escala}x{18 * esperado_escala})")
    verificar(ori is not None and abs(ori[0] - P[0]) <= 1 and abs(ori[1] - P[1]) <= 1, f"{nombre}: la punta sigue en el puntero {ori}")
    return con


def main():
    img = imagem_de_teste("h482", ["--vesa", "--resolucao", "1024x768", "--dados-mb", "16"])
    with Maquina("h482", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 240)
        time.sleep(3)
        out = m.comando("configuracion cursor mostrar")
        verificar("tema clasico tamano normal (12x19)" in out, "de serie: clásico y normal")
        out = m.comando("configuracion cursor oscuro grande")
        verificar("tema oscuro tamano grande (24x38)" in out, "la orden aplica oscuro y grande")
        out = m.comando("configuracion cursor rojo")
        verificar("configuracion: cursor [" in out, "un tema desconocido da error")
        # Configuración abierta en su página: clics reales
        n0 = len(m.serial())
        m.comando("configuracion --ventana cursores &", 20)
        fim = time.time() + 40
        while time.time() < fim and "cursores opciones" not in m.serial()[n0:]:
            time.sleep(0.3)
        g = re.findall(r"cursores opciones x=(\d+) tema_y=(\d+) tam_y=(\d+)", m.serial()[n0:])
        if not g: print(m.serial()[n0:][-1500:])
        verificar(bool(g), "la página «Cursor del ratón» se abre")
        xl, ty, zy = (int(v) for v in g[-1])
        v = ca.ventana(m, n0)
        time.sleep(1.5)
        m.captura("H482-pagina-oscuro-grande")
        # Mover la ventana fuera del camino no hace falta: la flecha se mide en el escritorio libre (P)
        m.clique(*ca.panel(v, xl + 20, ty + 2 * 24 + 6))      # tema moderno
        time.sleep(0.8)
        verificar("tema 2 tamano 1" in m.serial(), "clic en «Moderno»: tema 2, tamaño sin cambiar")
        m.clique(*ca.panel(v, xl + 20, zy + 2 * 24 + 6))      # muy grande
        time.sleep(0.8)
        verificar("tema 2 tamano 2" in m.serial(), "clic en «Muy grande»: tamaño 2")
        con = medir(m, "moderno-muygrande", 3)
        # el color del interior de la flecha moderna: azul translúcido sobre el escritorio
        r, gg, b = cor_do_pixel(con, P[0] + 6, P[1] + 20)
        verificar(b > r + 30, f"moderno: interior azulado {(r, gg, b)}")
        r, gg, b = cor_do_pixel(con, P[0] + 1, P[1] + 3)
        verificar(r > 230 and gg > 230 and b > 230, f"moderno: contorno blanco {(r, gg, b)}")
        m.clique(*ca.panel(v, xl + 20, ty + 0 * 24 + 6))      # clásico
        m.clique(*ca.panel(v, xl + 20, zy + 1 * 24 + 6))      # grande
        con = medir(m, "clasico-grande", 2)
        r, gg, b = cor_do_pixel(con, P[0] + 1, P[1] + 3)
        verificar(r < 20 and gg < 20 and b < 20, f"clásico: contorno negro {(r, gg, b)}")
        r, gg, b = cor_do_pixel(con, P[0] + 6, P[1] + 20)
        verificar(r > 235 and gg > 235 and b > 235, f"clásico: interior blanco {(r, gg, b)}")
        m.clique(150, 12)   # el botón del Terminal de la barra: el teclado vuelve a la consola
        time.sleep(1)
        m.comando("configuracion cursor normal")
        medir(m, "clasico-normal", 1)
        m.comando("configuracion cursor oscuro muygrande")
        con = medir(m, "oscuro-muygrande", 3)
        r, gg, b = cor_do_pixel(con, P[0] + 6, P[1] + 20)
        verificar(r < 50 and gg < 50 and b < 60, f"oscuro: interior oscuro {(r, gg, b)}")
        out = m.comando("cat /home/.config/sagui.conf")
        verificar("cursor_tema=1" in out and "cursor_tamano=2" in out, "se guardó en sagui.conf")
        m.comando("sync")
        time.sleep(1)
    # reinicio: la misma imagen, el /home persistente
    with Maquina("h482", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 240)
        time.sleep(5)
        out = m.comando("configuracion cursor mostrar")
        verificar("tema oscuro tamano muygrande (36x57)" in out, "tras reiniciar siguen oscuro y muy grande: " + out.strip()[-80:])
        medir(m, "reinicio", 3)
        m.comando("configuracion cursor clasico normal")
    print("H482 ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
