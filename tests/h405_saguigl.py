#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H405 (D404) - SaguiGL más realista: filtro bilineal y mipmaps, perspectiva, luces por
vértice, formatos, la escena «paisaje», el cargador de OBJ y la caché de texturas en la GPU.

Partes (sin argumentos, todas menos «modelos» si no se han bajado):
  unidad    en el anfitrión (tests/sgl/h405_anfitriao.c): ajedrez reducido con y sin mipmaps,
            trilineal, niveles dados a mano, bilineal contra la fórmula, perspectiva (el centro
            del mundo con u = 0,5; la ruta de v6 sigue afín), luz difusa a 0 y 60 grados,
            especular, dos luces, atenuación, RGB565, RGBA4444, luminancia, 2048x2048, 300
            texturas, y el lector de OBJ/MTL (caras de 4, índices negativos, materiales).
            Además, h77 y h101 dan las mismas sumas que antes de la D404 (los píxeles de los
            programas de siempre no cambian).
  modelos   (si tools/modelos3d_datos.sh los bajó) los OBJ CC0 se leen en el anfitrión con sus
            triángulos (biplane 668).
  capturas  docs/capturas/H405-paisaje.png y H405-avion.png (sintéticas: todo por código).
  somas     «sgldemo -c paisaje» y «-c avion» en el SaguiOS dan las sumas del anfitrión.
  medida    «sgldemo -p --mem» (fps y triángulos/s por software, con y sin --texturas) y un OBJ
            con MTL y textura PPM leído desde un pendrive («--modelo»).
  simulado  --gpu3d=simulado: «sgldemo -g» con las siete escenas (bilineal, luces y paisaje
            incluidos) parecido a software, y «sgldemo -p --mem --gpu --texturas»: 32 huecos,
            texturas reutilizadas entre cuadros y subidas solo las de carga que no caben.

Uso: h405_saguigl.py [unidad] [modelos] [capturas] [somas] [medida] [simulado]
"""
import os
import re
import struct
import subprocess
import sys
import tempfile
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, FalhaTeste, verificar, imagem_console, imagem_de_teste,  # noqa: E402
                  criar_pendrive, pendrive_qemu, RAIZ)

FLAGS = ["-O2", "-msse2", "-mfpmath=sse", "-ffp-contract=off", "-fno-math-errno", "-w", "-DSGL_ANFITRIAO"]
LIB = os.path.join(RAIZ, "userland", "libsgl")
EXE = os.path.join(RAIZ, "build", "h405_anfitriao")
MODELOS = os.path.join(RAIZ, "userland", "terceros", "modelos3d", "datos")
CAPTURAS = os.path.join(RAIZ, "docs", "capturas")


def compilar():
    os.makedirs(os.path.dirname(EXE), exist_ok=True)
    subprocess.run(["gcc"] + FLAGS + ["-o", EXE, os.path.join(RAIZ, "tests/sgl/h405_anfitriao.c"),
                                      os.path.join(LIB, "sgl.c"), os.path.join(LIB, "sgl_obj.c"),
                                      os.path.join(LIB, "paisaje.c")], check=True)


def rodar(*args):
    return subprocess.run([EXE] + list(args), capture_output=True, text=True)


def somas_anfitriao():
    out = rodar("somas").stdout
    return {(m.group(1), int(m.group(2)), int(m.group(3))): m.group(4)
            for m in re.finditer(r"^(\w+) z(\d+) q(\d+) ([0-9a-f]{8})$", out, re.M)}


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


def sumas_viejas(rev):
    """Las sumas de h77 (escenas fijas) y h101 (ruta v6) con el sgl.c de `rev` y con el de ahora."""
    r = {}
    for nome, fonte in (("antes", None), ("ahora", LIB)):
        with tempfile.TemporaryDirectory() as d:
            if fonte is None:
                for f in ("sgl.c", "sgl.h"):
                    with open(os.path.join(d, f), "wb") as o:
                        o.write(subprocess.run(["git", "-C", RAIZ, "show", f"{rev}:userland/libsgl/{f}"],
                                               check=True, capture_output=True).stdout)
                inc = d
            else:
                inc = fonte
            sgl = os.path.join(inc, "sgl.c")
            a = os.path.join(d, "a77")
            subprocess.run(["gcc"] + FLAGS + ["-I" + inc, "-o", a, os.path.join(RAIZ, "tests/sgl/anfitriao.c"), sgl,
                            os.path.join(LIB, "cenas.c")], check=True)
            b = os.path.join(d, "a101")
            subprocess.run(["gcc"] + FLAGS + ["-I" + inc, "-o", b, os.path.join(RAIZ, "tests/sgl/sglv6.c"), sgl], check=True)
            r[nome] = subprocess.run([a], capture_output=True, text=True).stdout + subprocess.run([b], capture_output=True, text=True).stdout
    return r


def unidad():
    compilar()
    p = rodar("unidad")
    print("   ", p.stdout.strip().splitlines()[-1])
    verificar(p.returncode == 0 and re.search(r"unidad: (\d+) bien, 0 mal", p.stdout), "unidad del anfitrión" + ("" if p.returncode == 0 else ":\n" + p.stdout[-800:]))
    # el sgl.c de antes del primer commit de la D404
    primeiro = subprocess.run(["git", "-C", RAIZ, "log", "--format=%H", "--grep", "SaguiGL (D404)", "--", "userland/libsgl/sgl.c"],
                              capture_output=True, text=True).stdout.split()
    if primeiro:
        s = sumas_viejas(primeiro[-1] + "~1")
        verificar(s["antes"] == s["ahora"] and "completa" in s["ahora"],
                  "h77 y h101: las mismas sumas que antes de la D404 (los programas de antes no cambian de píxel)")


def modelos():
    if not os.path.exists(os.path.join(MODELOS, ".preparado")):
        print("    (sin modelos: tools/modelos3d_datos.sh)")
        return
    compilar()
    for f, tri in (("biplane.obj", 668), ("funky.obj", 696), ("ww1.obj", 5676)):
        out = rodar("obj", os.path.join(MODELOS, f)).stdout
        print("   ", f, out.strip())
        verificar(f"obj 0 triangulos {tri} " in out, f"{f}: {tri} triángulos")


def capturas():
    compilar()
    with tempfile.TemporaryDirectory() as d:
        p = rodar("ppm", d, "640", "480")
        verificar(p.returncode == 0, "las imágenes del paisaje")
        os.makedirs(CAPTURAS, exist_ok=True)
        for n in ("paisaje", "avion"):
            png_de_ppm(os.path.join(d, n + ".ppm"), os.path.join(CAPTURAS, f"H405-{n}.png"))
        bip = os.path.join(MODELOS, "biplane.obj")
        if os.path.exists(bip):
            # CC0 (mfep): en el anfitrión sin su textura (el PNG lo lee la libsagui)
            rodar("ppm", d, "640", "480", bip, "90")
            png_de_ppm(os.path.join(d, "avion_obj.ppm"), os.path.join(CAPTURAS, "H405-avion-biplane.png"))


def somas():
    compilar()
    esp = somas_anfitriao()
    verificar(len(esp) == 8 and len(set(esp.values())) == 8, f"el anfitrión da 8 sumas distintas ({len(esp)})")
    with Maquina("h405", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        for e in ("paisaje", "avion"):
            out = m.comando(f"sgldemo -c {e}", 900)
            for (n, z, q), v in esp.items():
                if n != e:
                    continue
                g = re.search(rf"^{e} z{z} q{q} ([0-9a-f]{{8}})$", out, re.M)
                verificar(g and g.group(1) == v, f"{e} Z{z} cuadro {q}: SaguiOS {g.group(1) if g else '?'} = anfitrión {v}")


OBJ_PRUEBA = """# cubo de prueba
mtllib cubo.mtl
v -1 -1 -1
v 1 -1 -1
v 1 1 -1
v -1 1 -1
v -1 -1 1
v 1 -1 1
v 1 1 1
v -1 1 1
vt 0 0
vt 1 0
vt 1 1
vt 0 1
usemtl ladrillo
f 1/1 4/4 3/3 2/2
f 5/1 6/2 7/3 8/4
f 1/1 5/2 8/3 4/4
f 2/1 3/2 7/3 6/4
usemtl gris
f 4 8 7 3
f 1 2 6 5
"""
MTL_PRUEBA = "newmtl ladrillo\nKd 1 1 1\nmap_Kd ladrillo.ppm\nnewmtl gris\nKd 0.5 0.5 0.5\nKs 1 1 1\nNs 200\n"


def ppm_prueba():
    px = bytearray()
    for y in range(16):
        for x in range(16):
            px += bytes((180, 60, 40) if (y % 4 and (x + (y // 4) * 4) % 8) else (200, 200, 190))
    return b"P6\n16 16\n255\n" + bytes(px)


def medida():
    pen = criar_pendrive("h405-pen", rotulo="OBJ", arquivos={"cubo.obj": OBJ_PRUEBA, "cubo.mtl": MTL_PRUEBA,
                                                             "ladrillo.ppm": ppm_prueba()})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h405m", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("sgldemo -p 6 --mem", 900)
        print("   ", "\n    ".join(l for l in out.splitlines() if "sgldemo" in l))
        g = re.search(r"320x240, 6 cuadros por software: (\d+) ms, ([\d.]+) fps, (\d+) triangulos por cuadro, (\d+) triangulos/s", out)
        verificar(g and int(g.group(3)) > 5000, "«sgldemo -p --mem»: fps y miles de triángulos por cuadro")
        verificar("memoria de la GPU: sin 3D por GPU" in out, "sin GPU lo dice")
        out = m.comando("sgldemo -p 3 --mem --texturas 2 --sin-mip -r 160x120", 900)
        verificar(re.search(r"\d+ KB de texturas, 8 de carga", out) and "3 cuadros por software" in out,
                  "«--texturas 2»: 8 texturas de carga de 256x256")
        time.sleep(1)
        out = m.comando("montar usb0", 30)
        verificar("montado" in out, "el pendrive con el OBJ")
        out = m.comando("sgldemo -p 2 --mem -r 160x120 --modelo /usb/cubo.obj", 600)
        print("   ", "\n    ".join(l for l in out.splitlines() if "modelo" in l))
        verificar("modelo /usb/cubo.obj: 12 triangulos, 2 materiales" in out, "«--modelo»: el OBJ con su MTL")
        verificar("2 cuadros por software" in out, "y el paisaje con ese avión se dibuja")
        out = m.comando("sgldemo -p 1 --mem -r 160x120 --modelo /usb/nada.obj", 300)
        verificar("no se pudo leer /usb/nada.obj" in out and "1 cuadros por software" in out,
                  "un OBJ que no existe: el avión hecho por código")


def simulado():
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado"])
    with Maquina("h405s", "ide", imagem=img) as m:
        fim = time.time() + 240
        while time.time() < fim and "area de trabalho no ar" not in m.serial() and "sagui:/$" not in m.serial():
            time.sleep(0.5)
        time.sleep(3)
        out = m.comando("sgldemo -g 2", 900)
        print("   ", "\n    ".join(l for l in out.splitlines() if l.startswith("gpu ")))
        for cena in ("triangulo", "cubo", "zcruzado", "textura", "bilineal", "luces", "paisaje"):
            verificar(re.search(rf"^gpu {cena}: .*: BIEN$", out, re.M), f"«sgldemo -g»: {cena} por la GPU simulada se parece a software")
        out = m.comando("sgldemo -p 4 --mem --gpu --sin-mip -r 160x120 --texturas 12", 900)
        print("   ", "\n    ".join(l for l in out.splitlines() if "sgldemo" in l))
        verificar("4 cuadros por la GPU" in out, "el paisaje por la GPU simulada")
        g = re.search(r"GPU: (\d+) huecos, (\d+) subidas \((\d+) KB, (\d+) por cuadro\), (\d+) reutilizadas", out)
        verificar(g and int(g.group(1)) == 32, "la biblioteca ve los 32 huecos del kernel")
        # 8 del paisaje + 48 de carga: las del paisaje se quedan, las de carga no caben todas
        verificar(g and int(g.group(5)) > 0 and int(g.group(2)) < 4 * 56, "texturas reutilizadas entre cuadros (caché)")
        verificar(re.search(r"memoria de la GPU: \d+ KB usados de \d+ KB", out), "memoria de la GPU usada")
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones ni pánicos")


PARTES = {"unidad": unidad, "modelos": modelos, "capturas": capturas, "somas": somas, "medida": medida, "simulado": simulado}


def main():
    partes = sys.argv[1:] or list(PARTES)
    for p in partes:
        print(f"== {p}")
        PARTES[p]()
    print("H405 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
