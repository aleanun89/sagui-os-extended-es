#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H423 - componentes modernos de la libsagui (D423, v8.5.5.2).

Criterio (`componentes --moderno`, datos sintéticos):
  (1) la demo abre y escribe la posición de cada componente;
  (2) botones: el primario, el secundario y el de icono responden al clic; el deshabilitado no;
  (3) el hover cambia el relleno de un botón plano y el tooltip sale tras el retardo (captura) y se va;
  (4) control segmentado: un clic cambia la vista y el seleccionado lleva el aro del color de acento;
  (5) interruptor: pulsarlo cambia de estado y su pista pasa al color de acento;
  (6) chip con «x»: se cierra; campo de búsqueda: teclear, borrar con la «x» y Enter;
  (7) toast: aparece abajo y se va solo, y deja lo que había debajo.
Las comprobaciones salen de la serie y de los colores de la pantalla; las capturas (H423-*) son de la demo.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA  # noqa: E402

ACENTO = (0x0B, 0x7A, 0x3E)


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
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def tela(m, nome):
    ppm = os.path.join(SAIDA, f"h423-{nome}.ppm")
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


def cercano(c, ref, tol=24):
    return all(abs(a - b) <= tol for a, b in zip(c, ref))


def posiciones(serial):
    d = {}
    for n, x, y, l, a in re.findall(r"componentes: pos (\w+) (\d+) (\d+) (\d+) (\d+)", serial):
        d[n] = tuple(int(v) for v in (x, y, l, a))
    return d


def centro(w, p):
    x, y, l, a = p
    return w["x"] + x + l // 2, w["y"] + y + a // 2


def principal():
    with Maquina("h423", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("componentes --moderno &")
        m.esperar_serial("componentes: moderno pronto", 20)
        time.sleep(1.0)
        w = janela(m.serial(), "Componentes modernos")
        pos = posiciones(m.serial())
        verificar(len(pos) >= 14, f"la demo anota la posición de sus componentes ({len(pos)})")
        m.captura("H423-1-demo")

        # ---- (2) botones
        marca = len(m.serial())
        m.clique(*centro(w, pos["boton0"]))
        esperar(m, r"componentes: boton 0\b", 8, desde=marca)
        m.clique(*centro(w, pos["boton1"]))
        esperar(m, r"componentes: boton 1\b", 8, desde=marca)
        m.clique(*centro(w, pos["boton4"]))
        esperar(m, r"componentes: boton 4\b", 8, desde=marca)
        marca = len(m.serial())
        m.clique(*centro(w, pos["boton7"]))
        time.sleep(0.6)
        verificar("componentes: boton 7" not in m.serial()[marca:], "el botón deshabilitado no responde")
        verificar(True, "los botones primario, secundario y de icono responden al clic")

        # ---- (3) hover y tooltip del botón plano «Opciones»
        x, y = centro(w, pos["boton2"])
        img0 = tela(m, "reposo")
        base = pixel(img0, w["x"] + pos["boton2"][0] + 3, y)
        m.mouse_para(x, y)
        time.sleep(0.25)
        img1 = tela(m, "hover")
        hov = pixel(img1, w["x"] + pos["boton2"][0] + 3, y)
        verificar(hov != base, f"el botón plano cambia de relleno con el ratón encima ({base} -> {hov})")
        time.sleep(0.9)
        m.captura("H423-2-tooltip")
        img2 = tela(m, "tooltip")
        ty = w["y"] + pos["boton2"][1] + pos["boton2"][3] + 12
        c = pixel(img2, x, ty)
        verificar(cercano(c, (0x26, 0x32, 0x38), 30), f"el tooltip aparece tras el retardo, debajo del botón ({c})")
        m.mouse_para(x, y + 140)
        time.sleep(0.4)
        c2 = pixel(tela(m, "sin-tooltip"), x, ty)
        verificar(not cercano(c2, (0x26, 0x32, 0x38), 30), "al irse el ratón, el tooltip desaparece y se restaura lo de debajo")

        # ---- (4) segmentado
        marca = len(m.serial())
        m.clique(*centro(w, pos["vista2"]))
        esperar(m, r"componentes: vista 2\b", 8, desde=marca)
        time.sleep(0.3)
        img = tela(m, "seg")
        vx, vy, vl, va = pos["vista2"]
        c = pixel(img, w["x"] + vx, w["y"] + vy + va // 2)
        verificar(cercano(c, ACENTO, 12), f"el segmento elegido lleva el aro del color de acento ({c})")
        m.mouse_para(w["x"] + 400, w["y"] + 470)
        m.captura("H423-3-segmentado")

        # ---- (5) interruptor «Avisos»
        marca = len(m.serial())
        sx, sy, sl, sa = pos["sw1"]
        m.clique(w["x"] + sx + 18, w["y"] + sy + 10)
        esperar(m, r"componentes: interruptor 1 sim", 8, desde=marca)
        time.sleep(0.5)
        img = tela(m, "sw")
        c = pixel(img, w["x"] + sx + 6, w["y"] + sy + 10)
        verificar(cercano(c, ACENTO, 30), f"el interruptor encendido tiene la pista de acento ({c})")

        # ---- (6) chip, búsqueda
        marca = len(m.serial())
        cx, cy, cl, ca = pos["chip3"]
        m.clique(w["x"] + cx + cl - 12, w["y"] + cy + ca // 2)
        esperar(m, r"componentes: chip fechado 3", 8, desde=marca)
        bx, by, bl, ba = pos["busca"]
        marca = len(m.serial())
        m.clique(w["x"] + bx + 60, w["y"] + by + ba // 2)
        m.digitar("informe", 0.06)
        esperar(m, r"componentes: busca 'informe'", 8, desde=marca)
        time.sleep(0.3)
        m.captura("H423-4-busqueda")
        marca = len(m.serial())
        m.clique(w["x"] + bx + bl - 17, w["y"] + by + ba // 2)
        esperar(m, r"componentes: busca limpiada", 8, desde=marca)
        verificar(True, "el campo de búsqueda: teclear y borrar con la «x»")

        # ---- (7) toast y tema oscuro
        marca = len(m.serial())
        m.clique(*centro(w, pos["boton9"]))
        esperar(m, r"componentes: toast", 8, desde=marca)
        time.sleep(0.4)
        m.captura("H423-5-toast")
        tx = w["x"] + w["l"] // 2
        ty = w["y"] + w["a"] - 14 - 16
        c = pixel(tela(m, "toast"), tx - 60, ty)
        verificar(cercano(c, (0x26, 0x32, 0x38), 30), f"el toast aparece abajo ({c})")
        time.sleep(2.6)
        c = pixel(tela(m, "sin-toast"), tx - 60, ty)
        verificar(not cercano(c, (0x26, 0x32, 0x38), 30), "el toast se va solo y deja lo que había debajo")
    return 0


if __name__ == "__main__":
    sys.exit(principal())
