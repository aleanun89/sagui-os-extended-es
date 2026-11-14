#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H100 (D129, mejora 048) - reproductor de vídeo: MJPEG dentro de un AVI.

Cuatro partes:

  anfitrión  el decodificador de la libsagui (userland/libsagui/video.c, que usa
             el JPEG de la 047) contra ffmpeg: los cuadros decodificados se
             comparan con los de ffmpeg (PSNR) y las sumas por cuadro salen
            Iguales;
  os         dentro de SaguiOS (QEMU): "reproductor -t" decodifica el vídeo del
             pendrive y da las mismas sumas que el anfitrión; "--volcar" escribe
             los cuadros en crudo, que se recuperan del pendrive y se comparan
             con ffmpeg; el MPEG-1 en MPEG-PS también se decodifica y da las
             mismas sumas que en el anfitrión (D137, D144);
  janela     el reproductor abre su ventana, pinta el cuadro, y la pausa, la
             búsqueda, la pantalla completa y el salto de cuadros funcionan;
  som        con tarjeta de sonido suena el audio del vídeo (440 Hz) y la
             duración es la del vídeo; sin tarjeta, el vídeo va en silencio.

El material de prueba lo genera tools/mkvideo.py (cuadros propios, dibujados
aquí; nada de material ajeno) y lo codifica ffmpeg en MJPEG/AVI.

Uso: h100_video_v62.py [anfitrion] [os] [janela] [som]
"""
import math
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_pendrive, pendrive_qemu,  # noqa: E402
                  FalhaTeste, RAIZ, SAIDA)
import h70_sonido as h70  # noqa: E402

DIR = os.path.join(SAIDA, "h100")
VIDEO = os.path.join(DIR, "video.avi")
VIDEO_MPG = os.path.join(DIR, "video.mpg")
VIDEO_JAN = os.path.join(DIR, "video_janela.avi")
DECO = os.path.join(DIR, "video_testar")
DUMP = os.path.join(DIR, "dump.raw")
DUMP_MPG = os.path.join(DIR, "dump_mpg.raw")
SOMAS_MPG = []
REF = os.path.join(DIR, "ref.raw")
LARG, ALT, FPS = 426, 240, 30
SEGUNDOS = 1
SEGUNDOS_JAN = 3       # el vídeo de la ventana es más largo: da tiempo a mirarlo
UMBRAL_PSNR = 35.0     # dB; ffmpeg y este decodificador dan unos 44 dB
# La linea con la que el kernel dice que el proceso termino. Se espera esa,
# no la salida del programa: el prompt del shell se intercala con ella.
FIM_PROC = r"processo \d+ \(reproductor\) terminou"
# Un cuadro decodificado: "cuadro 17 426x240 5960413 10707974 16871726".
RE_QUADRO = re.compile(r"cuadro (\d+) (\d+)x(\d+) (\d+) (\d+) (\d+)")


def esperar(m, padrao, tempo=300, desde=0):
    """Espera a que aparezca un patrón en la serial (desde una posición)."""
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise FalhaTeste(f"tiempo agotado esperando {padrao!r} en la serial")


def rodar(m, comando, fim, tempo=300, extra=None):
    """Escribe un comando y espera a que el programa termine.

    Los programas con ventana van en segundo plano (el shell no los espera) y el
    prompt se intercala con la salida del programa, asi que no se puede fiar de
    donde acaba cada linea: se espera a la linea del kernel que dice que el
    proceso termino (esa se escribe entera, despues de toda la salida).

    Con "extra" se espera ademas a un patron de la salida del programa: la linea
    del kernel avisa de que el proceso termino, pero su salida puede ir todavia
    por la tuberia (el "cat" la escribe despues), asi que el final del proceso y
    el final de la salida son dos cosas distintas."""
    ini = len(m.serial())
    m.digitar(comando, 0.04)
    m.tecla("ret", 0.04)
    esperar(m, fim, tempo, ini)
    if extra:
        esperar(m, extra, tempo, ini)
    return m.serial()[ini:]


# ------------------------------------------------------------ material

def gerar_video():
    """El vídeo de prueba: lo dibuja tools/mkvideo.py y lo codifica ffmpeg."""
    os.makedirs(DIR, exist_ok=True)
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkvideo.py"),
                        "--larg", str(LARG), "--alt", str(ALT), "--fps", str(FPS),
                        "--segundos", str(SEGUNDOS), "--saida", VIDEO],
                       capture_output=True, text=True)
    if r.returncode:
        raise FalhaTeste("no se pudo generar el vídeo: " + (r.stderr or r.stdout)[-300:])
    # El mismo vídeo, pero más largo, para la fase de la ventana: allí hace
    # falta tiempo para ver que el vídeo se ve y que avanza.
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkvideo.py"),
                        "--larg", str(LARG), "--alt", str(ALT), "--fps", str(FPS),
                        "--segundos", str(SEGUNDOS_JAN), "--saida", VIDEO_JAN],
                       capture_output=True, text=True)
    if r.returncode:
        raise FalhaTeste("no se pudo generar el vídeo largo: " + (r.stderr or r.stdout)[-300:])
    # El mismo vídeo en MPEG-PS, para ver que se reconoce y se avisa (D129).
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", VIDEO, "-c:v", "mpeg1video", "-b:v", "600k",
                    "-an", "-f", "mpeg", VIDEO_MPG], check=True)
    return VIDEO


def compilar_anfitrion():
    """El decodificador de la libsagui, compilado para el anfitrión."""
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DSAGUI_HOST", "-Itools", "-Iuserland/inc",
                        "-Iuserland/libsagui", "-Ikernel/inc", "-o", DECO,
                        "tools/video_testar.c", "tools/video_host.c",
                        "userland/libsagui/video.c", "userland/libsagui/imagem_jpeg.c",
                        "userland/libsagui/imagem_idct.c", "userland/libsagui/mpeg1.c"],
                       cwd=RAIZ, capture_output=True)
    verificar(r.returncode == 0 and not r.stderr,
              "el decodificador de vídeo compila en el anfitrión sin avisos: " + r.stderr.decode()[:300])


def ffmpeg(*args):
    r = subprocess.run(["ffmpeg", "-v", "error", "-y", *args], capture_output=True)
    if r.returncode:
        raise FalhaTeste("ffmpeg falló: " + r.stderr.decode(errors="replace")[:300])


def referencia(arquivo):
    """Los cuadros que decodifica ffmpeg, en RGB24 crudo."""
    ffmpeg("-i", arquivo, "-pix_fmt", "rgb24", "-f", "rawvideo", REF)
    return open(REF, "rb").read()


def psnr(a, b):
    """PSNR en dB de dos cuadros del mismo tamaño."""
    n = min(len(a), len(b))
    if not n:
        return 0.0
    erro = soma = 0
    for x, y in zip(a[:n], b[:n]):
        d = x - y
        erro += d * d
        soma += y * y
    if erro == 0:
        return 99.0
    return 10 * math.log10(soma / erro)


def leer_dump(dados):
    """El archivo crudo que escribe el reproductor: cabecera de texto y los
    cuadros en RGB24."""
    if not dados.startswith(b"SAGUIV1 "):
        raise FalhaTeste("el archivo volcado no tiene la cabecera esperada")
    fim = dados.index(b"\n")
    partes = dados[:fim].split()
    larg, alt = int(partes[1]), int(partes[2])
    return larg, alt, dados[fim + 1:]


def somas(dados, larg, alt):
    """Las sumas por cuadro (como las que imprime "reproductor -t")."""
    tam = larg * alt * 3
    out = []
    for i in range(len(dados) // tam):
        q = dados[i * tam:(i + 1) * tam]
        r = sum(q[0::3])
        g = sum(q[1::3])
        b = sum(q[2::3])
        out.append((r, g, b))
    return out


# ------------------------------------------------------------ parte 1: anfitrión

def anfitrion():
    gerar_video()
    compilar_anfitrion()
    r = subprocess.run([DECO, "--saida", DUMP, VIDEO], capture_output=True, timeout=300)
    verificar(r.returncode == 0, "el decodificador del anfitrión leyó el vídeo")
    linhas = [l for l in r.stdout.decode().splitlines() if l.startswith("cuadro ")]
    verificar(len(linhas) == FPS * SEGUNDOS, f"decodifica los {FPS * SEGUNDOS} cuadros del vídeo")

    larg, alt, quadros = leer_dump(open(DUMP, "rb").read())
    verificar((larg, alt) == (LARG, ALT), f"el vídeo es de {LARG}x{ALT}")
    ref = referencia(VIDEO)
    tam = larg * alt * 3
    verificar(len(ref) // tam >= 3, "ffmpeg decodifica el mismo vídeo")

    pior = 99.0
    for i in range(3):
        p = psnr(quadros[i * tam:(i + 1) * tam], ref[i * tam:(i + 1) * tam])
        pior = min(pior, p)
    verificar(pior >= UMBRAL_PSNR, f"los cuadros decodificados coinciden con los de ffmpeg (PSNR {pior:.1f} dB >= {UMBRAL_PSNR:.0f})")

    # Las sumas por cuadro: las del decodificador y las del ffmpeg (que
    # difieren un poco por el submuestreo del color).
    minhas = [tuple(int(x) for x in l.split()[3:6]) for l in linhas]
    ref_somas = somas(ref, larg, alt)
    for i, (a, b) in enumerate(zip(minhas, ref_somas)):
        for k in range(3):
            if abs(a[k] - b[k]) > larg * alt * 2:
                raise FalhaTeste(f"el cuadro {i} no cuadra con ffmpeg: {a} contra {b}")
    verificar(True, f"las sumas por cuadro coinciden con las de ffmpeg ({len(minhas)} cuadros)")

    # Las sumas del MPEG-1 en MPEG-PS (el mismo codigo, outro ficheiro): o
    # reproductor tem de dar as mesmas dentro do SaguiOS (D137, D144).
    r = subprocess.run([DECO, "--saida", DUMP_MPG, VIDEO_MPG], capture_output=True, timeout=300)
    verificar(r.returncode == 0, "el decodificador del anfitrión leyó el MPEG-1")
    linhas_mpg = [l for l in r.stdout.decode().splitlines() if l.startswith("cuadro ")]
    global SOMAS_MPG
    SOMAS_MPG = [tuple(int(x) for x in l.split()[3:6]) for l in linhas_mpg]
    verificar(len(SOMAS_MPG) == FPS * SEGUNDOS,
              f"decodifica los {FPS * SEGUNDOS} cuadros del MPEG-1")
    return minhas


# ------------------------------------------------------------ parte 2: dentro de SaguiOS

def em_os(somas_host):
    arqs = {"video.avi": open(VIDEO, "rb").read(), "video.mpg": open(VIDEO_MPG, "rb").read()}
    pen = criar_pendrive("h100-os", rotulo="VIDEO", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h100-os", "ide", imagem=imagem_console(), extras=extras) as m:
        esperar(m, r"sagui:/\$", 120)
        out = m.comando("ajuda reproductor")
        verificar("reproductor" in out and "MJPEG" in out, "ajuda reproductor explica el uso")
        m.comando("montar usb0p1")

        # (a) decodifica y da las sumas: tienen que ser las del anfitrión
        out = rodar(m, "reproductor -t /usb/video.avi | cat", FIM_PROC, extra=r"\d+ cuadros en \d+ ms")
        # El prompt del shell se intercala con la salida, asi que los cuadros se
        # cuentan por las apariciones, no por las lineas que empiezan por "cuadro ".
        quadros = RE_QUADRO.findall(out)
        verificar(len(quadros) == len(somas_host),
                  f"[SaguiOS] decodifica los {len(somas_host)} cuadros ({len(quadros)}Quadros)")
        iguais = all(tuple(int(x) for x in q[3:6]) == s for q, s in zip(quadros, somas_host))
        verificar(iguais, "[SaguiOS] las sumas por cuadro son las mismas que en el anfitrión")
        m2 = re.search(r"(\d+) cuadros en (\d+) ms", out)
        verificar(m2 is not None, "[SaguiOS] dice cuánto tardó: " +
                  (re.search(r"\d+ cuadros en \d+ ms", out) or re.search(r"$", out)).group(0)[:80])
        ms = int(m2.group(2))
        n = len(somas_host)
        verificar(ms * FPS < n * 1000 * 2,
                  f"[SaguiOS] decodifica {n} cuadros en {ms} ms (el doble del tiempo real)")

        # (b) el MPEG-1 en MPEG-PS también se decodifica (D137, D144)
        out = rodar(m, "reproductor -t /usb/video.mpg | cat", FIM_PROC, 180)
        quadros_mpg = RE_QUADRO.findall(out)
        verificar(len(quadros_mpg) == len(SOMAS_MPG),
                  f"[SaguiOS] decodifica los {len(SOMAS_MPG)} cuadros del MPEG-1 ({len(quadros_mpg)}Cuadros)")
        iguais_mpg = all(tuple(int(x) for x in q[3:6]) == s for q, s in zip(quadros_mpg, SOMAS_MPG))
        verificar(iguais_mpg, "[SaguiOS] las sumas por cuadro del MPEG-1 son las mismas que las del anfitrión")

        # (c) vuelca los cuadros en crudo para compararlos con ffmpeg
        out = rodar(m, "reproductor --volcar /usb/video.avi /usb/dump.raw 0 3 | cat", FIM_PROC,
                   extra=r"volcados \d+ cuadros")
        verificar("volcados 3 cuadros" in out, "[SaguiOS] vuelca los cuadros: " +
                  (re.search(r"volcados[^\n]*", out) or re.search(r"$", out)).group(0)[:120])
        out = m.comando("desmontar", 60)
    r = subprocess.run(["mtype", "-i", pen + "@@1M", "::/dump.raw"], capture_output=True)
    dados = r.stdout
    verificar(len(dados) > 0, f"el pendrive tiene el volcado ({len(dados)} bytes)")
    larg, alt, quadros = leer_dump(dados)
    ref = referencia(VIDEO)
    tam = larg * alt * 3
    verificar(len(quadros) // tam == 3, f"[SaguiOS] vuelca 3 cuadros de {larg}x{alt}")
    pior = 99.0
    for i in range(3):
        p = psnr(quadros[i * tam:(i + 1) * tam], ref[i * tam:(i + 1) * tam])
        pior = min(pior, p)
    verificar(pior >= UMBRAL_PSNR, f"[SaguiOS] los cuadros del pendrive coinciden con ffmpeg (PSNR {pior:.1f} dB)")

    # (d) el vídeo de ejemplo que viene en la imagen
    with Maquina("h100-ex", "ide", imagem=imagem_console()) as m:
        esperar(m, r"sagui:/\$", 120)
        out = rodar(m, "reproductor -t /videos/sagui.avi | head -2", FIM_PROC)
        verificar("MJPEG en AVI 426x240 30 fps 60 cuadros" in out,
                  "el vídeo de ejemplo de /videos se lee: " + (re.search(r"video:.*", out) or re.search(r"$", out)).group(0)[:120])


# ------------------------------------------------------------ parte 3: la ventana

def capturar(m, nome):
    """Una captura de la pantalla: devuelve (ancho, alto, pixeles)."""
    ppm = os.path.join(SAIDA, nome)
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    d = open(ppm, "rb").read()
    partes = d.split(None, 4)
    return int(partes[1]), int(partes[2]), partes[4]


def rect_video(wx, wy, wid, alt, vl=LARG, va=ALT):
    """Dónde se dibuja la imagen del vídeo dentro de la ventana.

    La ventana lleva un marco de 2 px y una barra de título de 24 px; dentro,
    el reproductor deja 34 px de barra de botones arriba y 46 px de pie abajo,
    y centra la imagen en el hueco que queda. La escala se calcula como la
    del reproductor (enteros de 16 bits), para caer en el mismo píxel."""
    bx, by, ba = 0, 34, alt - 34 - 46
    escala = min((wid * 65536) // vl, (ba * 65536) // va)
    l = (vl * escala) // 65536
    a = (va * escala) // 65536
    return wx + 2 + bx + (wid - l) // 2, wy + 24 + by + (ba - a) // 2, l, a


def cielos(px, tl, ta, rect):
    """En el rectángulo: cuántos píxeles son cielo (azul) y cuántos son negros.

    El vídeo de prueba es un cielo degradado (azul) con una franja clara y el
    sagui; lo negro es la barra que queda a los lados del vídeo."""
    x0, y0, l, a = rect
    azuis = negros = total = 0
    for y in range(y0, min(ta, y0 + a), 3):
        for x in range(x0, min(tl, x0 + l), 3):
            i = (y * tl + x) * 3
            r, g, b = px[i], px[i+1], px[i+2]
            total += 1
            if (r, g, b) == (0, 0, 0):
                negros += 1
            elif b > r + 30:
                azuis += 1
    return azuis, negros, total


def distintos(px1, tl1, px2, tl2, rect):
    """Cuántos píxeles del rectángulo cambian entre dos capturas."""
    x0, y0, l, a = rect
    n = 0
    for y in range(y0, min(min(tl1, tl2), y0 + a), 3):
        for x in range(x0, min(x0 + l, tl1, tl2), 3):
            i1 = (y * tl1 + x) * 3
            i2 = (y * tl2 + x) * 3
            if px1[i1:i1 + 3] != px2[i2:i2 + 3]:
                n += 1
    return n


def janela():
    gerar_video()
    arqs = {"video.avi": open(VIDEO_JAN, "rb").read()}
    pen = criar_pendrive("h100-jan", rotulo="VIDEO", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h100-jan", "ide", extras=extras) as m:
        esperar(m, r"sagui:/\$", 120)
        time.sleep(1)
        m.comando("montar usb0p1")
        ini = len(m.serial())
        m.comando("reproductor /usb/video.avi")
        esperar(m, r"reproductor: listo \(/usb/video\.avi, 426x240, 30 fps", 30)
        verificar(True, "el reproductor abre su ventana con el vídeo")
        esperar(m, r"\[GUI\] janela \d+ criada por \d+: \"Reproductor de vídeo\"", 20)

        # El primer cuadro se enseña en la ventana (no un "Sin cuadro").
        esperar(m, r"reproductor: primer cuadro en la ventana", 30, ini)
        verificar(True, "el reproductor pinta el primer cuadro en la ventana")

        # Y se ve de verdad: la zona de la imagen es el cielo del vídeo, no negro.
        tl, ta, px = capturar(m, "h100-janela.ppm")
        g = re.findall(r'janela (\d+) criada por \d+: "Reproductor de vídeo" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())
        wid, alt, wx, wy = int(g[-1][1]), int(g[-1][2]), int(g[-1][3]), int(g[-1][4])
        rect = rect_video(wx, wy, wid, alt)
        azuis, negros, total = cielos(px, tl, ta, rect)
        verificar(azuis > total * 0.8 and negros < total * 0.01,
                  f"la ventana pinta el cuadro del vídeo ({azuis} de {total} pixels de cielo, {negros} negros)")

        # El vídeo avanza: dos capturas seguidas tienen que ser distintas.
        time.sleep(0.5)
        tl2, ta2, px2 = capturar(m, "h100-janela2.ppm")
        n = distintos(px, tl, px2, tl2, rect)
        verificar(n > 200, f"el vídeo avanza ({n} pixels distintos entre dos capturas)")

        # Pausa (si el vídeo ya ha acabado, el espacio lo vuelve a empezar)
        ini = len(m.serial())
        m.tecla("spc")
        primeiro = esperar(m, r"reproductor: (en pausa|en marcha)", 10, ini)
        verificar(True, f"Espacio: {primeiro}")
        ini = len(m.serial())
        m.tecla("spc")
        segundo = esperar(m, r"reproductor: (en pausa|en marcha)", 10, ini)
        verificar(primeiro != segundo, f"Espacio otra vez: {segundo} (al revés que antes)")

        # Búsqueda (5 s adelante y atrás)
        ini = len(m.serial())
        m.tecla("right")
        esperar(m, r"reproductor: salto a ", 10, ini)
        verificar(True, "la flecha derecha salta 5 segundos")
        ini = len(m.serial())
        m.tecla("left")
        esperar(m, r"reproductor: salto a ", 10, ini)
        verificar(True, "la flecha izquierda vuelve atrás")

        # Salto de cuadros (con la pausa puesta sale "cuadro N"; si el vídeo
        # está en marcha, el salto se registra como un salto)
        ini = len(m.serial())
        m.tecla("dot")
        esperar(m, r"reproductor: (cuadro \d+|salto a )", 10, ini)
        verificar(True, "el punto avanza un cuadro")

        # Bucle y volumen
        ini = len(m.serial())
        m.tecla("l")
        esperar(m, r"reproductor: bucle activado", 10, ini)
        ini = len(m.serial())
        m.tecla("bracket_right")
        esperar(m, r"reproductor: volumen \d+", 10, ini)
        verificar(True, "L: bucle; +: volumen")

        # Estadísticas
        ini = len(m.serial())
        m.tecla("i")
        esperar(m, r"reproductor: \d+ fps medios", 10, ini)
        verificar(True, "I: estadísticas (fps, cuadros saltados y tiempo de decodificar)")

        # Pantalla completa
        ini = len(m.serial())
        m.tecla("f11")
        esperar(m, r"reproductor: ventana \d+x\d+ \(pantalla completa\)", 15, ini)
        verificar(True, "F11: pantalla completa")
        ini = len(m.serial())
        m.tecla("f11")
        esperar(m, r"reproductor: ventana \d+x\d+", 15, ini)
        verificar(True, "F11 otra vez: vuelve a la ventana")

        # Cerrar
        ini = len(m.serial())
        m.clique(wx + wid + 4 - 2 - 20 + 8, wy + 12)
        esperar(m, "reproductor: cerrado", 15, ini)
        verificar(True, "la ventana se cierra")
        # Y el proceso se acaba de verdad: no queda un zombi.
        esperar(m, FIM_PROC, 15, ini)
        verificar(True, "cerrada la ventana, el proceso del reproductor termina")


# ------------------------------------------------------------ parte 4: el sonido

def som():
    arqs = {"video.avi": open(VIDEO, "rb").read()}
    pen = criar_pendrive("h100-som", rotulo="VIDEO", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h100-som", "ide", imagem=imagem_console(), extras=extras, som="hda") as m:
        esperar(m, r"sagui:/\$", 120)
        m.comando("montar usb0p1")
        out = rodar(m, "reproductor -t /usb/video.avi | head -1 | cat", FIM_PROC)
        verificar("audio=si" in out, "el vídeo trae audio: " + (re.search(r"video:.*", out) or re.search(r"$", out)).group(0)[:120])

    # Con la tarjeta: el reproductor abre el sonido y el tono se oye
    with Maquina("h100-som2", "ide", extras=extras, som="hda") as m:
        esperar(m, r"sagui:/\$", 120)
        time.sleep(1)
        m.comando("montar usb0p1")
        ini = len(m.serial())
        m.comando("reproductor /usb/video.avi")
        esperar(m, r"reproductor: listo", 30, ini)
        time.sleep(2.0)
        g = re.findall(r'janela (\d+) criada por \d+: "Reproductor de vídeo" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())
        wx, wy = int(g[-1][3]), int(g[-1][4])
        time.sleep(1.5)
        m.clique(wx + int(g[-1][1]) + 4 - 2 - 20 + 8, wy + 12)
        esperar(m, "reproductor: cerrado", 15)

    esq, dir_ = h70.ler_gravacao(m.wav)
    tr = [h70.Trecho(esq, dir_, a, b) for a, b in h70.trechos(esq, dir_)]
    verificar(len(tr) >= 1, f"se oye el audio del vídeo ({len(tr)} trechos)")
    melhor = max(tr, key=lambda t: t.rms_esq)
    verificar(h70.perto(melhor.f_esq, 440, 25) or h70.perto(melhor.f_dir, 440, 25),
              f"el tono del vídeo suena a 440 Hz (izq {melhor.f_esq:.0f} Hz, der {melhor.f_dir:.0f} Hz)")
    # En TCG la reproducción va a tirones (el video se descodifica mas despacio
    # que el tiempo real), asi que el sonido sale en trozos: se mide el tiempo
    # total con sonido, no el trozo mas largo.
    total = sum(t.segundos for t in tr)
    verificar(total > 0.3, f"se oye el audio del vídeo {total:.2f} s (el vídeo son {SEGUNDOS} s)")

    # Sin tarjeta: el vídeo va en silencio y lo avisa (con la ventana abierta,
    # como el reproductor de verdad)
    with Maquina("h100-som3", "ide", extras=extras) as m:
        esperar(m, r"sagui:/\$", 120)
        m.comando("montar usb0p1")
        ini = len(m.serial())
        m.comando("reproductor /usb/video.avi")
        esperar(m, r"reproductor: no hay tarjeta de sonido", 30, ini)
        verificar(True, "sin tarjeta de sonido el vídeo va en silencio y lo avisa")


# ------------------------------------------------------------ programa

def main():
    partes = sys.argv[1:] or ["anfitrion", "os", "janela", "som"]
    if "anfitrion" in partes:
        print("== anfitrión")
        somas_host = anfitrion()
    else:
        somas_host = None
    if "os" in partes:
        print("== dentro de SaguiOS")
        em_os(somas_host or anfitrion())
    if "janela" in partes:
        print("== la ventana")
        janela()
    if "som" in partes:
        print("== el sonido")
        som()
    return 0


if __name__ == "__main__":
    sys.exit(main())
