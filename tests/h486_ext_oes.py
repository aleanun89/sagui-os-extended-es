#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H486 (D486) - SaguiGL 2.0: GL_OES_depth_texture, GL_OES_mapbuffer, GL_OES_depth24 y GL_OES_packed_depth_stencil.

  fuentes   la D486, los tokens y prototipos de gl2ext.h, las extensiones en glGetString, la prueba en el Makefile,
            docs/saguigl2.md y el ROADMAP; sin otros alfabetos;
  unidad    en el PC: tests/sgl/es20_ext486.c (EGL con Z de 24, renderbuffers DEPTH_COMPONENT24 y DEPTH24_STENCIL8, texturas de
            profundidad UNSIGNED_SHORT/UNSIGNED_INT y DEPTH_STENCIL muestreadas en el sombreador, FBO sin color, errores,
            glMapBufferOES/glUnmapBufferOES) sin fallos, también con ASan y UBSan; las pruebas de ES 1.1 (que ya tenían mapbuffer)
            siguen igual (h475 es11);
  qemu      el mismo programa (es20ext) dentro del SaguiOS por la CPU: sin fallos y con la misma suma que el PC;
  virgl     -vga none -device virtio-vga-gl con -display sdl,gl=on: es20ext por la GPU (VirGL, gpu3d v4) compara cada escena con la
            CPU leyendo los píxeles; las de Z de 24, DEPTH24_STENCIL8 y la de mapbuffer las dibuja la GPU; la de textura de
            profundidad cae a la CPU (documentado) con el mismo resultado; se omite sola sin pantalla con GL;
  glmark2   en el SaguiOS: shadow, refract y buffer:update-method=map ya no dicen «Unsupported» y dan FPS; y --off-screen
            arranca sin --visual-config (Z de 24 en un renderbuffer);
  medir     (opcional) glmark2 por VirGL en una ventana de 320x240, con y sin GPU (build/testes/h486-virgl.txt).

Uso: h486_ext_oes.py [fuentes] [unidad] [qemu] [virgl] [glmark2] [medir]   (sin nada: todo menos medir)
"""
import io
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, FalhaTeste, imagem_console, imagem_de_teste, verificar, APPS_RAIZ, SAIDA  # noqa: E402
import h475_saguigl2 as h475  # noqa: E402
import h480_saguigl2_virgl as h480  # noqa: E402
import h161_acel2d as h161  # noqa: E402

bs = chr(92)
ALFABETOS = "[" + bs + "u0370-" + bs + "u03ff" + bs + "u0400-" + bs + "u04ff" + bs + "u3000-" + bs + "u9fff]"   # griego, cirílico, CJK
FUENTE = os.path.join(RAIZ, "tests", "sgl", "es20_ext486.c")
EXTENSIONES = ("GL_OES_depth_texture", "GL_OES_mapbuffer", "GL_OES_depth24", "GL_OES_packed_depth_stencil")
SCENAS_NUEVAS = ["shadow", "refract", "buffer:update-fraction=0.5:update-dispersion=0.9:columns=200:update-method=map:interleave=false"]


def ler(caminho):
    with io.open(os.path.join(RAIZ, caminho), encoding="utf-8") as f:
        return f.read()


def fuentes():
    for f in ("tests/sgl/es20_ext486.c", "tests/h486_ext_oes.py", "userland/libsgl/sgl2.c", "userland/libsgl/sgl2_gpu.c",
              "userland/libsgl/sgl2_desenho.c", "userland/inc/GLES2/gl2ext.h", "docs/saguigl2.md"):
        t = ler(f)
        verificar(not re.search(ALFABETOS, t), f"{f}: sin otros alfabetos")
    verificar("## D486." in ler("docs/decisoes.md"), "la decisión D486 está en docs/decisoes.md")
    sg = ler("docs/saguigl2.md")
    verificar("D486" in sg and "GL_OES_depth_texture" in sg and "VirGL" in sg, "docs/saguigl2.md habla de la D486")
    verificar("D486" in ler("docs/ROADMAP.md"), "docs/ROADMAP.md habla de la D486")
    h = ler("userland/inc/GLES2/gl2ext.h")
    for t in ("GL_DEPTH_COMPONENT24_OES", "GL_DEPTH24_STENCIL8_OES", "GL_DEPTH_STENCIL_OES", "GL_UNSIGNED_INT_24_8_OES", "GL_WRITE_ONLY_OES",
              "glMapBufferOES", "glUnmapBufferOES", "glGetBufferPointervOES", "GL_DEPTH_STENCIL_ATTACHMENT"):
        verificar(t in h, f"gl2ext.h: {t}")
    c = ler("userland/libsgl/sgl2.c")
    for e in EXTENSIONES:
        verificar(e in c, f"glGetString(GL_EXTENSIONS) anuncia {e}")
    verificar("es20ext" in ler("Makefile"), "el Makefile construye es20ext")
    verificar("sgl2_glMapBufferOES" in ler("userland/libsgl/sgl_es.c"), "glMapBufferOES con ES 2.0 actual va a sgl2.c (la ES 1.1 sigue en sgl_es.c)")


def unidad():
    exe = h475.compilar("es20ext", FUENTE, h475.SGL2, ["-DSGL_ES2"])
    p = subprocess.run([exe], capture_output=True, text=True)
    g = re.search(r"conformidad: (\d+) bien, (\d+) mal", p.stdout)
    s = re.search(r"suma ([0-9a-f]{8})", p.stdout)
    print("   ", g.group(0) if g else p.stdout[-600:])
    verificar(p.returncode == 0 and g and g.group(2) == "0" and int(g.group(1)) >= 100, "es20ext en el PC: las extensiones de la D486" + ("" if p.returncode == 0 else ":\n" + p.stdout[-2500:]))
    exe2 = h475.compilar("es20ext_asan", FUENTE, h475.SGL2, ["-DSGL_ES2", "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    p2 = subprocess.run([exe2], capture_output=True, text=True, env=env)
    s2 = re.search(r"suma ([0-9a-f]{8})", p2.stdout)
    verificar(p2.returncode == 0 and "runtime error" not in p2.stderr and "ERROR: AddressSanitizer" not in p2.stderr,
              "es20ext con ASan y UBSan, sin avisos" + ("" if p2.returncode == 0 else ":\n" + (p2.stdout + p2.stderr)[-2500:]))
    verificar(s and s2 and s.group(1) == s2.group(1), f"la misma suma con -O1 y saneadores ({s2 and s2.group(1)}) que con -O2 ({s and s.group(1)})")
    return s.group(1) if s else None


def qemu():
    suma = unidad()
    with Maquina("h486", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("es20ext --cpu", 600)
        g = re.search(r"conformidad: (\d+) bien, (\d+) mal", out)
        print("   ", g.group(0) if g else out[-600:])
        verificar(g and g.group(2) == "0", "es20ext dentro del SaguiOS (por la CPU): sin fallos" + ("" if g and g.group(2) == "0" else ":\n" + out[-2000:]))
        ss = re.search(r"suma ([0-9a-f]{8})", out)
        verificar(ss and ss.group(1) == suma, f"es20ext: la suma del SaguiOS {ss and ss.group(1)} = la del PC {suma}")
        verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones")


def parse_gvc(out):
    return {n: (int(d), int(g), int(c)) for n, d, g, c in re.findall(r"gpu-vs-cpu (\w+): (\d+) pixeles distintos, gpu (\d+), cpu (\d+)", out)}


def virgl():
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    try:
        with Maquina("h486g", "ide", imagem=img, extras=h480.GL) as m:
            err = h480.Err(m)
            try:
                m.esperar_serial("area de trabalho no ar", 150)
            except FalhaTeste:
                e = err.leer() if m.proc.poll() is not None else ""
                if "opengl is not available" in e or "display" in e or "egl" in e.lower():
                    print("    omitida: este QEMU no tiene pantalla con GL (" + e.strip().splitlines()[-1][:80] + ")")
                    return
                raise
            time.sleep(3)
            verificar("[VIRGL] autoprueba: BIEN" in m.serial(), "la autoprueba de VirGL del arranque sigue bien")
            out = m.comando("es20ext --cpu", 600)
            g = re.search(r"conformidad: (\d+) bien, (\d+) mal", out)
            verificar(g and g.group(2) == "0", "es20ext por la CPU en la máquina con VirGL: sin fallos")
            out = m.comando("es20ext", 900)
            g = re.search(r"conformidad: (\d+) bien, (\d+) mal", out)
            print("   ", g.group(0) if g else out[-600:])
            print("   ", "\n    ".join(l for l in out.splitlines() if l.startswith("MAL") or l.startswith("gpu"))[:3000])
            verificar(g and g.group(2) == "0", "es20ext por la GPU (VirGL): sin fallos, cada escena como la de la CPU" + ("" if g and g.group(2) == "0" else ":\n" + out[-2500:]))
            gp = re.search(r"gpu: estado (-?\d+), (\d+) dibujos y (\d+) limpiezas por la GPU, (\d+) dibujos", out)
            verificar(gp and int(gp.group(2)) >= 10, f"la GPU v4 dibujó ({gp and gp.group(0)})")
            r = parse_gvc(out)
            for n, (d, gg, c) in sorted(r.items()):
                print(f"    {n}: {d} píxeles distintos, {gg} dibujos por la GPU, {c} devueltos a la CPU")
            for n in ("depth24_stencil8_z", "depth24_stencil8_estencil", "depth24", "mapbuffer"):
                verificar(n in r and r[n][1] >= 1, f"{n}: dibujado por la GPU ({r.get(n)})")
                verificar(n in r and r[n][0] <= 64 * 64 // 20, f"{n}: como la CPU salvo bordes ({r.get(n)})")
            verificar("depth_texture" in r and r["depth_texture"][2] >= 1 and r["depth_texture"][0] <= 64 * 64 // 20,
                      f"depth_texture: cae a la CPU (documentado) con el mismo resultado ({r.get('depth_texture')})")
            e = err.leer()
            verificar("Failed to parse" not in e and "compile" not in e.lower(), "virglrenderer no se queja de ningún sombreador")
            verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones ni pánicos")
    except OSError as e:
        print("    omitida:", e)


def puntuacion(out):
    r = re.search(r"glmark2 Score: (\d+)", out)
    return int(r.group(1)) if r else None


def glmark2():
    with Maquina("h486w", "ide", imagem=imagem_de_teste("", ["--vesa"])) as m:
        h161.arrancar(m)
        orden = "glmark2-es2 -s 320x240 -b :duration=2.0 " + " ".join("-b '%s'" % s for s in SCENAS_NUEVAS)
        out = m.comando(orden, 240)
        print("   ", " | ".join(l.strip() for l in out.splitlines() if "Unsupported" in l or "rror" in l or "FPS" in l)[:500])
        verificar(out.count("Unsupported") == 0, f"shadow, refract y buffer(map) ya no dicen Unsupported ({out.count('Unsupported')})")
        nf = [int(n) for n in re.findall(r"FPS: (\d+)", out)]
        verificar(len(nf) == 3 and all(v > 0 for v in nf), f"las tres escenas dan FPS ({nf})")
        p = puntuacion(out)
        verificar(p and p > 0, f"glmark2 Score de las tres: {p}")
        out = m.comando("glmark2-es2 --off-screen -s 320x240 -b :duration=2.0 -b build:use-vbo=true -b shading:shading=gouraud", 240)
        verificar("Selected Renderbuffer" in out or puntuacion(out), "--off-screen sin --visual-config arranca")
        p2 = puntuacion(out)
        print("    --off-screen sin --visual-config: puntuación", p2)
        verificar(p2 and p2 > 0 and "rror" not in out, f"--off-screen sin --visual-config: puntuación {p2}" + ("" if p2 else ":\n" + out[-1200:]))
        verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones")


def medir():
    """glmark2 con la lista de 33 escenas por VirGL (GPU) y con la GPU apagada (CPU), ventana de 320x240."""
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    res = {}
    with Maquina("h486m", "ide", imagem=img, extras=h480.GL) as m:
        err = h480.Err(m)
        m.esperar_serial("area de trabalho no ar", 150)
        time.sleep(3)
        for modo, pre in (("gpu", ""), ("cpu", "export SAGUI_GL_V4=0; ")):
            out = m.comando(pre + "glmark2-es2 -s 320x240 -f /apps/glmark2/benchmarks.txt", 1800)
            p = puntuacion(out)
            nf = re.findall(r"^\[(\S+?)\].*?FPS: (\d+)", out, re.M)
            res[modo] = (p, [int(x[1]) for x in nf], out)
            print(f"    glmark2 por {modo}: Score {p}, {len(nf)} escenas")
        err.leer()
    os.makedirs(SAIDA, exist_ok=True)
    with open(os.path.join(SAIDA, "h486-virgl.txt"), "w") as f:
        for modo in ("gpu", "cpu"):
            f.write(f"{modo}: {res[modo][0]}\n{res[modo][2]}\n")


def main():
    pedidas = sys.argv[1:] or ["fuentes", "unidad", "qemu", "virgl", "glmark2"]
    for p in pedidas:
        print("==", p)
        globals()[p]()
    print("H486 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
