#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H247 - Buscaminas propio (D248).

Criterio:
  - el programa abre desde Inicio > Juegos > Buscaminas y se dibuja (capturas H247-*);
  - con `--tam 6,6,4 --semilla 7` (personalizado): el primer clic nunca es mina y abre una zona;
    el clic derecho pone bandera, luego la marca «?» y luego la quita; el acorde (clic en un
    número con sus banderas completas) descubre las vecinas; pisar una mina pierde la partida;
  - con `--nivel 1 --semilla 11` se mueve con el teclado (flecha y espacio), se gana una partida
    descubriendo solo casillas sin mina, y el récord queda guardado en /home/.config/buscaminas.conf.
El estado lo escribe el programa por la serie (`buscaminas: estado ...` y `buscaminas: fN ...`).
Capturas en docs/capturas/H247-*.png.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402
from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui  # noqa: E402

TITULO = "Buscaminas"


def janela(serial):
    g = re.findall(rf'criada por \d+: "{TITULO}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def layout(serial):
    g = re.findall(r"buscaminas: layout L=(\d+) A=(\d+) ox=(\d+) oy=(\d+) cel=(\d+) cols=(\d+) filas=(\d+)", serial)[-1]
    return dict(zip(("L", "A", "ox", "oy", "cel", "cols", "filas"), map(int, g)))


def estado(serial):
    """El último bloque de estado: dict con estado, rest, abiertas y las filas del tablero."""
    s = serial.replace("\r", "")
    i = s.rindex("buscaminas: estado ")
    cab = re.match(r"buscaminas: estado (\S+) (\S+) niv=(\d+) (\d+)x(\d+) minas=(\d+) rest=(-?\d+) "
                   r"abiertas=(\d+) banderas=(\d+) t=(\d+)", s[i:])
    filas = re.findall(r"buscaminas: f(\d+) (\S+)", s[i:])
    n = int(cab.group(5))
    return dict(que=cab.group(1), est=cab.group(2), rest=int(cab.group(7)), abiertas=int(cab.group(8)),
                banderas=int(cab.group(9)), filas=[f for _, f in filas[:n]])


def minas(serial, filas):
    s = serial.replace("\r", "")
    i = s.rindex("buscaminas: m0 ")
    return [f for _, f in re.findall(r"buscaminas: m(\d+) (\S+)", s[i:])[:filas]]


def esperar_estado(m, n_antes, tempo=15):
    fim = time.time() + tempo
    while time.time() < fim:
        sr = m.serial()
        if sr.count("buscaminas: estado ") > n_antes:
            try:
                e = estado(sr)
                filas = int(re.findall(r"buscaminas: estado \S+ \S+ niv=\d+ \d+x(\d+)", sr)[-1])
                if len(e["filas"]) == filas:
                    return e
            except (ValueError, AttributeError, IndexError):
                pass
        time.sleep(0.2)
    raise AssertionError("el programa no escribió un estado nuevo")


class Juego:
    def __init__(self, m):
        self.m = m
        s = m.serial()
        self.j = janela(s)
        self.lay = layout(s)

    def pos(self, x, y):
        l = self.lay
        return (self.j["x"] + l["ox"] + x * l["cel"] + l["cel"] // 2, self.j["y"] + l["oy"] + y * l["cel"] + l["cel"] // 2)

    def clic(self, x, y, botao=1):
        n = self.m.serial().count("buscaminas: estado ")
        px, py = self.pos(x, y)
        self.m.clique(px, py, botao)
        return esperar_estado(self.m, n)

    def tecla(self, nome):
        n = self.m.serial().count("buscaminas: estado ")
        self.m.tecla(nome, 0.3)
        return esperar_estado(self.m, n)


def vecinos(x, y, c, f):
    return [(x + dx, y + dy) for dy in (-1, 0, 1) for dx in (-1, 0, 1)
            if (dx or dy) and 0 <= x + dx < c and 0 <= y + dy < f]


def main():
    with Maquina("h247", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---- desde el menú Inicio > Juegos
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        sub = submenu_pelo_mouse(m, p, "Juegos", "Buscaminas")
        verificar("Buscaminas" in sub["linhas"], f"Juegos contiene Buscaminas ({', '.join(sub['linhas'])})")
        time.sleep(0.4)
        m.captura("H247-1-menu-juegos")
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /apps/bin/buscaminas", desde=marca)
        m.esperar_serial("buscaminas: pronto", 40)
        time.sleep(1.5)
        g = Juego(m)
        verificar(g.lay["cols"] == 9 and g.lay["filas"] == 9, "el menú abre el nivel Principiante 9x9")
        m.captura("H247-2-inicio")
        m.clique(g.j["x"] + g.j["l"] - 12, g.j["y"] - 12)          # la X de la moldura
        m.esperar_serial("buscaminas: cerrado", 15)
        time.sleep(1)

        # ---- partida personalizada 6x6 con 4 minas
        m.digitar("buscaminas --tam 6,6,4 --semilla 7\n", 0.04)
        m.esperar_serial("buscaminas: pronto", 40)
        time.sleep(1.5)
        g = Juego(m)
        verificar((g.lay["cols"], g.lay["filas"]) == (6, 6), "tamaño personalizado 6x6")
        e = g.clic(2, 2)
        mm = minas(m.serial(), 6)
        verificar(sum(r.count("*") for r in mm) == 4, "se colocaron 4 minas tras el primer clic")
        verificar(mm[2][2] == ".", "la primera casilla no es mina")
        verificar(all(mm[y][x] == "." for (x, y) in vecinos(2, 2, 6, 6)), "el entorno del primer clic queda libre")
        verificar(e["abiertas"] >= 9 and e["est"] == "jugando", f"el primer clic abre una zona ({e['abiertas']} casillas)")
        m.captura("H247-3-primer-clic")

        # bandera, «?» y quitar, sobre una casilla oculta cualquiera
        ocultas = [(x, y) for y in range(6) for x in range(6) if e["filas"][y][x] == "#"]
        minas_pos = [(x, y) for y in range(6) for x in range(6) if mm[y][x] == "*"]
        bx, by = minas_pos[0]
        e = g.clic(bx, by, 2)
        verificar(e["filas"][by][bx] == "F" and e["rest"] == 3, "clic derecho: bandera y contador de minas -1")
        e = g.clic(bx, by, 2)
        verificar(e["filas"][by][bx] == "?" and e["rest"] == 4, "segundo clic derecho: marca «?»")
        e = g.clic(bx, by, 2)
        verificar(e["filas"][by][bx] == "#", "tercer clic derecho: casilla limpia")
        e = g.clic(bx, by, 2)
        m.captura("H247-4-bandera")

        # acorde: un número abierto con una mina vecina oculta
        cand = None
        for y in range(6):
            for x in range(6):
                if e["filas"][y][x].isdigit():
                    mv = [v for v in vecinos(x, y, 6, 6) if mm[v[1]][v[0]] == "*"]
                    nv = [v for v in vecinos(x, y, 6, 6) if mm[v[1]][v[0]] != "*" and e["filas"][v[1]][v[0]] == "#"]
                    if mv and nv and len(mv) == int(e["filas"][y][x]):
                        cand = (x, y, mv, nv)
        verificar(cand is not None, "hay un número con vecinas ocultas sin mina para probar el acorde")
        x, y, mv, nv = cand
        for (a, b) in mv:
            if e["filas"][b][a] != "F":
                e = g.clic(a, b, 2)
        e = g.clic(x, y, 1)
        verificar(all(e["filas"][b][a] != "#" for (a, b) in nv), "el acorde descubre las vecinas sin mina")
        verificar(e["est"] == "jugando", "el acorde con banderas correctas no pierde")
        m.captura("H247-5-acorde")

        # pisar una mina sin bandera (si todas están marcadas, se quita una)
        libres = [v for v in minas_pos if e["filas"][v[1]][v[0]] != "F"]
        if not libres:
            a, b = minas_pos[0]
            e = g.clic(a, b, 2)
            e = g.clic(a, b, 2)
            e = g.clic(a, b, 2)
            libres = [minas_pos[0]]
        a, b = libres[0]
        e = g.clic(a, b, 1)
        verificar(e["est"] == "perdida", "pisar una mina pierde la partida")
        time.sleep(0.3)
        m.captura("H247-6-perdida")
        m.clique(g.j["x"] + g.j["l"] - 12, g.j["y"] - 12)
        m.esperar_serial("buscaminas: cerrado", 15)
        time.sleep(1)

        # ---- Principiante con teclado y victoria + récord
        m.digitar("buscaminas --nivel 1 --semilla 11\n", 0.04)
        m.esperar_serial("buscaminas: pronto", 40)
        time.sleep(1.5)
        g = Juego(m)
        m.tecla("right", 0.3)       # el cursor sale del centro (4,4) -> (5,4)
        e = g.tecla("spc")
        mm = minas(m.serial(), 9)
        verificar(e["est"] == "jugando" and mm[4][5] == ".", "teclado: flecha y espacio descubren la casilla (5,4)")
        m.captura("H247-7-teclado")
        for _ in range(100):
            pend = [(x, y) for y in range(9) for x in range(9) if mm[y][x] == "." and e["filas"][y][x] == "#"]
            if not pend:
                break
            e = g.clic(*pend[0], 1)
        verificar(e["est"] == "ganada", "descubrir todas las casillas sin mina gana la partida")
        verificar(e["rest"] == 0, "al ganar el contador de minas llega a 0")
        time.sleep(0.4)
        m.captura("H247-8-ganada")
        esperar(m, r"buscaminas: record principiante \d+", tempo=5)
        m.clique(g.j["x"] + g.j["l"] - 12, g.j["y"] - 12)
        m.esperar_serial("buscaminas: cerrado", 15)
        s = m.comando("cat /home/.config/buscaminas.conf", 20) + m.comando("cat /tmp/buscaminas.conf", 20)
        verificar(re.search(r"principiante=[1-9]\d*", s) is not None, "el récord se guardó (/home/.config/buscaminas.conf o /tmp si no hay /home)")

        # ---- ventana de récords
        m.digitar("buscaminas --nivel 1 --semilla 12\n", 0.04)
        m.esperar_serial("buscaminas: pronto", 40)
        time.sleep(1.5)
        m.tecla("r", 0.8)
        m.captura("H247-9-records")
        s = m.serial()
        verificar("excepci" not in s.lower(), "ninguna excepción durante la prueba")
    print("H247 ok")


if __name__ == "__main__":
    main()
