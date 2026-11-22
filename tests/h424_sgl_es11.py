#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H424 (D425) - SaguiGL OpenGL ES 1.1 completo, sus extensiones, EGL y gpu3d v2.

  unidad       en el PC: tests/sgl/es11_conformidad.c (solo cabeceras estándar: TexEnv, COMBINE,
               multitextura, esténcil, tijera, máscaras, Z, mezcla, lógica, VBO, puntos, DXT, paletas,
               cubos, FBO, planos, paleta de matrices, draw_texture, consultas, coma fija) sin fallos;
               tests/sgl/es11_gpu_falsa.c (el camino por la GPU, v1 y v2, contra una GPU falsa por la CPU);
               la demo es11demo (EGL con pbuffer) da sus cuatro sumas.
  fuentes      las cabeceras GLES/EGL/KHR propias y completas (cada función con definición y en la tabla de
               eglGetProcAddress), la D425, docs/gpu3d.md «gpu3d v2», sin otros alfabetos.
  rendimiento  el camino de siempre (una textura con MODULATE y el color plano) no va más lento que con el
               sgl.c de la v8.5.5.2 (tests/sgl/es11_rendimiento.c en el PC; da los números).
  qemu         en el SaguiOS: es11conf da 0 fallos y la misma suma que el PC; es11demo -c 1..4 las mismas
               sumas que el PC.
  capturas     las cuatro escenas de la demo (hechas en el PC, sintéticas) en docs/capturas/H424-*.png.
  gpu          (D426) con el driver simulado (--gpu3d=simulado), que entiende gpu3d v2: sus pruebas v2 pasan
               todas, y es11demo -g 1..4 dibuja cada escena por software y por la GPU simulada (dibujos v2) con la
               misma suma de software que el PC y pocos píxeles distintos (bordes de los triángulos).
  tcc          el TCC cruzado compila y enlaza con /apps/tcc (cabeceras GLES/EGL, -lGLESv1_CM -lEGL y SDL_GL_*)
               el ejemplo gles.c y la demo es11demo.c sin tocarlos (si TCC está compilado).

Uso: h424_sgl_es11.py [unidad] [fuentes] [rendimiento] [qemu] [capturas] [gpu] [tcc]   (sin nada: todo)
"""
import os
import re
import struct
import subprocess
import sys
import tempfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, imagem_de_teste, FalhaTeste, RAIZ  # noqa: E402

LIB = os.path.join(RAIZ, "userland", "libsgl")
INC = os.path.join(RAIZ, "userland", "inc")
FLAGS = ["-O2", "-msse2", "-mfpmath=sse", "-ffp-contract=off", "-fno-math-errno", "-w", "-DSGL_ANFITRIAO",
         "-I" + INC, "-I" + LIB]
SGL = [os.path.join(LIB, f) for f in ("sgl.c", "sgl_fixo.c", "sgl_egl.c")]
TMP = os.path.join(RAIZ, "build", "h424")


def compilar(nome, fonte, extra=None, fontes_sgl=None):
    os.makedirs(TMP, exist_ok=True)
    exe = os.path.join(TMP, nome)
    subprocess.run(["gcc"] + FLAGS + (extra or []) + ["-o", exe, fonte] + (fontes_sgl or SGL), check=True)
    return exe


def conformidade():
    exe = compilar("es11conf", os.path.join(RAIZ, "tests/sgl/es11_conformidad.c"))
    p = subprocess.run([exe], capture_output=True, text=True)
    g = re.search(r"conformidad: (\d+) bien, (\d+) mal", p.stdout)
    s = re.search(r"suma ([0-9a-f]{8})", p.stdout)
    return p, g, s


def demo_sumas():
    exe = compilar("es11demo", os.path.join(RAIZ, "userland/programas/es11demo.c"))
    out = subprocess.run([exe], capture_output=True, text=True, check=True).stdout
    return dict(re.findall(r"^(\w+) ([0-9a-f]{8})$", out, re.M)), exe


def unidad():
    p, g, s = conformidade()
    print("   ", (g.group(0) if g else p.stdout[-400:]))
    verificar(p.returncode == 0 and g and g.group(2) == "0" and int(g.group(1)) >= 170,
              "conformidad de OpenGL ES 1.1 en el PC" + ("" if p.returncode == 0 else ":\n" + p.stdout[-1500:]))
    exe = compilar("es11_gpu_falsa", os.path.join(RAIZ, "tests/sgl/es11_gpu_falsa.c"), ["-DSGL_GPU_FALSA"],
                   [os.path.join(LIB, "sgl.c"), os.path.join(LIB, "sgl_fixo.c")])
    p = subprocess.run([exe], capture_output=True, text=True)
    for linha in p.stdout.splitlines():
        print("   ", linha)
    verificar(p.returncode == 0 and "gpu_falsa: bien" in p.stdout, "SaguiGL por gpu3d v1 y v2 (GPU falsa) da lo mismo que por software")
    sumas, _ = demo_sumas()
    verificar(len(sumas) == 4 and len(set(sumas.values())) == 4, f"es11demo: cuatro escenas distintas ({sumas})")


def fuentes():
    import io
    nomes = []
    for h in ("GLES/gl.h", "GLES/glext.h"):
        for m in re.finditer(r"GL_APIENTRY \**(gl[A-Za-z0-9]+)\(", io.open(os.path.join(INC, h), encoding="utf-8").read()):
            nomes.append(m.group(1))
    procs = io.open(os.path.join(LIB, "sgl_procs.inc"), encoding="utf-8").read()
    faltam = [n for n in nomes if f"P({n})" not in procs]
    verificar(not faltam, f"eglGetProcAddress conoce todas las funciones de las cabeceras (faltan {faltam[:5]})")
    os.makedirs(TMP, exist_ok=True)
    objs = []
    for f in ("sgl.c", "sgl_fixo.c", "sgl_egl.c"):
        o = os.path.join(TMP, f + ".o")
        subprocess.run(["gcc"] + FLAGS + ["-c", "-o", o, os.path.join(LIB, f)], check=True)
        objs.append(o)
    definidas = set()
    for o in objs:
        for l in subprocess.run(["nm", o], capture_output=True, text=True, check=True).stdout.splitlines():
            c = l.split()
            if len(c) == 3 and c[1] == "T":
                definidas.add(c[2])
    sem = [n for n in nomes if n not in definidas]
    verificar(not sem, f"cada función de GLES/gl.h y glext.h tiene definición ({len(nomes)}; faltan {sem[:5]})")
    egl = re.findall(r"EGLAPIENTRY \**(egl[A-Za-z]+)\(", io.open(os.path.join(INC, "EGL/egl.h"), encoding="utf-8").read())
    sem = [n for n in egl if n not in definidas]
    verificar(len(egl) >= 34 and not sem, f"EGL 1.4: {len(egl)} funciones, todas definidas (faltan {sem})")
    for h in ("GLES/gl.h", "GLES/glext.h", "GLES/glplatform.h", "EGL/egl.h", "EGL/eglplatform.h", "KHR/khrplatform.h"):
        t = io.open(os.path.join(INC, h), encoding="utf-8").read()
        verificar("SPDX-License-Identifier: GPL-2.0-or-later" in t and "Khronos Group Inc" not in t,
                  f"{h}: propia (sin texto de Khronos)")
    dec = io.open(os.path.join(RAIZ, "docs/decisoes.md"), encoding="utf-8").read()
    verificar("## D425." in dec, "D425 en docs/decisoes.md")
    g3d = io.open(os.path.join(RAIZ, "docs/gpu3d.md"), encoding="utf-8").read()
    verificar("## gpu3d v2" in g3d and "G3D_CAP_TEX_NPOT_MIP" in g3d, "docs/gpu3d.md: «gpu3d v2» con sus bits")
    for f in ("userland/libsgl/sgl_es.c", "userland/libsgl/sgl_egl.c", "userland/libsgl/sgl_fixo.c", "kernel/inc/abi.h",
              "userland/inc/GLES/gl.h", "userland/inc/EGL/egl.h", "tests/sgl/es11_conformidad.c", "userland/programas/es11demo.c"):
        t = io.open(os.path.join(RAIZ, f), encoding="utf-8").read()
        verificar(not re.search("[Ͱ-ϿЀ-ӿ　-鿿]", t), f"{f}: sin otros alfabetos")


def rendimiento():
    with tempfile.TemporaryDirectory() as d:
        for f in ("sgl.c", "sgl.h"):
            with open(os.path.join(d, f), "wb") as o:
                o.write(subprocess.run(["git", "-C", RAIZ, "show", f"b162e204:userland/libsgl/{f}"],
                                       check=True, capture_output=True).stdout)
        fonte = os.path.join(RAIZ, "tests/sgl/es11_rendimiento.c")
        base = os.path.join(d, "base")
        flags = [f for f in FLAGS if not f.startswith("-I")]
        subprocess.run(["gcc"] + flags + ["-I" + d, "-o", base, fonte, os.path.join(d, "sgl.c")], check=True)
        novo = compilar("es11_rendimiento", fonte, None, [os.path.join(LIB, "sgl.c")])
        res = {}
        for nome, exe in (("v8.5.5.2", base), ("ahora", novo)):
            outs = [subprocess.run([exe, "60"], capture_output=True, text=True, check=True).stdout for _ in range(3)]
            g = [re.search(r"textura ([\d.]+) plano ([\d.]+) suma ([0-9a-f]+)", o) for o in outs]
            res[nome] = (min(float(x.group(1)) for x in g), min(float(x.group(2)) for x in g), g[0].group(3))
            print(f"    {nome}: textura {res[nome][0]:.2f} ms/cuadro, plano {res[nome][1]:.2f} ms/cuadro (640x480), suma {res[nome][2]}")
        verificar(res["v8.5.5.2"][2] == res["ahora"][2], "el camino de siempre da los mismos píxeles que en la v8.5.5.2")
        verificar(res["ahora"][0] <= res["v8.5.5.2"][0] * 1.15 + 0.1 and res["ahora"][1] <= res["v8.5.5.2"][1] * 1.15 + 0.1,
                  "el camino de siempre no va más lento (margen del 15 % por el ruido)")


def png_de_ppm(ppm, png):
    d = open(ppm, "rb").read()
    partes = d.split(b"\n", 3)
    larg, alt = map(int, partes[1].split())
    px = partes[3]
    cru = b"".join(b"\0" + px[y * larg * 3:(y + 1) * larg * 3] for y in range(alt))

    def trozo(t, b):
        return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF)
    with open(png, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", larg, alt, 8, 2, 0, 0, 0)) +
                trozo(b"IDAT", zlib.compress(cru, 9)) + trozo(b"IEND", b""))


def capturas():
    _, exe = demo_sumas()
    cap = os.path.join(RAIZ, "docs", "capturas")
    with tempfile.TemporaryDirectory() as d:
        for n, nome in enumerate(("lightmap", "particulas", "sombra", "reflejo"), 1):
            ppm = os.path.join(d, f"{n}.ppm")
            subprocess.run([exe, "-p", str(n), ppm], check=True)
            png_de_ppm(ppm, os.path.join(cap, f"H424-{nome}.png"))
    verificar(all(os.path.exists(os.path.join(cap, f"H424-{n}.png")) for n in ("lightmap", "particulas", "sombra", "reflejo")),
              "las cuatro capturas de la demo")


def qemu():
    _, _, s = conformidade()
    sumas, _ = demo_sumas()
    with Maquina("h424", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("es11conf", 300)
        g = re.search(r"conformidad: (\d+) bien, (\d+) mal", out)
        print("   ", g.group(0) if g else out[-400:])
        verificar(g and g.group(2) == "0", "es11conf dentro del SaguiOS: sin fallos" + ("" if g and g.group(2) == "0" else ":\n" + out[-1500:]))
        ss = re.search(r"suma ([0-9a-f]{8})", out)
        verificar(ss and ss.group(1) == s.group(1), f"es11conf: la suma del SaguiOS {ss and ss.group(1)} = la del PC {s.group(1)}")
        for n, nome in enumerate(("lightmap", "particulas", "sombra", "reflejo"), 1):
            out = m.comando(f"es11demo -c {n}", 180)
            g = re.search(rf"^{nome} ([0-9a-f]{{8}})", out, re.M)
            verificar(g and g.group(1) == sumas[nome], f"es11demo -c {n} ({nome}): {g and g.group(1)} = PC {sumas[nome]}")


def gpu():
    sumas, _ = demo_sumas()
    img = imagem_de_teste("", ["--vesa", "--console", "--gpu3d=simulado"])
    with Maquina("h424g", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        for n, nome in enumerate(("lightmap", "particulas", "sombra", "reflejo"), 1):
            out = m.comando(f"es11demo -g {n}", 240).replace("\r", "")
            g = re.search(rf"es11demo -g: {nome} software ([0-9a-f]{{8}}) gpu ([0-9a-f]{{8}}) \((GPU|CPU)\) distintos (\d+) de (\d+)", out)
            print("   ", g.group(0) if g else out[-400:])
            verificar(g, f"es11demo -g {n} ({nome}) responde")
            verificar(g.group(1) == sumas[nome], f"{nome}: la suma por software {g.group(1)} = PC {sumas[nome]}")
            dif, tot = int(g.group(4)), int(g.group(5))
            verificar(g.group(3) == "GPU", f"{nome}: el contexto sigue en la GPU")
            # Los bordes no son iguales (el simulado rasteriza triángulos con enteros); en «particulas» hay
            # cientos de sprites pequeños superpuestos con mezcla aditiva: casi todo son bordes (2026-11-20: 8129).
            tope = 12 if nome == "particulas" else 1
            verificar(dif * 100 <= tot * tope, f"{nome}: por la GPU simulada casi igual que por software ({dif} de {tot} distintos; tope {tope} %)")
        s = m.serial()
        verificar(re.search(r"simulado v2: pruebas: pedidas 0x[0-9a-f]+, superadas 0x[0-9a-f]+ \(faltan 0x0\)", s),
                  "las pruebas v2 (g3d_v2_provas.c) pasan todas en el simulado")
        verificar("simulado: primer dibujo v2" in s, "SaguiGL manda dibujos v2 al driver")
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones")


def tcc():
    cruzado = os.path.join(RAIZ, "build", "tcc", "anfitrion", "i386-tcc")
    raiz = os.path.join(RAIZ, "build", "tcc", "raiz")
    if not os.path.exists(cruzado) or not os.path.exists(os.path.join(raiz, "lib", "libGLESv1_CM.a")):
        print("    (sin TCC compilado: se salta)")
        return
    os.makedirs(TMP, exist_ok=True)
    for nome, fonte, libs in (("gles", "userland/terceros/tcc/ejemplos/gles.c", ["-lSDL2", "-lGLESv1_CM", "-lEGL"]),
                              ("es11demo_tcc", "userland/programas/es11demo.c", ["-lEGL", "-lGLESv1_CM"])):
        r = subprocess.run([cruzado, "-B" + raiz, "-I" + os.path.join(raiz, "include", "sagui"), "-o", os.path.join(TMP, nome),
                            os.path.join(RAIZ, fonte)] + libs, capture_output=True, text=True)
        verificar(r.returncode == 0, f"tcc compila y enlaza {fonte}" + ("" if r.returncode == 0 else ":\n" + r.stderr[-1500:]))


def main():
    pedidas = sys.argv[1:] or ["unidad", "fuentes", "rendimiento", "qemu", "capturas", "gpu", "tcc"]
    for p in pedidas:
        print("==", p)
        globals()[p]()
    print("H424 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
