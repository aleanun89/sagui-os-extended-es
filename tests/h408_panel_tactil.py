#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H408 (D407) - paneles táctiles: Synaptics, ALPS y Elantech en modo absoluto, con gestos.

QEMU no emula ningún panel táctil, así que la parte del panel se prueba en el PC y en QEMU se
comprueba que nada de lo de siempre se ha roto:

  unidad   tests/painel_unidad.c: paquetes construidos a mano (Synaptics con W, AGM y palanca; ALPS v2
           con palanca; Elantech v2, v3 y v4): reconocer el modelo por sus respuestas, decodificar
           (posición, presión, dedos, botones), y los gestos: mover con aceleración y sensibilidad,
           toque = clic (dos dedos derecho, tres central), doble toque para arrastrar, doble clic,
           desplazamiento con dos dedos (vertical, horizontal, sentido natural) y por el borde,
           palma, al escribir y panel apagado;
  qemu     el ratón PS/2 de QEMU (sin vmmouse) sigue igual: el reconocimiento del panel no lo toma
           por un panel, la rueda IntelliMouse sigue (id 3), el cursor va donde se le lleva, el clic
           y la rueda llegan; «panel» y «touchpad» dicen que no hay panel y cambian las opciones en
           el kernel; «panel ps2» sin panel no reinicia nada y el ratón sigue; la Configuración
           (sección Teclado) enseña el bloque «Panel táctil».

Uso: h408_panel_tactil.py [unidad] [qemu]
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, SAIDA, RAIZ  # noqa: E402


def unidad():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "painel_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-o", exe, os.path.join(RAIZ, "tests", "painel_unidad.c")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila sin avisos\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True, timeout=60)
    print("   ", r.stdout.strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "decodificación y gestos de Synaptics, ALPS y Elantech")


def sin_excepciones(s, que):
    verificar("EXCECAO" not in s and "PANICO" not in s, f"{que}: sin excepciones")


def clic_en(m, x, y, desde):
    """Clic en (x, y); devuelve dónde dice el kernel que se pulsó."""
    m.clique(x, y)
    g = re.findall(r"\[MOUSE\] botao esquerdo apertado em \((\d+),(\d+)\)", m.serial()[desde:])
    return (int(g[-1][0]), int(g[-1][1])) if g else None


def qemu():
    with Maquina("h408", "ide") as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(1)
        s = m.serial()
        sin_excepciones(s, "arranque")
        verificar(re.search(r"\[I8042\] .*-> ligado", s) is not None, "el i8042 dice que el ratón respondió")
        verificar("IntelliMouse (id 3)" in s, "la rueda IntelliMouse se reconoce como antes (id 3, paquetes de 4 bytes)")
        verificar("modo absoluto" not in s, "el ratón de QEMU no se toma por un panel táctil")
        verificar(re.search(r"\[PAINEL\] (Synaptics|ALPS|Elantech)", s) is None,
                  "ninguna respuesta de QEMU parece de Synaptics, ALPS o Elantech")

        # el ratón: posición, clic y rueda como siempre
        d = len(m.serial())
        p = clic_en(m, 400, 300, d)
        verificar(p is not None and abs(p[0] - 400) <= 2 and abs(p[1] - 300) <= 2, f"clic en (400,300): el kernel dice {p}")
        d = len(m.serial())
        m.monitor("mouse_move 0 0 -1")
        time.sleep(0.6)
        verificar(re.search(r"\[MOUSE\] roda 1 em", m.serial()[d:]) is not None, "la rueda llega (una muesca hacia abajo)")

        # la orden de consola
        out = m.comando("panel")
        verificar("no se ha reconocido ninguno" in out, "panel: no hay panel táctil en QEMU\n" + out)
        verificar("sensibilidad=5" in out and "toque=1" in out, "panel: las opciones de serie\n" + out)
        d = len(m.serial())
        out = m.comando("panel sensibilidad=7 borde=1 escribir_ms=500")
        verificar("panel: aplicado (0)" in out, "panel CLAVE=N: aplicado en el kernel\n" + out)
        verificar(re.search(r"\[PAINEL\] opciones: .*borde 1.*sensibilidad 7, al escribir 500 ms", m.serial()[d:]) is not None,
                  "el kernel recibe las opciones")
        out = m.comando("touchpad")
        verificar("sensibilidad=7" in out and "borde=1" in out and "escribir_ms=500" in out,
                  "touchpad (el mismo programa) ve lo cambiado\n" + out)
        out = m.comando("panel sensibilidad=99")
        verificar("sensibilidad=10" in m.comando("panel"), "sensibilidad fuera de rango: se queda en 10")
        out = m.comando("panel tocar=1")
        verificar("no es CLAVE=N" in out, "una clave que no existe se rechaza\n" + out)
        d = len(m.serial())
        out = m.comando("panel ps2")
        verificar("panel: aplicado (0)" in out, "panel ps2\n" + out)
        verificar("raton reiniciado" not in m.serial()[d:], "sin panel reconocido, «panel ps2» no reinicia el ratón")
        p = clic_en(m, 200, 150, d)
        verificar(p is not None and abs(p[0] - 200) <= 2 and abs(p[1] - 150) <= 2, f"tras «panel ps2» el ratón sigue: {p}")
        m.comando("panel restablecer")
        verificar("sensibilidad=5" in m.comando("panel"), "panel restablecer")

        # la Configuración enseña el bloque
        d = len(m.serial())
        m.comando("configuracion --ventana teclado &", tempo=30)
        fin = time.time() + 40
        while time.time() < fin and "bloque Panel tactil" not in m.serial()[d:]:
            time.sleep(0.5)
        verificar(re.search(r"configuracion: bloque Panel tactil \(ninguno, raton PS/2\)", m.serial()[d:]) is not None,
                  "la Configuración (Teclado) dibuja el bloque «Panel táctil»")
        time.sleep(1.5)
        m.captura("H408-configuracion-panel")
        sin_excepciones(m.serial(), "final")


def main():
    partes = sys.argv[1:] or ["unidad", "qemu"]
    if "unidad" in partes:
        print("== unidad")
        unidad()
    if "qemu" in partes:
        print("== qemu")
        qemu()
    print("H408 OK")


if __name__ == "__main__":
    main()
