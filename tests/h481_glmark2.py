#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H481 (D481) - glmark2-es2 en SaguiOS: el banco de pruebas de OpenGL ES 2.0 de Linaro sobre SaguiGL 2.0.

  fuentes  la descarga con versión y SHA-256 fijos (tools/glmark2_fuentes.sh), sin las fuentes en git, el parche, el
           estado nativo de SaguiOS, la D481 y su fila en docs/terceros.md, la guía de compilación en sus tres
           idiomas y docs/saguigl2.md con la sección de medidas; sin otros alfabetos.
  qemu     en el SaguiOS (con KVM si SAGUI_KVM=1): glmark2-es2 --version y --list-scenes; un subconjunto de escenas
           en una ventana de 320x240 que termina, da FPS en cada escena y una puntuación (se guarda en
           build/testes/h481-puntuacion.txt) con capturas de pantalla a mitad de cada escena que NO están negras;
           las escenas de la D486 (shadow y refract piden GL_OES_depth_texture; buffer con update-method=map pide
           GL_OES_mapbuffer) ya no dicen «Unsupported»; y pantalla completa.
  pc       (opcional) compila glmark2-es2 en el PC contra Mesa (tools/glmark2_pc.sh) y da su puntuación con la
           misma lista de escenas: la referencia en el mismo ordenador. Se salta sin g++, X11, libpng o libjpeg.
  capturas con H481_CAPTURAS=1, la medusa y el terreno en docs/capturas/H481-*.png (los datos de glmark2 son GPL).

Uso: h481_glmark2.py [fuentes] [qemu] [pc]   (sin nada: fuentes y qemu)
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste, RAIZ, SAIDA, ler_ppm  # noqa: E402
import h161_acel2d as h161  # noqa: E402

GM = os.path.join(RAIZ, "userland", "terceros", "glmark2")
# El subconjunto de la prueba: una escena de cada familia que usa algo distinto (VBO, mipmaps, luz por fragmento, normal
# map con 100 000 triángulos, FBO con texturas, medusa con texturas y alfa, bucles y funciones del sombreador).
SUBCONJUNTO = ["build:use-vbo=true", "texture:texture-filter=mipmap", "shading:shading=phong", "bump:bump-render=high-poly",
               "desktop:windows=4:effect=shadow", "jellyfish", "function:fragment-steps=5:fragment-complexity=medium",
               "loop:vertex-steps=5:fragment-steps=5:fragment-uniform=true"]
DURACION = 6          # segundos por escena: cabe una captura a mitad
bs = chr(92)
ALFABETOS = "[" + bs + "u0370-" + bs + "u03ff" + bs + "u0400-" + bs + "u04ff" + bs + "u3000-" + bs + "u9fff]"   # griego, cirílico, CJK


def leer(ruta):
    with open(ruta, encoding="utf-8") as f:
        return f.read()


def fuentes():
    s = leer(os.path.join(RAIZ, "tools", "glmark2_fuentes.sh"))
    sha = re.search(r"^SHA256=([0-9a-f]{64})$", s, re.M)
    verificar(sha, "tools/glmark2_fuentes.sh: SHA-256 fijo de 64 cifras")
    verificar(re.search(r"^VERSION=2023\.01$", s, re.M) and "github.com/glmark2/glmark2/archive/refs/tags/" in s,
              "glmark2_fuentes.sh: versión fija 2023.01 y el tarball de GitHub")
    verificar(os.access(os.path.join(RAIZ, "tools", "glmark2_fuentes.sh"), os.X_OK), "glmark2_fuentes.sh ejecutable")
    gi = leer(os.path.join(RAIZ, ".gitignore"))
    verificar("/userland/terceros/glmark2/fuentes" in gi, "las fuentes de glmark2 no van a git")
    tracked = subprocess.run(["git", "ls-files", "userland/terceros/glmark2/fuentes"], cwd=RAIZ, capture_output=True, text=True).stdout
    verificar(not tracked.strip(), "ningún fichero de glmark2/fuentes está en git")
    for f in ("glmark2.mk", "parches/001-sagui.patch", "plataforma/native-state-sagui.cpp", "plataforma/native-state-sagui.h",
              "plataforma/image-reader.cpp", "plataforma/shared-library.cpp", "plataforma/janela.c", "benchmarks.txt", "leeme.txt"):
        verificar(os.path.isfile(os.path.join(GM, f)), f"userland/terceros/glmark2/{f}")
    # Si las fuentes están bajadas: el parche está aplicado y es la versión fijada
    prep = os.path.join(GM, "fuentes", ".preparado")
    if os.path.exists(prep):
        verificar(leer(os.path.join(GM, "fuentes", "VERSION")).strip() == "2023.01", "las fuentes bajadas son la 2023.01")
        verificar("NativeStateSagui" in leer(os.path.join(GM, "fuentes", "src", "main.cpp")), "el parche de main.cpp está aplicado")
    d = leer(os.path.join(RAIZ, "docs", "decisoes.md"))
    verificar("## D481." in d, "docs/decisoes.md: D481")
    t = leer(os.path.join(RAIZ, "docs", "terceros.md"))
    verificar("glmark2" in t and "D481" in t, "docs/terceros.md: glmark2 (D481)")
    c = leer(os.path.join(RAIZ, "compilar_versiones.md"))
    verificar(c.count("glmark2_fuentes.sh") >= 3, f"compilar_versiones.md: glmark2 en las tres secciones ({c.count('glmark2_fuentes.sh')} menciones)")
    sg = leer(os.path.join(RAIZ, "docs", "saguigl2.md"))
    verificar("glmark2" in sg and "GL_OES_depth_texture" in sg, "docs/saguigl2.md: medidas de glmark2 y lo que falta")
    mk = leer(os.path.join(GM, "glmark2.mk"))
    verificar("glmark2-es2" in mk and "APPS_RECURSOS += glmark2:" in mk, "glmark2.mk: el binario va a /apps/bin y los datos a /apps/glmark2")
    ficheros = [os.path.join(RAIZ, p) for p in ("tools/glmark2_fuentes.sh", "tools/glmark2_pc.sh", "tests/h481_glmark2.py")]
    for dp, _, fs in os.walk(GM):
        if "fuentes" in dp.split(os.sep)[len(GM.split(os.sep)):]:
            continue
        ficheros += [os.path.join(dp, f) for f in fs]
    for f in ficheros:
        verificar(not re.search(ALFABETOS, leer(f)), f"{os.path.relpath(f, RAIZ)}: sin otros alfabetos")


def region_ventana(serial):
    """(x, y) de la esquina del contenido de la ventana de glmark2, de la línea de log del gestor de ventanas."""
    r = re.findall(r'janela \d+ criada por \d+: "glmark2" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)
    verificar(r, "el gestor de ventanas creó la ventana de glmark2")
    w, h, x, y = map(int, r[-1])
    return x, y + 22, w, h   # el marco de título son ~22 píxeles


def no_negro(m, nome, x, y, w, h):
    """Captura la pantalla y devuelve (fracción de píxeles no negros dentro de la ventana, ruta del PPM)."""
    ppm = os.path.join(SAIDA, f"{m.nome}-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.4)
    l, a, px = ler_ppm(ppm)
    tot = nn = 0
    for yy in range(y, min(a, y + h), 2):
        for xx in range(x, min(l, x + w), 2):
            o = (yy * l + xx) * 3
            tot += 1
            nn += (px[o] + px[o + 1] + px[o + 2]) > 30
    return (nn / tot if tot else 0.0), ppm


def puntuacion(out):
    r = re.search(r"glmark2 Score: (\d+)", out)
    return int(r.group(1)) if r else None


def fps_de(out):
    # La salida de la serie y la del terminal se entremezclan: se busca «FPS: n» suelto
    return [int(n) for n in re.findall(r"FPS: (\d+)", out)]


def qemu():
    with Maquina("h481", "ide", imagem=imagem_de_teste("", ["--vesa"])) as m:
        h161.arrancar(m)
        out = m.comando("glmark2-es2 --version", 60)
        verificar("2023.01" in out, "glmark2-es2 --version: 2023.01")
        out = m.comando("glmark2-es2 --list-scenes", 60)
        for escena in ("build", "shading", "jellyfish", "terrain", "conditionals"):
            verificar(f"[Scene] {escena}" in out, f"--list-scenes incluye {escena}")

        # El subconjunto, con capturas a mitad de cada escena
        orden = "glmark2-es2 -s 320x240 -b :duration=%d.0 " % DURACION + " ".join("-b '%s'" % s for s in SUBCONJUNTO)
        antes = m.serial()
        n = antes.count("sagui:")
        m.digitar(orden, 0.03)
        m.tecla("ret", 0.04)
        t0 = time.time()
        while "criada por" not in m.serial()[len(antes):]:
            if time.time() - t0 > 90:
                raise FalhaTeste("glmark2 no abrió su ventana")
            time.sleep(0.3)
        x, y, w, h = region_ventana(m.serial()[len(antes):])
        capturas = []
        tc = time.time()
        for i in range(len(SUBCONJUNTO)):
            # a mitad de cada escena (cada una dura DURACION s más la carga de datos): una captura cada DURACION+2 s
            objetivo = tc + 3 + i * (DURACION + 2.4)
            while time.time() < objetivo and m.serial().count("sagui:") <= n:
                time.sleep(0.2)
            if m.serial().count("sagui:") > n:
                break
            f, ppm = no_negro(m, f"escena{i}", x, y, w, h)
            capturas.append((f, ppm))
        fin = time.time() + 240
        while m.serial().count("sagui:") <= n:
            if time.time() > fin:
                raise FalhaTeste("el subconjunto de glmark2 no volvió al prompt")
            time.sleep(0.3)
        out = m.serial()[len(antes):].replace("\r", "")
        nf = fps_de(out)
        verificar(len(nf) == len(SUBCONJUNTO) and all(v > 0 for v in nf),
                  f"cada escena del subconjunto da FPS ({len(nf)} de {len(SUBCONJUNTO)}: {nf})" + ("" if len(nf) == len(SUBCONJUNTO) else ":\n" + out[-1500:]))
        p = puntuacion(out)
        verificar(p and p > 0, f"glmark2 Score: {p}")
        print(f"    puntuación glmark2-es2 en SaguiOS (CPU, 320x240, subconjunto de {len(SUBCONJUNTO)}): {p}   FPS: {nf}")
        verificar(len(capturas) >= len(SUBCONJUNTO) - 2, f"capturas a mitad de escena: {len(capturas)}")
        for i, (f, _) in enumerate(capturas):
            verificar(f > 0.02, f"captura de la escena {i} ({SUBCONJUNTO[i].split(':')[0]}) no está negra ({f * 100:.0f}% de píxeles con color)")
        os.makedirs(SAIDA, exist_ok=True)
        with open(os.path.join(SAIDA, "h481-puntuacion.txt"), "w") as fo:
            fo.write(f"glmark2-es2 SaguiGL 2.0 CPU 320x240 subconjunto: {p}\nFPS: {nf}\n")

        # Las escenas que pedían extensiones (D486: GL_OES_depth_texture para shadow y refract, GL_OES_mapbuffer para buffer:map)
        # ya no dicen «Unsupported»: corren (la prueba h486 las mide)
        out = m.comando("glmark2-es2 -s 320x240 -b :duration=1.0 -b shadow -b refract -b "
                        "'buffer:update-fraction=0.5:update-dispersion=0.9:columns=200:update-method=map:interleave=false'", 180)
        print("   ", " | ".join(l.strip() for l in out.splitlines() if "Unsupported" in l or "rror" in l)[:400])
        verificar(out.count("Unsupported") == 0, f"shadow, refract y buffer(map) ya no dicen Unsupported ({out.count('Unsupported')})")
        verificar(len(fps_de(out)) == 3, f"shadow, refract y buffer(map) dan FPS ({fps_de(out)})")
        verificar("terminou com codigo 0" in out, "y glmark2 termina bien")

        # Pantalla completa
        out = m.comando("glmark2-es2 --fullscreen -b :duration=2.0 -b build:use-vbo=true -b shading:shading=gouraud", 180)
        verificar("fullscreen" in out, "--fullscreen: la ventana pasó a pantalla completa")
        p2 = puntuacion(out)
        verificar(p2 and p2 > 0, f"pantalla completa: puntuación {p2}")
        verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones")
        # Las capturas para docs/capturas (solo con H481_CAPTURAS=1): la medusa y el terreno, cada una en su ejecución
        if os.environ.get("H481_CAPTURAS") == "1":
            for nombre, escena in (("H481-1-medusa", "jellyfish"), ("H481-2-terreno", "terrain")):
                antes = m.serial()
                n = antes.count("sagui:")
                m.digitar(f"glmark2-es2 -s 320x240 -b {escena}:duration=14", 0.03)
                m.tecla("ret", 0.04)
                t0 = time.time()
                while "criada por" not in m.serial()[len(antes):]:
                    if time.time() - t0 > 90:
                        raise FalhaTeste("glmark2 no abrió su ventana")
                    time.sleep(0.3)
                time.sleep(6)
                m.captura(nombre)
                while m.serial().count("sagui:") <= n:
                    if time.time() - t0 > 200:
                        raise FalhaTeste(f"{escena} no terminó")
                    time.sleep(0.3)


def pc():
    bn = os.path.join(RAIZ, "build", "glmark2-pc", "glmark2-es2")
    if not os.path.exists(bn):
        r = subprocess.run([os.path.join(RAIZ, "tools", "glmark2_pc.sh")], capture_output=True, text=True)
        if r.returncode != 0:
            print("    pc: no se pudo compilar glmark2 en el PC (faltan g++, X11, libpng o libjpeg): se salta")
            return
    if not os.environ.get("DISPLAY"):
        print("    pc: sin DISPLAY: se salta")
        return
    r = subprocess.run([bn, "-s", "320x240", "-f", os.path.join(GM, "benchmarks.txt")], capture_output=True, text=True, timeout=900)
    p = puntuacion(r.stdout)
    verificar(p and p > 0, f"glmark2-es2 en el PC con Mesa: puntuación {p}")
    ren = re.search(r"GL_RENDERER:\s+(.*)", r.stdout)
    print(f"    puntuación glmark2-es2 en el PC ({ren.group(1) if ren else '?'}, 320x240, 29 escenas): {p}")


def main():
    pedidas = sys.argv[1:] or ["fuentes", "qemu"]
    for p in pedidas:
        print("==", p)
        globals()[p]()
    print("H481 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
