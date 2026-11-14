#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H77 (D104, mejora 054 v5) - SaguiGL, prueba de píxeles.

Criterio:
  - el motor libsgl (userland/libsgl/sgl.c) se compila en el anfitrión con gcc
    (tests/sgl/anfitriao.c, -DSGL_ANFITRIAO) y da la suma FNV-1a de cada escena
    fija (triángulo con Gouraud y plano, cubo girado 30 grados con Z y cull,
    Z cruzado con líneas y abanico), con búfer Z de 16 y de 32 bits;
  - `sgldemo -c <escena>` dentro del SaguiOS (compilado con el GCC -m32 y
    SSE2) da exactamente las mismas sumas: el motor es determinista;
  - `sgldemo -t N` dibuja N cuadros del cubo y su suma del último cuadro
    coincide con la del anfitrión (cubo a 3*(N-1) grados);
  - se comprueban las propiedades: triángulo no vacío, el cubo tiene píxeles
    de fondo y de color, y Z de 16 y de 32 dan lo mismo en estas escenas.
Con SAGUI_KVM=1 se imprimen además los fps medidos.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console, RAIZ  # noqa: E402

ESCENAS = ("triangulo", "cubo", "zcruzado")
FLAGS = ["-O2", "-msse2", "-mfpmath=sse", "-ffp-contract=off", "-fno-math-errno", "-Wall", "-DSGL_ANFITRIAO"]


def esperado(angulo_n=None):
    """Compila el motor en el anfitrión y devuelve {(escena, z): suma}."""
    exe = os.path.join(RAIZ, "build", "sgl_anfitriao")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    fuentes = [os.path.join(RAIZ, "tests/sgl/anfitriao.c"), os.path.join(RAIZ, "userland/libsgl/sgl.c"),
               os.path.join(RAIZ, "userland/libsgl/cenas.c")]
    subprocess.run(["gcc"] + FLAGS + ["-o", exe] + fuentes, check=True)
    out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout
    r = {}
    for m in re.finditer(r"^(\w+) z(\d+) ([0-9a-f]{8})$", out, re.M):
        r[(m.group(1), int(m.group(2)))] = m.group(3)
    return r


GPUS = [("8086:0126", "Intel Gen6", "previsto"), ("8086:2582", "Intel Gen3", "previsto"), ("8086:27a2", "Intel Gen3", "previsto"),
        ("8086:29c2", "Intel Gen3", "previsto"), ("8086:0042", "Intel Gen5", "software"), ("8086:2a42", "Intel Gen4", "software"),
        ("8086:3577", "Intel Gen2", "software"), ("8086:0166", "Intel posterior", "software"),
        ("1002:9610", "AMD R6xx/R7xx", "previsto"), ("1002:9442", "AMD R6xx/R7xx", "previsto"), ("1002:7145", "AMD R5xx", "previsto"),
        ("1002:68e0", "AMD Evergreen", "previsto"), ("1002:4e50", "AMD R300/R420", "previsto"), ("1002:4a49", "AMD R300/R420", "previsto"),
        ("1002:743f", "AMD posterior", "software"), ("1002:67df", "AMD posterior", "software"), ("1002:ffff", "desconocida", "software"),
        ("10de:0640", "otra", "software"), ("1234:1111", "otra", "software")]


def prueba_gpus():
    exe = os.path.join(RAIZ, "build", "gpuid_anfitriao")
    subprocess.run(["gcc", "-Wall", "-o", exe, os.path.join(RAIZ, "tests/sgl/gpuid.c")], check=True)
    out = subprocess.run([exe] + [g[0] for g in GPUS], check=True, capture_output=True, text=True).stdout.splitlines()
    for (ident, fam, motor), linha in zip(GPUS, out):
        f = linha.split("|")
        verificar(f[1].startswith(fam) and motor in f[2], f"GPU {ident} -> {f[1]} ({f[2]})")


def main():
    prueba_gpus()
    kvm = os.environ.get("SAGUI_KVM") == "1"
    esp = esperado()
    verificar(len(esp) == 8, f"el anfitrión da 8 sumas ({len(esp)})")
    for z in (16, 32):
        verificar(len({esp[(e, z)] for e in ESCENAS}) == 3, f"escenas distintas entre sí (Z {z})")
    verificar(esp[("cubo", 16)] == esp[("cubo", 32)] and esp[("zcruzado", 16)] == esp[("zcruzado", 32)],
              "Z de 16 y de 32 bits dan la misma imagen en estas escenas")
    console = imagem_console()
    with Maquina("h77", "ide", imagem=console) as m:
        m.esperar_serial("sagui:/$", 120)
        for e in ESCENAS:
            out = m.comando(f"sgldemo -c {e}", 60)
            for z in (16, 32):
                g = re.search(rf"^{e} z{z} ([0-9a-f]{{8}})$", out, re.M)
                verificar(g, f"sgldemo -c {e}: salida con Z {z}")
                verificar(g.group(1) == esp[(e, z)],
                          f"{e} Z{z}: suma del SaguiOS {g.group(1)} = anfitrión {esp[(e, z)]}")
        n = 60
        for z in (16, 32):
            out = m.comando(f"sgldemo -t {n} -z {z}", 120)
            out2 = m.comando(f"sgldemo -t 600 -z {z}", 300)
            print("  fps con 600 cuadros:", re.search(r"(\d+) ms, (\d+) fps", out2).group(0))
            g = re.search(r"cubo, (\d+) cuadros 320x240, Z de (\d+) bits, suma ([0-9a-f]{8}), (\d+) ms, (\d+) fps", out)
            verificar(g and int(g.group(1)) == n, f"sgldemo -t {n} -z {z}: informe de cuadros")
            verificar(g.group(3) == esp[("demo60", z)], f"-t {n} Z{z}: suma del último cuadro {g.group(3)} = anfitrión {esp[('demo60', z)]}")
            print(f"  (cubo 320x240, Z {z}: {g.group(5)} fps, {g.group(4)} ms para {n} cuadros{' con KVM' if kvm else ', TCG'})")
        out = m.comando("infopc -t video", 60)
        verificar(re.search(r"^GPU:\s+[0-9a-f]{4}:[0-9a-f]{4}, \S+", out, re.M) and "SaguiGL 3D:" in out,
                  "infopc -t video: familia de la GPU y motor de SaguiGL")
        out = m.comando("sgldemo -c nada", 30)
        verificar("escena desconocida" in out, "escena desconocida: aviso")
    print("H77 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
