#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H457 (D457) - gpu3d v3: la GPU guarda los vértices en su memoria y los transforma.

QEMU no emula ninguna GPU con un procesador de vértices de los drivers reales: lo de cada tarjeta se
prueba en la máquina (docs/gpu3d.md, «gpu3d v3»). Lo que se comprueba aquí:

  unidad   en el PC: tests/g3d_v3_unidad.c (la coma flotante por software del kernel contra la del
           procesador, bit a bit; la transformación por la CPU de kernel/gui/gpu3d_v3.c contra una
           referencia de OpenGL ES 1.1 en coma flotante; los límites: búferes cortos, índices fuera,
           basura), también con ASan/UBSan; tests/sgl/es11_gpu_falsa.c con -DFALSA_V3 (SaguiGL por
           v3 contra una GPU falsa: glBufferData/glBufferSubData/glDrawArrays/glDrawElements con VBO
           van a los búferes de la GPU, cada búfer se sube una vez, glBufferSubData sube solo el
           trozo, con poca memoria hay expulsiones y se vuelve a subir, con luces va por la CPU; y
           la misma imagen que por software); tests/r500_v3_unidad.c (Radeon R5xx: el programa de
           vértices generado, ejecutado por un simulador del PVS, y las órdenes del dibujo desde
           búferes); la escena 5 de es11demo (un mundo en VBO) en el PC.
  fuentes  la ABI (tamaños fijos), la interfaz del driver (dos funciones al final), la capa valida
           todo antes del driver, el simulado lo implementa, y la D457 y los documentos.
  qemu     con el driver simulado (--gpu3d=simulado): la autoprueba v3 de la capa pasa; es11demo -g 5
           dibuja el mundo por software y por la GPU con dibujos v3 (25 búferes subidos una vez) y
           casi los mismos píxeles, y la suma por software es la del PC; con poca «memoria de GPU»
           (0x1014) hay expulsiones y la imagen no cambia; con una «GPU que transforma mal» (0x1018)
           la autoprueba v3 lo nota, v3 queda apagado, el 3D sigue encendido y la escena va por v2;
           y las cuatro escenas de siempre siguen iguales.

Uso: h457_gpu3d_v3.py [unidad] [fuentes] [qemu]
"""
import io
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste, RAIZ  # noqa: E402

DIR = os.path.join(RAIZ, "build", "h457")
LIB = os.path.join(RAIZ, "userland", "libsgl")
KINC = ["-I", os.path.join(RAIZ, "kernel", "inc")]
SGL_FLAGS = ["-O2", "-msse2", "-mfpmath=sse", "-ffp-contract=off", "-fno-math-errno", "-w", "-DSGL_ANFITRIAO",
             "-I" + os.path.join(RAIZ, "userland", "inc"), "-I" + LIB]


def ler(rel):
    with io.open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def gcc(args, que):
    r = subprocess.run(["gcc"] + args, capture_output=True, text=True)
    verificar(r.returncode == 0, f"{que} compila\n" + r.stderr[-3000:])


def suma_mundo_pc():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "es11demo")
    gcc(SGL_FLAGS + ["-o", exe, os.path.join(RAIZ, "userland/programas/es11demo.c")] +
        [os.path.join(LIB, f) for f in ("sgl.c", "sgl_fixo.c", "sgl_egl.c")], "es11demo en el PC")
    out = subprocess.run([exe, "-c", "5"], capture_output=True, text=True, check=True).stdout
    g = re.search(r"^mundo ([0-9a-f]{8})$", out, re.M)
    verificar(g, "es11demo -c 5 en el PC da la suma del mundo")
    return g.group(1)


def unidad():
    os.makedirs(DIR, exist_ok=True)
    fontes = [os.path.join(RAIZ, "tests", "g3d_v3_unidad.c"), os.path.join(RAIZ, "kernel/gui/gpu3d_v3.c"),
              os.path.join(RAIZ, "kernel/gui/gpu3d_mem.c")]
    for nome, flags in (("g3d_v3_unidad", ["-O2", "-Wall", "-Wextra"]),
                        ("g3d_v3_unidad_asan", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"])):
        exe = os.path.join(DIR, nome)
        gcc(flags + ["-fno-builtin", "-Wno-builtin-declaration-mismatch"] + KINC + ["-o", exe] + fontes + ["-lm"], nome)
        r = subprocess.run([exe], capture_output=True, text=True)
        print("   ", (r.stdout + r.stderr).strip().replace("\n", "\n    "))
        verificar(r.returncode == 0 and "todo bien" in r.stdout, f"{nome}: coma flotante, transformación contra ES 1.1 y límites")
    # Radeon R5xx: el programa de vértices generado (simulador del PVS) y las órdenes del dibujo desde búferes
    exe = os.path.join(DIR, "r500_v3_unidad")
    gcc(["-O1", "-fno-builtin", "-Wall", "-Wno-builtin-declaration-mismatch"] + KINC + ["-I", os.path.join(RAIZ, "kernel", "dispositivos"),
        "-o", exe, os.path.join(RAIZ, "tests", "r500_v3_unidad.c"), os.path.join(RAIZ, "kernel/gui/gpu3d_mem.c"), "-lm"], "r500_v3_unidad")
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", (r.stdout + r.stderr).strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "r500_v3_unidad: el programa de vértices de la R5xx hace la cuenta de ES 1.1 y las órdenes están bien formadas")
    # Radeon R6xx/R7xx: el sombreador de vértices generado, leído por un decodificador propio y ejecutado por un simulador
    exe = os.path.join(DIR, "r600_v3_unidad")
    gcc(["-O2", "-Wall", "-DR600_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe, os.path.join(RAIZ, "tests", "r600_v3_unidad.c"),
         os.path.join(RAIZ, "kernel/dispositivos/r600_3d_pacotes.c"), "-lm"], "r600_v3_unidad")
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", (r.stdout + r.stderr).strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "r600_v3_unidad: el sombreador de vértices v3 de las R6xx/R7xx hace la cuenta de ES 1.1, "
              "cumple las reglas de los puertos y las órdenes están bien formadas")
    # SaguiGL por v3 contra la GPU falsa (que transforma con la misma función que el simulado del kernel)
    objs = []
    for f in ("gpu3d_v3", "gpu3d_mem"):
        o = os.path.join(DIR, f + ".o")
        gcc(["-O2", "-fno-builtin", "-Wno-builtin-declaration-mismatch"] + KINC + ["-c", "-o", o, os.path.join(RAIZ, "kernel/gui", f + ".c")], f)
        objs.append(o)
    exe = os.path.join(DIR, "es11_gpu_falsa_v3")
    gcc(SGL_FLAGS + ["-DSGL_GPU_FALSA", "-DFALSA_V3", "-o", exe, os.path.join(RAIZ, "tests/sgl/es11_gpu_falsa.c"),
                     os.path.join(LIB, "sgl.c"), os.path.join(LIB, "sgl_fixo.c")] + objs, "es11_gpu_falsa con v3")
    r = subprocess.run([exe], capture_output=True, text=True)
    for linha in r.stdout.splitlines():
        if linha.startswith("v3 ") or linha.startswith("gpu_falsa"):
            print("   ", linha)
    verificar(r.returncode == 0 and "gpu_falsa: bien" in r.stdout and r.stdout.count("v3 modo") == 12 and " MAL" not in r.stdout,
              "SaguiGL por gpu3d v3 (GPU falsa): búferes subidos una vez, trozos, expulsión, luces por la CPU y la misma imagen")
    print("    es11demo -c 5 (mundo) en el PC:", suma_mundo_pc())


def fuentes():
    abi = ler("kernel/inc/abi.h")
    capa = ler("kernel/gui/gpu3d.c")
    h = ler("kernel/inc/gpu3d.h")
    sim = ler("kernel/dispositivos/gpu3d_sim.c")
    for nome, num in (("GPU3D_OP_VBUF", 15), ("GPU3D_OP_TRANSF", 16), ("GPU3D_OP_DESENHAR3", 17), ("GPU3D_OP_LUZES", 18)):
        verificar(re.search(rf"#define {nome}\s+{num}\b", abi), f"abi.h: {nome} = {num}")
    verificar(re.search(r"sizeof\(gpu3d_vbuf_t\) == 32 && sizeof\(gpu3d_atrib_t\) == 8 && sizeof\(gpu3d_transf_t\) == 580 &&\s*"
                        r"sizeof\(gpu3d_luz_t\) == 96 && sizeof\(gpu3d_luzes_t\) == 880 && sizeof\(gpu3d_desenho3_t\) == 80", capa),
              "la capa fija el tamaño de la ABI v3 con _Static_assert")
    verificar("sizeof(gpu3d_capas_t) == 64" in capa and "unsigned int bits3;" in abi, "gpu3d_capas_t sigue midiendo 64 bytes con bits3")
    verificar(re.search(r"int\s+\(\*apresentar\)\(const g3d_apres_t \*p\);.*?int\s+\(\*vbuf\)\(int vaga, uint32_t bytes, uint32_t desloc, "
                        r"const void \*dados, uint32_t n\);.*?int\s+\(\*desenhar3\)\(const g3d_desenho3_t \*d, g3d_vala_t \*vala\);\s*\} gpu3d_driver_t;", h, re.S),
              "gpu3d_driver_t: vbuf y desenhar3 al final (pueden ser NULL)")
    verificar("s_apresentar,\n    s_vbuf, s_desenhar3,\n};" in sim and "g3d_v3_cpu(&g, &c)" in sim, "el driver simulado implementa v3 con g3d_v3_cpu")
    r6 = ler("kernel/dispositivos/gpu3d_r600.c")
    verificar("r3_apresentar,\n    r3_vbuf, r3_desenhar3,\n};" in r6 and "v3_confirmada()" in r6 and "R3_REG_DEPURAR" in r6,
              "R6xx/R7xx: vbuf y desenhar3, v3 solo en las confirmadas o con «inicio 28ffc 2», y la bandera no se escribe como registro")
    # la capa: sin las dos funciones o sin v2 no hay v3; todo se valida antes del driver; la autoprueba decide
    verificar("if (!drv->vbuf || !drv->desenhar3 || !(capas.bits & G3D_CAP_V2)) capas.bits3 = 0;" in capa, "sin vbuf/desenhar3 o sin v2 no hay v3")
    op = capa[capa.index("case GPU3D_OP_DESENHAR3:"):capa.index("default:\n        return ERRO_INVALIDO;")]
    verificar(op.index("atrib_cabe(&g.d.pos") < op.index("drv->desenhar3(") and "GPU3D_EXPULSO" in capa and "k->cheio" in capa,
              "DESENHAR3: cada atributo debe caber en lo subido a su búfer antes de llamar al driver")
    verificar("vb_expulsar((int)b.vaga)" in capa and "n_vb_expulsos++" in capa, "la capa expulsa el búfer menos usado si uno nuevo no cabe")
    verificar("static bool autoprova3(void)" in capa and "g3d_v3_cpu(&g, &c)" in capa and "capas.bits3 = b3; prova3 = 1;" in capa,
              "la autoprueba v3 compara el desenhar3 del driver con la CPU y solo entonces anuncia v3")
    v3 = ler("kernel/gui/gpu3d_v3.c")
    verificar("float " not in re.sub(r"/\*.*?\*/", "", v3, flags=re.S) and "double" not in v3, "gpu3d_v3.c no usa coma flotante de la máquina")
    sgl = ler("userland/libsgl/sgl_es.c")
    verificar("static int es_v3_desenhar(" in sgl and "GPU3D_OP_DESENHAR3" in sgl and "es_vb_mudou(b, (int)desl, (int)tam);" in sgl,
              "SaguiGL: glDraw* con VBO por v3 y glBufferSubData sube el trozo")
    dec = ler("docs/decisoes.md")
    verificar(re.search(r"^## D457\. .*\(decidido el 2026-11-21; v8\.5\.5\.3, rama `v8\.5\.5\.3-vbo-gpu`\)$", dec, re.M), "la D457 en docs/decisoes.md")
    doc = ler("docs/gpu3d.md")
    verificar("## gpu3d v3" in doc and "GPU3D_OP_DESENHAR3" in doc and "G3D_C3_V3" in doc, "docs/gpu3d.md describe gpu3d v3")
    verificar("D457" in ler("docs/gpus.md"), "docs/gpus.md (tabla viva) nombra la D457")
    for rel in ("kernel/gui/gpu3d_v3.c", "kernel/inc/gpu3d_v3.h", "tests/g3d_v3_unidad.c", "tests/h457_gpu3d_v3.py"):
        verificar(not any(0x0370 <= ord(c) <= 0x04FF or 0x3000 <= ord(c) <= 0x9FFF for c in ler(rel)), f"{rel}: sin otros alfabetos")


def demo_g(m, n, nome):
    out = m.comando(f"es11demo -g {n}", 400).replace("\r", "")
    g = re.search(rf"es11demo -g: {nome} software ([0-9a-f]{{8}}) gpu ([0-9a-f]{{8}}) \((GPU|CPU)\) distintos (\d+) de (\d+)", out)
    print("   ", g.group(0) if g else out[-400:])
    verificar(g, f"es11demo -g {n} ({nome}) responde")
    return g.group(1), g.group(3), int(g.group(4)), int(g.group(5))


def v3_dibujos(s):
    """El último resumen de v3 del registro: dibujos, subidas, expulsiones."""
    r = re.findall(r"\[GPU3D\] v3 \(D457\): (\d+) dibujos desde buferes \((\d+) vertices\), (\d+) subidas \((\d+) KB\), (\d+) buferes \((\d+) KB\) "
                   r"al soltar, (\d+) expulsiones", s)
    return [int(x) for x in r[-1]] if r else None


def qemu():
    suma = suma_mundo_pc()
    img = imagem_de_teste("h457", ["--vesa", "--console", "--gpu3d=simulado"])
    with Maquina("h457", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        # 1. la autoprueba de la capa (v1 y v3) y lo que dice «gpu3d»
        out = m.comando("gpu3d probar --detalle", 240)
        s = m.serial()
        verificar(re.search(r"\[GPU3D\] v3 \(la GPU transforma y guarda los vertices, D457\): autoprueba BIEN en \d+ us; bits 0x[0-9a-f]+", s),
                  "la autoprueba v3 de la capa pasa con el simulado")
        for e in range(3):
            verificar(re.search(rf"\[GPU3D\] v3: escena {e} \([^)]*\): bien; \d+ pixeles distintos", s), f"autoprueba v3: escena {e} bien (con --detalle)")
        out = m.comando("gpu3d", 60)
        verificar(re.search(r"vertices por la GPU \(v3, D457\): si \(bits 0x[0-9a-f]+\)", out), "«gpu3d» dice que los vértices van por la GPU")
        # 2. el mundo en VBO: por software = PC; por la GPU con dibujos v3
        sw, onde, dif, tot = demo_g(m, 5, "mundo")
        verificar(sw == suma, f"mundo: la suma por software {sw} = PC {suma}")
        verificar(onde == "GPU" and dif * 100 <= tot * 3, f"mundo: por la GPU simulada con v3 casi igual que por software ({dif} de {tot})")
        s = m.serial()
        verificar("simulado: primer dibujo v3" in s, "SaguiGL manda dibujos v3 al driver")
        d = v3_dibujos(s)
        print("    v3: dibujos, vertices, subidas, KB, buferes, KB, expulsiones =", d)
        verificar(d and d[0] == 25 and d[2] == 25 and d[4] == 25 and d[6] == 0 and d[1] > 30000,
                  "el mundo son 25 dibujos desde 25 búferes subidos una vez, sin expulsiones")
        # 3. las escenas de siempre (sin VBO) siguen por v2
        for n, nome in ((1, "lightmap"), (3, "sombra")):
            _, onde, dif, tot = demo_g(m, n, nome)
            verificar(onde == "GPU" and dif * 100 <= tot, f"{nome}: como antes ({dif} de {tot})")
        # 4. poca memoria para búferes (400 KB; el mundo pide ~1,2 MB): expulsa y vuelve a subir, misma imagen
        m.comando("echo 'inicio 1014 190' > /sistema/conf/gpu3d-simulado.conf")
        m.comando("gpu3d apagar", 60)
        m.comando("gpu3d encender", 240)
        sw, onde, dif2, tot = demo_g(m, 5, "mundo")
        d = v3_dibujos(m.serial())
        print("    con 400 KB: dibujos, vertices, subidas, KB, buferes, KB, expulsiones =", d)
        verificar(onde == "GPU" and dif2 * 100 <= tot * 3 and d and d[0] == 25 and d[6] > 0 and d[4] < 25,
                  f"con poca memoria de GPU hay expulsiones ({d and d[6]}) y la imagen es la misma ({dif2} de {tot})")
        verificar("la memoria de la GPU para vertices esta llena" in m.serial(), "la capa avisa de la primera expulsión")
        # 5. una GPU que transforma mal: la autoprueba v3 lo nota, v3 se apaga, el 3D sigue y el mundo va por v2
        m.comando("echo 'inicio 1018 1' > /sistema/conf/gpu3d-simulado.conf")
        m.comando("gpu3d apagar", 60)
        antes = len(m.serial())
        m.comando("gpu3d encender", 240)
        s = m.serial()[antes:]
        verificar(re.search(r"\[GPU3D\] v3: escena \d \([^)]*\): FALLO; \d+ pixeles distintos", s) and
                  "autoprueba FALLO" in s and "la transformacion sigue por la CPU (v2), el 3D sigue encendido" in s,
                  "con la transformación mal, la autoprueba v3 falla y lo dice")
        out = m.comando("gpu3d", 60)
        verificar("3D por GPU: activo" in out or "activo" in out.split("\n")[0], "el 3D sigue encendido")
        verificar(re.search(r"vertices por la GPU \(v3, D457\): NO: su autoprueba fallo en la escena \d", out), "«gpu3d» dice que v3 falló y en qué escena")
        antes = len(m.serial())
        sw, onde, dif3, tot = demo_g(m, 5, "mundo")
        s = m.serial()[antes:]
        verificar(onde == "GPU" and dif3 * 100 <= tot * 3 and "dibujos desde buferes" not in s,
                  f"sin v3 el mundo va por v2 (la CPU transforma) y sale igual ({dif3} de {tot})")
        # 6. el driver sin v3 (0x101C): nada de v3 y todo como antes
        m.comando("echo 'inicio 101c 1' > /sistema/conf/gpu3d-simulado.conf")
        m.comando("gpu3d apagar", 60)
        m.comando("gpu3d encender", 240)
        out = m.comando("gpu3d", 60)
        verificar("este driver no lo tiene" in out, "un driver sin v3: «gpu3d» lo dice")
        m.comando("rm /sistema/conf/gpu3d-simulado.conf")
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones")


def main():
    pedidas = sys.argv[1:] or ["unidad", "fuentes", "qemu"]
    for p in pedidas:
        print("==", p)
        globals()[p]()
    print("H457 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
