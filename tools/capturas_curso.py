#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
capturas_curso.py - las capturas de pantalla del curso «Fundamentos de Sistemas Operativos» de la Ayuda.

Uso: tools/capturas_curso.py [carpeta_de_salida]   (por defecto rootfs/ayuda/sistemas-operativos/img)

Arranca la imagen de prueba en QEMU (sin KVM, para que el PC salga como un «QEMU Virtual CPU» y no
con el procesador de quien lo ejecute), abre cada aplicación, la recorta de la pantalla y graba el PNG:
  cap-terminal-ps.png   ps y mem en el Terminal
  cap-tareas.png        Administrador de tareas
  cap-visorlogs.png     Visor de registros con el arranque
  cap-discos.png        Discos
  cap-infopc.png        Información del PC
Todo es sintético: la máquina virtual de pruebas, sin cuentas ni datos de nadie. Hay que compilar antes
(`make`) para que build/saguios.img exista.
"""
import os
import re
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
sys.path.insert(0, os.path.join(RAIZ, "tests"))
sys.path.insert(0, AQUI)
from qemu import Maquina, SAIDA  # noqa: E402
from diagramas_so import escribir_png  # noqa: E402


def leer_ppm(ruta):
    d = open(ruta, "rb").read()
    partes = d.split(None, 4)
    w, h = int(partes[1]), int(partes[2])
    return w, h, d[len(d) - w * h * 3:]


def janela(serial, titulo):
    ms = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)
    if not ms:
        raise RuntimeError("no se abrió la ventana " + titulo)
    return tuple(int(v) for v in ms[-1])   # ancho, alto, x, y


def recortar(m, nombre, g, destino, extra=(0, 0, 0, 0)):
    """Recorta la ventana (con su marco y su barra de título) de la pantalla y la graba en PNG."""
    w, h, x, y = g
    # el cursor, en una esquina que no caiga dentro del recorte
    for cx, cy in ((1012, 756), (4, 756), (1012, 400), (4, 400)):
        if not (x - 4 <= cx <= x + w + 8 and y - 4 <= cy <= y + h + 30):
            m.mouse_para(cx, cy)
            break
    time.sleep(0.8)
    ppm = os.path.join(SAIDA, "capcurso-" + nombre + ".ppm")
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    pw, ph, rgb = leer_ppm(ppm)
    os.remove(ppm)
    x0, y0 = max(0, x - 1), max(0, y - 1)
    x1, y1 = min(pw, x + w + 5), min(ph, y + h + 26)
    filas = b"".join(rgb[(yy * pw + x0) * 3:(yy * pw + x1) * 3] for yy in range(y0, y1))
    n = escribir_png(os.path.join(destino, nombre + ".png"), x1 - x0, y1 - y0, filas)
    print("%-22s %4dx%-4d %6d bytes" % (nombre + ".png", x1 - x0, y1 - y0, n))


def lanzar(m, orden):
    """Escribe la orden y la ejecuta sin esperar el prompt (las aplicaciones gráficas lo tardan)."""
    m.digitar(orden, 0.05)
    m.tecla("ret")


def cerrar_y_volver(m, term):
    """Cierra la ventana con el foco (Alt+F4) y devuelve el foco al Terminal."""
    m.tecla("alt-f4")
    time.sleep(1.5)
    m.clique(term[2] + 200, term[3] + 200)   # un clic en el cuerpo del Terminal
    time.sleep(0.8)


def main():
    destino = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "rootfs", "ayuda", "sistemas-operativos", "img")
    os.makedirs(destino, exist_ok=True)
    with Maquina("capcurso", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(2)
        # Terminal: ps y mem
        m.comando("clear")
        m.comando("ps")
        m.comando("mem")
        time.sleep(1)
        term = janela(m.serial(), "Terminal")
        recortar(m, "cap-terminal-ps", term, destino)
        # Administrador de tareas
        lanzar(m, "tareas")
        time.sleep(3)
        recortar(m, "cap-tareas", janela(m.serial(), "Administrador de tareas"), destino)
        cerrar_y_volver(m, term)
        # Visor de registros (el arranque)
        lanzar(m, "visorlogs")
        time.sleep(3)
        m.tecla("ctrl-home")    # el principio del registro: el arranque
        time.sleep(1)
        recortar(m, "cap-visorlogs", janela(m.serial(), "Visor de registros"), destino)
        cerrar_y_volver(m, term)
        # Discos
        lanzar(m, "discos --ventana")
        time.sleep(3)
        recortar(m, "cap-discos", janela(m.serial(), "Discos"), destino)
        cerrar_y_volver(m, term)
        # Información del PC
        lanzar(m, "infopc")
        time.sleep(4)
        recortar(m, "cap-infopc", janela(m.serial(), "Información del PC"), destino)


if __name__ == "__main__":
    main()
