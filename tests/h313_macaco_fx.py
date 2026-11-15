#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H313 - Macaco: asistente «Insertar función», catálogo de funciones, autocompletado y ayuda de
argumentos (mejora 117, D311, subsección «Asistente de funciones»).

Criterio:
  - `macaco --fx` (sin ventana) evalúa el ejemplo de cada función del catálogo y unas fórmulas de
    precedencia, texto y errores: ningún fallo;
  - fx abre el asistente (paso 1); buscar «suma» y elegir SUMA lleva al paso 2; contraer, señalar B1:B3 con
    el ratón y volver muestra el valor en vivo (`= {10;20;30}` y resultado 60); Aceptar deja =SUMA(B1:B3);
  - anidar: `=SI(` + Mayús+F3 + SUMA(B1:B3) + `>10;"alto";"bajo")` da «alto»;
  - reabrir el asistente sobre la celda con una función lo abre en el paso 2;
  - autocompletado: `=BUS` + Tab da `=BUSCARV(`; ayuda de argumentos al abrir el paréntesis;
  - el detector de la GUI (D300) no avisa de nada.
Capturas H313-*.png.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402

GRID_X, GRID_Y = 40, 118
COL_W, ROW_H = 72, 20


def janela(serial, titulo):
    achada = re.findall(rf'criada por \d+: "{re.escape(titulo)}[^"]*" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)
    m = achada[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24)


def celula(a, l, c):
    return (a["x"] + GRID_X + c * COL_W + COL_W // 2, a["y"] + GRID_Y + l * ROW_H + ROW_H // 2)


def boton_fx(a):
    return (a["x"] + 122, a["y"] + 88)


def escrever(m, texto):
    m.digitar(texto, 0.05)
    m.tecla("ret", 0.25)


def ir_a(m, a, l, c):
    m.clique(*celula(a, l, c))


def esperar_desde(m, texto, desde, tempo=15):
    """Espera `texto` en la serial a partir de la posición `desde` (los textos de los pasos se repiten)."""
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return
        time.sleep(0.2)
    raise AssertionError("tiempo esgotado esperando %r" % texto)


def fase_catalogo(m):
    out = m.comando("macaco --fx", tempo=60)
    for l in out.splitlines():
        if "macaco: fx" in l and ("FALLO" in l or "total" in l or "catalogo" in l):
            print("   " + l.strip()[:150])
    verificar("macaco: fx total ok=" in out, "la autoprueba del catálogo termina")
    verificar("fallos=0" in out and "FALLO" not in out, "ningún ejemplo del catálogo ni fórmula extra falla")
    n = int(re.search(r"catalogo (\d+) funciones", out).group(1))
    verificar(n >= 40, "el catálogo tiene %d funciones" % n)
    for alias in ("SUM", "IF", "VLOOKUP", "COUNTIF", "IFERROR", "CONCATENATE", "MEDIAN", "TODAY", "YEAR", "INDEX", "MATCH"):
        verificar(("fx %s " % alias) in out, "el catálogo evalúa %s" % alias)


def fase_gui(m, a):
    # ---- datos: A1:A3 = 10, 20, 30 y B1:B3 = 10, 20, 30
    ir_a(m, a, 0, 0)
    for v in ("10", "20", "30"):
        escrever(m, v)
    ir_a(m, a, 0, 1)
    for v in ("10", "20", "30"):
        escrever(m, v)

    # ---- E1: fx -> paso 1, buscar «suma», elegir SUMA -> paso 2
    ir_a(m, a, 0, 4)
    marca = len(m.serial())
    m.clique(*boton_fx(a))
    m.esperar_serial("macaco: funcion paso 1 elegir", 15)
    time.sleep(0.6)
    m.captura("H313-1-elegir")
    m.digitar("suma", 0.1)
    time.sleep(0.4)
    m.captura("H313-2-buscar-suma")
    m.tecla("ret", 0.5)
    m.esperar_serial("macaco: funcion paso 2 SUM", 15)
    time.sleep(0.5)
    w = janela(m.serial(), "Insertar función")

    # contraer el primer argumento y señalar B1:B3 arrastrando
    marca = len(m.serial())
    m.clique(w["x"] + 391, w["y"] + 70)
    m.esperar_serial("macaco: funcion contraido SUMA", 15)
    time.sleep(0.5)
    m.mouse_para(*celula(a, 0, 1))
    m.monitor("mouse_button 1")
    time.sleep(0.2)
    m.mouse_mover(*celula(a, 2, 1))
    time.sleep(0.4)
    m.captura("H313-3-contraido-senalar")
    m.monitor("mouse_button 0")
    time.sleep(0.4)
    m.tecla("ret", 0.6)
    m.esperar_serial("macaco: funcion senalado B1:B3", 15)
    verificar("macaco: funcion senalado B1:B3" in m.serial()[marca:], "se señala B1:B3 con el ratón")
    time.sleep(0.8)
    m.captura("H313-4-argumentos")
    ser = m.serial()[marca:]
    verificar("macaco: funcion vivo =SUMA(B1:B3) -> 60 | arg1 = {10;20;30}" in ser,
              "el asistente muestra en vivo el valor del argumento ({10;20;30}) y el resultado (60)")
    m.tecla("ret", 0.5)                                     # Aceptar (Intro en el cuadro)
    m.esperar_serial("celula E1 = 60 (=SUMA(B1:B3))", 15)

    # ---- F1: anidar =SI(SUMA(B1:B3)>10;"alto";"bajo")
    ir_a(m, a, 0, 5)
    m.digitar("=SI(", 0.05)
    marca = len(m.serial())
    m.tecla("shift-f3", 0.5)
    esperar_desde(m, "macaco: funcion paso 1 elegir", marca)
    m.digitar("suma", 0.1)
    m.tecla("ret", 0.5)
    time.sleep(0.5)
    m.digitar("B1:B3", 0.05)
    time.sleep(0.5)
    m.tecla("ret", 0.5)
    time.sleep(0.5)
    m.digitar('>10;"alto";"bajo")', 0.05)
    time.sleep(0.3)
    m.tecla("ret", 0.4)
    m.esperar_serial('celula F1 = alto (=SI(SUMA(B1:B3)>10;"alto";"bajo"))', 15)

    # ---- reabrir sobre E1: se abre en el paso 2 con sus argumentos
    marca = len(m.serial())
    ir_a(m, a, 0, 4)
    m.clique(*boton_fx(a))
    esperar_desde(m, "macaco: funcion paso 2 SUM", marca)
    verificar("funcion paso 1" not in m.serial()[marca:], "sobre una celda con función el asistente se abre en el paso 2")
    time.sleep(0.8)
    m.captura("H313-5-reabierto")
    verificar("arg1 = {10;20;30}" in m.serial()[marca:], "…con el argumento B1:B3 ya puesto")
    m.tecla("esc", 0.6)

    # ---- autocompletado: =BUS + Tab
    ir_a(m, a, 0, 6)
    m.digitar("=BUS", 0.1)
    time.sleep(0.5)
    m.captura("H313-6-autocompletar")
    m.tecla("tab", 0.5)
    m.esperar_serial("macaco: autocompletado VLOOKUP", 15)
    marca = len(m.serial())
    m.digitar("20;", 0.1)
    time.sleep(0.3)
    m.digitar("A1:B3;", 0.1)
    time.sleep(0.5)
    m.captura("H313-7-ayuda-argumentos")
    ser = m.serial()
    verificar("macaco: ayuda VLOOKUP argumento 1" in ser and "macaco: ayuda VLOOKUP argumento 3" in ser[marca:],
              "la ayuda de argumentos sigue el argumento actual (1 ... 3)")
    m.digitar("2;0)", 0.1)
    m.tecla("ret", 0.4)
    m.esperar_serial("celula G1 = 20 (=BUSCARV(20;A1:B3;2;0))", 15)


def principal():
    with Maquina("h313", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        fase_catalogo(m)
        m.digitar("macaco\n", 0.05)
        m.esperar_serial("macaco: pronto", 30)
        time.sleep(1.5)
        a = janela(m.serial(), "Macaco")
        fase_gui(m, a)

        m.tecla("ctrl-s", 0.5)
        m.esperar_serial('"Guardar como"', 15)
        time.sleep(0.5)
        for _ in range(30):
            m.tecla("backspace", 0.03)
        m.digitar("/tmp/h313.mxs", 0.05)
        m.tecla("ret", 0.5)
        m.esperar_serial("macaco: guardado /tmp/h313.mxs", 20)

        # ---- D300: el detector de problemas de la GUI no ve nada en Macaco ni en el asistente
        gr = [l for l in m.serial().splitlines() if "[GUI-REVISAR]" in l and "detector activo" not in l and "cobertura" not in l]
        av = [l for l in gr if "macaco" in l.lower() or "Insertar función" in l]
        print("   avisos GUI-REVISAR de Macaco: %d" % len(av))
        for l in av[:12]:
            print("     " + l[:160])

        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("macaco: cerrado", 20)
        time.sleep(0.8)
        out = m.comando("macaco -t /tmp/h313.mxs")
        for ref, val, f in (("E1", "60", "=SUMA(B1:B3)"), ("F1", "alto", '=SI(SUMA(B1:B3)>10;"alto";"bajo")'),
                            ("G1", "20", "=BUSCARV(20;A1:B3;2;0)")):
            verificar(("celula Hoja1!%s = %s [%s]" % (ref, val, f)) in out, "%s = %s (%s) tras guardar y reabrir" % (ref, val, f))
        verificar(not av, "el detector de la GUI (D300) no avisa de nada en Macaco ni en el asistente de funciones")
    return 0


if __name__ == "__main__":
    sys.exit(principal())
