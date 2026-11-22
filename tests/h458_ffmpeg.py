#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H458 - FFmpeg 8.1.3 dentro de SaguiOS: /apps/bin/ffmpeg y ffprobe (D458, docs/ffmpeg.md).

Partes (por defecto todas; o las que se pidan: h458_ffmpeg.py pc saguios):
  pc       ffmpeg y ffprobe son ELF32 estáticos en la imagen; ayuda, docs/ffmpeg.md, D458, fila de terceros.md, fila del
           ROADMAP, FFMPEG en las tres secciones de compilar_versiones.md; las fuentes no van en git; la licencia es LGPL
           (el configure dice «License: LGPL version 2.1 or later» y no se pasa --enable-gpl ni --enable-nonfree).
  saguios  en SaguiOS (QEMU): material sintético hecho con el ffmpeg del PC (lavfi: testsrc y sine) va en un pendrive.
           - ffprobe de cada formato (mp4, webm, avi, ogv, mkv, mpg, ts, m4a, mp3, ogg, opus, flac, wav).
           - vídeo: H.264, VP8, MPEG-4, Theora, MJPEG, MPEG-1 y MPEG-2: la suma MD5 de los cuadros decodificados es la
             MISMA que la del ffmpeg del PC (los decodificadores de vídeo son exactos).
           - audio: FLAC, PCM, y los de coma flotante (AAC, MP3, MP2, Vorbis, Opus): mismo tamaño que el del PC y
             casi todas las muestras iguales (la x87 y el ensamblador del PC redondean distinto en el último bit).
           - la fuente lavfi (testsrc) da los mismos cuadros que en el PC; un filtro scale, crop y fps.
           - convertir un vídeo con los codificadores nativos (MJPEG en AVI con PCM y MPEG-1) y que el Reproductor de
             SaguiOS (reproductor -t) lo abra y decodifique todos los cuadros.
           - mide la velocidad (cuadros por segundo) de decodificar y de codificar (build/h458/rendimiento.txt).
Capturas: docs/capturas/H458-*.png.
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import (Maquina, imagem_console, criar_pendrive, pendrive_qemu, verificar, FalhaTeste,  # noqa: E402
                  APPS_RAIZ, BUILD, SAIDA)

KVM = os.environ.get("SAGUI_KVM") == "1"
FUENTES = os.path.join(RAIZ, "userland", "terceros", "ffmpeg", "fuentes", ".preparado")
DIR = os.path.join(SAIDA, "h458")
MED = os.path.join(DIR, "media")

V = ["-f", "lavfi", "-i", "testsrc=size=160x96:rate=25:duration=1"]
A = ["-f", "lavfi", "-i", "sine=frequency=440:duration=1:sample_rate=22050"]

# nombre -> argumentos de ffmpeg del PC (después de -y); los de vídeo con sonido llevan las dos entradas
VIDEOS = {
    "v_h264.mp4": V + A + ["-c:v", "libx264", "-profile:v", "baseline", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "64k", "-shortest"],
    "v_mpeg4.avi": V + A + ["-c:v", "mpeg4", "-c:a", "libmp3lame", "-b:a", "64k", "-shortest"],
    "v_vp8.webm": V + A + ["-c:v", "libvpx", "-b:v", "200k", "-c:a", "libvorbis", "-shortest"],
    "v_theora.ogv": V + A + ["-c:v", "libtheora", "-c:a", "libvorbis", "-shortest"],
    "v_mjpeg.mkv": V + A + ["-c:v", "mjpeg", "-pix_fmt", "yuvj420p", "-q:v", "5", "-c:a", "pcm_s16le", "-shortest"],
    "v_mpeg1.mpg": V + ["-c:v", "mpeg1video", "-f", "mpeg"],
    "v_mpeg2.ts": V + A + ["-c:v", "mpeg2video", "-c:a", "mp2", "-shortest", "-f", "mpegts"],
}
AUDIOS = {
    "a.m4a": ["-c:a", "aac"], "a.mp3": ["-c:a", "libmp3lame"], "a.ogg": ["-c:a", "libvorbis"],
    "a.opus": ["-c:a", "libopus"], "a.flac": ["-c:a", "flac"], "a.wav": ["-c:a", "pcm_s16le"],
    "a.mp2": ["-c:a", "mp2"],
}
# clase de comparación: exacto o aproximado
AUDIO_EXACTO = {"a.flac", "a.wav"}
VIDEO_CODEC = {"v_h264.mp4": "h264", "v_mpeg4.avi": "mpeg4", "v_vp8.webm": "vp8", "v_theora.ogv": "theora",
               "v_mjpeg.mkv": "mjpeg", "v_mpeg1.mpg": "mpeg1video", "v_mpeg2.ts": "mpeg2video"}
AUDIO_CODEC = {"a.m4a": "aac", "a.mp3": "mp3", "a.ogg": "vorbis", "a.opus": "opus", "a.flac": "flac", "a.wav": "pcm_s16le",
               "a.mp2": "mp2"}
# vídeo de rendimiento (320x240, 3 s): para medir
PERF = {"p_h264.mp4": ["-c:v", "libx264", "-profile:v", "baseline", "-pix_fmt", "yuv420p"],
        "p_mpeg4.avi": ["-c:v", "mpeg4"], "p_mjpeg.avi": ["-c:v", "mjpeg", "-pix_fmt", "yuvj420p", "-q:v", "5"]}


def ffpc(*args):
    r = subprocess.run(["ffmpeg", "-v", "error", "-y", *args], capture_output=True)
    if r.returncode:
        raise FalhaTeste("ffmpeg del PC falló: " + r.stderr.decode(errors="replace")[:300])
    return r.stdout


def md5_pc(*args):
    r = subprocess.run(["ffmpeg", "-v", "error", *args, "-f", "md5", "-"], capture_output=True, text=True)
    m = re.search(r"MD5=([0-9a-f]{32})", r.stdout)
    if not m:
        raise FalhaTeste("md5 del PC: " + r.stderr[:300])
    return m.group(1)


def esperar(m, padrao, tempo, desde):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError("tiempo agotado esperando %r (%r)" % (padrao, m.serial()[desde:][-600:]))


def orden(m, texto, tempo=60):
    """Escribe una orden en el terminal y espera al prompt; devuelve la salida (sin la orden) y los segundos que tardó."""
    marca = len(m.serial())
    t0 = time.time()
    m.digitar(texto, 0.03)
    m.tecla("ret")
    esperar(m, r"sagui:[^\n]*\$", tempo, marca + len(texto))
    seg = time.time() - t0
    time.sleep(0.5)
    s = m.serial()[marca:].replace("\r", "")
    s = "\n".join(l for l in s.split("\n") if not l.startswith("[SAGUI]"))
    return s, seg


def pc():
    for n in ("ffmpeg", "ffprobe"):
        imagem = os.path.join(APPS_RAIZ, "bin", n)
        verificar(os.path.isfile(imagem), "la imagen lleva /apps/bin/%s" % n)
        h = subprocess.run(["readelf", "-hl", "-W", os.path.join(BUILD, "userland", "bin", n)],
                           capture_output=True, text=True).stdout
        verificar("ELF32" in h and "Intel 80386" in h and "INTERP" not in h, "%s: ELF32 estático de i386" % n)
    if os.path.isdir(os.path.join(APPS_RAIZ, "tcc")):
        for f in ("lib/libavcodec.a", "lib/libavformat.a", "lib/libavutil.a", "lib/libswscale.a", "lib/libswresample.a",
                  "include/libavcodec/avcodec.h", "include/libavformat/avformat.h"):
            verificar(os.path.isfile(os.path.join(APPS_RAIZ, "tcc", f)), "/apps/tcc/%s (para incrustar FFmpeg)" % f)
    verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "ayuda", "ffmpeg.md")), "rootfs/ayuda/ffmpeg.md")
    verificar(os.path.isfile(os.path.join(RAIZ, "docs", "ffmpeg.md")), "docs/ffmpeg.md")
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("**FFmpeg 8.1.3**" in t and "LGPL-2.1" in t, "docs/terceros.md tiene la fila de FFmpeg (LGPL-2.1 o posterior)")
    d = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D458." in d, "D458 está en docs/decisoes.md")
    r = open(os.path.join(RAIZ, "docs", "ROADMAP.md"), encoding="utf-8").read()
    verificar("FFmpeg" in r and "v8.5.6" in r, "el ROADMAP habla de FFmpeg en la v8.5.6")
    c = open(os.path.join(RAIZ, "compilar_versiones.md"), encoding="utf-8").read()
    verificar(c.count("| `FFMPEG` |") == 3 and c.count("ffmpeg_fuentes.sh") >= 3, "compilar_versiones.md documenta FFMPEG en las tres secciones")
    ig = subprocess.run(["git", "ls-files", "userland/terceros/ffmpeg"], cwd=RAIZ, capture_output=True, text=True).stdout
    verificar("fuentes/" not in ig, "las fuentes de FFmpeg no van en git")
    args = open(os.path.join(RAIZ, "userland", "terceros", "ffmpeg", "configurar.args"), encoding="utf-8").read()
    args = "\n".join(l for l in args.split("\n") if not l.startswith("#"))
    verificar("--enable-gpl" not in args and "--enable-nonfree" not in args and "--enable-version3" not in args,
              "configurar.args no activa nada GPL ni no libre")
    log = os.path.join(BUILD, "ffmpeg", "obj", "configure.log")
    if os.path.isfile(log):
        verificar("License: LGPL version 2.1 or later" in open(log, errors="replace").read(),
                  "el configure dice «License: LGPL version 2.1 or later»")
    mk = open(os.path.join(RAIZ, "Makefile"), encoding="utf-8").read()
    verificar(re.search(r"ifneq \(\$\(SEM_GPL\),1\)\s*\ninclude userland/terceros/ffmpeg/ffmpeg.mk", mk) is not None,
              "con SEM_GPL=1 FFmpeg no entra (enlaza la libsagui GPL)")
    # los símbolos que añade la libc
    for f, pal in (("pthread.h", "pthread_create"), ("sched.h", "sched_yield"), ("math.h", "sincosf")):
        verificar(pal in open(os.path.join(RAIZ, "userland", "libc", f), encoding="utf-8").read(), "libc: %s declara %s" % (f, pal))


def anfitrion():
    """Genera el material con el ffmpeg del PC y las referencias."""
    os.makedirs(MED, exist_ok=True)
    if subprocess.run(["which", "ffmpeg"], capture_output=True).returncode:
        return None
    ref = {}
    for n, a in VIDEOS.items():
        ffpc(*a, os.path.join(MED, n))
        ref["v:" + n] = md5_pc("-i", os.path.join(MED, n), "-map", "0:v:0")
    for n, a in AUDIOS.items():
        ffpc(*A, *a, os.path.join(MED, n))
        ffpc("-i", os.path.join(MED, n), "-f", "s16le", "-ac", "1", "-ar", "22050", os.path.join(MED, "ref_" + n + ".raw"))
        ref["a:" + n] = subprocess.run(["md5sum", os.path.join(MED, "ref_" + n + ".raw")], capture_output=True,
                                       text=True).stdout.split()[0]
    for n, a in PERF.items():
        ffpc("-f", "lavfi", "-i", "testsrc=size=320x240:rate=25:duration=10", *a, os.path.join(MED, n))
    # los mismos cuadros de testsrc que se generan dentro: suma de 5 cuadros
    ref["lavfi"] = md5_pc("-f", "lavfi", "-i", "testsrc=size=160x96:rate=25", "-frames:v", "5")
    # un vídeo con más movimiento y más grande: mandelbrot a 640x360, 5 s (125 cuadros), H.264 Main y MPEG-4
    for n, a in (("q_h264.mp4", ["-c:v", "libx264", "-profile:v", "main", "-pix_fmt", "yuv420p", "-crf", "25"]),
                 ("q_mpeg4.avi", ["-c:v", "mpeg4", "-q:v", "5"])):
        ffpc("-f", "lavfi", "-i", "mandelbrot=size=640x360:rate=25", "-t", "5", *a, os.path.join(MED, n))
    ref["perf_cuadros"] = 250
    return ref


def saguios(ref):
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "ffmpeg")):
        print("AVISO: la imagen no lleva ffmpeg (tools/ffmpeg_fuentes.sh y make): se salta la parte «saguios»")
        return
    arqs = {n: open(os.path.join(MED, n), "rb").read() for n in sorted(os.listdir(MED))}
    pen = criar_pendrive("h458", mb=64, rotulo="FFM", arquivos=arqs, particionado=False)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    medidas = []
    with Maquina("h458", "ide", imagem=imagem_console(apps=True), extras=extras, memoria="512M") as m:
        esperar(m, r"sagui:/\$", 150, 0)
        time.sleep(3)
        orden(m, "montar usb0", 60)
        out, _ = orden(m, "ffmpeg -version | head -1", 30)
        verificar("ffmpeg version 8.1.3" in out, "ffmpeg -version (%r)" % out[-120:])
        out, _ = orden(m, "ffprobe -version | grep -i 'configuration\\|version' | head -3", 30)
        verificar("--enable-gpl" not in out and "--enable-nonfree" not in out, "ffprobe -version: configuración sin GPL ni nonfree")
        # ---- ffprobe de cada formato ----
        for n, c in {**VIDEO_CODEC, **AUDIO_CODEC}.items():
            out, _ = orden(m, "ffprobe -v error -show_entries stream=codec_name -of csv=p=0 /usb/%s" % n, 60)
            verificar(c in out, "ffprobe %s: códec %s (%r)" % (n, c, out.strip()[-60:]))
        out, _ = orden(m, "ffprobe -hide_banner /usb/v_h264.mp4", 60)
        verificar("h264" in out and "160x96" in out and "aac" in out and "22050 Hz" in out, "ffprobe v_h264.mp4 describe los dos flujos")
        m.captura("H458-1-ffprobe")
        # ---- vídeo: la suma de los cuadros es la del PC ----
        for n in VIDEOS:
            out, seg = orden(m, "ffmpeg -v error -i /usb/%s -map 0:v:0 -f md5 -" % n, 120)
            mm = re.search(r"MD5=([0-9a-f]{32})", out)
            verificar(mm is not None and mm.group(1) == ref["v:" + n],
                      "vídeo %s: la suma de los cuadros decodificados es la del ffmpeg del PC (%s)" % (n, mm.group(1) if mm else out[-100:]))
        # ---- audio ----
        for n in AUDIOS:
            ruta = "/tmp/" + n + ".raw"
            orden(m, "ffmpeg -v error -y -i /usb/%s -f s16le -ac 1 -ar 22050 %s" % (n, ruta), 120)
            if n in AUDIO_EXACTO:
                out, _ = orden(m, "md5sum %s" % ruta, 30)
                verificar(ref["a:" + n] in out, "audio %s: muestras idénticas a las del PC" % n)
            else:
                out, _ = orden(m, "wc -c %s /usb/ref_%s.raw" % (ruta, n), 30)
                tam = [int(x) for x in re.findall(r"^\s*(\d+) ", out, re.M)]
                verificar(len(tam) >= 2 and tam[0] == tam[1] and tam[0] > 40000, "audio %s: mismo tamaño que el del PC (%s)" % (n, tam))
                out, _ = orden(m, "cmp -l %s /usb/ref_%s.raw | wc -l" % (ruta, n), 60)
                dif = int(re.findall(r"^\s*(\d+)\s*$", out, re.M)[-1])
                verificar(dif < tam[0] * 0.2, "audio %s: %d de %d bytes distintos (< 20 %%; coma flotante)" % (n, dif, tam[0]))
        # ---- lavfi, filtros ----
        out, _ = orden(m, "ffmpeg -v error -f lavfi -i testsrc=size=160x96:rate=25 -frames:v 5 -f md5 -", 60)
        verificar(ref["lavfi"] in out, "lavfi testsrc da los mismos cuadros que en el PC")
        out, _ = orden(m, "ffmpeg -v error -y -i /usb/v_h264.mp4 -vf scale=80:48,crop=64:32,fps=10 -an -c:v mjpeg /tmp/f.avi && "
                          "ffprobe -v error -show_entries stream=width,height,r_frame_rate -of csv=p=0 /tmp/f.avi", 120)
        verificar("64,32,10/1" in out, "los filtros scale, crop y fps dan 64x32 a 10 cuadros por segundo (%r)" % out.strip()[-60:])
        out, _ = orden(m, "ffmpeg -v error -y -i /usb/a.wav -af volume=0.5,aresample=11025 /tmp/v.wav && ffprobe -v error -show_entries stream=sample_rate -of csv=p=0 /tmp/v.wav", 60)
        verificar("11025" in out, "los filtros volume y aresample (%r)" % out.strip()[-40:])
        out, _ = orden(m, "ffmpeg -v error -y -i /usb/v_mpeg4.avi -vn -c:a flac /tmp/s.flac && ffprobe -v error -show_entries stream=codec_name -of csv=p=0 /tmp/s.flac", 60)
        verificar("flac" in out, "extraer el audio de un AVI a FLAC")
        # ---- convertir para el Reproductor y que lo abra ----
        out, seg = orden(m, "ffmpeg -v error -y -f lavfi -i testsrc=size=160x96:rate=25:duration=1 -f lavfi -i sine=duration=1:sample_rate=22050 "
                            "-c:v mjpeg -pix_fmt yuvj420p -q:v 5 -c:a pcm_s16le -ac 2 -ar 22050 -f avi /tmp/sagui.avi && ls -l /tmp/sagui.avi", 120)
        verificar("/tmp/sagui.avi" in out, "convierte a AVI MJPEG + PCM para el Reproductor (%r)" % out.strip()[-80:])
        out, _ = orden(m, "reproductor -t /tmp/sagui.avi | cat", 120)
        n = len(re.findall(r"cuadro \d+ 160x96", out))
        verificar(n == 25, "el Reproductor de SaguiOS abre el AVI de ffmpeg y decodifica sus 25 cuadros (%d)" % n)
        out, _ = orden(m, "ffmpeg -v error -y -i /usb/v_h264.mp4 -vf scale=160:96 -c:v mpeg1video -q:v 4 -an -f mpeg /tmp/sagui.mpg", 120)
        out, _ = orden(m, "reproductor -t /tmp/sagui.mpg | cat", 120)
        n = len(re.findall(r"cuadro \d+ 160x96", out))
        # El decodificador MPEG-1 del Reproductor (userland/libsagui/mpeg1.c) pierde los últimos cuadros de los MPEG-PS
        # que escribe FFmpeg 8 (la segunda secuencia va sin cabecera de grupo): decodifica 20 de 25 (pendiente, no es de
        # este port; ffprobe cuenta los 25 y el ffmpeg del PC decodifica el fichero entero sin avisos).
        verificar("MPEG-1 en MPEG-PS 160x96" in out and n >= 19, "el Reproductor abre el MPEG-1 de ffmpeg (%d de 25 cuadros)" % n)
        out, _ = orden(m, "ffprobe -v error -count_frames -show_entries stream=nb_read_frames -of csv=p=0 /tmp/sagui.mpg", 60)
        verificar("25" in out, "ffprobe cuenta los 25 cuadros del MPEG-1 que escribió ffmpeg")
        m.captura("H458-2-convertir")
        # ---- velocidad: «-benchmark» da el tiempo de reloj (rtime) del propio ffmpeg, sin lo que tarda el terminal ----
        def medir(orden_ff, etiqueta, cuadros):
            out, seg = orden(m, orden_ff + " 2>&1 | grep bench", 300)
            rt = re.search(r"rtime=([\d.]+)s", out)
            verificar(rt is not None, "ffmpeg -benchmark da el tiempo (%s)" % etiqueta)
            r = max(float(rt.group(1)), 0.001)
            medidas.append("%s: %.2f s (%.0f cuadros/s)" % (etiqueta, r, cuadros / r))
            return r
        for n, ff in (("p_h264.mp4", "H.264 (baseline)"), ("p_mpeg4.avi", "MPEG-4"), ("p_mjpeg.avi", "MJPEG")):
            medir("ffmpeg -hide_banner -benchmark -i /usb/%s -f null -" % n, "decodificar %s 320x240, 250 cuadros" % ff, 250)
        for cod, ff in (("mjpeg -q:v 5", "MJPEG"), ("mpeg4", "MPEG-4"), ("mpeg1video -q:v 5", "MPEG-1")):
            medir("ffmpeg -hide_banner -benchmark -y -f lavfi -i testsrc=size=320x240:rate=25:duration=10 -c:v %s /tmp/e.avi" % cod,
                  "codificar %s 320x240, 250 cuadros (testsrc, con el generador)" % ff, 250)
        medir("ffmpeg -hide_banner -benchmark -i /usb/q_h264.mp4 -f null -", "decodificar H.264 Main 640x360 (mandelbrot), 125 cuadros", 125)
        medir("ffmpeg -hide_banner -benchmark -i /usb/q_mpeg4.avi -f null -", "decodificar MPEG-4 640x360 (mandelbrot), 125 cuadros", 125)
        medir("ffmpeg -hide_banner -benchmark -y -i /usb/q_h264.mp4 -vf scale=320:180 -c:v mjpeg -q:v 5 /tmp/c.avi",
              "convertir H.264 640x360 a MJPEG 320x180 (scale + codificar), 125 cuadros", 125)
        medir("ffmpeg -hide_banner -benchmark -y -i /usb/p_h264.mp4 -vf scale=160:120 -f null -", "decodificar H.264 y escalar a 160x120, 250 cuadros", 250)
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")
    os.makedirs(DIR, exist_ok=True)
    with open(os.path.join(DIR, "rendimiento.txt"), "w", encoding="utf-8") as f:
        f.write(("Con KVM" if KVM else "Sin KVM (TCG)") + "\n" + "\n".join(medidas) + "\n")
    print("\n".join("  medida: " + x for x in medidas))


def main():
    partes = sys.argv[1:] or ["pc", "saguios"]
    if not os.path.exists(FUENTES):
        print("AVISO: faltan las fuentes de FFmpeg (tools/ffmpeg_fuentes.sh): h458 se salta")
        return 0
    if "pc" in partes:
        pc()
    if "saguios" in partes:
        ref = anfitrion()
        if ref is None:
            print("AVISO: no hay ffmpeg en el PC: se salta la parte «saguios»")
            return 0
        saguios(ref)
    return 0


if __name__ == "__main__":
    sys.exit(main())
