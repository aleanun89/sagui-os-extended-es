#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H469 - componente de gráficos de la libsagui, módulo sagui_grafico y control Grafico de VisualPythonS (D469).

Criterio:
  0. en el PC: la parte de cálculo (tests/grafico_unidad.c: escalas 1-2-5, formato de números, apilado,
     histograma, curvas, colisiones de sector) compila y pasa;
  1. la autoprueba de vps (autoprueba_grafico.py, dentro de SaguiOS): escala y formato desde Python, los nueve
     tipos con los colores de la paleta contados por píxeles, la animación (las barras crecen con el tiempo y
     terminan), el ratón (resalte, tooltip con serie, categoría y valor, clic con índices, ocultar con la
     leyenda), datos en vivo con ventana deslizante, exportar a PNG y BMP, y el control (propiedades como
     texto, métodos, eventos, .vpf, el ejemplo y el autocompletado);
  2. el ejemplo «graficos» con el ratón y píxeles de la pantalla: se dibujan las barras (azul y naranja), la tarta
     (cuatro colores) y las líneas; la tarta se abre despacio (dos capturas distintas); el ratón encima de una
     barra enseña el tooltip; un clic escribe la línea «grafico-click: Grafico1 1 5 240» en el registro; pulsar la
     leyenda oculta la serie (menos azul/naranja) y escribe «grafico-leyenda»; «Nuevos datos» anima las barras;
     el gráfico en vivo cambia solo con su Temporizador; «Exportar» deja un PNG válido.
Capturas: docs/capturas/VPS-manual-grafico-*.png y H469-*.png
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA  # noqa: E402
from h303_vps_manual import (janela, esperar, cerrar, region, foto, autoprueba, lanzar, clic, reg)  # noqa: E402

AZUL, NARANJA, VERDE, ROJO = (0x2A, 0x6F, 0xD8), (0xE5, 0x83, 0x2B), (0x1E, 0x9E, 0x6B), (0xC8, 0x45, 0x3C)


def unidad():
    d = os.path.join(SAIDA, "h469")
    os.makedirs(d, exist_ok=True)
    exe = os.path.join(d, "grafico_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-I", os.path.join(RAIZ, "userland", "inc"), "-o", exe,
                        os.path.join(RAIZ, "tests", "grafico_unidad.c"),
                        os.path.join(RAIZ, "userland", "libsagui", "grafico_calc.c"), "-lm"], capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print(r.stdout)
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "cálculo: escalas, formato, apilado, bins, curvas y colisiones")


def cuenta(datos, color):
    return datos.count(bytes(color))


def neutro(m, j):
    m.mouse_para(j["x"] + 600, j["y"] - 12)
    time.sleep(0.3)


def principal():
    unidad()
    with Maquina("h469", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---------------------------------------------------------- 1. la autoprueba
        oks = autoprueba(m, "autoprueba_grafico.py", 40)
        for clave in ("calculo: escala 1-2-5", "calculo: formato de números", "barras: las dos series", "barras apiladas",
                      "barras_h: dibuja", "lineas: dibuja", "area: dibuja", "histograma: dibuja", "dispersion: dibuja",
                      "velas: verde si sube", "tarta: cuatro porciones", "anillo: las porciones",
                      "animación: las barras crecen", "animación: termina", "animación: al cambiar los datos",
                      "ratón: se resalta la barra", "ratón: tooltip con serie", "ratón: el clic da serie",
                      "leyenda: pulsarla oculta", "en vivo: la ventana deslizante", "exportar: PNG válido",
                      "exportar: BMP válido", "exportar: el PNG se lee", "control: tipo y series como texto",
                      "control: cambiar tipo por código", "control: agregar_punto con ventana", "control: exportar(ruta)",
                      "vpf: el control y su tipo", "ejemplo: construye los cuatro", "ejemplo: el temporizador alimenta",
                      "ejemplo: el clic escribe", "autocompletado: métodos"):
            verificar(any(clave in o for o in oks), "autoprueba: " + clave)

        # ---------------------------------------------------------- 2. el ejemplo con el ratón
        marca = len(m.serial())
        j = lanzar(m, "graficos", "Gr", espera=0.2)
        tarta0 = reg(m, j, "t0", 8, 256, 350, 220)          # enseguida: la tarta (2,5 s) se está abriendo
        time.sleep(4)
        neutro(m, j)
        tarta1 = reg(m, j, "t1", 8, 256, 350, 220)
        verificar(tarta0 != tarta1, "la tarta se abre despacio: dos capturas distintas")
        barras = reg(m, j, "b", 8, 8, 350, 240)
        verificar(cuenta(barras, AZUL) > 300 and cuenta(barras, NARANJA) > 300, "barras: azul (2025) y naranja (2026) de la paleta")
        verificar(all(cuenta(tarta1, c) > 300 for c in (AZUL, NARANJA, VERDE, ROJO)), "tarta: las cuatro porciones con sus colores")
        lineas = reg(m, j, "l", 366, 8, 340, 240)
        verificar(cuenta(lineas, AZUL) > 100 and cuenta(lineas, NARANJA) > 30, "líneas: las dos series")
        foto(m, "grafico-1", j)
        m.captura("H469-1-ejemplo")

        # tooltip: el ratón sobre la barra de Jun 2026 (la más alta)
        sin = reg(m, j, "h0", 8, 8, 350, 240)
        clic_x, clic_y = 8 + 320, 8 + 150
        m.mouse_para(j["x"] + clic_x, j["y"] + clic_y)
        time.sleep(0.8)
        con = reg(m, j, "h1", 8, 8, 350, 240)
        verificar(sin != con, "el ratón encima de una barra la resalta y enseña el tooltip")
        foto(m, "grafico-2", j)
        # clic: línea en el registro serie
        marca = len(m.serial())
        clic(m, j, clic_x, clic_y)
        esperar(m, "grafico-click: Grafico1 1 5 240", marca, 15)
        verificar(True, "clic en la barra: «grafico-click: Grafico1 1 5 240» (serie, categoría, valor)")
        # leyenda: ocultar «2025»
        neutro(m, j)
        antes = reg(m, j, "g0", 8, 8, 350, 240)
        marca = len(m.serial())
        clic(m, j, 8 + 127, 8 + 212)
        esperar(m, "grafico-leyenda: serie 0 oculta=True", marca, 15)
        time.sleep(1.0)
        neutro(m, j)
        despues = reg(m, j, "g1", 8, 8, 350, 240)
        verificar(cuenta(despues, AZUL) < cuenta(antes, AZUL) // 4 and cuenta(despues, NARANJA) > 300,
                  "leyenda: la serie 2025 (azul) desaparece y la otra sigue")
        foto(m, "grafico-3", j)
        clic(m, j, 8 + 127, 8 + 212)                  # vuelve a mostrarla
        time.sleep(1.0)
        neutro(m, j)
        verificar(cuenta(reg(m, j, "g2", 8, 8, 350, 240), AZUL) > 300, "leyenda: pulsarla otra vez la muestra")
        # nuevos datos: animación de las barras
        neutro(m, j)
        a0 = reg(m, j, "n0", 8, 8, 350, 240)
        clic(m, j, 474 + 60, 468 + 13)
        time.sleep(0.15)
        neutro(m, j)
        a1 = reg(m, j, "n1", 8, 8, 350, 240)
        time.sleep(1.5)
        neutro(m, j)
        a2 = reg(m, j, "n2", 8, 8, 350, 240)
        verificar(a0 != a2, "«Nuevos datos»: las barras pasan a los valores nuevos con animación")
        # en vivo
        v0 = reg(m, j, "v0", 366, 256, 340, 180)
        time.sleep(2.5)
        v1 = reg(m, j, "v1", 366, 256, 340, 180)
        verificar(v0 != v1 and cuenta(v1, AZUL) > 100, "gráfico en vivo: cambia solo con el Temporizador")
        foto(m, "grafico-4", j)
        # exportar
        marca = len(m.serial())
        clic(m, j, 366 + 50, 468 + 13)
        esperar(m, "grafico-exportado", marca, 15)
        time.sleep(0.5)
        cerrar(m, j)
        time.sleep(0.8)
        out = m.comando("python -c \"print('PNG-' + ('OK' if open('/tmp/grafico-ventas.png','rb').read(4)==b'\\x89PNG' else 'MAL'))\"")
        verificar("PNG-OK" in out, "exportar: /tmp/grafico-ventas.png es un PNG")
        m.comando("rm -f /tmp/grafico-ventas.png /tmp/grafico-aut.png /tmp/grafico-aut.bmp /tmp/grafico-ctl.png")


if __name__ == "__main__":
    principal()
    print("h469: todo bien")
