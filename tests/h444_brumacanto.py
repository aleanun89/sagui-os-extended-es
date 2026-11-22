#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H444 (D444) - Brumacanto: juego de vóxeles con SDL 2 y OpenGL ES 1.1 sobre SaguiGL.

  unidad     en el PC: tests/world_test.c del juego (generación determinista, guardar y cargar, mundo por sectores).
  fuentes    el código MIT y los gráficos CC0 con sus licencias y ORIGEM.md; nada de third_party, .exe, .dll ni binarios;
             fila en docs/terceros.md, decisión D444, entrada en Inicio > Juegos, ayuda y sin otros alfabetos.
  saguios    en QEMU (software): abre una ventana con SaguiGL, genera el mundo con semilla fija (captura H444-1),
             guarda con F5, sale limpio desde la pausa, vuelve a cargar la partida y mide los cuadros por segundo.
  gpu        con --gpu3d=simulado: lo mismo, y la imagen se parece a la de software (captura H444-2).

Uso: h444_brumacanto.py [unidad] [fuentes] [saguios] [gpu]   (sin nada: todo)
"""
import os
import re
import struct
import subprocess
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste, RAIZ, APPS_RAIZ, SAIDA  # noqa: E402

D = os.path.join(RAIZ, "userland", "terceros", "brumacanto")
SEMILLA_HASH = "2198853968"   # seed_hash("Prueba")


def unidad():
    os.makedirs(os.path.join(SAIDA, "h444"), exist_ok=True)
    exe = os.path.join(SAIDA, "h444", "world_test")
    src = os.path.join(D, "fuentes", "src")
    subprocess.run(["gcc", "-std=c11", "-O1", "-w", "-I" + src, "-o", exe, os.path.join(D, "fuentes", "tests", "world_test.c"),
                    os.path.join(src, "world.c"), os.path.join(src, "sim.c"), os.path.join(src, "save.c"), "-lm"], check=True)
    r = subprocess.run([exe, os.path.join(SAIDA, "h444", "save.bruma")], capture_output=True, text=True)
    print("   ", (r.stdout.strip().splitlines() or [""])[-1])
    verificar(r.returncode == 0, "world_test del juego sin fallos: " + r.stdout[-300:] + r.stderr[-300:])


def fuentes():
    for f in ("LICENSE.txt", "LICENSE-kenney.txt", "ORIGEM.md", "brumacanto.mk", "datos/atlas.rgba"):
        verificar(os.path.isfile(os.path.join(D, f)), "existe " + f)
    verificar("MIT License" in open(os.path.join(D, "LICENSE.txt")).read(), "LICENSE.txt es la MIT")
    for dirpath, _, nombres in os.walk(D):
        for n in nombres:
            verificar(not n.lower().endswith((".exe", ".dll")), "sin binarios de Windows: " + n)
    verificar(not os.path.exists(os.path.join(D, "third_party")) and not os.path.exists(os.path.join(D, "fuentes", "third_party")),
              "no se copia third_party (SDL y Khronos del ZIP)")
    verificar("brumacanto" in open(os.path.join(RAIZ, "docs", "terceros.md")).read().lower(), "fila en docs/terceros.md")
    verificar("## D444." in open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read(), "decisión D444")
    verificar("## D449." in open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read(), "decisión D449")
    # D449: texturas de Luanti, solo CC BY-SA con créditos por fichero y la licencia
    L = os.path.join(D, "datos", "luanti")
    creditos = open(os.path.join(L, "CREDITOS.md"), encoding="utf-8").read()
    verificar(os.path.isfile(os.path.join(L, "LICENCIA-CC-BY-SA-3.0.txt")), "licencia CC BY-SA en datos/luanti")
    pngs = [n for n in os.listdir(L) if n.endswith(".png")]
    verificar(len(pngs) >= 15, "texturas de Luanti presentes (%d)" % len(pngs))
    for n in pngs:
        verificar("`%s`" % n in creditos, "créditos de " + n)
    for fila in creditos.splitlines():
        if fila.startswith("| `"):
            verificar("CC BY-SA 3.0" in fila and "github.com/luanti-org/minetest_game" in fila, "fila con licencia y origen: " + fila[:40])
    verificar("LGPL" in creditos, "créditos: no se copia código LGPL")
    verificar("## D451." in open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read(), "decisión D451 (por la GPU)")
    verificar("--escala" in open(os.path.join(D, "fuentes", "src", "main.c"), encoding="utf-8").read(), "--escala y --fullscreen (D451)")
    verificar("/apps/bin/brumacanto" in open(os.path.join(RAIZ, "kernel", "gui", "barra.c"), encoding="utf-8").read(), "entrada en Inicio > Juegos")
    verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "ayuda", "brumacanto.md")), "ayuda en rootfs/ayuda")
    mk = open(os.path.join(D, "brumacanto.mk")).read()
    verificar("SEM_GPL" not in mk, "el juego entra también en la variante sin GPL")
    for ruta in (os.path.join(D, "brumacanto.mk"), os.path.join(D, "ORIGEM.md"), os.path.join(D, "datos", "luanti", "CREDITOS.md"), os.path.join(RAIZ, "rootfs", "ayuda", "brumacanto.md"),
                 os.path.join(RAIZ, "docs", "brumacanto.md")):
        t = open(ruta, encoding="utf-8").read()
        verificar(not re.search("[Ͱ-ӿ　-鿿]", t), "sin otros alfabetos en " + os.path.basename(ruta))


# ---- PNG mínimo (RGB/RGBA de 8 bits) para comparar capturas
def leer_png(p):
    d = open(p, "rb").read()
    i, idat = 8, b""
    while i < len(d):
        n, t = struct.unpack(">I4s", d[i:i + 8])
        c = d[i + 8:i + 8 + n]
        if t == b"IHDR":
            w, h, _, ct = struct.unpack(">IIBB", c[:10])
        if t == b"IDAT":
            idat += c
        i += 12 + n
    raw = zlib.decompress(idat)
    bpp = {2: 3, 6: 4}[ct]
    st = w * bpp
    filas, prev, o = [], bytearray(st), 0
    for _ in range(h):
        f = raw[o]
        linea = bytearray(raw[o + 1:o + 1 + st])
        o += 1 + st
        for x in range(st):
            a = linea[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                linea[x] = (linea[x] + a) & 255
            elif f == 2:
                linea[x] = (linea[x] + b) & 255
            elif f == 3:
                linea[x] = (linea[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                linea[x] = (linea[x] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        filas.append(linea)
        prev = linea
    return w, h, bpp, filas


def parecidas(p1, p2, x0=40, y0=140, x1=1000, y1=690):
    """Fracción de píxeles (de la ventana) cuya diferencia por canal es pequeña."""
    w, h, b1, f1 = leer_png(p1)
    _, _, b2, f2 = leer_png(p2)
    bien = tot = 0
    for y in range(y0, y1, 2):
        for x in range(x0, x1, 2):
            a, b = f1[y][x * b1:x * b1 + 3], f2[y][x * b2:x * b2 + 3]
            tot += 1
            if max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])) <= 40:
                bien += 1
    return bien / tot


def lanzar(m, args, patron, tiempo=300):
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    h136.focar_terminal(m)
    marca = len(m.serial())
    m.digitar("/apps/bin/brumacanto " + args, 0.04)
    m.tecla("ret")
    return marca, esperar(m, patron, tiempo, desde=marca)


def sesion(nome, extras, captura):
    from h60_barra_tareas import esperar
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "brumacanto")):
        raise FalhaTeste("la imagen no lleva brumacanto (make)")
    img = imagem_de_teste("h444" + nome, ["--vesa", "--apps", APPS_RAIZ] + extras)
    with Maquina("h444" + nome, "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        marca, g = lanzar(m, "--seed Prueba --size 160x160x48 --view 64", r"BRUMA_MUNDO nuevo 160x160x48 semilla (\d+)")
        verificar(g == SEMILLA_HASH, "el mundo se genera con la semilla fija (%s)" % g)
        verificar("SaguiGL" in m.serial()[marca:], "el renderizador es SaguiGL (OpenGL ES-CM 1.1)")
        time.sleep(6)
        m.captura(captura)
        if "gpu" not in nome:   # D449: la mochila con los objetos nuevos y la lista de recetas de 8 columnas
            m.tecla("e")
            time.sleep(3)
            m.captura("H444-3-brumacanto-mochila")
            m.tecla("e")
            time.sleep(2)
        m.tecla("f5")
        esperar(m, r"BRUMA_GUARDADO (\S+)", 30, desde=marca)
        verificar(True, "F5 guarda la partida")
        for t in ("esc", "down", "down", "ret"):   # pausa > guardar y salir (con la GPU simulada cada cuadro tarda más de 1 s)
            m.tecla(t, 3 if "gpu" in nome else 0.3)
        esperar(m, r"processo \d+ \(brumacanto\) terminou com codigo (\d+)", 300, desde=marca)
        verificar(re.findall(r"\(brumacanto\) terminou com codigo (\d+)", m.serial()[marca:])[-1] == "0", "sale limpio (código 0)")
        esperar(m, r"sagui:/\$", 60, desde=marca)
        time.sleep(2)
        marca, g = lanzar(m, "--seed Prueba --size 160x160x48 --view 64 --bench 30", r"BRUMA_MUNDO (nuevo|cargado) ")
        verificar(g == "cargado", "la partida guardada se carga (%s)" % g)
        r = esperar(m, r"BRUMA_BENCH cuadros (\d+) tiempo_ms (\d+) fps_x10 (\d+) peor_cuadro_ms (\d+) vista (\d+)", 600, desde=marca)
        print("    cuadros/s (%s, vista %s): %.1f, peor cuadro %s ms" % (nome, r[4], int(r[2]) / 10.0, r[3]))
        verificar(int(r[2]) > 0 and r[4] == "64", "mide los cuadros por segundo")
        if nome == "sw":
            # D451: la escala de dibujo (--escala 50) y el perfil de SaguiGL
            esperar(m, r"sagui:/\$", 60, desde=marca)
            time.sleep(2)
            marca, _ = lanzar(m, "--seed Prueba --size 160x160x48 --view 64 --bench 20 --escala 50", r"BRUMA_MUNDO (nuevo|cargado) ")
            r2 = esperar(m, r"BRUMA_BENCH cuadros (\d+) tiempo_ms (\d+) fps_x10 (\d+) peor_cuadro_ms (\d+) vista (\d+)", 600, desde=marca)
            print("    cuadros/s (%s, vista 64, escala 50 %%): %.1f, peor cuadro %s ms" % (nome, int(r2[2]) / 10.0, r2[3]))
            verificar("escala de dibujo 50 %" in m.serial()[marca:], "--escala 50: SaguiGL dibuja a la mitad (D451)")
            verificar(int(r2[2]) > 0, "mide los cuadros por segundo con la escala al 50 %")
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones")
        return os.path.join(RAIZ, "docs", "capturas", captura + ".png")


def saguios():
    sesion("sw", [], "H444-1-brumacanto-software")


def gpu():
    a = os.path.join(RAIZ, "docs", "capturas", "H444-1-brumacanto-software.png")
    if not os.path.exists(a):
        a = sesion("sw", [], "H444-1-brumacanto-software")
    b = sesion("gpu", ["--gpu3d=simulado"], "H444-2-brumacanto-gpu-simulada")
    f = parecidas(a, b)
    print("    parecido software / GPU simulada: %.1f %%" % (f * 100))
    verificar(f >= 0.80, "la imagen por la GPU simulada se parece a la de software (%.1f %%)" % (f * 100))


def main():
    pedidas = sys.argv[1:] or ["unidad", "fuentes", "saguios", "gpu"]
    for p in pedidas:
        print("==", p)
        globals()[p]()
    print("H444 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
