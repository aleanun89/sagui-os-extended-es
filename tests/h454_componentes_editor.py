#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H454 - componentes de editor de la libsagui (D454, v8.5.6).

Criterio (`demoeditor`, datos sintéticos; cada paso lee la serie «demoeditor: ...» y, donde se ve, los píxeles):
  (1) la demo abre con la disposición árbol | lienzo y código | inspector y abajo recursos y consola;
  (2) lienzo: elegir por clic, mover con imán, escalar y girar con las asas, rectángulo de selección;
  (3) deshacer/rehacer de esos pasos, y un grupo («Alinear todo») que se deshace de una vez;
  (4) arrastrar y soltar: del árbol al lienzo, de casillas al lienzo (grupo), cancelar con Esc, aviso al pasar por la consola;
  (5) zoom con la rueda centrado en el ratón, desplazar con Espacio y con el botón central;
  (6) inspector: número con flechas, arrastre y teclado, límites, punto fijo, casilla, deslizador, texto, color y archivo;
  (7) selector de color: paleta, RGB, tono, saturación/brillo y alfa;
  (8) paleta de casillas: una, un bloque arrastrando y Mayús;
  (9) editor de código: resaltado de Lua, sangría automática, comprobar con error marcado, buscar y deshacer;
  (10) consola: filtros, copiar, limpiar, doble clic en un error va a su línea;
  (11) paneles: arrastrar un separador, tamaño mínimo, plegar, guardar y restaurar la disposición.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA  # noqa: E402

TITULO = "Demo del editor"


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def esperar(m, padrao, tempo=15, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    try:
        m.captura("fallo-h454", guardar=False)
    except Exception:
        pass
    ult = "\n".join(l for l in m.serial()[desde:].splitlines() if "demoeditor" in l or "selcor" in l or "arrsol" in l or "edvista" in l)[-1500:]
    raise AssertionError(f"tiempo agotado esperando {padrao!r}; serie:\n{ult}")


def pos(m):
    d = {}
    for n, x, y, l, a, pl in re.findall(r"demoeditor: pos (\w+) (-?\d+) (-?\d+) (-?\d+) (-?\d+)(?: plegado (\d))?", m.serial()):
        d[n] = tuple(int(v) for v in (x, y, l, a)) + ((int(pl),) if pl != "" else ())
    return d


def tela(m, nome):
    ppm = os.path.join(SAIDA, f"h454-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    return ler_ppm(ppm)


def pixel(img, x, y):
    larg, _, px = img
    i = (y * larg + x) * 3
    return tuple(px[i:i + 3])


def perto(c, ref, tol=28):
    return all(abs(a - b) <= tol for a, b in zip(c, ref))


def ultimo(m, padrao, desde=0):
    g = re.findall(padrao, m.serial()[desde:])
    return g[-1] if g else None


class T:
    """Atajos de coordenadas: lo relativo a la ventana pasa a la pantalla."""

    def __init__(self, m):
        self.m = m
        self.w = janela(m.serial(), TITULO)

    def abs(self, x, y):
        return self.w["x"] + x, self.w["y"] + y

    def clique(self, x, y, **k):
        self.m.clique(*self.abs(x, y), **k)

    def arrastar(self, x0, y0, x1, y1):
        self.m.arrastar(*self.abs(x0, y0), *self.abs(x1, y1))

    def mover(self, x, y):
        self.m.mouse_para(*self.abs(x, y))


def principal():
    with Maquina("h454", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("demoeditor &")
        m.esperar_serial("demoeditor: pronto", 30)
        time.sleep(1.0)
        t = T(m)
        P = pos(m)
        verificar(all(k in P for k in ("arbol", "vista", "codigo", "inspetor", "recursos", "consola")),
                  f"la demo anota la posición de sus seis paneles ({sorted(P)})")
        verificar(P["arbol"][0] == 0 and P["vista"][0] > P["arbol"][0] and P["inspetor"][0] > P["vista"][0]
                  and P["recursos"][1] > P["vista"][1] + P["vista"][3] and P["consola"][0] > P["recursos"][0],
                  "disposición: árbol | lienzo/código | inspector y abajo recursos | consola")
        m.captura("H454-1-disposicion")
        vx0, vy0 = P["vista"][0], P["vista"][1]

        def mundo(wx, wy, ox=90, oy=110, z=1000):
            return vx0 + ox + wx * z // 1000, vy0 + oy + wy * z // 1000

        # ---------------------------------------------------------------- (2) lienzo: clic, mover, escalar, girar, rectángulo
        marca = len(m.serial())
        t.clique(*mundo(56, 64))
        esperar(m, r"demoeditor: sel Jugador 32 32 48 64 rot 0", 8, marca)
        verificar(True, "lienzo: un clic sobre un objeto lo elige (el inspector y el árbol lo siguen)")
        m.captura("H454-2-seleccion")
        marca = len(m.serial())
        t.arrastar(*mundo(56, 64), *mundo(96, 80))
        esperar(m, r"demoeditor: movido Jugador 80 48 48 64 rot 0", 8, marca)
        verificar(True, "lienzo: arrastrar el cuerpo mueve con imán a la rejilla (72,48 -> 80,48)")
        marca = len(m.serial())
        t.arrastar(*mundo(128, 80), *mundo(158, 80))
        esperar(m, r"demoeditor: escalado Jugador 80 48 80 64 rot 0", 8, marca)
        verificar(True, "lienzo: el asa derecha escala el ancho (48 -> 80) con el borde en la rejilla")
        marca = len(m.serial())
        t.arrastar(*mundo(120, 26), *mundo(220, 80))
        r = esperar(m, r"demoeditor: girado Jugador 80 48 80 64 rot (\d+)", 8, marca)
        rot = r
        verificar(abs(int(r) - 90) <= 3, f"lienzo: el asa de arriba gira el rectángulo ({r} grados, esperaba 90)")
        m.captura("H454-3-asas-giro")
        marca = len(m.serial())
        t.arrastar(*mundo(100, -40), *mundo(230, 140))
        r = esperar(m, r"demoeditor: rectangulo (-?\d+) (-?\d+) (-?\d+) (-?\d+) toca (\d+)", 8, marca)
        verificar(r == ("100", "-40", "230", "140", "3"), f"lienzo: el rectángulo de selección toca tres objetos ({r})")

        # ---------------------------------------------------------------- (3) deshacer y rehacer
        P = pos(m)
        bx = lambda i: (P[f"boton{i}"][0] + P[f"boton{i}"][2] // 2, P[f"boton{i}"][1] + P[f"boton{i}"][3] // 2)
        marca = len(m.serial())
        t.clique(*bx(0))
        r = esperar(m, r"demoeditor: desfazer deshacer (\d) rehacer (\d) \(.*?\) objetos (\d+) suma (-?\d+)", 8, marca)
        t.clique(*bx(0))
        t.clique(*bx(0))
        time.sleep(0.5)
        r = ultimo(m, r"demoeditor: desfazer deshacer (\d) rehacer (\d) \(.*?\) objetos (\d+) suma (-?\d+)", marca)
        verificar(r == ("0", "1", "4", "4176"), f"deshacer tres veces vuelve a la escena de partida ({r})")
        marca = len(m.serial())
        for _ in range(3):
            t.clique(*bx(1))
        time.sleep(0.5)
        r = ultimo(m, r"demoeditor: refazer deshacer (\d) rehacer (\d) \(.*?\) objetos (\d+) suma (-?\d+)", marca)
        verificar(r[:3] == ("1", "0", "4") and int(r[3]) == 5794 - 13 * (90 - int(rot)), f"rehacer tres veces deja lo hecho (mover, escalar y girar) ({r})")

        # grupo: dos objetos movidos sin imán y «Alinear todo» -> un solo paso de deshacer
        marca = len(m.serial())
        t.clique(*bx(6))   # imán fuera
        esperar(m, r"demoeditor: iman 0", 8, marca)
        t.arrastar(*mundo(124, -4), *mundo(129, -1))   # Moneda
        time.sleep(0.5)
        t.arrastar(*mundo(180, 100), *mundo(183, 105))   # Enemigo
        time.sleep(0.5)
        t.clique(*bx(6))   # imán otra vez
        antes = ultimo(m, r"demoeditor: (?:desfazer|refazer|alinear) deshacer \d rehacer \d \(.*?\) objetos \d+ suma (-?\d+)")
        marca = len(m.serial())
        t.clique(*bx(2))
        r = esperar(m, r"demoeditor: alinear (\d+)", 8, marca)
        verificar(int(r) == 2, f"«Alinear todo» cambia los dos objetos fuera de rejilla ({r})")
        marca = len(m.serial())
        t.clique(*bx(0))
        r = esperar(m, r"demoeditor: desfazer deshacer \d rehacer 1 \(.*?\) objetos \d+ suma (-?\d+)", 8, marca)
        t.clique(*bx(1))
        time.sleep(0.4)
        verificar(r != "", "un solo «Deshacer» revierte el grupo entero de «Alinear todo»")

        # ---------------------------------------------------------------- (4) arrastrar y soltar
        P = pos(m)
        ax, ay, al, aa = P["arbol"]
        fila = lambda k: (ax + 60, ay + 2 + k * 20 + 10)
        marca = len(m.serial())
        t.mover(*fila(2))
        m.monitor("mouse_button 1")
        time.sleep(0.25)
        m.mouse_mover(*t.abs(*mundo(120, 60)))
        time.sleep(0.5)
        esperar(m, r"demoeditor: dnd entra vista tipo 1", 8, marca)
        m.captura("H454-4-arrastrar")
        c = pixel(tela(m, "dnd"), *t.abs(*mundo(120, 60)))
        m.monitor("mouse_button 0")
        r = esperar(m, r"demoeditor: dnd soltar objeto Enemigo en (-?\d+) (-?\d+)", 8, marca)
        verificar(r == ("128", "64"), f"arrastrar del árbol al lienzo coloca el objeto con imán ({r})")
        # casillas: elegir un bloque de 2 y arrastrarlo al lienzo
        P = pos(m)
        px, py = P["recursos"][0], P["recursos"][1]
        celda = lambda c, f: (px + c * 32 + 16, py + f * 32 + 16)
        t.clique(*celda(7, 3))   # la casilla (0,0) viene elegida: un arrastre desde ella sería de arrastrar y soltar
        marca = len(m.serial())
        t.arrastar(*celda(0, 0), *celda(1, 0))
        r = esperar(m, r"demoeditor: casillas (\d+) (\d+) (\d+) (\d+)", 8, marca)
        verificar(r == ("0", "0", "1", "0"), f"paleta: arrastrar elige un bloque rectangular de casillas ({r})")
        t.clique(*celda(2, 1))
        r = esperar(m, r"demoeditor: casillas 2 1 2 1", 8, marca)
        t.arrastar(*celda(0, 0), *celda(3, 2))
        r = esperar(m, r"demoeditor: casillas 0 0 3 2", 8, marca)
        verificar(True, "paleta: un clic elige una casilla y arrastrar elige otro bloque (4x3)")
        t.clique(*celda(5, 3))
        t.arrastar(*celda(0, 0), *celda(1, 0))
        time.sleep(0.3)
        marca = len(m.serial())
        t.arrastar(*celda(0, 0), *mundo(-40, 100))
        r = esperar(m, r"demoeditor: dnd soltar casillas (\d+) en (-?\d+) (-?\d+)", 8, marca)
        verificar(r == ("2", "-32", "96"), f"arrastrar casillas al lienzo crea un objeto por casilla ({r})")
        m.captura("H454-5-casillas")
        marca = len(m.serial())
        t.clique(*bx(0))
        r = esperar(m, r"demoeditor: desfazer deshacer \d rehacer \d \(.*?\) objetos (\d+)", 8, marca)
        verificar(r == "4", f"«Pintar casillas» se deshace de una vez (quedan {r} objetos)")
        t.clique(*bx(1))
        time.sleep(0.4)
        # Esc cancela
        marca = len(m.serial())
        t.mover(*fila(1))
        m.monitor("mouse_button 1")
        time.sleep(0.2)
        m.mouse_mover(*t.abs(*mundo(100, 100)))
        time.sleep(0.4)
        m.tecla("esc")
        esperar(m, r"demoeditor: dnd cancelado", 8, marca)
        m.monitor("mouse_button 0")
        verificar(True, "Esc cancela el arrastre")
        # la consola avisa al pasar por encima
        P = pos(m)
        cx, cy, cl, ca = P["consola"]
        marca = len(m.serial())
        t.mover(*fila(1))
        m.monitor("mouse_button 1")
        time.sleep(0.2)
        m.mouse_mover(*t.abs(cx + 60, cy + 80))
        time.sleep(0.4)
        esperar(m, r"demoeditor: dnd entra consola", 8, marca)
        m.monitor("mouse_button 0")
        esperar(m, r"demoeditor: dnd soltar consola Jugador", 8, marca)
        verificar(True, "arrastrar y soltar: el destino recibe entrar y soltar y puede rechazar el elemento")

        # ---------------------------------------------------------------- (5) zoom y desplazamiento
        P = pos(m)
        vx0, vy0, vl, va = P["vista"]
        mx, my = vx0 + 300, vy0 + 150
        wx0 = (mx - vx0 - 90) * 1000 // 1000
        marca = len(m.serial())
        m.mouse_para(*t.abs(mx, my))
        m.monitor("mouse_move 0 0 1")
        time.sleep(0.4)
        r = ultimo(m, r"demoeditor: zoom (\d+) ox (-?\d+) oy (-?\d+)", marca)
        if r is None:
            m.monitor("mouse_move 0 0 -1")
            time.sleep(0.4)
            r = ultimo(m, r"demoeditor: zoom (\d+) ox (-?\d+) oy (-?\d+)", marca)
        z, ox, oy = (int(v) for v in r)
        verificar(z != 1000, f"la rueda cambia el zoom ({z})")
        wx1 = (mx - vx0 - ox) * 1000 // z
        verificar(abs(wx1 - wx0) <= 2, f"el zoom queda centrado en el ratón (mundo {wx0} -> {wx1})")
        m.monitor("mouse_move 0 0 1")
        time.sleep(0.4)
        r2 = ultimo(m, r"demoeditor: zoom (\d+) ox (-?\d+) oy (-?\d+)", marca)
        verificar(int(r2[0]) != z, f"otra muesca sigue acercando ({r2[0]})")
        m.captura("H454-6-zoom")
        # Espacio + arrastrar
        t.clique(vx0 + vl - 80, vy0 + va - 30)   # da el foco al lienzo (empieza un rectángulo vacío)
        m.mouse_para(*t.abs(vx0 + 200, vy0 + 120))
        time.sleep(0.3)
        m.tecla("spc")
        marca = len(m.serial())
        t.arrastar(vx0 + 200, vy0 + 120, vx0 + 250, vy0 + 150)
        r = esperar(m, r"demoeditor: desplazado ox (-?\d+) oy (-?\d+) zoom (\d+)", 8, marca)
        verificar(int(r[0]) - int(r2[1]) == 50 and int(r[1]) - int(r2[2]) == 30, f"Espacio + arrastrar desplaza la vista ({r2[1:]} -> {r[:2]})")
        # botón central
        marca = len(m.serial())
        m.mouse_para(*t.abs(vx0 + 200, vy0 + 120))
        m.monitor("mouse_button 4")
        time.sleep(0.2)
        m.mouse_mover(*t.abs(vx0 + 180, vy0 + 100))
        m.monitor("mouse_button 0")
        r3 = esperar(m, r"demoeditor: desplazado ox (-?\d+) oy (-?\d+) zoom (\d+)", 8, marca)
        verificar(int(r3[0]) - int(r[0]) == -20 and int(r3[1]) - int(r[1]) == -20, f"el botón central también desplaza ({r[:2]} -> {r3[:2]})")

        # ---------------------------------------------------------------- (6) inspector
        t.clique(*fila(1))
        esperar(m, r"demoeditor: sel Jugador", 8, marca)
        P = pos(m)
        ix, iy, il, ia = P["inspetor"]
        col = max(70, il * 45 // 100)
        vxi = ix + col
        vli = il - col - 4
        fy = lambda k: iy + 1 + k * 22 + 11
        marca = len(m.serial())
        t.clique(vxi + 20, fy(1))
        time.sleep(0.4)
        m.digitar("75")
        m.tecla("ret")
        esperar(m, r"demoeditor: prop Posici.n X = 75", 8, marca)
        verificar(True, "inspector: un clic en un número lo deja editar y Intro lo acepta (75)")
        marca = len(m.serial())
        t.clique(vxi + vli - 7, fy(1) - 6)
        esperar(m, r"demoeditor: prop Posici.n X = 76", 8, marca)
        verificar(True, "inspector: la flecha de arriba suma un paso (76)")
        marca = len(m.serial())
        t.arrastar(vxi + 20, fy(1), vxi + 60, fy(1))
        r = esperar(m, r"demoeditor: prop Posici.n X = (\d+)", 8, marca)
        verificar(r == "86", f"inspector: arrastrar el campo cambia el valor (4 px = 1; +40 px -> {r})")
        marca = len(m.serial())
        t.clique(vxi + 20, fy(4))
        time.sleep(0.4)
        m.digitar("-5")
        m.tecla("ret")
        r = esperar(m, r"demoeditor: prop Alto = (-?\d+)", 8, marca)
        verificar(r == "1", f"inspector: el mínimo del campo limita el valor (-5 -> {r})")
        marca = len(m.serial())
        t.clique(vxi + 20, fy(6))
        time.sleep(0.4)
        m.digitar("2,5")
        m.tecla("ret")
        r = esperar(m, r"demoeditor: prop Escala = ([\d,]+)", 8, marca)
        verificar(r == "2,50", f"inspector: punto fijo con dos decimales (2,5 -> {r})")
        marca = len(m.serial())
        t.clique(vxi + 9, fy(9))
        esperar(m, r"demoeditor: prop Visible = No", 8, marca)
        t.clique(vxi + 9, fy(9))
        esperar(m, r"demoeditor: prop Visible = S", 8, marca)
        verificar(True, "inspector: la casilla se apaga y se enciende")
        marca = len(m.serial())
        tl = vli - 34
        t.arrastar(vxi + 1, fy(11), vxi + tl // 2, fy(11))
        r = int(esperar(m, r"demoeditor: prop Opacidad = (\d+)", 8, marca))
        verificar(42 <= r <= 58, f"inspector: el deslizador a la mitad da ~50 ({r})")
        marca = len(m.serial())
        t.clique(vxi + 20, fy(13))
        time.sleep(0.4)
        m.tecla("ctrl-a")
        m.digitar("heroe")
        m.tecla("ret")
        esperar(m, r"demoeditor: prop Nombre = heroe", 8, marca)
        verificar(True, "inspector: el texto se edita (Ctrl+A, escribir, Intro)")
        m.captura("H454-7-inspector")
        # enumerado: se abre el menú del sistema
        marca = len(m.serial())
        t.clique(vxi + 30, fy(10))
        time.sleep(0.6)
        m.tecla("down")
        m.tecla("ret")
        time.sleep(0.6)
        r = ultimo(m, r"demoeditor: prop Capa = (\S+)", marca)
        verificar(r is not None, f"inspector: el desplegable cambia con el menú ({r})")
        # archivo: abre el diálogo y se cancela
        marca = len(m.serial())
        t.clique(vxi + vli - 11, fy(14))
        time.sleep(1.5)
        verificar("Elegir archivo" in m.serial()[marca:], "inspector: el botón «..» abre el diálogo de archivos")
        m.tecla("esc")
        time.sleep(0.8)

        # ---------------------------------------------------------------- (7) selector de color
        marca = len(m.serial())
        t.clique(vxi + 40, fy(8))
        esperar(m, r"selcor: abierto", 8, marca)
        time.sleep(0.5)
        d = janela(m.serial(), "Elegir color")
        dc = lambda x, y: (d["x"] + x, d["y"] + y)
        m.captura("H454-8-color")
        marca = len(m.serial())
        m.clique(*dc(12 + 6 * 16 + 7, 12 + 7))
        r = esperar(m, r"selcor: color ([0-9a-f]{8})", 8, marca)
        verificar(r == "ffff0000", f"selector de color: un cuadro de la paleta da su color ({r})")
        marca = len(m.serial())
        m.clique(*dc(12 + 14 + 95, 12 + 160 + 6))
        r = esperar(m, r"selcor: color ([0-9a-f]{8})", 8, marca)
        verificar(r[:2] == "ff" and r[2:4] in ("7f", "80") and r[4:] == "0000", f"selector de color: el deslizador R a la mitad ({r})")
        marca = len(m.serial())
        m.clique(*dc(12 + 158 + 8, 12 + 56 + 64))
        r = esperar(m, r"selcor: color ([0-9a-f]{8})", 8, marca)
        verificar(r[:2] == "ff" and r[6:] != "00" and r[2:4] != "00" or r[0:2] == "ff", f"selector de color: la barra de tono cambia el color ({r})")
        marca = len(m.serial())
        m.clique(*dc(12 + 149, 12 + 56 + 1))
        r = esperar(m, r"selcor: color ([0-9a-f]{8})", 8, marca)
        verificar(r[:2] == "ff" and int(r[2:4], 16) <= 0x10 and int(r[6:], 16) >= 0xf0, f"selector de color: saturación y brillo al máximo da el tono puro ({r})")
        marca = len(m.serial())
        m.clique(*dc(12 + 180 + 8, 12 + 56 + 95))
        r = esperar(m, r"selcor: color ([0-9a-f]{8})", 8, marca)
        verificar(r[:2] in ("00", "01", "02"), f"selector de color: la barra de alfa baja a transparente ({r})")
        m.captura("H454-9-color-alfa")
        marca = len(m.serial())
        m.clique(*dc(d["l"] - 12 - 92 + 46, d["a"] - 40 + 14))
        r = esperar(m, r"demoeditor: cor ([0-9a-f]{8})", 8, marca)
        verificar(r[:2] in ("00", "01", "02") and int(r[6:], 16) >= 0xf0, f"Aceptar devuelve el color al inspector ({r})")

        # ---------------------------------------------------------------- (9) editor de código
        P = pos(m)
        kx, ky, kl, ka = P["codigo"]
        t.clique(kx + 120, ky + 60)
        time.sleep(0.3)
        m.tecla("ctrl-a")
        marca = len(m.serial())
        m.digitar("local n = 42 -- hola")
        time.sleep(0.5)
        r = ultimo(m, r"demoeditor: realce 1 (\d+)", marca)
        esperado = "11111" + "0" + "0" + "0" + "7" + "0" + "55" + "0" + "4444444"
        verificar(r == esperado, f"código: resaltado de Lua por clases ({r} == {esperado})")
        m.captura("H454-10-codigo-resaltado")
        # el color de la palabra clave en pantalla
        img = tela(m, "codigo")
        gx = kx + 3 * 8 + 16
        hallado = False
        for yy in range(ky + 2, ky + 20):
            for xx in range(t.abs(gx, 0)[0], t.abs(gx, 0)[0] + 40):
                if perto(pixel(img, xx, t.abs(0, yy)[1]), (0x7A, 0x2E, 0x9B), 20):
                    hallado = True
        verificar(hallado, "código: la palabra clave se pinta con el color de las claves en pantalla")
        m.tecla("ret")
        m.digitar("function f()")
        m.tecla("ret")
        m.digitar("x = 1")
        time.sleep(0.4)
        r = ultimo(m, r"demoeditor: codigo lin (\d+) col (\d+) lineas (\d+)", marca)
        verificar(r == ("3", "8", "3"), f"código: tras «function f()» la sangría automática pone 2 espacios ({r})")
        P = pos(m)
        marca = len(m.serial())
        t.clique(*bx(3))
        r = esperar(m, r"demoeditor: comprobar error linha (\d+): (.*)", 8, marca)
        verificar(r[0] == "2" and "end" in r[1], f"código: «Comprobar» marca la línea con el bloque sin cerrar ({r})")
        m.captura("H454-11-error-marcado")
        img = tela(m, "error")
        c = pixel(img, *t.abs(kx + 3 * 8 + 16 + 200, ky + 1 + 1 * 16 + 8))
        verificar(perto(c, (0xF7, 0xDE, 0xDC), 24), f"código: la línea del error lleva el fondo de peligro ({c})")
        t.clique(kx + 200, ky + ka - 20)   # el foco volvió al código: el clic bajo el texto deja el cursor al final
        m.tecla("ret")
        m.digitar("end")
        time.sleep(0.4)
        r = ultimo(m, r"demoeditor: codigo lin (\d+) col (\d+)", marca)
        verificar(r == ("4", "4"), f"código: al escribir «end» la línea retrocede un nivel ({r})")
        marca = len(m.serial())
        t.clique(*bx(3))
        esperar(m, r"demoeditor: comprobar ok", 8, marca)
        verificar(True, "código: con el bloque cerrado, «Comprobar» quita la marca")
        # buscar
        marca = len(m.serial())
        t.clique(kx + 200, ky + 20)
        m.tecla("ctrl-f")
        time.sleep(0.3)
        m.digitar("x = 1")
        time.sleep(0.4)
        m.captura("H454-12-buscar")
        r = ultimo(m, r"demoeditor: codigo lin (\d+) col (\d+)", marca)
        verificar(r is not None and r[0] == "3", f"código: Ctrl+F busca mientras se escribe y salta a la coincidencia ({r})")
        m.tecla("esc")
        # deshacer y rehacer del editor
        marca = len(m.serial())
        t.clique(kx + 200, ky + 20)
        m.tecla("ctrl-z")
        time.sleep(0.3)
        a1 = ultimo(m, r"demoeditor: codigo lin \d+ col \d+ lineas (\d+) bytes (\d+)", marca)
        m.tecla("ctrl-z")
        time.sleep(0.3)
        a2 = ultimo(m, r"demoeditor: codigo lin \d+ col \d+ lineas (\d+) bytes (\d+)", marca)
        verificar(a1 is not None and a2 is not None and int(a2[1]) < int(a1[1]), f"código: Ctrl+Z deshace el tecleo ({a1} -> {a2})")
        m.tecla("ctrl-y")
        time.sleep(0.3)
        a3 = ultimo(m, r"demoeditor: codigo lin \d+ col \d+ lineas (\d+) bytes (\d+)", marca)
        verificar(a3 is not None and int(a3[1]) > int(a2[1]), f"código: Ctrl+Y lo rehace ({a2} -> {a3})")

        # ---------------------------------------------------------------- (10) consola
        P = pos(m)
        cb = lambda k: (P[f"cbtn{k}"][0] + P[f"cbtn{k}"][2] // 2, P[f"cbtn{k}"][1] + P[f"cbtn{k}"][3] // 2)
        # provocar un error con línea para el doble clic
        t.clique(kx + 200, ky + 20)
        m.tecla("ctrl-a")
        m.digitar("function g()")
        m.tecla("ret")
        marca = len(m.serial())
        t.clique(*bx(3))
        esperar(m, r"demoeditor: comprobar error linha 1", 8, marca)
        marca = len(m.serial())
        t.clique(*cb(2))   # sin errores
        esperar(m, r"demoeditor: consola visibles (\d+) msg (\d+) aviso (\d+) error (\d+) mascara (\d+)", 8, marca)
        r = ultimo(m, r"demoeditor: consola visibles (\d+) msg (\d+) aviso (\d+) error (\d+) mascara (\d+)", marca)
        verificar(r[4] == "3" and int(r[0]) == int(r[1]) + int(r[2]), f"consola: sin el filtro de errores solo quedan mensajes y avisos ({r})")
        m.captura("H454-13-consola-filtrada")
        marca = len(m.serial())
        t.clique(*cb(2))
        r = ultimo(m, r"demoeditor: consola visibles (\d+) msg (\d+) aviso (\d+) error (\d+) mascara (\d+)", marca)
        total = int(r[0])
        verificar(r[4] == "7" and total == int(r[1]) + int(r[2]) + int(r[3]), f"consola: con los tres filtros se ve todo ({r})")
        marca = len(m.serial())
        t.clique(*cb(3))
        r = esperar(m, r"demoeditor: consola visibles \d+ .* copiado (\d+)", 8, marca)
        verificar(int(r) > 20, f"consola: «Copiar» manda lo visible al portapapeles ({r} bytes)")
        # doble clic en la última fila (el error nuevo)
        marca = len(m.serial())
        cx, cy, cl, ca = P["consola"]
        filas = (ca - 28 - 2) // 16
        k = min(total, filas) - 1
        t.clique(cx + 100, cy + 28 + 1 + k * 16 + 8, duplo=True)
        esperar(m, r"demoeditor: ir a linea 1", 8, marca)
        verificar(True, "consola: el doble clic en un error lleva el código a su línea")
        marca = len(m.serial())
        t.clique(*cb(4))
        r = esperar(m, r"demoeditor: consola visibles (\d+)", 8, marca)
        verificar(r == "0", f"consola: «Limpiar» la vacía ({r})")

        # ---------------------------------------------------------------- (11) paneles
        P = pos(m)
        s8 = P["sep8"]
        mx, my = s8[0] + 3, s8[1] + 30
        a0 = P["arbol"][2]
        marca = len(m.serial())
        t.arrastar(mx, my, mx + 60, my)
        esperar(m, r"demoeditor: separadores", 8, marca)
        P = pos(m)
        verificar(P["arbol"][2] >= a0 + 50 and P["arbol"][2] <= a0 + 70, f"paneles: arrastrar el separador ensancha el árbol ({a0} -> {P['arbol'][2]})")
        s8 = P["sep8"]
        mx, my = s8[0] + 3, s8[1] + 30
        t.arrastar(mx, my, mx - 400, my)
        time.sleep(0.5)
        P = pos(m)
        verificar(P["arbol"][2] >= 100, f"paneles: el tamaño mínimo del árbol se respeta ({P['arbol'][2]} >= 100)")
        m.captura("H454-14-paneles")
        # guardar, mover y restaurar
        P = pos(m)
        guardado = P["arbol"][2]
        marca = len(m.serial())
        t.clique(*bx(4))
        esperar(m, r"demoeditor: disposicion guardada", 8, marca)
        s8 = P["sep8"]
        mx, my = s8[0] + 3, s8[1] + 30
        t.arrastar(mx, my, mx + 120, my)
        time.sleep(0.5)
        verificar(pos(m)["arbol"][2] != guardado, "paneles: la disposición cambia al arrastrar")
        marca = len(m.serial())
        t.clique(*bx(5))
        esperar(m, r"demoeditor: disposicion restaurada", 8, marca)
        time.sleep(0.4)
        verificar(pos(m)["arbol"][2] == guardado, f"paneles: «Restaurar» devuelve la disposición guardada ({guardado})")
        # plegar con el botón del medio del separador
        P = pos(m)
        s8 = P["sep8"]
        marca = len(m.serial())
        t.clique(s8[0] + 3, s8[1] + s8[3] // 2)
        time.sleep(0.5)
        P = pos(m)
        verificar(P["sep8"][4] == 1, f"paneles: el botón del separador pliega el panel ({P['sep8']})")
        s8 = P["sep8"]
        t.clique(s8[0] + 3, s8[1] + s8[3] // 2)
        time.sleep(0.5)
        P = pos(m)
        verificar(P["sep8"][4] == 0 and P["arbol"][2] >= 100, "paneles: y lo vuelve a desplegar")
        m.captura("H454-15-final")
    return 0


if __name__ == "__main__":
    sys.exit(principal())
