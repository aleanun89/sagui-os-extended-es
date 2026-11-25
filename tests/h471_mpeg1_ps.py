#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H471 (D471) - el decodificador de MPEG-1 (userland/libsagui/mpeg1.c) con MPEG-PS «difíciles» (la introducción intro.MPG de Tzar).

Se genera un MPEG-1 con el ffmpeg y se empaqueta de varias formas; el decodificador del anfitrión (el mismo código que va a la
libsagui) tiene que dar todos los cuadros (50) en cada una:
  - MPEG-1 sistema con audio (el caso de siempre);
  - MPEG-2 PS (cabecera de paquete de 14 bytes, VOB) con vídeo MPEG-1: antes se comía el primer 0x00 del código siguiente al leer
    el relleno de la cabecera y se perdían casi todos los cuadros (9 de 50);
  - sin cabeceras de grupo (GOP);
  - paquetes PES pequeños (de 100 a 400 bytes): los códigos de cuadro y la cabecera de secuencia quedan partidos entre dos
    paquetes; antes la cuenta de cuadros fallaba y la cabecera de secuencia a medias dejaba el vídeo sin tamaño («no es un vídeo
    que se lea»);
  - archivo que empieza por un paquete de vídeo y no por la cabecera de paquete (0xBA).
Uso: h471_mpeg1_ps.py
"""
import os
import re
import shutil
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(RAIZ, "build", "h471")
DECO = os.path.join(DIR, "video_testar")
CUADROS = 50


def falha(msg):
    print("FALHA: " + msg)
    sys.exit(1)


def ffmpeg():
    exe = shutil.which("ffmpeg") or os.path.expanduser("~/.local/saguitools/extra/usr/bin/ffmpeg")
    if not os.path.exists(exe):
        print("H471 omitida: no hay ffmpeg")
        sys.exit(0)
    return exe


def compilar():
    cmd = ["gcc", "-O2", "-Wall", "-Wextra", "-DSAGUI_HOST", "-DH264_HOST", "-Wno-unused-function", "-Itools", "-Iuserland/inc",
           "-Iuserland/libsagui", "-Ikernel/inc", "-o", DECO, "tools/video_testar.c", "tools/video_host.c", "userland/libsagui/video.c",
           "userland/libsagui/imagem_jpeg.c", "userland/libsagui/imagem_idct.c", "userland/libsagui/mpeg1.c", "userland/libsagui/video_mp4.c",
           "userland/libsagui/h264.c", "userland/libsagui/aac.c", "userland/libsagui/mp4.c", "-lm"]
    r = subprocess.run(cmd, cwd=RAIZ, capture_output=True, text=True)
    if r.returncode != 0:
        falha("no compila el decodificador:\n" + r.stderr[-2000:])


def run(args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        falha("%s:\n%s" % (" ".join(args[:6]), r.stderr[-1500:]))


def cuadros(ruta):
    """(cuadros decodificados, cuadros que dice la cabecera, texto) con el programa de prueba del decodificador."""
    r = subprocess.run([DECO, ruta], capture_output=True, text=True)
    n = len(re.findall(r"^cuadro \d+ ", r.stdout, re.M))
    cab = re.search(r"(\d+) cuadros \d+ ms", r.stdout + r.stderr)
    return n, (int(cab.group(1)) if cab else -1), (r.stdout[-200:] + r.stderr[-200:])


def main():
    ff = ffmpeg()
    os.makedirs(DIR, exist_ok=True)
    compilar()
    es = os.path.join(DIR, "es.m1v")
    run([ff, "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc=size=320x240:rate=25:duration=2", "-c:v", "mpeg1video", "-b:v", "600k",
         "-bf", "0", "-f", "mpeg1video", es])
    d = open(es, "rb").read()
    sin = bytearray()
    i = 0
    while i < len(d):
        if d[i:i + 4] == b"\x00\x00\x01\xb8":
            i += 8
            continue
        sin.append(d[i])
        i += 1
    nogop = os.path.join(DIR, "nogop.m1v")
    open(nogop, "wb").write(sin)
    casos = []
    a = os.path.join(DIR, "sistema.mpg")
    run([ff, "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc=size=320x240:rate=25:duration=2", "-f", "lavfi", "-i", "sine=duration=2",
         "-c:v", "mpeg1video", "-b:v", "600k", "-bf", "2", "-c:a", "mp2", "-f", "mpeg", a])
    casos.append(("MPEG-1 sistema con audio", a))
    v = os.path.join(DIR, "vob.mpg")
    run([ff, "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc=size=320x240:rate=25:duration=2", "-f", "lavfi", "-i", "sine=duration=2",
         "-c:v", "mpeg1video", "-b:v", "600k", "-bf", "2", "-c:a", "mp2", "-f", "vob", v])
    casos.append(("MPEG-2 PS (cabecera de paquete de 14 bytes) con vídeo MPEG-1", v))
    g = os.path.join(DIR, "nogop.mpg")
    run([ff, "-v", "error", "-y", "-r", "25", "-f", "mpegvideo", "-i", nogop, "-c:v", "copy", "-f", "mpeg", g])
    casos.append(("sin cabeceras de grupo", g))
    for ps in (100, 150, 400):
        p = os.path.join(DIR, "ps%d.mpg" % ps)
        run([ff, "-v", "error", "-y", "-r", "25", "-f", "mpegvideo", "-i", nogop, "-c:v", "copy", "-packetsize", str(ps), "-f", "mpeg", p])
        casos.append(("paquetes PES de %d bytes (códigos partidos entre paquetes)" % ps, p))
    # sin la cabecera de paquete ni la del sistema al principio: el archivo empieza en un paquete de vídeo
    s = open(a, "rb").read()
    k = s.find(b"\x00\x00\x01\xe0")
    corte = os.path.join(DIR, "corte.mpg")
    open(corte, "wb").write(s[k:])
    casos.append(("empieza por un paquete de vídeo", corte))
    mal = 0
    for nome, ruta in casos:
        n, cab, txt = cuadros(ruta)
        ok = n == CUADROS and cab == CUADROS
        print("%s %s: %d cuadros (cabecera %d)" % ("ok  " if ok else "MAL ", nome, n, cab))
        if not ok:
            print("     " + txt.replace("\n", " | "))
            mal += 1
    if mal:
        falha("%d casos sin los %d cuadros" % (mal, CUADROS))
    print("H471 ok: %d paquetes de MPEG-PS dan los %d cuadros" % (len(casos), CUADROS))


if __name__ == "__main__":
    main()
