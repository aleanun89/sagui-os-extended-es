#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H217 - la rueda del Archivos conserva la vista elegida (D217).

Fallo del hardware real: en una carpeta con muchos archivos, mientras se hacia
scroll con la rueda se veia la lista generica (solo nombres) en lugar de la
vista de iconos o de detalles, hasta que se paraba y se hacia clic. La causa:
lista_tratar (libsagui) redibujaba con lista_desenhar, sin encabezado, columnas
ni rejilla. Ahora lista_t tiene un gancho de dibujo (desenhar) que Archivos usa.

Criterio (con /bin, mas de 100 elementos):
  (a) vista de detalles: tras varias muescas de la rueda, SIN clic despues, la
      captura muestra el texto de la columna «Tipo» en las filas (la lista
      generica solo escribe el nombre) y el registro dice vista=detalhes;
  (b) vista de iconos: tras la rueda, la captura muestra celdas de la rejilla
      en la 4.a columna (la lista generica no llega ahi) y vista=icones;
  (c) la rueda hacia arriba vuelve al principio y la vista sigue siendo la misma;
  (d) RePag/AvPag mueven la seleccion y tampoco cambian la vista;
  (e) un clic en un hueco de la vista de iconos no selecciona nada raro y la
      vista sigue en iconos.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA  # noqa: E402

LATERAL, Y_LISTA, LARG_LISTA = 140, 98, 566
LISTA_X = LATERAL + 6                      # 146
COL_TIPO = LISTA_X + LARG_LISTA - 240      # 472
# D423: las vistas son un control segmentado (30 px cada una desde x=398; centro de la fila en y=20)
SEG_VISTA = {"lista": 413, "detalhes": 443, "icones": 473, "mosaicos": 503}


def janela(serial, titulo):
    m = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24)


def roda(m, entalhes):
    """Gira la rueda: + hacia abajo (en el monitor dz > 0 es hacia arriba)."""
    passo = -1 if entalhes > 0 else 1
    for _ in range(abs(entalhes)):
        m.monitor(f"mouse_move 0 0 {passo}")
        time.sleep(0.15)
    time.sleep(0.5)


def tela(m, nome):
    ppm = os.path.join(SAIDA, f"h217-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    return ler_ppm(ppm)


def contar(img, x0, y0, l, a, escuro):
    """Pixeles oscuros (escuro=True) o no blancos (False) en un rectangulo de la pantalla."""
    larg, _, px = img
    n = 0
    for y in range(y0, y0 + a):
        for x in range(x0, x0 + l):
            r, g, b = px[(y * larg + x) * 3:(y * larg + x) * 3 + 3]
            if escuro:
                n += (r + g + b) < 300
            else:
                n += (r, g, b) != (255, 255, 255)
    return n


def ultima_lista(m):
    g = re.findall(r"arquivos: lista desenhada vista=(\w+) topo=(\d+)", m.serial())
    return g[-1] if g else None


def poner_vista(m, a, nombre):
    for _ in range(4):
        g = re.findall(r"arquivos: vista (\w+)", m.serial())
        if g and g[-1] == nombre:
            return
        m.clique(a["x"] + SEG_VISTA[nombre], a["y"] + 20)
        time.sleep(0.6)
    raise AssertionError(f"no se pudo poner la vista {nombre}")


def main():
    with Maquina("h217", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("arquivos /bin")
        m.esperar_serial("arquivos: pronto", 20)
        time.sleep(0.8)
        a = janela(m.serial(), "Archivos")
        n = int(re.findall(r"arquivos: pasta /bin \((\d+) itens\)", m.serial())[-1])
        verificar(n > 100, f"/bin tiene {n} elementos (mas de 100: hay scroll en todas las vistas)")
        lx, ly = a["x"], a["y"]

        # ---- (a) detalles
        poner_vista(m, a, "detalhes")
        m.mouse_para(lx + 300, ly + 250)
        roda(m, 4)
        l = ultima_lista(m)
        verificar(l == ("detalhes", "12"), f"rueda en detalles: el registro dice {l} (4 muescas x 3)")
        img = tela(m, "detalles")
        tipo = contar(img, lx + COL_TIPO, ly + Y_LISTA + 18 + 2, 80, 100, True)
        verificar(tipo > 20, f"detalles durante el scroll: la columna Tipo se ve ({tipo} pixeles de texto)")
        verificar(contar(img, lx + COL_TIPO, ly + Y_LISTA + 1, 60, 16, True) > 5,
                  "el encabezado de columnas sigue ahi")
        m.captura("H217-1-detalles-scroll")

        # ---- (d) RePag / AvPag en detalles: la vista no cambia
        m.clique(lx + 300, ly + Y_LISTA + 18 + 30)
        time.sleep(0.4)
        m.tecla("pgdn")
        time.sleep(0.6)
        img = tela(m, "detalles-pgdn")
        verificar(contar(img, lx + COL_TIPO, ly + Y_LISTA + 18 + 2, 80, 100, True) > 20,
                  "detalles tras AvPag: la columna Tipo sigue a la vista")
        m.tecla("pgup")
        time.sleep(0.4)

        # ---- (b) iconos
        poner_vista(m, a, "icones")
        m.mouse_para(lx + 300, ly + 250)
        roda(m, 3)
        l = ultima_lista(m)
        col = (LARG_LISTA - 12) // 92
        verificar(l is not None and l[0] == "icones" and int(l[1]) > 0 and int(l[1]) % col == 0,
                  f"rueda en iconos: registro {l}, siempre al inicio de una fila de la rejilla ({col} columnas)")
        img = tela(m, "iconos")
        celda4 = contar(img, lx + 6 + 3 * 92, ly + Y_LISTA + 4, 88, 68, False)
        verificar(celda4 > 50, f"iconos durante el scroll: la 4.a columna de la rejilla tiene celdas ({celda4} pixeles)")
        m.captura("H217-2-iconos-scroll")

        # ---- (c) vuelta al principio con la rueda hacia arriba
        roda(m, -20)
        l = ultima_lista(m)
        verificar(l == ("icones", "0"), f"rueda hacia arriba hasta el principio: {l}")

        # ---- (f) AvPag / RePag en iconos: la rejilla no se corre de lado (notas del revisor)
        m.tecla("pgdn")
        time.sleep(0.4)
        l = ultima_lista(m)
        verificar(l is not None and l[0] == "icones" and int(l[1]) > 0 and int(l[1]) % col == 0,
                  f"AvPag en iconos: registro {l}, topo al inicio de una fila ({col} columnas)")
        m.tecla("pgup")
        time.sleep(0.4)
        l = ultima_lista(m)
        verificar(l is not None and l[0] == "icones" and int(l[1]) % col == 0,
                  f"RePag en iconos: registro {l}, topo al inicio de una fila")

        # ---- (e) clic en un hueco de la vista de iconos
        marca = len(m.serial())
        m.clique(lx + LISTA_X + LARG_LISTA - 20, ly + Y_LISTA + 100)
        time.sleep(0.5)
        img = tela(m, "iconos-clic")
        verificar(contar(img, lx + 6 + 3 * 92, ly + Y_LISTA + 4, 88, 68, False) > 50,
                  "tras un clic en la rejilla la vista sigue siendo de iconos")
        verificar("vista=detalhes" not in m.serial()[marca:] and "vista=lista" not in m.serial()[marca:],
                  "ninguna redibujada intermedia con otra vista")
        m.captura("H217-3-iconos-clic")
    return 0


if __name__ == "__main__":
    sys.exit(main())
