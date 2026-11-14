#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H246 - Command & Conquer fluido: la cadena de dibujo de la SDL 2 (D247).

Partes (por defecto, todas; o las pedidas: h246_cnc_fluido.py sintetico cnc):
  sintetico  /apps/bin/sdl2cuadro (320x200 u 640x400 de 8 bits con paleta que se anima, la misma
             cadena que video_sdl2.cpp: superficie -> superficie de 32 bits -> textura ->
             RenderCopy -> RenderPresent) en tres configuraciones: ventana (la lógica de 640x400
             con el cuadro ampliado x2), ventana 1:1 y pantalla completa (escalado no entero).
             Comprueba que los píxeles finales de la ventana y del lienzo son los del camino
             antiguo (sumas FNV-1a fijas, HASHES) y escribe los fps.
  cnc        Command & Conquer con --sdl-medir si hay datos en build/jogos/raiz: la línea
             «[CNC] cuadro: juego X us, paleta Y us, escalado Z us, presentar W us, fps F» en el
             menú y en la primera misión, y que no baja de un mínimo de fps. Sin datos, se salta.
"""
import os
import re
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA, BUILD, APPS_RAIZ  # noqa: E402
from h60_barra_tareas import esperar  # noqa: E402
import h228_openjazz as h228  # noqa: E402
import h230_conquer as h230  # noqa: E402

# Sumas FNV-1a (ventana del sistema, lienzo lógico) tras el último cuadro, medidas con el camino
# antiguo (antes de D247). La pantalla completa depende del modo de vídeo de la prueba (VESA).
HASHES = {
    "ventana x2": ("5a5e1145", "5a5e1145"),
    "ventana 1:1": ("d3b8ee25", "d3b8ee25"),
    "completa": ("58d293d5", "79577865"),
    "escala x2": ("5a5e1145", "b2891145"),
    "completa 320x200": ("128bff09", "b38848c5"),
}
CONFIGS = [("ventana x2", "200 320 200"), ("ventana 1:1", "200 640 400"), ("completa", "120 640 400 c"),
           ("escala x2", "200 320 200 e"), ("completa 320x200", "120 320 200 c")]


def sintetico():
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h246-sint", "ide", imagem=img) as m:
        h228.terminal(m)
        for nome, args in CONFIGS:
            out = m.comando("/apps/bin/sdl2cuadro " + args, 180)
            f = re.search(r"\[SDL2CUADRO\] (\d+) cuadros: (\d+) ms, (\d+\.\d) fps", out)
            h = re.search(r"hash ventana ([0-9a-f]{8}) lienzo ([0-9a-f]{8})", out)
            verificar(f and h, f"{nome}: sdl2cuadro termina y da fps y sumas ({out.strip()[-200:]})")
            print(f"    {nome}: {f.group(3)} fps ({f.group(1)} cuadros en {f.group(2)} ms); ventana {h.group(1)} lienzo {h.group(2)}")
            esperado = HASHES.get(nome)
            if esperado:
                verificar((h.group(1), h.group(2)) == tuple(esperado),
                          f"{nome}: los píxeles son los del camino antiguo ({h.group(1)} {h.group(2)})")


def cnc():
    carpeta = h230.tem_dados_cnc(h230.JOGOS)
    if not carpeta:
        print("AVISO: sin datos de C&C en %s: se salta la parte «cnc»" % h230.JOGOS)
        return
    img = h230.imagem(h230.JOGOS)
    with Maquina("h246-cnc", "ide", imagem=img, som="hda-output") as m:
        h228.terminal(m)
        marca = len(m.serial())
        m.digitar("/apps/bin/conquer --sdl-medir", 0.04)
        m.tecla("ret")
        esperar(m, r"\[CNC\] primer cuadro dibujado", 120, desde=marca)
        time.sleep(12)
        menu = re.findall(r"\[CNC\] cuadro: [^\r\n]*", m.serial()[marca:])
        verificar(menu, "el menú escribe líneas «cuadro:» con --sdl-medir")
        print("    menú:", menu[-1])
        h230.clique_jogo(m, 320, 380)
        m.tecla("ret"); time.sleep(4)
        m.tecla("ret"); time.sleep(6)
        h230.clique_jogo(m, 160, 200)
        time.sleep(30)
        m.tecla("ret")
        time.sleep(10)
        j = re.findall(r"\[CNC\] cuadro: [^\r\n]*", m.serial()[marca:])
        print("    misión:", j[-1])
        fps = float(re.search(r"fps (\d+\.\d)", j[-1]).group(1))
        verificar(fps >= 5.0, f"la misión va a {fps} fps (mínimo 5 en QEMU)")
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[CNC\] fin", 30, desde=marca)


PARTES = {"sintetico": sintetico, "cnc": cnc}


def main():
    os.makedirs(SAIDA, exist_ok=True)
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        if p not in PARTES:
            print("uso: h246_cnc_fluido.py [%s]" % " ".join(PARTES))
            return 2
    for p in pedidas:
        print("== %s" % p)
        PARTES[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
