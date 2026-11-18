#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H75 (D098-D100, mejora 053) - decodificadores de audio: IMA ADPCM, MP3 y FLAC.

Todo el audio es GENERADO aquí (tonos, barridos, ruido, silencios, estallidos;
nada con derechos de terceros) y codificado con ffmpeg. Cuatro partes:

  anfitrion  los mismos .c de la libsagui (tools/audio_decodificar.c, gcc) contra
             ffmpeg en decenas de variantes: ADPCM y FLAC idénticos bit a bit;
             MP3 con SNR >= UMBRAL_MP3 dB y la misma duración (también MPEG-2 a
             16, 22,05 y 24 kHz, CBR, VBR, mono, estéreo conjunto y normal, con y
             sin cabecera Xing, etiquetas ID3v1/ID3v2.3/ID3v2.4 y ID3 UTF-8);
  os         dentro de SaguiOS (QEMU): "reproducir --volcar" decodifica desde el
             pendrive; el PCM se recupera con mtools y se compara igual que arriba,
             y con la salida del decodificador del anfitrión (idéntica);
  sonido     reproducción real por la tarjeta (-audiodev wav, como h70): un MP3, un
             FLAC y un ADPCM suenan con la frecuencia y la duración correctas; se
             muestran formato y etiquetas;
  danados    archivos truncados, corrompidos y basura: el decodificador no se cuelga
             y da un error claro; --medir: velocidad de decodificación.

Uso: h75_codecs_audio.py [anfitrion] [os] [sonido] [danados]
"""
import array
import math
import os
import random
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_pendrive, pendrive_qemu, mtools, RAIZ, SAIDA, FalhaTeste  # noqa: E402
import h70_sonido as h70  # noqa: E402
import h56_visor_imagenes as h56  # noqa: E402

UMBRAL_MP3 = 80.0     # dB; ffmpeg y este decodificador dan ~95 dB (la diferencia es el redondeo a 16 bits)
DIR = os.path.join(SAIDA, "h75")
DECO = os.path.join(DIR, "audio_decodificar")


# ------------------------------------------------------------ señales generadas

def escribir_wav(nombre, taxa, canales, muestras):
    d = array.array("h")
    n = len(muestras[0])
    for i in range(n):
        for c in range(canales):
            d.append(max(-32768, min(32767, int(muestras[c][i]))))
    datos = d.tobytes()
    cab = b"RIFF" + struct.pack("<I", 36 + len(datos)) + b"WAVE" + b"fmt " + struct.pack("<IHHIIHH", 16, 1, canales, taxa, taxa * canales * 2, canales * 2, 16)
    with open(nombre, "wb") as f:
        f.write(cab + b"data" + struct.pack("<I", len(datos)) + datos)


def barrido(taxa, seg, estereo, semilla=7):
    """Barrido de 200 a 3000 Hz + tono de 440 Hz + estallidos (transitorios, para que el MP3 use
    bloques cortos) + ruido; el canal derecho es otra mezcla."""
    rnd = random.Random(semilla)
    n = int(taxa * seg)
    izq, der = [], []
    for i in range(n):
        t = i / taxa
        fase = 2 * math.pi * (200 * t + (3000 - 200) * t * t / (2 * seg))
        v = 9000 * math.sin(fase) + 4000 * math.sin(2 * math.pi * 440 * t)
        if (i // (taxa // 4)) % 4 == 3 and (i % (taxa // 4)) < taxa // 200:
            v += rnd.uniform(-15000, 15000)
        v += rnd.gauss(0, 300)
        izq.append(v)
        der.append(0.7 * v + 6000 * math.sin(2 * math.pi * 1000 * t) if estereo else v)
    return izq, der


def generar_base():
    os.makedirs(DIR, exist_ok=True)
    rnd = random.Random(3)
    for taxa in (44100, 48000, 32000, 22050, 24000, 16000, 8000):
        i, d = barrido(taxa, 3.0, True)
        escribir_wav(f"{DIR}/est{taxa}.wav", taxa, 2, [i, d])
        escribir_wav(f"{DIR}/mono{taxa}.wav", taxa, 1, [i])
    n = 44100 * 2
    escribir_wav(f"{DIR}/silencio.wav", 44100, 1, [[0] * n])
    escribir_wav(f"{DIR}/ruido.wav", 44100, 2, [[rnd.uniform(-28000, 28000) for _ in range(n)] for _ in range(2)])
    t = [32000 * math.sin(2 * math.pi * 997 * k / 44100) for k in range(n)]
    escribir_wav(f"{DIR}/tomalto.wav", 44100, 2, [t, t])
    # tonos limpios y distintos por canal para la prueba con la tarjeta (2 s)
    for nombre, taxa, f1, f2 in (("tono_mp3", 44100, 600, 1500), ("tono_flac", 44100, 800, 1700), ("tono_adpcm", 22050, 500, 1200)):
        n = taxa * 2
        escribir_wav(f"{DIR}/{nombre}.wav", taxa, 2, [[12000 * math.sin(2 * math.pi * f1 * k / taxa) for k in range(n)],
                                                       [12000 * math.sin(2 * math.pi * f2 * k / taxa) for k in range(n)]])
    # versiones cortas (1,5 s) para las pruebas dentro de QEMU (el pendrive es lento)
    for nombre, taxa, estereo in (("est44100", 44100, True), ("mono22050", 22050, False), ("mono48000", 48000, False), ("est24000", 24000, True)):
        i, d = barrido(taxa, 1.5, estereo, 11)
        escribir_wav(f"{DIR}/c_{nombre}.wav", taxa, 2 if estereo else 1, [i, d] if estereo else [i])


def ffmpeg(*args):
    r = subprocess.run(["ffmpeg", "-v", "error", "-y", *args], capture_output=True)
    if r.returncode:
        raise FalhaTeste("ffmpeg falló: " + r.stderr.decode(errors="replace")[:300])


def referencia(archivo):
    """PCM de 16 bits intercalado que decodifica ffmpeg."""
    r = subprocess.run(["ffmpeg", "-v", "error", "-i", archivo, "-f", "s16le", "-"], capture_output=True, check=True)
    return r.stdout


def snr(a, b):
    """SNR en dB de a frente a la referencia b (misma longitud)."""
    x, y = array.array("h"), array.array("h")
    x.frombytes(a)
    y.frombytes(b)
    e = sum((p - q) ** 2 for p, q in zip(x, y))
    s = sum(q * q for q in y)
    if e == 0:
        return 999.0
    return 10 * math.log10(s / e) if s else 0.0


# ------------------------------------------------------------ variantes

ADPCM = [  # (nombre, fuente, opciones)
    ("adpcm_est44", "est44100", []), ("adpcm_mono22", "mono22050", []), ("adpcm_mono8", "mono8000", []),
    ("adpcm_est48", "est48000", []), ("adpcm_ruido", "ruido", []), ("adpcm_alto", "tomalto", []),
    ("adpcm_silencio", "silencio", []), ("adpcm_b256", "est44100", ["-block_size", "256"]),
    ("adpcm_b2048", "mono44100", ["-block_size", "2048"]),
]
FLAC = [
    ("flac_l0", "est44100", ["-compression_level", "0"]), ("flac_l3", "est44100", ["-compression_level", "3"]),
    ("flac_l5", "est44100", ["-compression_level", "5"]), ("flac_l8", "est44100", ["-compression_level", "8"]),
    ("flac_l12", "est44100", ["-compression_level", "12"]), ("flac_mono", "mono48000", []),
    ("flac_8k", "mono8000", ["-compression_level", "8"]), ("flac_ruido", "ruido", []), ("flac_alto", "tomalto", []),
    ("flac_silencio", "silencio", []), ("flac_24", "est44100", ["-sample_fmt", "s32", "-compression_level", "8"]),
    ("flac_24mono", "mono48000", ["-sample_fmt", "s32"]), ("flac_bs1000", "est48000", ["-frame_size", "1000"]),
    ("flac_bs16k", "est48000", ["-frame_size", "16384", "-compression_level", "12", "-lpc_passes", "3"]),
    ("flac_ls", "est44100", ["-ch_mode", "left_side"]), ("flac_rs", "est44100", ["-ch_mode", "right_side"]),
    ("flac_ms", "est44100", ["-ch_mode", "mid_side"]), ("flac_ind", "est44100", ["-ch_mode", "indep"]),
    ("flac_22", "est22050", []),
]
MP3 = [
    ("mp3_c128", "est44100", ["-b:a", "128k"]), ("mp3_c192_48", "est48000", ["-b:a", "192k"]),
    ("mp3_c96_32", "est32000", ["-b:a", "96k"]), ("mp3_c320", "est44100", ["-b:a", "320k"]),
    ("mp3_c32m", "mono44100", ["-b:a", "32k"]), ("mp3_c64m", "mono44100", ["-b:a", "64k"]),
    ("mp3_v0", "est44100", ["-q:a", "0"]), ("mp3_v4m48", "mono48000", ["-q:a", "4"]), ("mp3_v9", "est44100", ["-q:a", "9"]),
    ("mp3_l22", "est22050", ["-b:a", "64k"]), ("mp3_l24", "est24000", ["-b:a", "64k"]), ("mp3_l16", "est16000", ["-b:a", "32k"]),
    ("mp3_l22m", "mono22050", ["-q:a", "4"]), ("mp3_l16m", "mono16000", ["-b:a", "24k"]),
    ("mp3_ruido", "ruido", ["-b:a", "256k"]), ("mp3_silencio", "silencio", ["-b:a", "128k"]), ("mp3_alto", "tomalto", ["-b:a", "128k"]),
    ("mp3_estereo", "est44100", ["-b:a", "128k", "-joint_stereo", "0"]), ("mp3_conjunto", "est44100", ["-b:a", "128k", "-joint_stereo", "1"]),
    ("mp3_sinxing", "est44100", ["-b:a", "128k", "-write_xing", "0"]),
    ("mp3_8k_a_16k", "est8000", ["-ar", "16000", "-b:a", "24k"]),
]


def codificar_todo():
    for nombre, fuente, op in ADPCM:
        ffmpeg("-i", f"{DIR}/{fuente}.wav", "-c:a", "adpcm_ima_wav", *op, f"{DIR}/{nombre}.wav")
    for nombre, fuente, op in FLAC:
        ffmpeg("-i", f"{DIR}/{fuente}.wav", "-c:a", "flac", *op, f"{DIR}/{nombre}.flac")
    for nombre, fuente, op in MP3:
        ffmpeg("-i", f"{DIR}/{fuente}.wav", "-c:a", "libmp3lame", *op, f"{DIR}/{nombre}.mp3")
    # etiquetas: ID3v2.3 con acentos (UTF-16), ID3v2.4 (UTF-8) y ID3v1 anexada a mano
    ffmpeg("-i", f"{DIR}/mono22050.wav", "-c:a", "libmp3lame", "-b:a", "64k", "-metadata", "title=Tono de prueba ñandú",
           "-metadata", "artist=Generador", "-metadata", "album=Álbum", "-id3v2_version", "3", f"{DIR}/tag_v23.mp3")
    ffmpeg("-i", f"{DIR}/mono22050.wav", "-c:a", "libmp3lame", "-b:a", "64k", "-metadata", "title=Título 4",
           "-metadata", "artist=Art 4", "-id3v2_version", "4", f"{DIR}/tag_v24.mp3")
    ffmpeg("-i", f"{DIR}/mono22050.wav", "-c:a", "libmp3lame", "-b:a", "64k", "-id3v2_version", "0", f"{DIR}/tag_v1.mp3")
    with open(f"{DIR}/tag_v1.mp3", "ab") as f:
        f.write((b"TAG" + b"Solo v1".ljust(30, b"\0") + b"Artista 1".ljust(30, b"\0") + b"Alb".ljust(30, b"\0")).ljust(128, b"\0"))
    ffmpeg("-i", f"{DIR}/mono22050.wav", "-c:a", "flac", "-metadata", "title=Flac ñ", "-metadata", "artist=Genera", f"{DIR}/tag.flac")
    # versiones cortas para QEMU
    ffmpeg("-i", f"{DIR}/c_est44100.wav", "-c:a", "adpcm_ima_wav", f"{DIR}/os_adpcm_est.wav")
    ffmpeg("-i", f"{DIR}/c_mono22050.wav", "-c:a", "adpcm_ima_wav", f"{DIR}/os_adpcm_mono.wav")
    ffmpeg("-i", f"{DIR}/c_est44100.wav", "-c:a", "flac", "-compression_level", "8", f"{DIR}/os_flac16.flac")
    ffmpeg("-i", f"{DIR}/c_est44100.wav", "-c:a", "flac", "-sample_fmt", "s32", f"{DIR}/os_flac24.flac")
    ffmpeg("-i", f"{DIR}/c_mono48000.wav", "-c:a", "flac", f"{DIR}/os_flac_mono.flac")
    ffmpeg("-i", f"{DIR}/c_est44100.wav", "-c:a", "libmp3lame", "-b:a", "128k", f"{DIR}/os_mp3_cbr.mp3")
    ffmpeg("-i", f"{DIR}/c_mono48000.wav", "-c:a", "libmp3lame", "-q:a", "4", f"{DIR}/os_mp3_vbr.mp3")
    ffmpeg("-i", f"{DIR}/c_mono22050.wav", "-c:a", "libmp3lame", "-b:a", "48k", f"{DIR}/os_mp3_l22.mp3")
    ffmpeg("-i", f"{DIR}/c_est24000.wav", "-c:a", "libmp3lame", "-b:a", "64k", f"{DIR}/os_mp3_l24.mp3")
    # sonido real
    ffmpeg("-i", f"{DIR}/tono_mp3.wav", "-c:a", "libmp3lame", "-b:a", "192k", "-metadata", "title=Tono MP3", "-metadata", "artist=Generador h75", f"{DIR}/tono.mp3")
    ffmpeg("-i", f"{DIR}/tono_flac.wav", "-c:a", "flac", "-metadata", "title=Tono FLAC", "-metadata", "artist=Generador h75", f"{DIR}/tono.flac")
    ffmpeg("-i", f"{DIR}/tono_adpcm.wav", "-c:a", "adpcm_ima_wav", f"{DIR}/tono_adpcm_ima.wav")


# ------------------------------------------------------------ parte 1: anfitrión

def compilar_anfitrion():
    fuentes = ["tools/audio_decodificar.c"] + [f"userland/libsagui/{f}" for f in
                                                ("audio_formato.c", "audio_midi.c", "audio_adpcm.c", "audio_flac.c", "audio_mp3.c", "audio_mp3_tabelas.c")]
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DAUDIO_HOST", "-Itools", "-Iuserland/inc", "-Iuserland/libsagui", "-o", DECO,
                        *fuentes], cwd=RAIZ, capture_output=True)
    verificar(r.returncode == 0 and not r.stderr, "el decodificador compila en el anfitrión sin avisos: " + r.stderr.decode()[:200])


def decodificar_anfitrion(archivo):
    r = subprocess.run([DECO, archivo], capture_output=True, timeout=120)
    return r.stdout, r.stderr.decode("latin-1"), r.returncode


def anfitrion():
    compilar_anfitrion()
    for nombre, _, _ in ADPCM:
        a = f"{DIR}/{nombre}.wav"
        pcm, err, rc = decodificar_anfitrion(a)
        verificar(rc == 0 and pcm == referencia(a), f"ADPCM {nombre}: idéntico bit a bit a ffmpeg ({len(pcm) // 2} muestras)")
    for nombre, _, _ in FLAC:
        a = f"{DIR}/{nombre}.flac"
        pcm, err, rc = decodificar_anfitrion(a)
        verificar(rc == 0 and pcm == referencia(a), f"FLAC {nombre}: idéntico bit a bit a ffmpeg ({len(pcm) // 2} muestras)")
    peor = 999.0
    for nombre, _, _ in MP3:
        a = f"{DIR}/{nombre}.mp3"
        pcm, err, rc = decodificar_anfitrion(a)
        ref = referencia(a)
        s = snr(pcm, ref) if len(pcm) == len(ref) else -1
        verificar(rc == 0 and len(pcm) == len(ref) and s >= UMBRAL_MP3,
                  f"MP3 {nombre}: misma duración ({len(pcm) // 2} muestras) y SNR {s:.1f} dB >= {UMBRAL_MP3:.0f} dB")
        peor = min(peor, s)
    print(f"  SNR peor caso de MP3: {peor:.1f} dB")
    # etiquetas
    for archivo, tit, art in (("tag_v23.mp3", "Tono de prueba ñandú", "Generador"), ("tag_v24.mp3", "Título 4", "Art 4"),
                              ("tag_v1.mp3", "Solo v1", "Artista 1"), ("tag.flac", "Flac ñ", "Genera")):
        _, err, rc = decodificar_anfitrion(f"{DIR}/{archivo}")
        verificar(f"titulo='{tit}'" in err and f"artista='{art}'" in err, f"etiquetas de {archivo}: {tit!r} / {art!r}")
    # variantes que no se saben decodificar: error claro, sin colgarse
    for nombre in ("ruido.wav",):
        verificar(decodificar_anfitrion(f"{DIR}/{nombre}")[2] == 0, f"{nombre}: el WAV PCM también pasa por la biblioteca")
    _, err, rc = decodificar_anfitrion(f"{DIR}/mp3_c128.mp3")
    verificar("MP3 (MPEG-1 Layer III) 44100 Hz 2 canais" in err and "3000 ms" in err, "la duración sale de Xing/LAME: 3000 ms exactos")
    _, err, rc = decodificar_anfitrion(f"{DIR}/flac_l5.flac")
    verificar("FLAC 44100 Hz 2 canais 132300 quadros 3000 ms" in err, "la duración del FLAC sale del STREAMINFO: 3000 ms exactos")
    _, err, rc = decodificar_anfitrion(f"{DIR}/adpcm_est44.wav")
    verificar("WAV IMA ADPCM 44100 Hz 2 canais" in err, "el WAV IMA ADPCM se detecta por el contenido")
    _, err, rc = decodificar_anfitrion(f"{DIR}/mp3_v4m48.mp3")
    verificar("VBR" in err, "el MP3 VBR se detecta (cabecera Xing)")


# ------------------------------------------------------------ parte 2: dentro de SaguiOS

OS_ARCHIVOS = ["os_adpcm_est.wav", "os_adpcm_mono.wav", "os_flac16.flac", "os_flac24.flac", "os_flac_mono.flac", "os_mp3_cbr.mp3",
               "os_mp3_vbr.mp3", "os_mp3_l22.mp3", "os_mp3_l24.mp3"]


def en_os():
    arqs = {}
    for n in OS_ARCHIVOS:
        arqs[n] = open(f"{DIR}/{n}", "rb").read()
    pen = criar_pendrive("h75-os", rotulo="CODECS", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    t0 = time.time()
    with Maquina("h75-os", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("ayuda reproducir")
        verificar("MP3" in out and "FLAC" in out and "--volcar" in out, "ajuda reproducir explica los formatos y --volcar")
        m.comando("montar usb0p1")
        for n in OS_ARCHIVOS:
            salida = n.rsplit(".", 1)[0] + ".pcm"
            out = m.comando(f"reproducir --volcar /usb/{n} /usb/{salida}", 120)
            verificar("volcados" in out and "(0 dañados)" in out, f"[SaguiOS] volcó {n}: " + (re.search(r"volcados[^\n]*", out) or re.search(r"$", out)).group(0))
        out = m.comando("desmontar", 60)
        verificar("Ya puedes retirar el pendrive." in out, "desmontar escribe todo")
    print(f"  (decodificar dentro de SaguiOS: {time.time() - t0:.0f} s)")
    for n in OS_ARCHIVOS:
        salida = n.rsplit(".", 1)[0] + ".pcm"
        r = subprocess.run(["mtype", "-i", pen + "@@1M", f"::{salida}"], capture_output=True)
        pcm = r.stdout
        verificar(r.returncode == 0 and len(pcm) > 0, f"el PCM {salida} está en el pendrive ({len(pcm)} bytes)")
        ref = referencia(f"{DIR}/{n}")
        host, _, _ = decodificar_anfitrion(f"{DIR}/{n}")
        verificar(pcm == host, f"{n}: lo que decodifica SaguiOS es idéntico a lo del anfitrión")
        if n.endswith(".mp3"):
            verificar(len(pcm) == len(ref), f"{n}: misma duración que ffmpeg ({len(pcm) // 2} muestras)")
            s = snr(pcm, ref)
            verificar(s >= UMBRAL_MP3, f"{n}: SNR {s:.1f} dB frente a ffmpeg")
        else:
            verificar(pcm == ref, f"{n}: idéntico bit a bit a ffmpeg")


# ------------------------------------------------------------ parte 3: sonido real

def sonido():
    arqs = {n: open(f"{DIR}/{n}", "rb").read() for n in ("tono.mp3", "tono.flac", "tono_adpcm_ima.wav")}
    arqs["fondo.wav"] = h70.gerar_wav(8000, 8, 1, 30.0, [200], 0.1)
    pen = criar_pendrive("h75-som", rotulo="SONIDOS", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h75-som", "ide", imagem=imagem_console(), extras=extras, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("montar usb0p1")
        m.comando("mkdir /tmp/s")
        for n in arqs:
            if n != "fondo.wav":
                m.comando(f"cp /usb/{n} /tmp/s/{n}", 120)
        o1 = m.comando("reproducir /tmp/s/tono.mp3", 90)
        o2 = m.comando("reproducir /tmp/s/tono.flac", 90)
        o3 = m.comando("reproducir /tmp/s/tono_adpcm_ima.wav", 90)
    verificar("MP3 (MPEG-1 Layer III), 44100 Hz, estéreo, 192 kbps, 2,0 s" in o1, "reproducir describe el MP3: formato, frecuencia, canales, kbps y duración")
    verificar("Título:  Tono MP3" in o1 and "Artista: Generador h75" in o1, "reproducir muestra título y artista del ID3")
    verificar("FLAC, 44100 Hz, estéreo, 16 bits" in o2 and "2,0 s" in o2, "reproducir describe el FLAC")
    verificar("Título:  Tono FLAC" in o2 and "Artista: Generador h75" in o2, "reproducir muestra las etiquetas VORBIS_COMMENT del FLAC")
    verificar("WAV IMA ADPCM, 22050 Hz, estéreo" in o3, "reproducir describe el WAV IMA ADPCM")
    esq, dir_ = h70.ler_gravacao(m.wav)
    tr = [h70.Trecho(esq, dir_, a, b) for a, b in h70.trechos(esq, dir_)]
    print("  trechos:", tr)

    def achar(f1, f2):
        return [t for t in tr if h70.perto(t.f_esq, f1, 12) and h70.perto(t.f_dir, f2, 12)]
    for nombre, f1, f2, seg in (("MP3", 600, 1500, 2.0), ("FLAC", 800, 1700, 2.0), ("ADPCM", 500, 1200, 2.0)):
        t = achar(f1, f2)
        verificar(len(t) == 1, f"{nombre} por la tarjeta: {f1} Hz a la izquierda y {f2} Hz a la derecha")
        verificar(h70.perto(t[0].segundos, seg, 0.12), f"{nombre} por la tarjeta: duración {t[0].segundos:.3f} s (esperado {seg})")
        verificar(h70.perto(t[0].rms_esq, h70.rms_esperado(12000 / 32767), 700), f"{nombre} por la tarjeta: nivel RMS {t[0].rms_esq:.0f} (esperado {h70.rms_esperado(12000 / 32767):.0f})")


# ------------------------------------------------------------ parte 4: dañados y velocidad

def preparar_danados():
    d = {}
    flac = open(f"{DIR}/flac_l5.flac", "rb").read()
    mp3 = open(f"{DIR}/mp3_c128.mp3", "rb").read()
    adp = open(f"{DIR}/adpcm_est44.wav", "rb").read()
    d["flac_corto.flac"] = flac[:len(flac) // 2]
    ruta = bytearray(flac)
    ruta[len(ruta) // 2:len(ruta) // 2 + 40] = bytes(40)
    d["flac_malo.flac"] = bytes(ruta)
    d["flac_cab.flac"] = flac[:30]
    d["mp3_corto.mp3"] = mp3[:len(mp3) // 3]
    rnd = random.Random(5)
    m2 = bytearray(mp3)
    for _ in range(300):
        m2[rnd.randrange(3000, len(m2))] = rnd.randrange(256)
    d["mp3_malo.mp3"] = bytes(m2)
    d["basura.mp3"] = bytes(rnd.randrange(256) for _ in range(40000))
    d["vacio.mp3"] = b""
    d["adpcm_corto.wav"] = adp[:len(adp) // 2]
    d["basura.flac"] = b"fLaC" + bytes(rnd.randrange(256) for _ in range(30000))
    return d


def danados():
    compilar_anfitrion()
    d = preparar_danados()
    # en el anfitrión
    for n, datos in d.items():
        with open(f"{DIR}/{n}", "wb") as f:
            f.write(datos)
        t0 = time.time()
        pcm, err, rc = decodificar_anfitrion(f"{DIR}/{n}")
        verificar(time.time() - t0 < 10, f"[anfitrión] {n}: no se cuelga ({time.time() - t0:.2f} s, rc {rc})")
    for n, esperado in (("flac_corto.flac", "-113"), ("flac_malo.flac", "-112"), ("flac_cab.flac", "-113"), ("mp3_corto.mp3", "-113"),
                        ("basura.mp3", "-110"), ("vacio.mp3", "-110"), ("adpcm_corto.wav", None), ("basura.flac", "-112")):
        _, err, rc = decodificar_anfitrion(f"{DIR}/{n}")
        if esperado:
            verificar(f"erro {esperado}" in err, f"[anfitrión] {n}: error {esperado}: {err.strip().splitlines()[0][:70]}")
    # dentro de SaguiOS
    arqs = {n: datos for n, datos in d.items() if datos}
    arqs["vacio.mp3"] = b"x"
    arqs["grande.mp3"] = open(f"{DIR}/mp3_c128.mp3", "rb").read()
    arqs["grande.flac"] = open(f"{DIR}/flac_l5.flac", "rb").read()
    arqs["grande_adpcm.wav"] = open(f"{DIR}/adpcm_est44.wav", "rb").read()
    arqs["grande_mono.mp3"] = open(f"{DIR}/mp3_c64m.mp3", "rb").read()
    pen = criar_pendrive("h75-mal", rotulo="MALOS", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h75-mal", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("montar usb0p1")
        casos = (("flac_corto.flac", "cortado"), ("flac_malo.flac", "dañado"), ("flac_cab.flac", "cortado"), ("mp3_corto.mp3", "cortado"),
                 ("basura.mp3", "no es un archivo de sonido"), ("vacio.mp3", "no es un archivo de sonido"), ("basura.flac", "dañado"),
                 ("adpcm_corto.wav", ""))
        for n, texto in casos:
            out = m.comando(f"reproducir --medir /usb/{n}", 120)
            if texto:
                verificar("reproducir: " in out and (texto in out), f"[SaguiOS] {n}: vuelve al prompt con error claro ({texto})")
            else:
                verificar("decodificados" in out and "reproducir: " not in out, f"[SaguiOS] {n}: se decodifica lo que hay, sin error")
        out = m.comando("reproducir --medir /usb/mp3_malo.mp3", 120)
        verificar("decodificados" in out, "[SaguiOS] mp3_malo.mp3 (300 bytes cambiados): decodifica lo que puede")
        out = m.comando("reproducir /usb/noexiste.mp3", 30)
        verificar("reproducir: /usb/noexiste.mp3:" in out, "[SaguiOS] un archivo que no existe da un error")
        # velocidad: copia a la RAM y mide solo la decodificación
        for n in ("grande.mp3", "grande.flac", "grande_adpcm.wav", "grande_mono.mp3"):
            m.comando(f"cp /usb/{n} /tmp/{n}", 180)
        modo = "KVM" if os.environ.get("SAGUI_KVM") == "1" else "TCG"
        print(f"  --- velocidad de decodificación en QEMU ({modo}) ---")
        for n in ("grande.mp3", "grande_mono.mp3", "grande.flac", "grande_adpcm.wav"):
            out = m.comando(f"reproducir --medir /tmp/{n}", 180)
            mm = re.search(r"([\d,]+) Mciclos por segundo de audio(?:; ([\d,]+) veces el tiempo real)?", out)
            verificar("decodificados" in out and mm, f"[SaguiOS] {n}: --medir da la velocidad")
            veces = float(mm.group(2).replace(",", ".")) if mm.group(2) else 999.0   # sin décimas de ms medibles: muy por encima
            print(f"     -> {mm.group(1)} Mciclos/s de audio, {'>100' if not mm.group(2) else mm.group(2)}x tiempo real")
            verificar(veces > 1.0, f"[SaguiOS/{modo}] {n} se decodifica más rápido que el tiempo real ({veces:.1f}x)")


def archivos():
    """Doble clic en Archivos sobre .flac, .mp3 y .wav los abre con el reproductor de música (arquivos.c, D115)."""
    arqs = {"a.flac": open(f"{DIR}/os_flac_mono.flac", "rb").read(), "b.mp3": open(f"{DIR}/os_mp3_l22.mp3", "rb").read(),
            "c.wav": open(f"{DIR}/os_adpcm_mono.wav", "rb").read()}
    pen = criar_pendrive("h75-arq", rotulo="ARCH", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h75-arq", "ide", extras=extras, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("montar usb0p1")
        ini = len(m.serial())
        m.comando("arquivos /usb")
        h56.esperar(m, r"arquivos: pronto", 30, ini)
        time.sleep(0.5)
        j = h56.janela(m.serial(), "Archivos")
        for i, nombre, tipo in ((0, "a.flac", "FLAC"), (1, "b.mp3", "MP3"), (2, "c.wav", "ADPCM")):
            ini = len(m.serial())
            m.clique(j["x"] + j["l"] // 2, j["y"] - 12)   # sube Archivos sobre las ventanas del reproductor
            m.clique(j["x"] + 146 + 120, j["y"] + 70 + 2 + i * 20 + 8, duplo=True)
            h56.esperar(m, r"arquivos: abrindo no musica /usb/" + nombre.replace(".", r"\."), 20, ini)
            h56.esperar(m, r"musica: reproduciendo " + nombre.replace(".", r"\."), 60, ini)
            verificar(True, f"doble clic en {nombre} lo abre con el reproductor de música ({tipo}, D115)")
            time.sleep(1.0)


def main():
    quais = sys.argv[1:] or ["anfitrion", "os", "sonido", "danados", "archivos"]
    generar_base()
    codificar_todo()
    if "anfitrion" in quais:
        anfitrion()
    if "os" in quais:
        en_os()
    if "sonido" in quais:
        sonido()
    if "danados" in quais:
        danados()
    if "archivos" in quais:
        archivos()
    return 0


if __name__ == "__main__":
    sys.exit(main())
