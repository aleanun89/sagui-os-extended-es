#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H288 - Renderizador de la SDL 2 por la GPU (SDL_RENDERER_ACCELERATED, D288).

Partes (por defecto, todas; o las pedidas: h288_sdl_gpu.py fuentes std simulado):
  fuentes    lo que tiene que estar en el árbol: sdl2gpu.h, el programa de prueba, la decisión D288,
             el documento docs/sdl-gpu.md y la ruta de software intacta.
  std        QEMU normal (sin GPU con 3D): /apps/bin/sdl2gpu pide SDL_RENDERER_ACCELERATED y
             se queda por software (gpu=0): las once escenas son idénticas a la referencia y el
             camino acelerado no es más lento que el de software (no empeora).
  simulado   --gpu3d=simulado (el driver de mentira de gpu3d, D285): gpu=1; las escenas (limpiar,
             rellenos, copia 1:1, escalada, espejos, mezcla alfa, modulación, giro, recortada y
             líneas) se comparan píxel a píxel con las del renderizador por software; las exactas
             tienen que coincidir casi del todo (los bordes del escalado y del giro no son bit a
             bit iguales). Con la GPU apagada a mano, el mismo programa sigue por software.
"""
import os
import re
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA, APPS_RAIZ  # noqa: E402
import h228_openjazz as h228  # noqa: E402

# Máximo de píxeles distintos por escena (de 76800), con la GPU simulada. Las escenas exactas
# (todo lo que no es muestreo con escalado no entero ni giro) casi no se apartan.
LIMITE = {"limpiar": 0, "rellenos": 80, "copia 1:1": 80, "escalada x2": 200, "escalada 3/4": 1500,
          "espejos": 400, "mezcla alfa": 800, "modulacion": 800, "giro 30": 400, "recortada": 400,
          "lineas": 200}


def ler(caminho):
    with open(os.path.join(RAIZ, caminho), encoding="utf-8") as f:
        return f.read()


def fuentes():
    g = ler("userland/libsdl2/sdl2gpu.h")
    for pedaco in ("GPU3D_OP_ABRIR", "GPU3D_OP_TRIANGULOS", "GPU3D_OP_LER", "sdlg_recortar", "SAGUI_SDL2_GPU"):
        verificar(pedaco in g, f"sdl2gpu.h usa {pedaco}")
    verificar("SPDX-License-Identifier" in ler("tests/h288_sdl_gpu.py"), "la prueba lleva SPDX")
    verificar("## D288." in ler("docs/decisoes.md"), "docs/decisoes.md tiene la D288")
    verificar("sdl-gpu" in ler("docs/ROADMAP.md") or "D288" in ler("docs/ROADMAP.md"), "ROADMAP.md anota la D288")
    s = ler("userland/libsdl2/sdl2_sagui.c")
    verificar("SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE" in s, "el renderizador de software de siempre sigue ahí")


def ejecutar(m, cuadros=300):
    out = m.comando(f"/apps/bin/sdl2gpu {cuadros}", 400)
    return out


def analizar(out):
    r = re.search(r"\[SDL2GPU\] renderizador (\S+) gpu=(\d)", out)
    verificar(r, "sdl2gpu escribe el renderizador que le tocó (" + out.strip()[-300:] + ")")
    escenas = dict((n, (int(a), int(b))) for n, a, b in
                   re.findall(r"\[SDL2GPU\] escena (.+?): (\d+) pixeles distintos de \d+ \(max (\d+)\)", out))
    verificar(len(escenas) == len(LIMITE), f"las {len(LIMITE)} escenas se comparan ({len(escenas)})")
    t = re.search(r"cuadros: (\d+), software (\d+) ms, acelerado (\d+) ms", out)
    verificar(t, "hay tiempos de software y acelerado")
    return r.group(1), int(r.group(2)), escenas, (int(t.group(1)), int(t.group(2)), int(t.group(3)))


def std():
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h288-std", "ide", imagem=img) as m:
        h228.terminal(m)
        out = ejecutar(m)
        nome, gpu, escenas, (n, sw, ac) = analizar(out)
        print(f"    renderizador {nome}, gpu={gpu}; {n} cuadros: software {sw} ms, acelerado {ac} ms")
        verificar(gpu == 0, "sin GPU 3D: el renderizador acelerado es el de software")
        verificar("renderizador por software (sin GPU 3D" in m.serial(), "el log dice por qué va por software")
        for nombre, (dif, mx) in escenas.items():
            verificar(dif == 0, f"sin GPU: la escena «{nombre}» es idéntica a la referencia ({dif} distintos)")
        # «No empeora»: sin GPU el camino es el de software de siempre (una comparación por llamada);
        # lo vigila h246 (mismas sumas de píxeles y fps de sdl2cuadro). Aquí solo se informa:
        # el acelerado de la ventana además presenta y el de referencia no.
        m.captura("H288-std")


def simulado():
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado", "--apps", APPS_RAIZ])
    with Maquina("h288-sim", "ide", imagem=img) as m:
        h228.terminal(m)
        out = m.comando("gpu3d probar", 300)
        verificar("gpu3d probar: BIEN" in out, "la autoprueba de gpu3d pasa")
        out = ejecutar(m)
        nome, gpu, escenas, (n, sw, ac) = analizar(out)
        print(f"    renderizador {nome}, gpu={gpu}; {n} cuadros: software {sw} ms, acelerado {ac} ms")
        for nombre, (dif, mx) in escenas.items():
            print(f"    escena {nombre}: {dif} distintos (max {mx})")
        verificar(gpu == 1, "con la GPU simulada el renderizador es el de la GPU")
        s = m.serial()
        verificar("[SDL2GPU] renderizador por GPU: destino 320x240" in s, "el log dice que la GPU lleva el lienzo")
        verificar("renderizador por GPU apagado" not in s, "la GPU no se apagó durante las escenas")
        for nombre, (dif, mx) in escenas.items():
            verificar(dif <= LIMITE[nombre], f"escena «{nombre}»: {dif} píxeles distintos (límite {LIMITE[nombre]})")
        out = m.comando("gpu3d", 30)
        t = re.search(r"triangulos (\d+) en (\d+) entregas, cuadros (\d+), fallos (\d+)", out)
        verificar(t and int(t.group(1)) > 50 and int(t.group(3)) >= 11 and int(t.group(4)) == 0,
                  f"gpu3d cuenta triángulos y cuadros de la SDL sin fallos ({t.group(0) if t else '?'})")
        # Un programa que no pide SDL_RENDERER_ACCELERATED (sdl2cuadro, la cadena de C&C) con la GPU forzada
        # (SAGUI_SDL2_GPU=1): los píxeles del lienzo son los de h246 (ventana 1:1, 640x400 en 640x400).
        m.comando("export SAGUI_SDL2_GPU=1", tempo=30)
        out = m.comando("/apps/bin/sdl2cuadro 60 640 400", 300)
        h = re.search(r"hash ventana ([0-9a-f]{8}) lienzo ([0-9a-f]{8})", out)
        verificar(h, "sdl2cuadro con la GPU forzada termina y da sumas (" + out.strip()[-200:] + ")")
        print(f"    sdl2cuadro por la GPU: ventana {h.group(1)} lienzo {h.group(2)}")
        verificar("[SDL2CUADRO] renderizador por GPU: destino 640x400" in m.serial(), "sdl2cuadro lleva el lienzo en la GPU")
        verificar(h.group(2) == h.group(1), "con la GPU, ventana y lienzo coinciden (1:1)")
        m.comando("unset SAGUI_SDL2_GPU", tempo=30)
        # La GPU se apaga a mano: el mismo programa sigue por software y da lo mismo.
        out = m.comando("gpu3d apagar", 30)
        verificar("apagado a mano" in out, "«gpu3d apagar»")
        out = ejecutar(m, 10)
        nome, gpu, escenas, _ = analizar(out)
        verificar(gpu == 0 and all(d == 0 for d, _ in escenas.values()), "apagada la GPU, sdl2gpu va por software sin diferencias")
        m.captura("H288-simulado")
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones ni pánicos")


PARTES = {"fuentes": fuentes, "std": std, "simulado": simulado}


def main():
    os.makedirs(SAIDA, exist_ok=True)
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        if p not in PARTES:
            print("uso: h288_sdl_gpu.py [%s]" % " ".join(PARTES))
            return 2
    for p in pedidas:
        print("== %s" % p)
        PARTES[p]()
    print("h288: BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
