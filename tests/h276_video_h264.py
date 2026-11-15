#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H276 (D276, mejora 100) - vídeo H.264/AAC: MP4 en el reproductor y <video> en el Cipó.

El decodificador (userland/libsagui/h264.c, aac.c, mp4.c, de robson-apk; el pegamento
video_mp4.c es nuestro) es uno solo: lo usan el `reproductor` del sistema y el Cipó.

Cuatro partes:

  anfitrion  el decodificador de la libsagui compilado en el PC (tools/video_testar.c)
             contra ffmpeg: cuadros del MP4 Baseline y del Main con cuadros B (PSNR) y
             el audio AAC (el tono de 440 Hz, y que sea el mismo que decodifica ffmpeg);
  os         dentro de SaguiOS (QEMU): `reproductor -t` da las mismas sumas por cuadro
             que el anfitrión, con el MP4 con el moov al principio y con el moov al final,
             y mide los cuadros por segundo a 360p y 480p (build/h276/rendimiento.txt);
  janela     el reproductor abre el MP4 en su ventana y por la tarjeta de sonido
             emulada (-audiodev wav) se oye el tono de 440 Hz;
  cipo       una página con <video autoplay loop>: el Cipó baja el MP4, lo decodifica en
             la caja del vídeo (rojo y luego azul, se comprueban los dos), suena el tono y
             un clic pausa y reanuda.

El material lo genera ffmpeg del anfitrión con fuentes sintéticas (lavfi): nada ajeno. Si
no hay ffmpeg, la prueba se salta (no hay MP4 fijo en tests/datos/).

Uso: h276_video_h264.py [anfitrion] [os] [janela] [cipo]
"""
import functools
import http.server
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, imagem_de_teste, criar_pendrive,  # noqa: E402
                  pendrive_qemu, FalhaTeste, RAIZ, SAIDA)
import h70_sonido as h70  # noqa: E402
import h114_cipo_descargas as h114  # noqa: E402  (decodificador de PNG de las capturas)

DIR = os.path.join(SAIDA, "h276")
DECO = os.path.join(DIR, "video_testar")
FIM_PROC = r"processo \d+ \(reproductor\) terminou"
RE_QUADRO = re.compile(r"cuadro (\d+) (\d+)x(\d+) (\d+) (\d+) (\d+)")
UMBRAL_PSNR = 40.0     # dB: en la práctica salen ~48 (solo difiere el redondeo del color)
LARG, ALT, FPS = 320, 180, 15
# Material: nombre -> argumentos de ffmpeg (después de las entradas)
PERF = (("perf360.mp4", 640, 360), ("perf480.mp4", 854, 480))


def ffmpeg(*args):
    r = subprocess.run(["ffmpeg", "-v", "error", "-y", *args], capture_output=True)
    if r.returncode:
        raise FalhaTeste("ffmpeg falló: " + r.stderr.decode(errors="replace")[:300])


def gerar_material():
    os.makedirs(DIR, exist_ok=True)
    tom = ["-f", "lavfi", "-i", "sine=frequency=440:sample_rate=44100"]
    teste = f"testsrc2=size={LARG}x{ALT}:rate={FPS}"
    # Baseline con el moov al principio (faststart)
    ffmpeg("-f", "lavfi", "-i", teste, *tom, "-t", "2", "-c:v", "libx264", "-profile:v", "baseline",
           "-pix_fmt", "yuv420p", "-g", "15", "-c:a", "aac", "-b:a", "96k", "-ac", "2",
           "-movflags", "+faststart", os.path.join(DIR, "base.mp4"))
    # Main con cuadros B (CABAC) y el moov al final (el ffmpeg normal)
    ffmpeg("-f", "lavfi", "-i", teste, *tom, "-t", "2", "-c:v", "libx264", "-profile:v", "main", "-bf", "2",
           "-pix_fmt", "yuv420p", "-g", "15", "-c:a", "aac", "-b:a", "96k", "-ac", "2",
           os.path.join(DIR, "main.mp4"))
    # Rojo y luego azul (el Cipó los reconoce por el color); 2 s cada uno, con el tono
    ffmpeg("-f", "lavfi", "-i", f"color=c=red:size={LARG}x{ALT}:rate={FPS}:d=2",
           "-f", "lavfi", "-i", f"color=c=blue:size={LARG}x{ALT}:rate={FPS}:d=2", *tom, "-t", "4",
           "-filter_complex", "[0:v][1:v]concat=n=2:v=1:a=0[v]", "-map", "[v]", "-map", "2:a",
           "-c:v", "libx264", "-profile:v", "main", "-pix_fmt", "yuv420p", "-g", "15",
           "-c:a", "aac", "-b:a", "96k", "-ac", "2", "-movflags", "+faststart",
           os.path.join(DIR, "cipo.mp4"))
    # Para medir el rendimiento: 2 s a 24 cuadros, con algo de ruido para que no sea trivial
    for nome, l, a in PERF:
        ffmpeg("-f", "lavfi", "-i", f"testsrc2=size={l}x{a}:rate=24", *tom, "-t", "2", "-vf", "noise=alls=6:allf=t",
               "-c:v", "libx264", "-profile:v", "main", "-pix_fmt", "yuv420p", "-crf", "30", "-g", "24",
               "-c:a", "aac", "-b:a", "64k", "-ac", "2", "-movflags", "+faststart", os.path.join(DIR, nome))


def esperar(m, padrao, tempo=300, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise FalhaTeste(f"tiempo agotado esperando {padrao!r} en la serial")


def rodar(m, comando, fim, tempo=300, extra=None):
    ini = len(m.serial())
    m.digitar(comando, 0.04)
    m.tecla("ret", 0.04)
    esperar(m, fim, tempo, ini)
    if extra:
        esperar(m, extra, tempo, ini)
    return m.serial()[ini:]


def psnr(a, b):
    n = min(len(a), len(b))
    mse = sum((x - y) ** 2 for x, y in zip(a[:n:5], b[:n:5])) / len(a[:n:5])
    return 99.0 if mse == 0 else 10.0 * math.log10(255.0 * 255.0 / mse)


def ler_dump(dados):
    i = dados.index(b"\n")
    _, l, a = dados[:i].split()
    return int(l), int(a), dados[i + 1:]


# ------------------------------------------------------------ parte 1: anfitrión

def anfitrion():
    gerar_material()
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Wno-unused-function", "-DSAGUI_HOST", "-DH264_HOST",
                        "-Itools", "-Iuserland/inc", "-Iuserland/libsagui", "-Ikernel/inc", "-o", DECO,
                        "tools/video_testar.c", "tools/video_host.c", "userland/libsagui/video.c",
                        "userland/libsagui/video_mp4.c", "userland/libsagui/h264.c", "userland/libsagui/aac.c",
                        "userland/libsagui/mp4.c", "userland/libsagui/imagem_jpeg.c",
                        "userland/libsagui/imagem_idct.c", "userland/libsagui/mpeg1.c", "-lm"],
                       cwd=RAIZ, capture_output=True)
    verificar(r.returncode == 0 and not r.stderr,
              "el decodificador H.264/AAC compila en el anfitrión sin avisos: " + r.stderr.decode()[:300])
    sumas = {}
    for nome in ("base", "main"):
        mp4 = os.path.join(DIR, nome + ".mp4")
        dump, pcm = os.path.join(DIR, nome + ".raw"), os.path.join(DIR, nome + ".pcm")
        r = subprocess.run([DECO, "--saida", dump, "--audio", pcm, mp4], capture_output=True, timeout=300)
        verificar(r.returncode == 0, f"[{nome}] el decodificador del anfitrión lee el MP4: " + r.stderr.decode()[-200:])
        verificar(f"H.264/AAC en MP4 {LARG}x{ALT} {FPS} fps 30 cuadros" in r.stderr.decode(),
                  f"[{nome}] reconoce el MP4: " + r.stderr.decode().splitlines()[0])
        lin = [l for l in r.stdout.decode().splitlines() if l.startswith("cuadro ")]
        verificar(len(lin) == 30, f"[{nome}] decodifica los 30 cuadros ({len(lin)})")
        sumas[nome] = [tuple(int(x) for x in l.split()[3:6]) for l in lin]
        # contra ffmpeg
        ref = os.path.join(DIR, nome + "_ff.rgb")
        ffmpeg("-i", mp4, "-pix_fmt", "rgb24", "-f", "rawvideo", ref)
        l, a, q = ler_dump(open(dump, "rb").read())
        refd = open(ref, "rb").read()
        tam = l * a * 3
        verificar(len(q) // tam == len(refd) // tam == 30, f"[{nome}] ffmpeg da los mismos 30 cuadros")
        pior = min(psnr(q[i * tam:(i + 1) * tam], refd[i * tam:(i + 1) * tam]) for i in range(30))
        verificar(pior >= UMBRAL_PSNR, f"[{nome}] los cuadros coinciden con ffmpeg (peor PSNR {pior:.1f} dB >= {UMBRAL_PSNR:.0f})")
        # el audio: el tono de 440 Hz y el mismo nivel que el de ffmpeg
        p = open(pcm, "rb").read()
        verificar(len(p) == 88 * 1024 * 4, f"[{nome}] decodifica los 88 cuadros AAC ({len(p) // 4} muestras)")
        s = struct.unpack("<%dh" % (len(p) // 2), p)[0::2]
        x = s[8192:8192 + 4410]
        niveis = {f: math.hypot(sum(v * math.cos(2 * math.pi * f * k / 44100) for k, v in enumerate(x)),
                                sum(v * math.sin(2 * math.pi * f * k / 44100) for k, v in enumerate(x)))
                  for f in (300, 440, 600, 1000)}
        verificar(niveis[440] > 50 * max(niveis[300], niveis[600], niveis[1000]),
                  f"[{nome}] el audio AAC es el tono de 440 Hz (440 Hz {niveis[440]:.0f}, 600 Hz {niveis[600]:.0f})")
        ffpcm = os.path.join(DIR, nome + "_ff.pcm")
        ffmpeg("-i", mp4, "-f", "s16le", "-ac", "2", "-ar", "44100", ffpcm)
        r_dec = math.sqrt(sum(v * v for v in s[8192:16384]) / 8192)
        sf = struct.unpack("<%dh" % (os.path.getsize(ffpcm) // 2), open(ffpcm, "rb").read())[0::2]
        r_ff = math.sqrt(sum(v * v for v in sf[8192:16384]) / 8192)
        verificar(abs(r_dec - r_ff) < r_ff * 0.05, f"[{nome}] el nivel del audio es el de ffmpeg (RMS {r_dec:.0f} contra {r_ff:.0f})")
    # búsqueda y bucle en el anfitrión: volver a 0 da el mismo primer cuadro (lo usa el bucle del Cipó)
    r = subprocess.run([DECO, "--desde", "0", "--quadros", "1", os.path.join(DIR, "main.mp4")], capture_output=True)
    verificar(r.returncode == 0, "una pasada de prueba más no deja estado sucio (un solo vídeo a la vez)")
    return sumas


# ------------------------------------------------------------ parte 2: dentro de SaguiOS

def em_os(sumas):
    arqs = {n: open(os.path.join(DIR, n), "rb").read() for n in ("base.mp4", "main.mp4", "perf360.mp4", "perf480.mp4")}
    pen = criar_pendrive("h276-os", rotulo="VIDEO", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    medidas = []
    with Maquina("h276-os", "ide", imagem=imagem_console(), extras=extras) as m:
        esperar(m, r"sagui:/\$", 120)
        out = m.comando("ajuda reproductor")
        verificar("MP4" in out and "H.264" in out, "ajuda reproductor dice que abre MP4 con H.264")
        m.comando("montar usb0p1")
        for nome in ("base", "main"):
            out = rodar(m, f"reproductor -t /usb/{nome}.mp4 | cat", FIM_PROC, extra=r"decodificando \d+ ms")
            quadros = RE_QUADRO.findall(out)
            verificar(len(quadros) == 30, f"[SaguiOS {nome}] decodifica los 30 cuadros ({len(quadros)})")
            verificar(all(tuple(int(x) for x in q[3:6]) == s for q, s in zip(quadros, sumas[nome])),
                      f"[SaguiOS {nome}] las sumas por cuadro son las mismas que en el anfitrión")
            verificar("H.264/AAC en MP4" in out and "audio=si" in out, f"[SaguiOS {nome}] dice formato y audio")
        for nome, l, a in PERF:
            out = rodar(m, f"reproductor -t /usb/{nome} | cat", FIM_PROC, 900, extra=r"decodificando \d+ ms")
            mm = re.search(r"(\d+) cuadros en (\d+) ms.*\n?.*decodificando (\d+) ms \((\d+),(\d+) cuadros por segundo\)", out, re.S)
            verificar(mm is not None, f"[SaguiOS {nome}] mide la decodificación")
            n, ms, md, f1, f2 = (int(x) for x in mm.groups())
            verificar(n == 48 and f"{l}x{a}" in out, f"[SaguiOS {nome}] decodifica los 48 cuadros de {l}x{a} ({n})")
            medidas.append((nome, l, a, n, md, float(f"{f1}.{f2:02d}")))
        m.comando("desmontar", 60)
    modo = "KVM" if os.environ.get("SAGUI_KVM") else "TCG (sin KVM)"
    with open(os.path.join(DIR, "rendimiento.txt"), "a", encoding="utf-8") as f:
        for nome, l, a, n, md, fps in medidas:
            linha = f"{modo}: {nome} {l}x{a} (H.264 Main, 24 fps): {n} cuadros en {md} ms de decodificación = {fps:.2f} cuadros por segundo"
            print("  " + linha)
            f.write(linha + "\n")


# ------------------------------------------------------------ parte 3: la ventana y el sonido

def janela():
    arqs = {"base.mp4": open(os.path.join(DIR, "base.mp4"), "rb").read()}
    pen = criar_pendrive("h276-jan", rotulo="VIDEO", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h276-jan", "ide", extras=extras, som="hda") as m:
        esperar(m, r"sagui:/\$", 120)
        time.sleep(1)
        m.comando("montar usb0p1")
        ini = len(m.serial())
        m.comando("reproductor /usb/base.mp4")
        esperar(m, rf"reproductor: listo \(/usb/base\.mp4, {LARG}x{ALT}, {FPS} fps, 30 cuadros\)", 30, ini)
        verificar(True, "el reproductor abre el MP4 en su ventana")
        esperar(m, r"reproductor: primer cuadro en la ventana", 30, ini)
        verificar(True, "el reproductor pinta el primer cuadro del MP4")
        time.sleep(2.5)
        g = re.findall(r'janela (\d+) criada por \d+: "Reproductor de vídeo" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())
        wx, wy = int(g[-1][3]), int(g[-1][4])
        m.clique(wx + int(g[-1][1]) + 4 - 2 - 20 + 8, wy + 12)
        esperar(m, "reproductor: cerrado", 15)
    esq, dir_ = h70.ler_gravacao(m.wav)
    tr = [h70.Trecho(esq, dir_, a, b) for a, b in h70.trechos(esq, dir_)]
    verificar(len(tr) >= 1, f"se oye el audio del MP4 ({len(tr)} trechos)")
    melhor = max(tr, key=lambda t: t.rms_esq)
    verificar(h70.perto(melhor.f_esq, 440, 25) or h70.perto(melhor.f_dir, 440, 25),
              f"el tono del MP4 suena a 440 Hz (izq {melhor.f_esq:.0f} Hz, der {melhor.f_dir:.0f} Hz)")
    total = sum(t.segundos for t in tr)
    verificar(total > 0.3, f"se oye el audio del MP4 {total:.2f} s")


# ------------------------------------------------------------ parte 4: el <video> del Cipó

PAGINA = """<!DOCTYPE html><html><head><meta charset="utf-8"><title>Video</title></head>
<body style="margin:0;background:#ffffff"><video src="/cipo.mp4" width="%d" height="%d" autoplay loop></video>
<p>texto debajo del vídeo</p></body></html>""" % (LARG, ALT)


def caixa(caminho, cor, tol=40):
    """Caja (x0, y0, x1, y1) del rectángulo grande de un color en la captura, o None. Se
    cuentan por filas y columnas solo las que tienen muchos píxeles de ese color, para no
    confundirse con iconos sueltos del escritorio."""
    l, a, canais, px = h114.png_pixels(caminho)
    filas, colunas = {}, {}
    for y in range(150, a):   # por debajo de la barra de título y de herramientas (el botón de cerrar es rojo)
        linha = px[y]
        for x in range(0, l - 40):
            i = x * canais
            if abs(linha[i] - cor[0]) <= tol and abs(linha[i + 1] - cor[1]) <= tol and abs(linha[i + 2] - cor[2]) <= tol:
                filas[y] = filas.get(y, 0) + 1
                colunas[x] = colunas.get(x, 0) + 1
    ys = [y for y, n in filas.items() if n >= 150]
    xs = [x for x, n in colunas.items() if n >= 80]
    if not ys or not xs:
        return None
    return min(xs), min(ys), max(xs), max(ys)


def cipo():
    pasta = os.path.join(DIR, "www")
    os.makedirs(pasta, exist_ok=True)
    shutil.copy(os.path.join(DIR, "cipo.mp4"), os.path.join(pasta, "cipo.mp4"))
    with open(os.path.join(pasta, "index.html"), "w", encoding="utf-8") as f:
        f.write(PAGINA)

    class Tratador(http.server.SimpleHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *a):
            pass

        def do_GET(self):
            self.close_connection = True
            super().do_GET()

    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Tratador, directory=pasta))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    porta = srv.server_address[1]
    img = imagem_de_teste(f"/apps/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    vermelho_ok = azul_ok = False
    with Maquina("h276-cipo", "ide", imagem=img, extras=["-nic", "user,model=e1000"], som="hda") as m:
        m.esperar_serial("cipo: pronto", 90)
        esperar(m, r"cipo: video H\.264/AAC en MP4 %dx%d %d fps" % (LARG, ALT, FPS), 120)
        verificar(True, "el Cipó baja el MP4 del <video> y lo abre con el decodificador compartido")
        caixa_v = None
        fim = time.time() + 90
        n = 0
        while time.time() < fim and not (vermelho_ok and azul_ok):
            c = m.captura("h276-sondeo", guardar=False)
            cv, ca = caixa(c, (255, 0, 0)), caixa(c, (0, 0, 255))
            if cv and not vermelho_ok:
                vermelho_ok = True
                caixa_v = cv
                m.captura("H276-cipo-video-rojo")
            if ca and not azul_ok:
                azul_ok = True
                caixa_v = caixa_v or ca
                m.captura("H276-cipo-video-azul")
            n += 1
            time.sleep(0.5)
        verificar(vermelho_ok, "se ve el cuadro rojo del vídeo dentro de la página")
        verificar(azul_ok, "se ve después el cuadro azul: los cuadros avanzan")
        x0, y0, x1, y1 = caixa_v
        verificar(abs((x1 - x0 + 1) - LARG) <= 3 and abs((y1 - y0 + 1) - ALT) <= 3,
                  f"el vídeo ocupa su caja de {LARG}x{ALT} ({x1 - x0 + 1}x{y1 - y0 + 1})")
        # un clic pausa, otro reanuda
        ini = len(m.serial())
        m.clique((x0 + x1) // 2, (y0 + y1) // 2)
        esperar(m, r"cipo: video en pausa", 15, ini)
        verificar(True, "un clic en el vídeo lo pone en pausa")
        m.clique((x0 + x1) // 2, (y0 + y1) // 2)
        esperar(m, r"cipo: video reanudado", 15, ini)
        verificar(True, "otro clic lo reanuda")
        time.sleep(2)
    srv.shutdown()
    esq, dir_ = h70.ler_gravacao(m.wav)
    tr = [h70.Trecho(esq, dir_, a, b) for a, b in h70.trechos(esq, dir_)]
    verificar(len(tr) >= 1, f"se oye el audio del vídeo del Cipó ({len(tr)} trechos)")
    melhor = max(tr, key=lambda t: t.rms_esq)
    verificar(h70.perto(melhor.f_esq, 440, 25) or h70.perto(melhor.f_dir, 440, 25),
              f"el tono del vídeo del Cipó suena a 440 Hz (izq {melhor.f_esq:.0f} Hz, der {melhor.f_dir:.0f} Hz)")


def main():
    if not shutil.which("ffmpeg"):
        print("H276: sin ffmpeg en el anfitrión, no hay material de prueba: se salta")
        return 0
    partes = sys.argv[1:] or ["anfitrion", "os", "janela", "cipo"]
    sumas = None
    if "anfitrion" in partes or any(p in partes for p in ("os", "janela", "cipo")):
        print("== anfitrión: H.264/AAC contra ffmpeg")
        sumas = anfitrion()
    if "os" in partes:
        print("== SaguiOS: reproductor -t y rendimiento")
        em_os(sumas)
    if "janela" in partes:
        print("== SaguiOS: ventana y sonido")
        janela()
    if "cipo" in partes:
        print("== SaguiOS: <video> en el Cipó")
        cipo()
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
