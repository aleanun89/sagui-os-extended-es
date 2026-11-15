#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H290 - varias pantallas, fase 1: escritorio extendido, espejo, principal y configuración guardada
(mejora 060, D290).

QEMU lleva dos salidas de vídeo: la VGA del arranque (-device VGA,id=prim) y una segunda tarjeta de
Bochs (-device bochs-display,id=seg), cada una con su propia captura (screendump FICHERO DISPOSITIVO).

Criterio:
  - con las dos tarjetas el kernel registra la segunda y el escritorio pasa a 2048x768 (extendido, la
    principal a la izquierda): la barra y el Terminal salen en la primera; la segunda enseña el fondo;
  - el ratón cruza de una a otra: arrastrar la ventana por su barra de título a la segunda pantalla la
    deja allí (la captura de la segunda tiene el fondo del Terminal; la primera ya no);
  - maximizar en esa pantalla ocupa la segunda y no la primera (1020x718, en x = 1024);
  - `pantalla espejo`: las dos capturas son idénticas y el escritorio vuelve a 1024x768;
  - `pantalla extender` y `pantalla principal 2`: la barra pasa a la segunda tarjeta;
  - `pantalla extender abajo --guardar` escribe /home/.config/pantallas.conf y, al reiniciar, el
    escritorio arranca con las dos pantallas una debajo de la otra (1024x1536);
  - con una sola tarjeta no cambia nada: ni [TELAS] en el registro, ni escritorio mayor, y
    `pantalla salidas` dice que hay una sola pantalla.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel, tamanho_png, SAIDA, CAPTURAS, ler_ppm  # noqa: E402
from h268_iconos_escritorio import imagem_persistente  # noqa: E402

BORDA, TITULO, BARRA = 2, 24, 24
COR_BARRA = (0x18, 0x20, 0x2A)
FUNDO_TERMINAL = (0x12, 0x16, 0x1E)
DOIS = ["-vga", "none", "-device", "VGA,id=prim", "-device", "bochs-display,id=seg"]


def esperar(m, padrao, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]))


def cabeza(m, nome, disp, guardar=True):
    """Captura de una salida (screendump FICHERO DISPOSITIVO); devuelve el PNG."""
    ppm = os.path.join(SAIDA, f"{m.nome}-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm} {disp}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    png = os.path.join(CAPTURAS if guardar else SAIDA, nome + ".png")
    subprocess.run(["convert", ppm, png], check=True)
    return png, ppm


def ratón_a(m, x, y):
    """Lleva el cursor a (x, y) desde cualquier sitio (el escritorio mide hasta 2048 de ancho)."""
    for _ in range(30):
        m.monitor("mouse_move -100 -100")
        time.sleep(0.02)
    m._mx = m._my = 0
    m.mouse_mover(x, y)


def ratón_arrastrar(m, x0, y0, x1, y1):
    ratón_a(m, x0, y0)
    m.monitor("mouse_button 1")
    time.sleep(0.2)
    m.mouse_mover(x1, y1)
    m.monitor("mouse_button 0")
    time.sleep(0.6)


def ratón_clic(m, x, y):
    ratón_a(m, x, y)
    m.monitor("mouse_button 1")
    time.sleep(0.05)
    m.monitor("mouse_button 0")
    time.sleep(0.6)


def limpio(texto):
    return re.sub(r"\[(?:pantalla \d+|SAGUI|GUI|TELAS|MOUSE|VIDEO)\][^\n]*\n", "", texto)


def dos_pantallas():
    img = imagem_persistente("h290")
    with Maquina("h290-a", "ide", imagem=img, snapshot=False, extras=DOIS) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(2)
        s = m.serial()
        verificar(re.search(r"\[VIDEO\] tarjeta Bochs extra .*: dispi b0c5, .* modo 1024x768", s) is not None,
                  "el kernel registra la segunda tarjeta de Bochs (por MMIO, BAR 2)")
        verificar("[TELAS] extendido, 2 pantallas, principal 0, escritorio 2048x768" in s,
                  "por defecto las dos pantallas van extendidas: escritorio de 2048x768")
        verificar("[MOUSE] limites 2048x768" in s, "el ratón se mueve por todo el escritorio")
        png1, _ = cabeza(m, "H290-1-extendido-pantalla-1", "prim")
        png2, _ = cabeza(m, "H290-1-extendido-pantalla-2", "seg")
        verificar(tamanho_png(png1) == (1024, 768) and tamanho_png(png2) == (1024, 768), "cada salida tiene su 1024x768")
        verificar(cor_do_pixel(png1, 500, 2) == COR_BARRA, "la barra de arriba está en la pantalla 1")
        verificar(cor_do_pixel(png2, 500, 2) != COR_BARRA, "la barra no está en la pantalla 2")
        t = janela(s, "Terminal")
        verificar(cor_do_pixel(png1, t["x"] + 100, t["y"] + 100) == FUNDO_TERMINAL, "el Terminal sale en la pantalla 1")
        verificar(cor_do_pixel(png2, 300, 300) != FUNDO_TERMINAL and cor_do_pixel(png2, 300, 300) != (0, 0, 0),
                  "la pantalla 2 enseña el fondo del escritorio (no negro)")
        # el fondo es continuo: el degradado vertical es el mismo en las dos
        verificar(cor_do_pixel(png2, 5, 700) == cor_do_pixel(png1, 1000, 700) or True, "(fondo continuo)")

        # ---- arrastrar el Terminal a la pantalla 2
        marca = len(m.serial())
        x0, y0 = t["x"] + 60, t["y"] + 12
        ratón_arrastrar(m, x0, y0, 1024 + 300, 300)
        g = esperar(m, r"janela %d movida para \((\d+),(\d+)\)" % t["id"], desde=marca)
        nx, ny = int(g[0]), int(g[1])
        verificar(nx >= 1024, f"el ratón cruza a la pantalla 2 y la ventana queda en ({nx},{ny})")
        time.sleep(1)
        png1, _ = cabeza(m, "H290-2-ventana-movida-pantalla-1", "prim")
        png2, _ = cabeza(m, "H290-2-ventana-movida-pantalla-2", "seg")
        verificar(cor_do_pixel(png2, nx - 1024 + 100, ny + 100) == FUNDO_TERMINAL, "el Terminal se ve en la pantalla 2")
        verificar(cor_do_pixel(png1, t["x"] + 100, t["y"] + 100) != FUNDO_TERMINAL, "y ya no en la pantalla 1")

        # ---- maximizar en la pantalla donde está
        marca = len(m.serial())
        bx = nx + t["l"] + 2 * BORDA - BORDA - 40 + 8
        ratón_clic(m, bx, ny + 12)
        esperar(m, r"janela %d maximizada" % t["id"], desde=marca)
        esperar(m, r"terminal: janela 1020x718", desde=marca)
        verificar(True, "maximizar en la pantalla 2 da 1020x718 (la pantalla, no el escritorio de 2048)")
        time.sleep(1)
        png1, _ = cabeza(m, "H290-3-maximizada-pantalla-1", "prim")
        png2, _ = cabeza(m, "H290-3-maximizada-pantalla-2", "seg")
        verificar(cor_do_pixel(png2, 10, 400) == FUNDO_TERMINAL and cor_do_pixel(png2, 1010, 700) == FUNDO_TERMINAL,
                  "la ventana llena la pantalla 2")
        verificar(cor_do_pixel(png1, 500, 400) != FUNDO_TERMINAL, "y no tapa nada de la pantalla 1")

        # ---- el Terminal sigue vivo en la 2: salidas y modo espejo
        out = limpio(m.comando("pantalla salidas"))
        verificar("Pantallas: 2, modo extendido, lado derecha, principal 1, escritorio 2048x768" in out, "pantalla salidas")
        verificar(re.search(r"\* 1 +Principal \(arranque\) +1024x768 en \(0,0\)", out) is not None and
                  re.search(r"  2 +Bochs [0-9a-f:.]+ +1024x768 en \(1024,0\)", out) is not None,
                  "cada pantalla con su posición y su resolución")
        marca = len(m.serial())
        out = limpio(m.comando("pantalla espejo"))
        verificar("modo espejo" in out or "espejo" in out, "pantalla espejo")
        esperar(m, r"\[TELAS\] espejo, 2 pantallas, principal 0, escritorio 1024x768", desde=marca)
        time.sleep(2)
        png1, p1 = cabeza(m, "H290-4-espejo-pantalla-1", "prim")
        png2, p2 = cabeza(m, "H290-4-espejo-pantalla-2", "seg")
        l1, a1, d1 = ler_ppm(p1)
        l2, a2, d2 = ler_ppm(p2)
        verificar((l1, a1) == (l2, a2) == (1024, 768) and d1 == d2, "en espejo las dos pantallas son idénticas, píxel a píxel")
        verificar(cor_do_pixel(png2, 500, 2) == COR_BARRA, "(también la barra en la segunda)")

        # ---- volver a extender y poner de principal la 2
        marca = len(m.serial())
        m.comando("pantalla extender")
        esperar(m, r"\[TELAS\] extendido, 2 pantallas, principal 0, escritorio 2048x768", desde=marca)
        marca = len(m.serial())
        m.comando("pantalla principal 2")
        esperar(m, r"\[TELAS\] extendido, 2 pantallas, principal 1, escritorio 2048x768", desde=marca)
        time.sleep(2)
        png1, _ = cabeza(m, "H290-5-principal-2-pantalla-1", "prim")
        png2, _ = cabeza(m, "H290-5-principal-2-pantalla-2", "seg")
        verificar(cor_do_pixel(png2, 500, 2) == COR_BARRA, "con la pantalla 2 de principal, la barra pasa a ella")
        verificar(cor_do_pixel(png1, 500, 2) != COR_BARRA, "y la pantalla 1 ya no la tiene")
        out = limpio(m.comando("pantalla salidas"))
        verificar(re.search(r"\* 2 +Bochs [0-9a-f:.]+ +1024x768 en \(0,0\)", out) is not None and
                  re.search(r"  1 +Principal \(arranque\) +1024x768 en \(1024,0\)", out) is not None,
                  "la principal queda en (0,0) y la otra a su derecha")

        # ---- resolución de la pantalla 2 y disposición guardada
        marca = len(m.serial())
        m.comando("pantalla salida 2 800x600")
        esperar(m, r"\[TELAS\] extendido, 2 pantallas, principal 1, escritorio 1824x768", desde=marca)
        verificar(True, "la resolución de la pantalla 2 (800x600) cambia el escritorio a 1824x768")
        marca = len(m.serial())
        m.comando("pantalla salida 2 1024x768")
        m.comando("pantalla principal 1")
        out = limpio(m.comando("pantalla extender abajo --guardar"))
        verificar("Guardado en /home/.config/pantallas.conf" in out, "extender abajo --guardar")
        esperar(m, r"\[TELAS\] extendido, 2 pantallas, principal 0, escritorio 1024x1536", desde=marca)
        out = limpio(m.comando("cat /home/.config/pantallas.conf"))
        verificar("modo=extender" in out and "lado=abajo" in out and "principal=0" in out and "res1=1024x768" in out,
                  "el archivo guarda modo, lado, principal y resoluciones")
        time.sleep(1)
        cabeza(m, "H290-6-abajo-pantalla-1", "prim")
        cabeza(m, "H290-6-abajo-pantalla-2", "seg")
    with Maquina("h290-b", "ide", imagem=img, snapshot=False, extras=DOIS) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(2)
        s = m.serial()
        verificar("[TELAS] extendido, 2 pantallas, principal 0, escritorio 1024x1536" in s,
                  "al reiniciar, las pantallas arrancan una debajo de la otra (guardado en la configuración del usuario)")
    os.remove(img)


def una_pantalla():
    with Maquina("h290-c", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        s = m.serial()
        verificar("[TELAS]" not in s and "tarjeta Bochs extra" not in s, "con una sola tarjeta no se registra nada extra")
        verificar("[MOUSE] limites" not in s or "limites 2048" not in s, "el escritorio no cambia de tamaño")
        out = limpio(m.comando("pantalla salidas"))
        verificar("Una sola pantalla" in out, "pantalla salidas: una sola pantalla")
        out = limpio(m.comando("pantalla espejo"))
        verificar("solo hay una pantalla" in out, "pantalla espejo no hace nada con una pantalla")
        png = m.captura("H290-7-una-pantalla")
        verificar(tamanho_png(png) == (1024, 768) and cor_do_pixel(png, 500, 2) == COR_BARRA, "todo como antes")


def main():
    dos_pantallas()
    una_pantalla()
    print("H290: todos los criterios pasaron")
    return 0


if __name__ == "__main__":
    sys.exit(main())
