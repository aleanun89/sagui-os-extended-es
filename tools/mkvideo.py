#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
mkvideo.py - genera el vídeo de ejemplo de SaguiOS (mejora 048, D129).

El vídeo es PROPIO: los cuadros se dibujan aquí (el sagui de arte.c de la
carpeta kernel/gui/, un degradado y una franja que se mueve) y se codifican
con ffmpeg en MJPEG dentro de un AVI, que es lo que reproduce el reproductor
de SaguiOS. También lleva una pista de audio (un tono propio) para que se
vea la sincronización.

Nada de material de terceros: ni la música ni las imágenes son de nadie.

Uso:
  mkvideo.py [--larg 426] [--alt 240] [--fps 30] [--segundos 2]
             [--calidade 8] [--sem-audio] [--saida rootfs/videos/sagui.avi]

Las pruebas (tests/h100_video_v62.py) usan este mismo generador para tener
vídeos propios y deterministas.
"""
import math
import os
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ------------------------------------------------------------ el sagui (arte.c)

def sagui_arte():
    """El dibujo del sagui de kernel/gui/arte.c, con su misma paleta."""
    src = open(os.path.join(RAIZ, "kernel", "gui", "arte.c"), encoding="utf-8").read()
    ini = src.index("static const char *const sagui[] = {")
    fim = src.index("};", ini)
    linhas = [l.strip().strip(",").strip('"') for l in src[ini:fim].splitlines()[1:] if '"' in l]
    paleta = {"P": (0x14, 0x12, 0x10), "E": (0x3C, 0x2D, 0x23), "M": (0x6E, 0x55, 0x3C),
              "c": (0xBE, 0xB4, 0xA5), "B": (0xF5, 0xF5, 0xF0), "R": (0xAA, 0x78, 0x64),
              "b": (0xFF, 0xFF, 0xFF)}
    return linhas, paleta


def desenhar_sagui(px, larg, alt, x0, y0, escala):
    """Pega el sagui (16x16) en el lienzo px, escalado."""
    linhas, cores = sagui_arte()
    for y in range(16 * escala):
        for x in range(16 * escala):
            linha = linhas[y // escala]
            c = linha[x // escala] if x // escala < len(linha) else " "
            cor = cores.get(c)
            if cor is None:
                continue
            px_x, px_y = x0 + x, y0 + y
            if 0 <= px_x < larg and 0 <= px_y < alt:
                px[px_y * larg + px_x] = cor


# ------------------------------------------------------------ los cuadros

def quadro(n, total, larg, alt):
    """El cuadro n de la animación: un degradado de cielo, una franja que se
    mueve y el sagui que rebota."""
    px = [(0, 0, 0)] * (larg * alt)
    t = n / max(1, total)
    for y in range(alt):
        for x in range(larg):
            # cielo: de azul oscuro arriba a claro abajo
            f = y / max(1, alt - 1)
            r = int(20 + 60 * f)
            g = int(40 + 120 * f)
            b = int(120 + 100 * f)
            # una franja clara que se mueve de izquierda a derecha
            banda = (x + n * 3) % (larg + 80) - 40
            if abs(banda) < 26:
                k = (26 - abs(banda)) / 26 * 70
                r = min(255, int(r + k))
                g = min(255, int(g + k))
                b = min(255, int(b + k))
            px[y * larg + x] = (r, g, b)
    # el sagui rebota en diagonal
    esc = 4
    lx = int((larg - 16 * esc) * (0.5 + 0.45 * math.sin(t * 2 * math.pi)))
    ly = int((alt - 16 * esc) * (0.5 - 0.45 * math.sin(t * 4 * math.pi)))
    desenhar_sagui(px, larg, alt, lx, ly, esc)
    return px


def audio(bruto, taxa, segundos, freq):
    """Un tono propio (seno de freq Hz), PCM de 16 bits mono."""
    n = int(taxa * segundos)
    dados = bytearray()
    for i in range(n):
        v = int(9000 * math.sin(2 * math.pi * freq * i / taxa))
        dados += struct.pack("<h", v)
    bruto.write(dados)


# ------------------------------------------------------------ el programa

def main():
    larg, alt, fps, segundos, qualidade = 426, 240, 30, 2, 8
    saida = os.path.join(RAIZ, "rootfs", "videos", "sagui.avi")
    com_audio = True
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--larg": larg = int(args[i + 1]); i += 2
        elif a == "--alt": alt = int(args[i + 1]); i += 2
        elif a == "--fps": fps = int(args[i + 1]); i += 2
        elif a == "--segundos": segundos = int(args[i + 1]); i += 2
        elif a == "--calidade": qualidade = int(args[i + 1]); i += 2
        elif a == "--sem-audio": com_audio = False; i += 1
        elif a == "--saida": saida = args[i + 1]; i += 2
        else:
            print(f"opción desconocida: {a}", file=sys.stderr)
            return 2
    total = max(1, int(fps * segundos))
    taxa = 22050
    os.makedirs(os.path.dirname(os.path.abspath(saida)), exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        arq_audio = os.path.join(tmp, "audio.pcm")
        with open(arq_audio, "wb") as f:
            if com_audio:
                audio(f, taxa, total / fps, 440.0)

        cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
               "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{larg}x{alt}", "-r", str(fps), "-i", "pipe:0"]
        if com_audio:
            cmd += ["-f", "s16le", "-ar", str(taxa), "-ac", "1", "-i", arq_audio]
            cmd += ["-c:a", "pcm_s16le"]
        else:
            cmd += ["-an"]
        cmd += ["-c:v", "mjpeg", "-q:v", str(qualidade), "-f", "avi", saida]

        ff = subprocess.Popen(cmd, stdin=subprocess.PIPE)
        for n in range(total):
            px = quadro(n, total, larg, alt)
            bruto = bytearray(larg * alt * 3)
            for i, (r, g, b) in enumerate(px):
                bruto[i * 3] = r
                bruto[i * 3 + 1] = g
                bruto[i * 3 + 2] = b
            ff.stdin.write(bruto)
        ff.stdin.close()
        if ff.wait() != 0:
            print("ffmpeg falló", file=sys.stderr)
            return 1

    print(f"mkvideo: {saida} ({larg}x{alt}, {fps} fps, {total} cuadros, "
          f"{os.path.getsize(saida)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
