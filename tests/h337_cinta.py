#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H337 - la cinta (Ribbon) de la libsagui en Pluma y Macaco (mejora 132, D334).

Arranca a 800x600 y a 1024x768 y, en cada una, abre Pluma y Macaco con `--cinta-log` (la cinta anota por el
puerto serie la posición de sus controles y las órdenes que da). Comprueba:
  - la cinta sale con sus pestañas y sus grupos, y las capturas se guardan en docs/capturas/H337-*;
  - un clic en un botón grande, en uno pequeño y en uno desplegable da la orden esperada;
  - los KeyTips: Alt+letra de pestaña, la letra del control y Alt+. para los de las pestañas;
  - doble clic en una pestaña minimiza la cinta, y otro la restaura;
  - la reducción de grupos al estrechar (a 1024 se maximiza y se restaura la ventana; a 800 la ventana ya
    estrecha reduce el grupo de menor prioridad);
  - el modo clásico (--clasico) deja los menús de siempre.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def esperar(m, patron, tempo, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-1800:])


def janela(m, titulo, desde):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)', m.serial()[desde:])
    verificar(bool(g), f"ventana {titulo!r} creada")
    l, a, x, y = (int(v) for v in g[-1])
    return dict(l=l, a=a, x=x + 2, y=y + 24, xv=x, yv=y)


def bloque(m, desde):
    """El último bloque de layout que anotó la cinta: pestaña, grupos (con su nivel) y controles."""
    s = m.serial()[desde:]
    i = s.rfind("cinta: pestana ")
    verificar(i >= 0, "la cinta anotó su layout")
    b = {"pestana": re.match(r"cinta: pestana (\S+)", s[i:]).group(1), "grupos": {}, "ctrl": {}, "tab": {}, "archivo": None}
    for l in s[i:].splitlines()[1:]:
        g = re.search(r"cinta: grupo (.+) nivel=(\d+) x=(\d+) l=(\d+)", l)
        if g:
            b["grupos"][g.group(1)] = (int(g.group(2)), int(g.group(3)), int(g.group(4)))
            continue
        g = re.search(r"cinta: ctrl (\d+) (-?\d+) (-?\d+) (\d+) (\d+) (.+)", l)
        if g:
            b["ctrl"][g.group(6).strip()] = tuple(int(v) for v in g.groups()[:5])
            continue
        g = re.search(r"cinta: tab \d+ (\S+) (\d+) (\d+) (\d+)", l)
        if g:
            b["tab"][g.group(1)] = tuple(int(v) for v in g.groups()[1:])
            continue
        g = re.search(r"cinta: archivo (\d+) (\d+) (\d+)", l)
        if g:
            b["archivo"] = tuple(int(v) for v in g.groups())
    return b


def clic_ctrl(m, w, b, nombre, dy=None, dx=None, **kw):
    cid, x, y, l, a = b["ctrl"][nombre]
    m.clique(w["x"] + x + (l // 2 if dx is None else dx), w["y"] + y + (a // 2 if dy is None else dy), **kw)


def clic_tab(m, w, b, nombre, **kw):
    x, y, l = b["tab"][nombre]
    m.clique(w["x"] + x + l // 2, w["y"] + y + 11, **kw)


def ordenes(m, desde):
    return re.findall(r"cinta: orden (\d+) (.+)", m.serial()[desde:])


def esperar_orden(m, texto, desde, tempo=8):
    return esperar(m, rf"cinta: orden \d+ {re.escape(texto)}", tempo, desde)


def foco_terminal(m):
    """El botón «Terminal» de la barra de tareas devuelve el foco al shell."""
    m.clique(170, 12)
    time.sleep(1.0)


def probar_app(m, res, app, comando, titulo, grande, pequeno, tab_desp, desplegable, indice_menu, clasico_pestana):
    L = int(res.split("x")[0])
    n0 = len(m.serial())
    m.comando(comando + " --cinta-log &", 20)
    esperar(m, rf"{app}: pronto", 40, n0)
    time.sleep(2.5)
    w = janela(m, titulo, n0)
    verificar(w["l"] <= L - 8, f"{app} {res}: la ventana cabe en la pantalla ({w['l']} de {L})")
    b = bloque(m, n0)
    verificar(b["pestana"] == "Inicio" and "Inicio" in b["tab"] and "Insertar" in b["tab"] and "Vista" in b["tab"],
              f"{app} {res}: pestañas de la cinta {sorted(b['tab'])}")
    verificar(b["archivo"] is not None, f"{app} {res}: hay pestaña Archivo")
    verificar(len(b["grupos"]) >= 5, f"{app} {res}: grupos de Inicio {sorted(b['grupos'])}")
    for nombre, (nivel, x, l) in b["grupos"].items():
        verificar(x + l <= w["l"], f"{app} {res}: el grupo {nombre} cabe en la ventana (x={x} l={l} de {w['l']})")
    m.captura(f"H337-{app}-{res}-inicio")

    # ---- botón grande (Pegar: la parte del icono) y pequeño
    k = len(m.serial())
    clic_ctrl(m, w, b, grande, dy=20)
    esperar_orden(m, grande, k)
    verificar(True, f"{app} {res}: clic en el botón grande «{grande}»")
    k = len(m.serial())
    clic_ctrl(m, w, b, pequeno)
    esperar_orden(m, pequeno, k)
    verificar(True, f"{app} {res}: clic en el botón pequeño «{pequeno}»")

    # ---- pestañas
    clic_tab(m, w, b, tab_desp)
    time.sleep(0.8)
    b2 = bloque(m, n0)
    verificar(b2["pestana"] == tab_desp, f"{app} {res}: la pestaña {tab_desp} se abre ({b2['pestana']})")
    m.captura(f"H337-{app}-{res}-{tab_desp.lower()}")

    # ---- botón desplegable: se abre el menú y se elige una línea con el teclado
    k = len(m.serial())
    cid, x, y, l, a = b2["ctrl"][desplegable]
    m.clique(w["x"] + x + l // 2, w["y"] + y + a - 6)
    time.sleep(1.0)
    m.captura(f"H337-{app}-{res}-desplegable")
    for _ in range(indice_menu + 1):
        m.tecla("down", 0.15)
    m.tecla("ret", 0.5)
    esperar(m, r"cinta: orden \d+ ", 8, k)
    verificar(True, f"{app} {res}: el desplegable «{desplegable}» da su orden: {ordenes(m, k)[-1:]}")

    # ---- KeyTips: Alt+I (pestaña Inicio con las letras de sus controles) y la letra de «Pegar»
    clic_tab(m, w, b2, "Vista")
    time.sleep(0.6)
    k = len(m.serial())
    m.tecla("alt-i", 0.8)
    esperar(m, r"cinta: keytips 2 Inicio", 8, k)
    time.sleep(0.5)
    m.captura(f"H337-{app}-{res}-keytips")
    verificar(True, f"{app} {res}: Alt+I abre Inicio con sus KeyTips")
    k = len(m.serial())
    m.tecla("v", 0.8)
    esperar_orden(m, "Pegar", k)
    verificar(True, f"{app} {res}: el KeyTip V da la orden «Pegar»")
    # Alt+. enseña las letras de las pestañas; N elige Insertar
    k = len(m.serial())
    m.tecla("alt-dot", 0.8)
    esperar(m, r"cinta: keytips 1", 8, k)
    m.captura(f"H337-{app}-{res}-keytips-pestanas")
    m.tecla("n", 0.8)
    esperar(m, r"cinta: keytips 2 Insertar", 8, k)
    m.tecla("esc", 0.5)
    verificar(True, f"{app} {res}: Alt+. y N llevan a la pestaña Insertar")

    # ---- minimizar con doble clic en la pestaña y restaurar
    b3 = bloque(m, n0)
    k = len(m.serial())
    clic_tab(m, w, b3, b3["pestana"], duplo=True)
    esperar(m, r"cinta: minimizada", 8, k)
    time.sleep(0.8)
    m.captura(f"H337-{app}-{res}-minimizada")
    verificar(True, f"{app} {res}: doble clic en la pestaña minimiza la cinta")
    k = len(m.serial())
    clic_tab(m, w, b3, b3["pestana"], duplo=True)
    esperar(m, r"cinta: restaurada", 8, k)
    time.sleep(0.6)
    verificar(True, f"{app} {res}: otro doble clic la restaura")
    k = len(m.serial())
    m.tecla("ctrl-f1", 0.8)
    esperar(m, r"cinta: minimizada", 8, k)
    m.tecla("ctrl-f1", 0.8)
    esperar(m, r"cinta: restaurada", 8, k)
    verificar(True, f"{app} {res}: Ctrl+F1 minimiza y restaura")

    # ---- reducción de grupos al estrechar
    clic_tab(m, w, b, "Inicio")
    time.sleep(0.8)
    ultimo = bloque(m, n0)
    verificar(ultimo["pestana"] == "Inicio", f"{app} {res}: de vuelta en Inicio")
    niveles = {nom: v[0] for nom, v in ultimo["grupos"].items()}
    reducidos = [nom for nom, v in niveles.items() if v > 0]
    print(f"   {app} {res}: ventana de {w['l']} px, grupos reducidos: {reducidos or 'ninguno'}")
    if L >= 1024 and app == "pluma":
        # se maximiza (todo cabe, ningún grupo se reduce) y se restaura (vuelven a reducirse)
        k = len(m.serial())
        m.clique(w["xv"] + w["l"] + 2 - 35, w["yv"] + 12)
        time.sleep(2.5)
        bm = bloque(m, k)
        verificar(all(v[0] == 0 for v in bm["grupos"].values()), f"{app} {res}: maximizada, ningún grupo se reduce ({ {n: v[0] for n, v in bm['grupos'].items()} })")
        m.captura(f"H337-{app}-{res}-maximizada")
        k = len(m.serial())
        m.clique(L - 35, 36)          # maximizada: el botón de restaurar queda arriba a la derecha de la pantalla
        time.sleep(2.5)
        # volver a mostrar Inicio (tras maximizar la pestaña sigue siendo la misma)
        br = bloque(m, k)
        verificar(any(v[0] > 0 for v in br["grupos"].values()), f"{app} {res}: restaurada, el grupo de menor prioridad se reduce")
    else:
        verificar(len(reducidos) > 0, f"{app} {res}: con {w['l']} px se reduce algún grupo ({reducidos})")
    return w


def modo_clasico(m, res, app, comando, titulo):
    n0 = len(m.serial())
    m.comando(comando + " --clasico --cinta-log &", 20)
    esperar(m, rf"{app}: pronto", 40, n0)
    time.sleep(2.0)
    verificar("cinta: pestana" not in m.serial()[n0:], f"{app} {res}: con --clasico no sale la cinta")
    m.captura(f"H337-{app}-{res}-clasico")


def probar(res):
    img = imagem_de_teste("", ["--vesa", "--resolucao", res])
    with Maquina("h337-" + res, "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        probar_app(m, res, "pluma", "pluma", "Documento1 - Pluma", "Pegar", "Copiar", "Vista", "Zoom", 3, None)
        # el modo clásico de Pluma deja los menús de siempre
        foco_terminal(m)
        modo_clasico(m, res, "pluma", "pluma", "Documento1 - Pluma")
        foco_terminal(m)
        probar_app(m, res, "macaco", "macaco", "Macaco - Libro1", "Pegar", "Copiar", "Inicio", "Bordes", 1, None)
        foco_terminal(m)
        modo_clasico(m, res, "macaco", "macaco", "Macaco - Libro1")


def main():
    for res in (sys.argv[1:] or ("800x600", "1024x768")):
        probar(res)
    print("H337 OK")


if __name__ == "__main__":
    main()
