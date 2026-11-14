#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H204 - OpenTyrian a pantalla completa (D204).

Criterio:
  - (pantalla 1280x720, 16:9, para tener bandas) con --pantalla-completa la ventana ocupa toda la pantalla y se dibuja el
    primer cuadro; la imagen del juego se escala a 4:3 (vecino mas proximo)
    centrada: el centro no es negro y fuera del rectangulo escalado todo es negro;
  - Alt+Intro vuelve a la ventana de 640x400 (sin escalar, como antes) y otra
    vez Alt+Intro a pantalla completa; F11 (del kernel) tambien la quita;
  - el motor guarda su opcion (fullscreen_enabled) y la sincroniza con lo que
    hay de verdad;
  - sin la opcion, el juego arranca en ventana de 640x400 como siempre (h136);
  - el juego no se cae (sin falta de pagina).
Capturas: docs/capturas/H204-*.png.
"""
import os
import re
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA  # noqa: E402
import h136_opentyrian as h136  # noqa: E402


def ultimo(m, patron, desde=0):
    r = re.findall(patron, m.serial()[desde:].replace("\r", ""))
    return r[-1] if r else None


def pixeles(m):
    ppm = os.path.join(SAIDA, "h204.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    w, h, rgb = h136.leer_ppm(ppm)
    os.remove(ppm)
    return w, h, rgb


def contar(rgb, w, x0, y0, x1, y1, paso=4):
    """(pixeles no negros, total) en un rectangulo."""
    no = tot = 0
    for y in range(y0, y1, paso):
        for x in range(x0, x1, paso):
            tot += 1
            if rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] != b"\x00\x00\x00":
                no += 1
    return no, tot


def main():
    # D228: los datos están en /jogos/opentyrian (antes, en la raíz de SAGUI-JOGOS).
    if not os.path.exists(os.path.join(h136.JOGOS, "opentyrian", "tyrian1.lvl")):
        raise AssertionError("falta %s (el make la extrae de dados.img.xz)" % h136.JOGOS)
    imagem = imagem_de_teste("", ["--vesa", "--resolucao", "1280x720", "--jogos", h136.JOGOS])
    with Maquina("h204", "ide", imagem=imagem) as m:
        h136.esperar(m, r"sagui:/\$", 90)
        time.sleep(3)
        h136.focar_terminal(m)
        m.comando("mkdir /home")     # la imagen de prueba no trae el /home persistente del disco instalado

        # ---- A: --pantalla-completa
        desde = len(m.serial())
        m.digitar("/bin/opentyrian --pantalla-completa")
        m.tecla("ret")
        h136.esperar(m, r"\[OPENTYRIAN\] pantalla completa activada: ventana \d+x\d+", 90, desde=desde)
        h136.esperar(m, r"\[OPENTYRIAN\] primeiro quadro desenhado", 30, desde=desde)
        time.sleep(3)
        wl, wa = map(int, ultimo(m, r"pantalla completa activada: ventana (\d+)x(\d+)", desde))
        esc = re.findall(r"escalado 4:3: superficie (\d+)x(\d+) en ventana (\d+)x(\d+) -> (\d+)x(\d+) en \((\d+),(\d+)\)",
                         m.serial()[desde:])
        verificar(bool(esc), "el log da el escalado 4:3")
        sl, sa, vl, va, dl, da, dx, dy = map(int, esc[-1])
        print("    ventana %dx%d, superficie %dx%d -> %dx%d en (%d,%d)" % (vl, va, sl, sa, dl, da, dx, dy))
        w, h, rgb = pixeles(m)
        verificar((wl, wa) == (w, h), "la ventana ocupa toda la pantalla (%dx%d = %dx%d)" % (wl, wa, w, h))
        verificar(dl * 3 == da * 4 or abs(dl * 3 - da * 4) <= 4, "la imagen sale en 4:3 (%dx%d)" % (dl, da))
        verificar(dl <= w and da <= h and (dl == w or da == h), "se escala lo mas grande posible")
        m.captura("H204-1-pantalla-completa", guardar=True)
        no, tot = contar(rgb, w, dx + dl // 4, dy + da // 4, dx + 3 * dl // 4, dy + 3 * da // 4)
        verificar(no * 100 // tot > 20, "el centro de la imagen no es negro (%d%%)" % (100 * no // tot))
        # bandas: fuera del rectangulo escalado todo negro (si las hay)
        if dx > 4:
            no, tot = contar(rgb, w, 0, 0, dx - 1, h)
            verificar(no == 0, "bandas laterales negras (%d pixeles no negros)" % no)
        if dy > 4:
            no, tot = contar(rgb, w, 0, 0, w, dy - 1)
            verificar(no == 0, "bandas superior negra (%d pixeles no negros)" % no)
        verificar("Fallo de página" not in m.serial()[desde:], "sin faltas de pagina")

        # ---- B: Alt+Intro -> ventana, Alt+Intro -> pantalla completa, F11 -> ventana
        marca = len(m.serial())
        m.tecla("alt-ret")
        h136.esperar(m, r"\[OPENTYRIAN\] ventana 640x400 \(ventana\)", 20, desde=marca)
        verificar(True, "Alt+Intro vuelve a la ventana de 640x400")
        time.sleep(2)
        m.captura("H204-2-ventana", guardar=True)
        marca = len(m.serial())
        m.tecla("alt-ret")
        h136.esperar(m, r"\[OPENTYRIAN\] ventana \d+x\d+ \(pantalla completa\)", 20, desde=marca)
        verificar(True, "Alt+Intro otra vez a pantalla completa")
        time.sleep(1.5)
        marca = len(m.serial())
        m.tecla("f11")
        h136.esperar(m, r"\[OPENTYRIAN\] ventana 640x400 \(ventana\)", 20, desde=marca)
        verificar(True, "F11 (kernel) saca de la pantalla completa y el juego se entera")
        s = m.serial()[desde:]
        verificar("Fallo de página" not in s and "Excepción" not in s, "sin faltas de pagina ni excepciones")

        # ---- C: la opcion se guarda en /home (el motor escribe su configuracion al salir)
        m.tecla("alt-ret")                     # otra vez a pantalla completa: eso es lo que debe quedar guardado
        time.sleep(2)
        h136.focar_jogo(m)
        marca = len(m.serial())
        for t in ("esc", "esc", "ret"):         # titulo: Esc pide salir; Intro confirma
            m.tecla(t)
            time.sleep(1.5)
        h136.esperar(m, r"\(opentyrian\) terminou", 40, desde=marca)
        time.sleep(2)
        h136.focar_terminal(m)
        out = m.comando("ls /home")
        verificar("opentyrian.conf" in out, "la configuracion se guarda en /home/opentyrian.conf")
        out = m.comando("cat /home/opentyrian.conf")
        verificar(re.search(r'"fullscreen"\s*:\s*true', out) is not None, "y recuerda la pantalla completa")
    return 0


if __name__ == "__main__":
    sys.exit(main())
