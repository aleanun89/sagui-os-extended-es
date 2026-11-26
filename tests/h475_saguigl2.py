#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H475 (D475) - SaguiGL 2.0, fase 1: OpenGL ES 2.0 por la CPU, GLSL ES 1.00 y la representación intermedia.

  unidad       en el PC: tests/sgl/es20_conformidad.c (compilación con errores y su línea, enlace, consultas,
               uniformes, errores de la API, funciones embutidas contra valores calculados aparte, control de
               flujo, escenas comparadas píxel a píxel, FBO, texturas, alternar con ES 1.1) sin fallos y también
               con ASan/UBSan; es20demo da sus cuatro sumas; la RI de unos sombreadores es válida y compacta.
  es11         los programas de OpenGL ES 1.1 siguen igual: es11conf y es11demo dan las mismas sumas con la
               libsgl de ES 2.0 (el despacho de sgl2_despacho.c) que sin ella.
  fuentes      cabeceras GLES2 propias y completas (cada función definida y en eglGetProcAddress), el despacho
               generado al día (tools/sgl/gerar_despacho.py), la D475 y docs/saguigl2.md, sin otros alfabetos.
  rendimiento  ms por cuadro de cada escena de es20demo en el PC (para comparar; no falla).
  qemu         en el SaguiOS: es20conf sin fallos y con la misma suma que el PC; es20demo -c 1..4 las mismas
               sumas que el PC; es11conf y es11demo -c 1..4 las de siempre; es20demo -m da los ms por cuadro.
  janela       en el escritorio del SaguiOS: es20demo -n 8 abre su ventana (superficie de ventana de EGL con un
               contexto de ES 2.0), dibuja las cuatro escenas y da los fps de cada una.
  capturas     las cuatro escenas de la demo (hechas en el PC, sintéticas) en docs/capturas/H475-*.png.

Uso: h475_saguigl2.py [unidad] [es11] [fuentes] [rendimiento] [qemu] [janela] [capturas]   (sin nada: todo)
"""
import io
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
SGL1 = [os.path.join(LIB, f) for f in ("sgl.c", "sgl_fixo.c", "sgl_egl.c")]
SGL2 = SGL1 + [os.path.join(LIB, f + ".c") for f in ("sgl2", "sgl2_desenho", "sgl2_despacho", "sgl2_gpu", "sgl_glsl", "sgl_ri", "sgl_ri_cpu", "sgl_tgsi")]
TMP = os.path.join(RAIZ, "build", "h475")
NOMES = ("triangulo", "cubo", "fbo", "plasma")


def compilar(nome, fonte, fontes, extra=None):
    os.makedirs(TMP, exist_ok=True)
    exe = os.path.join(TMP, nome)
    subprocess.run(["gcc"] + FLAGS + (extra or []) + ["-o", exe, fonte] + fontes, check=True)
    return exe


def conformidade(extra=None, nome="es20conf"):
    exe = compilar(nome, os.path.join(RAIZ, "tests/sgl/es20_conformidad.c"), SGL2, ["-DSGL_ES2"] + (extra or []))
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    p = subprocess.run([exe], capture_output=True, text=True, env=env)
    g = re.search(r"conformidad: (\d+) bien, (\d+) mal", p.stdout)
    s = re.search(r"suma ([0-9a-f]{8})", p.stdout)
    return p, g, s


def demo_sumas():
    exe = compilar("es20demo", os.path.join(RAIZ, "userland/programas/es20demo.c"), SGL2, ["-DSGL_ES2"])
    out = subprocess.run([exe], capture_output=True, text=True, check=True).stdout
    return dict(re.findall(r"^(\w+) ([0-9a-f]{8})$", out, re.M)), exe


SOMBREADORES = {
    "luz.f": ("f", """precision mediump float;
uniform sampler2D u_tex; uniform vec3 u_luz; varying vec3 v_n, v_p; varying vec2 v_tc;
void main() { vec3 n = normalize(v_n), l = normalize(u_luz - v_p), h = normalize(l - normalize(v_p));
  float d = max(dot(n, l), 0.0), s = pow(max(dot(n, h), 0.0), 32.0);
  gl_FragColor = vec4(texture2D(u_tex, v_tc).rgb * (0.15 + 0.85 * d) + vec3(0.6) * s, 1.0); }"""),
    "laco.f": ("f", """precision highp float; uniform vec4 u_v[8]; uniform int u_n; varying vec2 v;
float f(float x) { for (int i = 0; i < 10; i++) { if (float(i) > x) return float(i); } return -1.0; }
void main() { vec4 a = vec4(0.0); for (int i = 0; i < 8; i++) { if (i >= u_n) break; a += u_v[i]; }
  if (a.x < 0.0) discard; gl_FragColor = a * f(v.x); }"""),
    "vertices.v": ("v", """attribute vec4 a_pos; attribute vec3 a_n; uniform mat4 u_mvp; uniform mat3 u_nm;
varying vec3 v_n; void main() { v_n = u_nm * a_n; gl_Position = u_mvp * a_pos; gl_PointSize = 2.0; }"""),
}


def unidad():
    p, g, s = conformidade()
    print("   ", g.group(0) if g else p.stdout[-600:])
    verificar(p.returncode == 0 and g and g.group(2) == "0" and int(g.group(1)) >= 150,
              "conformidad de OpenGL ES 2.0 en el PC" + ("" if p.returncode == 0 else ":\n" + p.stdout[-2000:]))
    p2, g2, s2 = conformidade(["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"], "es20conf_asan")
    verificar(p2.returncode == 0 and g2 and g2.group(2) == "0" and "runtime error" not in p2.stderr and "ERROR: AddressSanitizer" not in p2.stderr,
              "la conformidad con ASan y UBSan, sin avisos" + ("" if p2.returncode == 0 else ":\n" + (p2.stdout + p2.stderr)[-2000:]))
    verificar(s2 and s and s2.group(1) == s.group(1), f"la misma suma con -O1 y saneadores ({s2 and s2.group(1)}) que con -O2 ({s.group(1)})")
    sumas, _ = demo_sumas()
    verificar(len(sumas) == 4 and len(set(sumas.values())) == 4, f"es20demo: cuatro escenas distintas ({sumas})")
    exe = compilar("es20_ri", os.path.join(RAIZ, "tests/sgl/es20_ri.c"), [os.path.join(LIB, f) for f in ("sgl_glsl.c", "sgl_ri.c", "sgl_ri_cpu.c")])
    with tempfile.TemporaryDirectory() as d:
        for nome, (tipo, fonte) in SOMBREADORES.items():
            f = os.path.join(d, nome)
            io.open(f, "w", encoding="utf-8").write(fonte)
            r = subprocess.run([exe, tipo, f], capture_output=True, text=True)
            cab = re.search(r"(\d+) instrucciones, (\d+) temporales", r.stdout)
            print(f"    {nome}: {cab.group(0) if cab else r.stdout[:200]}")
            verificar(r.returncode == 0 and cab, f"RI de {nome} válida")
            verificar(int(cab.group(2)) <= 32, f"{nome}: temporales compactados ({cab.group(2)} <= 32, lo que tiene una R300)")
            if nome == "laco.f":
                verificar("BGNLOOP" in r.stdout and "KILL" in r.stdout and "a0.x+" in r.stdout, "laco.f: bucles, KILL e índice dinámico en la RI")


def es11():
    for nome, fonte in (("es11conf", "tests/sgl/es11_conformidad.c"), ("es11demo", "userland/programas/es11demo.c")):
        a = compilar(nome + "_es1", os.path.join(RAIZ, fonte), SGL1)
        b = compilar(nome + "_es2", os.path.join(RAIZ, fonte), SGL2, ["-DSGL_ES2"])
        sa = subprocess.run([a], capture_output=True, text=True).stdout
        sb = subprocess.run([b], capture_output=True, text=True).stdout
        sa_s = re.findall(r"([0-9a-f]{8})$", sa, re.M)
        print(f"    {nome}: {' '.join(sa_s)}")
        verificar(sa_s and sa == sb, f"{nome}: lo mismo con la libsgl de ES 2.0 que sin ella")


def fuentes():
    nomes = re.findall(r"GL_APIENTRY \**(gl[A-Za-z0-9]+)\(", io.open(os.path.join(INC, "GLES2/gl2.h"), encoding="utf-8").read())
    verificar(len(nomes) == 142, f"GLES2/gl2.h: las 142 funciones de OpenGL ES 2.0 ({len(nomes)})")
    procs = io.open(os.path.join(LIB, "sgl_procs.inc"), encoding="utf-8").read() + io.open(os.path.join(LIB, "sgl2_procs.inc"), encoding="utf-8").read()
    faltam = [n for n in nomes if f"P({n})" not in procs]
    verificar(not faltam, f"eglGetProcAddress conoce todas las de ES 2.0 (faltan {faltam[:5]})")
    os.makedirs(TMP, exist_ok=True)
    definidas = set()
    for f in SGL2:
        o = os.path.join(TMP, os.path.basename(f) + ".es2.o")
        subprocess.run(["gcc"] + FLAGS + ["-DSGL_ES2", "-c", "-o", o, f], check=True)
        for l in subprocess.run(["nm", o], capture_output=True, text=True, check=True).stdout.splitlines():
            c = l.split()
            if len(c) == 3 and c[1] == "T":
                definidas.add(c[2])
    sem = [n for n in nomes if n not in definidas]
    verificar(not sem, f"cada función de GLES2/gl2.h tiene definición (faltan {sem[:5]})")
    for h in ("GLES2/gl2.h", "GLES2/gl2ext.h", "GLES2/gl2platform.h"):
        t = io.open(os.path.join(INC, h), encoding="utf-8").read()
        verificar("SPDX-License-Identifier: GPL-2.0-or-later" in t and "Khronos Group Inc" not in t, f"{h}: propia (sin texto de Khronos)")
    with tempfile.TemporaryDirectory() as d:
        antes = {f: io.open(os.path.join(LIB, f), encoding="utf-8").read() for f in ("sgl_es1_nomes.h", "sgl2_comuns.h", "sgl2_despacho.c")}
        subprocess.run([sys.executable, os.path.join(RAIZ, "tools/sgl/gerar_despacho.py")], check=True)
        depois = {f: io.open(os.path.join(LIB, f), encoding="utf-8").read() for f in antes}
        verificar(antes == depois, "el despacho generado está al día (tools/sgl/gerar_despacho.py)")
    dec = io.open(os.path.join(RAIZ, "docs/decisoes.md"), encoding="utf-8").read()
    verificar("## D475." in dec, "D475 en docs/decisoes.md")
    verificar(os.path.exists(os.path.join(RAIZ, "docs/saguigl2.md")), "docs/saguigl2.md")
    for f in ("userland/libsgl/sgl_ri.h", "userland/libsgl/sgl_ri.c", "userland/libsgl/sgl_ri_cpu.c", "userland/libsgl/sgl_glsl.c",
              "userland/libsgl/sgl_glsl_ri.h", "userland/libsgl/sgl2.c", "userland/libsgl/sgl2.h", "userland/libsgl/sgl2_desenho.c",
              "userland/inc/GLES2/gl2.h", "tests/sgl/es20_conformidad.c", "userland/programas/es20demo.c", "docs/saguigl2.md"):
        t = io.open(os.path.join(RAIZ, f), encoding="utf-8").read()
        verificar(not re.search("[Ͱ-ϿЀ-ӿ　-鿿]", t), f"{f}: sin otros alfabetos")


def rendimiento():
    _, exe = demo_sumas()
    out = subprocess.run([exe, "-m", "20"], capture_output=True, text=True, check=True).stdout
    for l in out.splitlines():
        print("   ", l)
    verificar(len(re.findall(r"ms por cuadro", out)) == 4, "es20demo -m: las cuatro escenas medidas")


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
        for n, nome in enumerate(NOMES, 1):
            ppm = os.path.join(d, f"{n}.ppm")
            subprocess.run([exe, "-p", str(n), ppm], check=True)
            png_de_ppm(ppm, os.path.join(cap, f"H475-{nome}.png"))
    verificar(all(os.path.exists(os.path.join(cap, f"H475-{n}.png")) for n in NOMES), "las cuatro capturas de la demo")


def qemu():
    _, g, s = conformidade()
    sumas, _ = demo_sumas()
    es11c = compilar("es11conf_es1", os.path.join(RAIZ, "tests/sgl/es11_conformidad.c"), SGL1)
    s11 = re.search(r"suma ([0-9a-f]{8})", subprocess.run([es11c], capture_output=True, text=True).stdout).group(1)
    es11d = compilar("es11demo_es1", os.path.join(RAIZ, "userland/programas/es11demo.c"), SGL1)
    d11 = dict(re.findall(r"^(\w+) ([0-9a-f]{8})$", subprocess.run([es11d], capture_output=True, text=True).stdout, re.M))
    with Maquina("h475", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("es20conf", 600)
        g2 = re.search(r"conformidad: (\d+) bien, (\d+) mal", out)
        print("   ", g2.group(0) if g2 else out[-600:])
        verificar(g2 and g2.group(2) == "0" and g2.group(1) == g.group(1), "es20conf dentro del SaguiOS: sin fallos" + ("" if g2 and g2.group(2) == "0" else ":\n" + out[-1500:]))
        ss = re.search(r"suma ([0-9a-f]{8})", out)
        verificar(ss and ss.group(1) == s.group(1), f"es20conf: la suma del SaguiOS {ss and ss.group(1)} = la del PC {s.group(1)}")
        for n, nome in enumerate(NOMES, 1):
            out = m.comando(f"es20demo -c {n}", 300)
            r = re.search(rf"^{nome} ([0-9a-f]{{8}})", out, re.M)
            verificar(r and r.group(1) == sumas[nome], f"es20demo -c {n} ({nome}): {r and r.group(1)} = PC {sumas[nome]}")
        out = m.comando("es11conf", 300)
        r = re.search(r"suma ([0-9a-f]{8})", out)
        verificar(r and r.group(1) == s11 and "0 mal" in out, f"es11conf sigue igual ({r and r.group(1)} = {s11})")
        for n, nome in enumerate(("lightmap", "particulas", "sombra", "reflejo"), 1):
            out = m.comando(f"es11demo -c {n}", 180)
            r = re.search(rf"^{nome} ([0-9a-f]{{8}})", out, re.M)
            verificar(r and r.group(1) == d11[nome], f"es11demo -c {n} ({nome}) sigue igual")
        out = m.comando("es20demo -m 3", 600)
        for l in re.findall(r"es20demo: .* ms por cuadro.*", out):
            print("   ", l.strip())
        verificar(len(re.findall(r"ms por cuadro", out)) == 4, "es20demo -m 3 en el SaguiOS")
        verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones")


def janela():
    import h161_acel2d as h161
    with Maquina("h475j", "ide", imagem=imagem_de_teste("", ["--vesa"])) as m:
        h161.arrancar(m)
        out = m.comando("es20demo -n 8", 600)
        linhas = re.findall(r"es20demo: (\w+) 8 cuadros en \d+ ms, \d+ fps", out)
        for l in re.findall(r"es20demo: .* fps", out):
            print("   ", l.strip())
        verificar(linhas == list(NOMES), f"es20demo en una ventana: las cuatro escenas ({linhas})" + ("" if linhas == list(NOMES) else ":\n" + out[-1200:]))
        verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones")


def main():
    pedidas = sys.argv[1:] or ["unidad", "es11", "fuentes", "rendimiento", "qemu", "janela", "capturas"]
    for p in pedidas:
        print("==", p)
        globals()[p]()
    print("H475 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
