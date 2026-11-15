#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H115 (D137, mejora 048, fase 2) - el decodificador de MPEG-1 contra el ffmpeg.

La prueba genera un vídeo MPEG-1 dentro de un MPEG-PS con el ffmpeg, lo decodifica
con el módulo nuevo de la libsagui (userland/libsagui/mpeg1.c, que es el que usa
el reproductor) y compara los cuadros con los del ffmpeg (PSNR).

Estado (ver tools/mpeg1_psnr.txt):
  - los cuadros I (intra) dan bien la parte de la DCT: los coeficientes son los
    mismos que los del ffmpeg;
  - la conversión de color usa el alcance limitado (el del estudio), como hace
    el ffmpeg en el MPEG-1;
  - los cuadros P y B ya están bien: la compensación de movimiento es la de la
    norma (7.6.3), el primer coeficiente de los bloques inter se lee, y las
    referencias de delante y de detrás son las ciertas (en el MPEG-1 el vector
    "de delante" apunta al cuadro ANTERIOR).

La prueba exige el objetivo de la mejora 048: 35 dB en cada cuadro.

Uso: h115_video_mpeg1.py
"""
import math
import os
import shutil
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(RAIZ, "build", "h115")
MPG = os.path.join(DIR, "video.mpg")
REF = os.path.join(DIR, "ref.rgb")
DECO = os.path.join(DIR, "video_testar")
LARG, ALT = 426, 240

# El objetivo de la mejora 048: 35 dB en todos los cuadros.
PSNR_MINIMO = 35.0        # cada cuadro tiene que estar por encima de esto
PSNR_MEDIO_MINIMO = 35.0   # y la media también


def falha(msg):
    print("FALHA: " + msg)
    sys.exit(1)


def ffmpeg():
    exe = shutil.which("ffmpeg")
    if not exe:
        exe = os.path.expanduser("~/.local/saguitools/extra/usr/bin/ffmpeg")
        if not os.path.exists(exe):
            falha("ffmpeg no está instalado")
            return None
    return exe


def compilar():
    """Compila el decodificador del anfitrión (el mismo código que va a la libsagui)."""
    cmd = ["gcc", "-O2", "-Wall", "-Wextra", "-DSAGUI_HOST", "-DH264_HOST", "-Wno-unused-function",
           "-Itools", "-Iuserland/inc", "-Iuserland/libsagui", "-Ikernel/inc",
           "-o", DECO,
           "tools/video_testar.c", "tools/video_host.c",
           "userland/libsagui/video.c", "userland/libsagui/imagem_jpeg.c",
           "userland/libsagui/imagem_idct.c", "userland/libsagui/mpeg1.c",
           "userland/libsagui/video_mp4.c", "userland/libsagui/h264.c", "userland/libsagui/aac.c",
           "userland/libsagui/mp4.c", "-lm"]
    r = subprocess.run(cmd, cwd=RAIZ, capture_output=True, text=True)
    if r.returncode != 0:
        falha("el decodificador no compila:\n" + r.stderr[-2000:])
    return DECO


def gerar(exe):
    """Genera el vídeo de prueba: un MPEG-1 dentro de un MPEG-PS."""
    os.makedirs(DIR, exist_ok=True)
    cmd = [exe, "-v", "error", "-y",
           "-f", "lavfi", "-i", "testsrc=size=%dx%d:rate=30:duration=1" % (LARG, ALT),
           "-c:v", "mpeg1video", "-b:v", "600k", "-bf", "2", "-f", "mpeg", MPG]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(MPG):
        falha("ffmpeg no generó el vídeo:\n" + r.stderr[-2000:])


def referencia(exe):
    cmd = [exe, "-v", "error", "-y", "-i", MPG, "-pix_fmt", "rgb24",
           "-f", "rawvideo", REF]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        falha("ffmpeg no generó la referencia:\n" + r.stderr[-2000:])


def psnr(a, b):
    n = min(len(a), len(b))
    total = 0
    for i in range(n):
        d = a[i] - b[i]
        total += d * d
    mse = total / n
    if mse == 0:
        return 99.0
    return 10.0 * math.log10(255.0 * 255.0 / mse)


def contraste():
    exe = ffmpeg()
    gerar(exe)
    referencia(exe)
    compilar()
    meu = os.path.join(DIR, "meu.rgb")
    r = subprocess.run([DECO, "--saida", meu, MPG], capture_output=True, text=True)
    if r.returncode != 0:
        falha("el decodificador falló:\n" + r.stderr[-2000:])

    with open(REF, "rb") as f:
        ref = f.read()
    with open(meu, "rb") as f:
        cab = f.readline()
        meu_rgb = f.read()
    if not cab.startswith(b"SAGUIV1"):
        falha("la salida del decodificador no tiene la cabecera esperada")
    tam = LARG * ALT * 3
    if len(ref) < tam or len(meu_rgb) < tam:
        falha("las imágenes son demasiado pequeñas")

    nquadros = min(len(ref), len(meu_rgb)) // tam
    valores = []
    for i in range(nquadros):
        a = ref[i * tam:(i + 1) * tam]
        b = meu_rgb[i * tam:(i + 1) * tam]
        valores.append(psnr(a, b))
        print("  cuadro %2d: %6.2f dB" % (i, valores[-1]))
    media = sum(valores) / len(valores)
    print("media: %.2f dB" % media)

    # Todos los cuadros (I, P y B) tienen que dar bien con el ffmpeg.
    if valores:
        pior = min(valores)
        if pior < PSNR_MINIMO:
            falha("un cuadro quedó por debajo de %.0f dB (%.2f dB)"
                  % (PSNR_MINIMO, pior))
    if media < PSNR_MEDIO_MINIMO:
        falha("la media quedó por debajo de %.0f dB (%.2f dB)"
              % (PSNR_MEDIO_MINIMO, media))
    print("OK")
    return 0


def main():
    print("== MPEG-1 contra el ffmpeg")
    return contraste()


if __name__ == "__main__":
    sys.exit(main())
