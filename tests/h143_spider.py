#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H143 - Solitario Spider (D173).

Criterio:
  - `spider --autoprueba` (sin ventana) comprueba las reglas: el reparto (104 cartas, 10 columnas
    de 6 y 5, sólo la última boca arriba, 50 en el montón), qué se puede coger y dónde se puede
    soltar, la escalera K..A que se retira (+100) y deja ver la carta de debajo, el reparto que no
    se admite con una columna vacía, deshacer, la pista legal, tres partidas jugadas solas sin
    perder una carta y la victoria con la octava escalera;
  - el programa abre desde Inicio > Juegos > Solitario Spider; la captura enseña el tapete y las
    10 columnas (se comprueban los colores: tapete verde, caras claras, dorsos azules);
  - un arrastre válido (el que da la pista, con el ratón), uno inválido (la carta vuelve) y deshacer
    con Ctrl+Z, todo comprobado en el estado que el programa escribe por la serie;
  - un reparto con un clic en el montón (una carta más por columna, 40 en el montón, -1 punto);
  - la pista (H) resalta el movimiento; F2 abre la elección de nivel y el nivel de 2 palos reparte
    con dos palos;
  - con `--casi-ganada` un clic gana la partida y arranca la animación final;
  - ninguna excepción durante todo esto.
Capturas en docs/capturas/H143-*.png.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel  # noqa: E402
from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui  # noqa: E402

TITULO = "Solitario Spider"


def janela(serial, titulo=TITULO):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), fx=int(g[2]), fy=int(g[3]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def layout(serial):
    g = re.findall(r"spider: layout L=(\d+) A=(\d+) cw=(\d+) ch=(\d+) x0=(\d+) gap=(\d+) ytab=(\d+) yfundo=(\d+) "
                   r"estoque=(\d+),(\d+),(\d+),(\d+)", serial)[-1]
    k = ("L", "A", "cw", "ch", "x0", "gap", "ytab", "yfundo", "ex", "ey", "ew", "eh")
    return dict(zip(k, map(int, g)))


def estado(serial, desde=0):
    """El último 'estado' escrito: n por columna, última carta (palo, valor), y de la última carta."""
    g = re.findall(r"spider: estado (\S+) n=([\d,]+) ult=([\d/,-]+) y=([\d,]+) estoque=(\d+) feitas=(\d+) "
                   r"pontos=(\d+) movs=(\d+)", serial[desde:])[-1]
    ult = [tuple(map(int, u.split("/"))) for u in g[2].split(",")]
    return dict(por=g[0], n=list(map(int, g[1].split(","))), ult=ult, y=list(map(int, g[3].split(","))),
                estoque=int(g[4]), feitas=int(g[5]), pontos=int(g[6]), movs=int(g[7]))


def xcol(lay, c):
    return lay["x0"] + c * (lay["cw"] + lay["gap"])


def esperar_serie(m, padrao, desde, tempo=25):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def fechar(m, j):
    m.clique(j["x"] + j["l"] - 12, j["y"] - 12)          # la X de la moldura
    time.sleep(1.0)


def clara(rgb):
    return min(rgb) >= 0xE0


def verde(rgb):
    return rgb[1] > rgb[0] + 30 and rgb[1] > rgb[2] + 20


def azul(rgb):
    return rgb[2] > rgb[0] + 40 and rgb[2] > rgb[1] + 20


def main():
    with Maquina("h143", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---- la autoprueba de las reglas, sin ventana
        m.digitar("spider --autoprueba --semilla 5\n", 0.04)
        m.esperar_serial("spider autoprueba:", 180)
        s = m.serial()
        verificar(re.search(r"spider: autoprueba ok \((\d+) comprobaciones, 0 fallos\)", s) is not None,
                  "la autoprueba de las reglas pasa sin ningún fallo")
        verificar("autoprueba FALLO" not in s, "la autoprueba no registra ningún FALLO")
        verificar(int(re.findall(r"ok \((\d+) comprobaciones", s)[-1]) >= 40, "la autoprueba hace 40 comprobaciones o más")
        m.esperar_serial("sagui:/$", 30)

        # ---- el programa desde el menú Inicio > Juegos
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        verificar(">Juegos" in p["linhas"], "el menú Inicio tiene el grupo Juegos")
        sub = submenu_pelo_mouse(m, p, "Juegos", "Solitario Spider")
        verificar("Solitario Spider" in sub["linhas"] and "Sagui Atrapa Bananas" in sub["linhas"],
                  f"Juegos contiene Solitario Spider ({', '.join(sub['linhas'])})")
        time.sleep(0.4)
        m.captura("H143-1-menu-juegos")
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /bin/spider", desde=marca)
        m.esperar_serial("spider: pronto", 40)
        time.sleep(2.0)
        j = janela(m.serial())
        verificar(j["l"] >= 700, f"la ventana del Spider tiene su tamaño ({j['l']}x{j['a']})")
        lay = layout(m.serial())
        est = estado(m.serial())
        verificar(est["n"] == [6, 6, 6, 6, 5, 5, 5, 5, 5, 5] and est["estoque"] == 50 and est["pontos"] == 500,
                  "reparto inicial: 4 columnas de 6, 6 de 5, 50 en el montón, 500 puntos")
        verificar(all(p_ == 0 for p_, v in est["ult"]), "el nivel por defecto es de un palo (picas)")

        # ---- la captura: tapete y 10 columnas
        png = m.captura("H143-2-tapete")
        verificar(verde(cor_do_pixel(png, j["x"] + 6, j["y"] + lay["A"] - 6)), "el tapete es verde")
        verificar(verde(cor_do_pixel(png, j["x"] + lay["L"] // 2, j["y"] + lay["A"] // 2)), "el centro del tapete es verde")
        for c in range(10):
            cx = j["x"] + xcol(lay, c) + lay["cw"] // 2
            ydorso = j["y"] + lay["ytab"] + 5
            yult = j["y"] + est["y"][c] + lay["ch"] - 6
            verificar(azul(cor_do_pixel(png, cx - 14, ydorso)), f"la columna {c + 1} muestra el dorso de las cartas boca abajo")
            verificar(clara(cor_do_pixel(png, cx, yult)), f"la columna {c + 1} acaba en una carta boca arriba")
        verificar(azul(cor_do_pixel(png, j["x"] + lay["ex"] + lay["ew"] - lay["cw"] + 14, j["y"] + lay["ey"] + 12)), "el montón de repartos (abajo a la derecha) enseña el dorso")

        # ---- un arrastre válido: el que da la pista
        antes = estado(m.serial())
        marca = len(m.serial())
        m.tecla("h", 0.3)
        d = esperar_serie(m, r"spider: dica de=(\d+) i=(\d+) para=(\d+) x=(\d+) y=(\d+) xd=(\d+) yd=(\d+)", marca)
        de, i, para, x0, y0, x1, y1 = map(int, d)
        time.sleep(0.4)
        m.captura("H143-3-pista")
        verificar(True, f"la pista propone mover de la columna {de + 1} a la {para + 1}")
        ox, oy = j["x"] + x0 + lay["cw"] // 2, j["y"] + y0 + 12
        dx, dy = j["x"] + x1 + lay["cw"] // 2, j["y"] + y1 + 12
        marca = len(m.serial())
        m.mouse_para(ox, oy)
        m.monitor("mouse_button 1")
        time.sleep(0.2)
        m.mouse_mover((ox + dx) // 2 + 3, (oy + dy) // 2 + 3)
        time.sleep(0.4)
        m.captura("H143-4-arrastre")
        m.mouse_mover(dx, dy)
        m.monitor("mouse_button 0")
        esperar_serie(m, r"spider: jogada ok de=\d+ i=\d+ para=\d+", marca)
        e1 = esperar_serie(m, r"spider: estado tras n=([\d,]+)", marca)
        time.sleep(0.8)
        despues = estado(m.serial())
        verificar(despues["movs"] == antes["movs"] + 1 and despues["pontos"] == antes["pontos"] - 1,
                  f"el arrastre válido cuesta un punto y cuenta un movimiento ({despues['pontos']} puntos, {despues['movs']} mov.)")
        q = antes["n"][de] - i
        verificar(despues["n"][para] == antes["n"][para] + q and despues["n"][de] == antes["n"][de] - q,
                  f"la pila de {q} carta(s) pasó de la columna {de + 1} a la {para + 1}")
        m.captura("H143-5-tras-mover")

        # ---- deshacer con Ctrl+Z
        marca = len(m.serial())
        m.tecla("ctrl-z", 0.3)
        esperar_serie(m, r"spider: estado deshacer", marca)
        des = estado(m.serial())
        verificar(des["n"] == antes["n"] and des["pontos"] == antes["pontos"] and des["movs"] == antes["movs"],
                  "Ctrl+Z devuelve la partida al estado anterior (columnas, puntos y movimientos)")

        # ---- un arrastre inválido: la carta vuelve
        a_, b_ = None, None
        for ca in range(10):
            for cb in range(10):
                if ca != cb and des["n"][cb] and des["n"][ca] and des["ult"][cb][1] != des["ult"][ca][1] + 1:
                    a_, b_ = ca, cb
                    break
            if a_ is not None:
                break
        marca = len(m.serial())
        ox, oy = j["x"] + xcol(lay, a_) + lay["cw"] // 2, j["y"] + des["y"][a_] + 12
        dx, dy = j["x"] + xcol(lay, b_) + lay["cw"] // 2, j["y"] + des["y"][b_] + 12
        m.arrastar(ox, oy, dx, dy)
        esperar_serie(m, r"spider: jogada invalida", marca)
        time.sleep(1.0)
        inv = estado(m.serial(), marca)
        verificar(inv["n"] == des["n"] and inv["pontos"] == des["pontos"] and inv["movs"] == des["movs"],
                  f"soltar en la columna {b_ + 1} no vale: la carta de la columna {a_ + 1} vuelve y no cambia nada")
        m.captura("H143-6-arrastre-invalido")

        # ---- un reparto con un clic en el montón
        marca = len(m.serial())
        m.clique(j["x"] + lay["ex"] + lay["ew"] - lay["cw"] // 2, j["y"] + lay["ey"] + lay["ch"] // 2)
        esperar_serie(m, r"spider: reparto ok estoque=40", marca)
        esperar_serie(m, r"spider: estado tras", marca)
        time.sleep(0.6)
        rep = estado(m.serial())
        verificar(rep["estoque"] == 40 and rep["n"] == [n + 1 for n in des["n"]],
                  "el reparto pone una carta boca arriba más en cada columna y deja 40 en el montón")
        verificar(rep["pontos"] == des["pontos"] - 1, "el reparto cuenta como un movimiento")
        m.captura("H143-7-reparto")

        # ---- F2: elegir el nivel y empezar de nuevo con dos palos
        marca = len(m.serial())
        m.tecla("f2", 0.4)
        esperar_serie(m, r"spider: dialogo de nivel", marca)
        time.sleep(0.5)
        m.captura("H143-8-nivel")
        m.tecla("2", 0.4)
        esperar_serie(m, r"spider: partida nivel=2", marca)
        time.sleep(0.8)
        n2 = estado(m.serial())
        verificar(n2["n"] == [6, 6, 6, 6, 5, 5, 5, 5, 5, 5] and n2["pontos"] == 500 and n2["estoque"] == 50,
                  "la partida nueva de 2 palos reparte otra vez 54 cartas y 500 puntos")
        verificar(set(p_ for p_, v in n2["ult"]) <= {0, 1}, "con 2 palos sólo hay picas y corazones")
        m.captura("H143-9-dos-palos")

        # ---- pantalla completa (F11): el reparto se adapta al tamaño nuevo
        marca = len(m.serial())
        m.tecla("f11", 0.5)
        L2 = int(esperar_serie(m, r"spider: layout L=(\d+)", marca))
        time.sleep(1.5)
        lay2 = layout(m.serial())
        verificar(L2 >= 1000 and lay2["cw"] > lay["cw"], f"en pantalla completa las cartas crecen ({lay['cw']} -> {lay2['cw']} px de ancho, ventana de {lay2['L']}x{lay2['A']})")
        est2 = estado(m.serial())
        verificar(est2["n"] == n2["n"], "al cambiar el tamaño la partida sigue igual")
        png = m.captura("H143-9b-pantalla-completa")
        ok = all(clara(cor_do_pixel(png, xcol(lay2, c) + lay2["cw"] // 2, est2["y"][c] + lay2["ch"] - 6)) for c in range(10))
        verificar(ok, "en pantalla completa las 10 columnas se redibujan con su carta de arriba")
        marca = len(m.serial())
        m.tecla("f11", 0.5)
        esperar_serie(m, r"spider: layout L=(\d+)", marca)
        time.sleep(1.0)

        verificar("EXCECAO" not in m.serial(), "ninguna excepción del kernel durante las pruebas")
        fechar(m, j)
        verificar("spider: cerrado" in m.serial(), "el programa cierra con la X de la ventana")

        # ---- victoria: una partida a un clic de ganar
        marca = len(m.serial())
        m.digitar("spider --casi-ganada --semilla 1\n", 0.04)
        m.esperar_serial("spider: pronto", 40)
        time.sleep(1.5)
        j = janela(m.serial())
        lay = layout(m.serial())
        est = estado(m.serial(), marca)
        verificar(est["feitas"] == 7, "partida casi ganada: siete escaleras retiradas")
        m.captura("H143-10-casi-ganada")
        m.clique(j["x"] + xcol(lay, 1) + lay["cw"] // 2, j["y"] + est["y"][1] + 12)
        m.esperar_serial("spider: victoria", 30)
        time.sleep(2.5)
        fin = estado(m.serial())
        verificar(fin["feitas"] == 8, "un clic lleva el As al 2, retira la octava escalera y gana")
        verificar(fin["pontos"] == 500 - 40 - 1 + 100, f"puntos al ganar: 460 - 1 + 100 ({fin['pontos']})")
        m.captura("H143-11-victoria")
        time.sleep(2.0)
        m.captura("H143-12-victoria-cartas")
        verificar("EXCECAO" not in m.serial(), "ninguna excepción tampoco en la animación final")
        fechar(m, j)


if __name__ == "__main__":
    main()
    print("H143 OK")
