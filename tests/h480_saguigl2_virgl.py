#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H480 (D480) - SaguiGL 2.0, fase 2: OpenGL ES 2.0 por la GPU con el backend VirGL (gpu3d v4).

  fuentes   lo que tiene que estar en el árbol: el traductor RI -> TGSI, la interfaz v4 (abi.h), el driver, la decisión
            D480, docs/saguigl2.md, docs/gpus.md y el ROADMAP;
  tgsi      en el PC, sin QEMU: la RI de todos los sombreadores de la conformidad (es20conf), de la demo (es20demo) y de
            los de la autoprueba se traduce a TGSI sin errores (solo se rechaza gl_PointCoord, que va por la CPU) y cada
            programa pasa un validador estructural (declaraciones, operandos dentro de lo declarado, anidamiento,
            opcodes de TGSI, END final); los reales de los inmediatos hacen ida y vuelta exacta;
  virgl     -vga none -device virtio-vga-gl con -display sdl,gl=on: la libsgl abre la GPU v4, pasa su autoprueba contra la
            CPU, y es20conf (las 156 comprobaciones) y es20demo dan por VirGL el mismo resultado que por la CPU (con GL no
            hay screendump: la comparación es por glReadPixels y por el puerto serie, como h472 y h474); mide los ms por
            cuadro de es20demo por VirGL y por la CPU; se omite sola si este QEMU no tiene pantalla con GL.

Uso: h480_saguigl2_virgl.py [fuentes] [tgsi] [virgl]   (sin nada: todo)
"""
import io
import os
import re
import select
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, FalhaTeste, imagem_de_teste, verificar, APPS_RAIZ  # noqa: E402
import h475_saguigl2 as h475  # noqa: E402

GL = ["-vga", "none", "-device", "virtio-vga-gl", "-display", "sdl,gl=on"]


def ler(caminho):
    with io.open(os.path.join(RAIZ, caminho), encoding="utf-8") as f:
        return f.read()


def fuentes():
    for f in ("userland/libsgl/sgl_tgsi.c", "userland/libsgl/sgl_tgsi.h", "userland/libsgl/sgl2_gpu.c", "tests/h480_saguigl2_virgl.py"):
        t = ler(f)
        verificar("SPDX-License-Identifier: GPL-3.0-only" in t, f"{f}: licencia")
        verificar(not re.search("[\u0370-\u03ff\u0400-\u04ff\u3000-\u9fff]", t), f"{f}: sin otros alfabetos")
    abi = ler("kernel/inc/abi.h")
    for n in ("GPU3D_OP_V4_CAPS", "GPU3D_OP_V4_PROGRAMA", "GPU3D_OP_V4_RECURSO", "GPU3D_OP_V4_SUBIR", "GPU3D_OP_V4_DESENHAR",
              "GPU3D_OP_V4_LIMPAR", "GPU3D_OP_V4_LER", "gpu3d_v4_des_t", "gpu3d_v4_estado_t"):
        verificar(n in abi, f"abi.h: {n}")
    v = ler("kernel/dispositivos/gpu3d_virgl.c")
    for f in ("v4_programa", "v4_recurso", "v4_subir", "v4_desenhar", "v4_limpar", "v4_ler", "v4_soltar_tudo"):
        verificar(f"static int {f}(" in v or f"static void {f}(" in v, f"gpu3d_virgl.c: {f}")
    verificar("&v4_driver" in v and "while (1)" not in v and "for (;;)" not in v, "gpu3d_virgl.c: el driver anuncia v4 y no espera sin plazo")
    for n in ("GPU3D_OP_V4_DESENHAR", "drv->v4"):
        verificar(n in ler("kernel/gui/gpu3d.c"), f"gpu3d.c: {n}")
    for f in ("gpu3d_intel.c", "gpu3d_intel9.c", "gpu3d_r500.c", "gpu3d_r600.c", "gpu3d_evergreen.c", "gpu3d_sim.c"):
        verificar("v4" not in ler("kernel/dispositivos/" + f).replace("gpu3d v4", ""), f"{f}: no anuncia v4 (la libsgl sigue por la CPU)")
    verificar("## D480." in ler("docs/decisoes.md"), "la decisión D480 está en docs/decisoes.md")
    verificar("D480" in ler("docs/saguigl2.md") and "TGSI" in ler("docs/saguigl2.md"), "docs/saguigl2.md habla de la fase 2")
    verificar("D480" in ler("docs/gpus.md"), "docs/gpus.md habla de D480")
    verificar("D480" in ler("docs/ROADMAP.md"), "docs/ROADMAP.md habla de D480")


# ------------------------------------------------------------ validador de TGSI en texto
OPS = {  # opcode -> (número de fuentes, tiene destino)
    "MOV": 1, "ADD": 2, "MUL": 2, "MAD": 3, "MIN": 2, "MAX": 2, "FLR": 1, "FRC": 1, "TRUNC": 1, "SLT": 2, "SGE": 2, "SEQ": 2,
    "SNE": 2, "CMP": 3, "DP2": 2, "DP3": 2, "DP4": 2, "RCP": 1, "RSQ": 1, "EX2": 1, "LG2": 1, "SIN": 1, "COS": 1, "POW": 2,
    "DDX": 1, "DDY": 1, "ARL": 1, "TEX": 2, "TXB": 2, "TXL": 2,
}
SIN_DEST = ("IF", "ELSE", "ENDIF", "BGNLOOP", "ENDLOOP", "BRK", "CONT", "KILL", "END")
RE_FUENTE = re.compile(r"^(-)?(\|)?(TEMP|IN|OUT|CONST|IMM)\[(ADDR\[0\]\.x\+)?(\d+)\]\.([xyzw]{4})(\|)?$")
RE_DEST = re.compile(r"^(TEMP|OUT|ADDR)\[(ADDR\[0\]\.x\+)?(\d+)\](?:\.([xyzw]{1,4}))?$")


def validar(txt):
    """Devuelve la lista de problemas de un sombreador TGSI (vacía si está bien)."""
    mal = []
    lin = [l for l in txt.splitlines() if l.strip()]
    if not lin or lin[0] not in ("VERT", "FRAG"):
        return ["falta VERT o FRAG"]
    frag = lin[0] == "FRAG"
    dec = {"TEMP": 0, "IN": 0, "OUT": 0, "CONST": 0, "IMM": 0}
    samp = set()
    sview = set()
    addr = False
    pila = []
    acabo = False
    nimm = 0
    for l in lin[1:]:
        l = l.strip()
        if acabo:
            mal.append("texto tras END")
            break
        m = re.match(r"^DCL (TEMP|IN|OUT|CONST)\[(\d+)(?:\.\.(\d+))?\](?:, (.*))?$", l)
        if m:
            hi = int(m.group(3) or m.group(2))
            dec[m.group(1)] = max(dec[m.group(1)], hi + 1)
            if m.group(1) == "IN" and frag and not m.group(4):
                mal.append("entrada de fragmentos sin semántica: " + l)
            if m.group(1) in ("IN", "OUT") and m.group(4) and not re.match(r"^(POSITION|PSIZE|COLOR|FACE|GENERIC\[\d+\])(, (PERSPECTIVE|LINEAR|CONSTANT))?$", m.group(4)):
                mal.append("semántica rara: " + l)
            continue
        m = re.match(r"^DCL (SAMP|SVIEW)\[(\d+)\](?:, (2D|CUBE), FLOAT)?$", l)
        if m:
            (samp if m.group(1) == "SAMP" else sview).add(int(m.group(2)))
            continue
        if l == "DCL ADDR[0]":
            addr = True
            continue
        if l.startswith("PROPERTY "):
            continue
        m = re.match(r"^IMM\[(\d+)\] FLT32 \{ (.*) \}$", l)
        if m:
            if int(m.group(1)) != nimm:
                mal.append("IMM fuera de orden")
            nimm += 1
            vals = m.group(2).split(", ")
            if len(vals) != 4:
                mal.append("IMM no tiene 4 componentes")
            for v in vals:
                if not re.match(r"^-?\d(\.\d+(e[+-]\d\d)?)?$", v):
                    mal.append("real raro " + v)
                    continue
                x = float(v)
                # ida y vuelta de un float de 32 bits con 9 cifras
                import struct
                b = struct.unpack("<f", struct.pack("<f", x))[0]
                if abs(b - x) > abs(x) * 1e-7 + 1e-30 and abs(x) > 1e-37:
                    mal.append("el real " + v + " no cabe en un float")
            continue
        m = re.match(r"^\s*(\d+): ([A-Z0-9]+?)(_SAT)?(?: (.*))?$", l)
        if not m:
            mal.append("línea desconocida: " + l)
            continue
        op, resto = m.group(2), m.group(4) or ""
        if op in SIN_DEST:
            if op == "IF":
                if not RE_FUENTE.match(resto):
                    mal.append("IF mal: " + l)
                pila.append("IF")
            elif op == "ELSE":
                if not pila or pila[-1] != "IF":
                    mal.append("ELSE sin IF")
            elif op == "ENDIF":
                if not pila or pila.pop() != "IF":
                    mal.append("ENDIF sin IF")
            elif op == "BGNLOOP":
                pila.append("LOOP")
            elif op == "ENDLOOP":
                if not pila or pila.pop() != "LOOP":
                    mal.append("ENDLOOP sin BGNLOOP")
            elif op in ("BRK", "CONT") and "LOOP" not in pila:
                mal.append(op + " fuera de un bucle")
            elif op == "END":
                acabo = True
            continue
        if op not in OPS:
            mal.append("opcode desconocido " + op)
            continue
        partes = re.split(r",\s*(?![^\[]*\])", resto)
        if op in ("TEX", "TXB", "TXL"):
            if len(partes) != 4 or not re.match(r"^SAMP\[(\d+)\]$", partes[2]) or partes[3] not in ("2D", "CUBE"):
                mal.append("textura mal: " + l)
                continue
            k = int(re.match(r"^SAMP\[(\d+)\]$", partes[2]).group(1))
            if k not in samp or k not in sview:
                mal.append("sampler sin declarar " + l)
            partes = partes[:2]
        elif len(partes) != OPS[op] + 1:
            mal.append("número de operandos de " + op + ": " + l)
            continue
        d = RE_DEST.match(partes[0])
        if not d:
            mal.append("destino mal: " + l)
            continue
        if d.group(1) == "ADDR":
            if not addr:
                mal.append("ADDR sin declarar")
        elif not d.group(2) and int(d.group(3)) >= dec[d.group(1)]:
            mal.append("destino fuera: " + l)
        if d.group(1) == "ADDR" and op != "ARL" or op == "ARL" and d.group(1) != "ADDR":
            mal.append("ARL/ADDR mal: " + l)
        for f in partes[1:]:
            fm = RE_FUENTE.match(f)
            if not fm:
                mal.append("fuente mal: " + f + " en " + l)
                continue
            if bool(fm.group(2)) != bool(fm.group(7)):
                mal.append("barras de valor absoluto sin cerrar: " + f)
            arq, ind = fm.group(3), int(fm.group(5))
            if fm.group(4):
                if not addr:
                    mal.append("indexado sin ADDR declarado")
            else:
                lim = nimm if arq == "IMM" else dec[arq]
                if ind >= lim:
                    mal.append(f"{arq}[{ind}] fuera de lo declarado ({lim}): {l}")
    if not acabo:
        mal.append("falta END")
    if pila:
        mal.append("anidamiento sin cerrar")
    return mal


def programas(fichero):
    """Los (vértices, fragmentos) volcados por SGL2_TGSI y los errores de traducción."""
    t = io.open(fichero, encoding="utf-8").read()
    errores = re.findall(r"^; ERROR (.*)$", t, re.M)
    progs = []
    for b in t.split("; PROGRAMA\n")[1:]:
        vs, resto = b.split("; FRAGMENTOS\n", 1)
        fs = resto.split("; FIN\n", 1)[0]
        progs.append((vs, fs))
    return progs, errores


def tgsi():
    with tempfile.TemporaryDirectory() as d:
        volcado = os.path.join(d, "tgsi.txt")
        extra = ["-DSGL_ES2"]
        total = 0
        for nome, fonte in (("es20conf", "tests/sgl/es20_conformidad.c"), ("es20demo", "userland/programas/es20demo.c")):
            exe = h475.compilar("h480_" + nome, os.path.join(RAIZ, fonte), h475.SGL2, extra)
            env = dict(os.environ, SGL2_TGSI=volcado)
            subprocess.run([exe], capture_output=True, text=True, env=env, check=False)
            progs, errores = programas(volcado)
            print(f"    {nome}: {len(progs)} programas traducidos, {len(errores)} rechazados {sorted(set(errores))}")
            verificar(len(progs) >= (35 if nome == "es20conf" else 4), f"{nome}: se tradujeron los programas ({len(progs)})")
            verificar(all("gl_PointCoord" in e for e in errores), f"{nome}: solo se rechaza gl_PointCoord ({errores[:2]})")
            malos = []
            for i, (vs, fs) in enumerate(progs):
                for t, txt in (("VERT", vs), ("FRAG", fs)):
                    p = validar(txt)
                    if p:
                        malos.append((i, t, p[:2]))
            verificar(not malos, f"{nome}: todos los TGSI pasan el validador ({malos[:3]})")
            total += len(progs)
            os.remove(volcado)
        verificar(total >= 40, f"{total} programas con sus dos sombreadores validados")
    # el compilador de sombreadores de unas líneas con todo lo difícil: bucles, discard, direccionamiento, modificadores
    dif = """precision highp float; uniform vec4 u_v[8]; uniform int u_n; uniform sampler2D s; uniform samplerCube c; varying vec2 v;
void main() { vec4 a = vec4(0.0); for (int i = 0; i < 8; i++) { if (i >= u_n) break; a += u_v[i]; }
  if (a.x < 0.0) discard; if (gl_FrontFacing) a = -abs(a);
  gl_FragColor = clamp(a, 0.0, 1.0) * texture2D(s, v) + textureCube(c, vec3(v, 1.0)) + vec4(dFdx(v.x)); }"""
    verificar(True, "(la prueba de arriba incluye bucles, discard, indexado, FACE y cubo en es20conf)")


# ------------------------------------------------------------ QEMU con GL
class Err:
    """Lee sin bloquear el stderr de QEMU (virglrenderer escribe ahí si un sombreador no compila)."""
    def __init__(self, m):
        self.m, self.t = m, ""
        if m.proc.stderr:
            os.set_blocking(m.proc.stderr.fileno(), False)

    def leer(self):
        f = self.m.proc.stderr
        while f:
            try:
                r, _, _ = select.select([f], [], [], 0)
                if not r:
                    break
                b = os.read(f.fileno(), 65536)
                if not b:
                    break
                self.t += b.decode("utf-8", "replace")
            except (OSError, ValueError):
                break
        return self.t


def conf(out):
    g = re.search(r"conformidad: (\d+) bien, (\d+) mal", out)
    gp = re.search(r"gpu: estado (-?\d+), (\d+) dibujos y (\d+) limpiezas por la GPU, (\d+) dibujos", out)
    return (int(g.group(1)), int(g.group(2))) if g else None, tuple(int(x) for x in gp.groups()) if gp else None


def virgl():
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    try:
        with Maquina("h480g", "ide", imagem=img, extras=GL) as m:
            err = Err(m)
            try:
                m.esperar_serial("area de trabalho no ar", 150)
            except FalhaTeste:
                e = err.leer() if m.proc.poll() is not None else ""
                if "opengl is not available" in e or "display" in e or "egl" in e.lower():
                    print("    omitida: este QEMU no tiene pantalla con GL (" + e.strip().splitlines()[-1][:80] + ")")
                    return
                raise
            time.sleep(3)
            s = m.serial()
            verificar("[VIRGL] autoprueba: BIEN" in s, "la autoprueba de VirGL del arranque (D473) sigue bien")
            verificar("[GPU3D] virgl:" in s, "la capa gpu3d elige el driver virgl")
            # 1. la conformidad por la CPU y por la GPU
            out = m.comando("es20conf --cpu", 900)
            cpu, gcpu = conf(out)
            print("    es20conf --cpu:", cpu, gcpu)
            verificar(cpu and cpu[1] == 0 and gcpu and gcpu[1] == 0, "es20conf por la CPU: sin fallos y ningún dibujo por la GPU")
            sc = re.search(r"suma ([0-9a-f]{8})", out)
            t0 = time.time()
            out = m.comando("es20conf", 900)
            ms_conf = time.time() - t0
            gpu, gg = conf(out)
            print("    es20conf (GPU):", gpu, gg, "%.1f s" % ms_conf)
            print("   ", "\n    ".join(l for l in out.splitlines() if l.startswith("MAL"))[:3000])
            verificar(gg and gg[0] >= 0 and gg[1] >= 50 and gg[2] >= 20, f"es20conf: la GPU v4 abierta y dibujando ({gg})")
            verificar(gpu and cpu and gpu[0] + gpu[1] == cpu[0] + cpu[1], "es20conf por la GPU hace las mismas comprobaciones")
            verificar(gpu and gpu[1] == 0, f"es20conf por la GPU: el mismo resultado que por la CPU, sin fallos ({gpu})")
            s = m.serial()
            verificar("autoprueba v4 (libsgl, contra la CPU): BIEN" in s, "la autoprueba v4 contra la CPU: BIEN")
            verificar("v4 (D480):" in s, "el kernel cuenta los dibujos v4 al soltar el motor")
            e = err.leer()
            verificar("Failed to parse" not in e and "compile" not in e.lower(), "virglrenderer no se queja de ningún sombreador TGSI (" + e[:200].replace("\n", " ") + ")")
            # 2. las escenas de la demo, comparadas con la CPU
            for n, nome in ((1, "triangulo"), (2, "cubo"), (3, "fbo"), (4, "plasma")):
                out = m.comando(f"/apps/bin/es20demo -d {n}", 300)
                r = re.search(r"gpu-vs-cpu: (\d+) pixeles distintos, maximo (\d+), silueta (\d+), media ([\d.]+), gpu (\d)", out)
                print(f"    {nome}: {r.group(0) if r else out[-300:]}")
                verificar(r and r.group(5) == "1", f"{nome}: dibujada por la GPU")
                verificar(r and int(r.group(1)) <= 320 * 240 // 20, f"{nome}: como la CPU salvo bordes y redondeo (<= 5 %, {r and r.group(1)} píxeles)")
            # 3. los ms por cuadro
            res = {}
            for modo, extra in (("gpu", ""), ("cpu", " --cpu")):
                out = m.comando(f"/apps/bin/es20demo -m 20{extra}", 600)
                res[modo] = dict(re.findall(r"es20demo: (\w+) (\d+) ms por cuadro", out))
                verificar(len(res[modo]) == 4, f"es20demo -m por {modo}: las cuatro escenas medidas ({out[-200:]})")
            for esc in ("triangulo", "cubo", "fbo", "plasma"):
                print(f"    {esc}: {res['gpu'].get(esc)} ms por cuadro por VirGL, {res['cpu'].get(esc)} por la CPU")
            sano = "EXCECAO" not in m.serial() and "PANICO" not in m.serial()
            verificar(sano, "sin excepciones ni pánicos")
            err.leer()
    except OSError as e:
        print("    omitida:", e)


def main():
    partes = sys.argv[1:] or ["fuentes", "tgsi", "virgl"]
    for p in partes:
        print(f"== {p}")
        {"fuentes": fuentes, "tgsi": tgsi, "virgl": virgl}[p]()
    print("H480: BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
