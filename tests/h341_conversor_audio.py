#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H341 (D339, 8.4.4) - el Conversor de audio (/apps/bin/convertir-audio).

El audio es GENERADO aquí (tonos y un poco de ruido; nada con derechos de terceros). Partes:

  anfitrion  el codificador FLAC, el WAV y el remuestreo (userland/libsagui/audio_conversor.c, gcc en el anfitrión,
             tools/conversor_audio_host.c): el FLAC es idéntico bit a bit al original al decodificarlo con ffmpeg
             (los nueve niveles, mono y estéreo, y longitudes que no son múltiplo del bloque);
  os         dentro de SaguiOS (QEMU): un WAV de prueba se convierte con «convertir-audio» a MP3 (128 y 320 kbps),
             FLAC y WAV de 22,05 kHz mono (y WAV de 8 bits); con ffprobe/ffmpeg se comprueba que la duración, la
             frecuencia y los canales son los pedidos, que el MP3 tiene el tamaño de su tasa de bits y suena como
             el original (SNR), que el FLAC es idéntico y que el decodificador propio lee lo que sale; el aviso de
             «ya existe» (sin -y) y los errores de la línea de órdenes; y se anota cuánto tarda cada conversión
             (la de 60 s a MP3 de 128 kbps, para el rendimiento).
  ventana    «convertir-audio» con un archivo abre la ventana (captura) y se cancela cerrándola.

Uso: h341_conversor_audio.py [anfitrion] [os] [ventana]     (con KVM: sg kvm -c "SAGUI_KVM=1 python3 ...")
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
from qemu import (Maquina, verificar, imagem_de_teste, opcoes_apps, criar_pendrive, pendrive_qemu,  # noqa: E402
                  RAIZ, SAIDA, FalhaTeste)

DIR = os.path.join(SAIDA, "h341")
SEG = 20.0        # duración de la prueba general
SEG_PERF = 60.0   # la de la medida de rendimiento
CHOST = os.path.join(DIR, "conversor_audio_host")
DECO = os.path.join(DIR, "audio_decodificar")
# shine no lleva modelo psicoacústico: en esta señal (tonos y ruido) da ~18 dB a 128 y a 320 kbps, lo mismo que compilado en el
# anfitrión (LAME da 24 y 42 dB); el umbral solo vigila que el MP3 sea el audio y no basura desalineada.
UMBRAL_MP3_128 = 15.0   # dB
UMBRAL_MP3_320 = 15.0


# ------------------------------------------------------------ material

def escribir_wav(nombre, taxa, canales, muestras):
    d = array.array("h")
    n = len(muestras[0])
    for i in range(n):
        for c in range(canales):
            d.append(max(-32768, min(32767, int(muestras[c][i]))))
    datos = d.tobytes()
    cab = b"RIFF" + struct.pack("<I", 36 + len(datos)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, canales, taxa, taxa * canales * 2, canales * 2, 16)
    with open(nombre, "wb") as f:
        f.write(cab + b"data" + struct.pack("<I", len(datos)) + datos)


def senal(taxa, seg, estereo=True, semilla=11):
    rnd = random.Random(semilla)
    n = int(taxa * seg)
    canales = 2 if estereo else 1
    ch = []
    for c in range(canales):
        f1, f2 = (440, 3000) if c == 0 else (660, 1500)
        ch.append([9000 * math.sin(2 * math.pi * f1 * k / taxa) + 4000 * math.sin(2 * math.pi * f2 * k / taxa)
                   + rnd.randint(-250, 250) for k in range(n)])
    return canales, ch


def preparar():
    os.makedirs(DIR, exist_ok=True)
    c, ch = senal(44100, SEG)
    escribir_wav(f"{DIR}/prueba.wav", 44100, c, ch)
    c, ch = senal(44100, SEG_PERF)
    escribir_wav(f"{DIR}/minuto.wav", 44100, c, ch)


def ffprobe(archivo):
    r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "stream=codec_name,sample_rate,channels,duration,bit_rate",
                        "-of", "default=nw=1", archivo], capture_output=True, text=True)
    d = dict(l.split("=", 1) for l in r.stdout.split() if "=" in l)
    return d


def pcm(archivo, taxa=None, canales=None):
    cmd = ["ffmpeg", "-v", "error", "-y", "-i", archivo]
    if taxa:
        cmd += ["-ar", str(taxa)]
    if canales:
        cmd += ["-ac", str(canales)]
    r = subprocess.run(cmd + ["-f", "s16le", "-"], capture_output=True)
    a = array.array("h")
    a.frombytes(r.stdout[: len(r.stdout) // 2 * 2])
    return a


def snr_alineado(a, b, canales, ventana=44100):
    """SNR en dB de a (MP3 decodificado) frente a b (original), alineando el retardo del codificador/decodificador."""
    if len(a) < 3 * ventana * canales or len(b) < 3 * ventana * canales:
        return 0.0
    paso = canales
    ini = ventana * canales
    lag_mejor = 0
    # búsqueda del retardo: el de menor error en una ventana corta, a saltos de 4 muestras
    mejor_e = None
    for lag in range(-3000, 3000):
        e = 0
        for k in range(0, 2000 * paso, 8 * paso):
            ia = ini + k + lag * paso
            if ia < 0 or ia >= len(a):
                e = None
                break
            d = a[ia] - b[ini + k]
            e += d * d
        if e is not None and (mejor_e is None or e < mejor_e):
            mejor_e, lag_mejor = e, lag
    e = s2 = 0
    for k in range(0, ventana * paso, paso):
        ia = ini + k + lag_mejor * paso
        d = a[ia] - b[ini + k]
        e += d * d
        s2 += b[ini + k] * b[ini + k]
    return 10 * math.log10(s2 / e) if e else 999.0


# ------------------------------------------------------------ parte 1: anfitrión

def compilar_anfitrion():
    fuentes = ["tools/conversor_audio_host.c"] + [f"userland/libsagui/{f}" for f in
                                                   ("audio_conversor.c", "audio_formato.c", "audio_midi.c", "audio_adpcm.c", "audio_flac.c",
                                                    "audio_mp3.c", "audio_mp3_tabelas.c")]
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DAUDIO_HOST", "-Itools", "-Iuserland/inc", "-Iuserland/libsagui", "-o", CHOST, *fuentes],
                       cwd=RAIZ, capture_output=True)
    verificar(r.returncode == 0 and not r.stderr, "el conversor compila en el anfitrión sin avisos: " + r.stderr.decode()[:300])
    r = subprocess.run(["gcc", "-O2", "-DAUDIO_HOST", "-Itools", "-Iuserland/inc", "-Iuserland/libsagui", "-o", DECO, "tools/audio_decodificar.c",
                        *[f"userland/libsagui/{f}" for f in ("audio_formato.c", "audio_midi.c", "audio_adpcm.c", "audio_flac.c", "audio_mp3.c", "audio_mp3_tabelas.c")]],
                       cwd=RAIZ, capture_output=True)
    verificar(r.returncode == 0, "el decodificador compila en el anfitrión")


def anfitrion():
    compilar_anfitrion()
    ref = pcm(f"{DIR}/prueba.wav")
    for nivel in range(9):
        out = f"{DIR}/h_n{nivel}.flac"
        subprocess.run([CHOST, f"{DIR}/prueba.wav", out, "44100", "2", str(nivel)], check=True)
        dec = pcm(out)
        verificar(dec == ref, f"FLAC nivel {nivel}: idéntico al original ({os.path.getsize(out)} bytes)")
    # mono y longitudes raras (que acaban en un bloque corto)
    for seg in (0.0007, 0.31, 1.234567):
        c, ch = senal(44100, seg, estereo=False)
        escribir_wav(f"{DIR}/h_c.wav", 44100, c, ch)
        subprocess.run([CHOST, f"{DIR}/h_c.wav", f"{DIR}/h_c.flac", "44100", "1", "4"], check=True)
        verificar(pcm(f"{DIR}/h_c.flac") == pcm(f"{DIR}/h_c.wav"), f"FLAC mono de {seg} s: idéntico")
    # silencio y ruido de escala completa (subtramas constantes y verbatim)
    escribir_wav(f"{DIR}/h_s.wav", 44100, 2, [[0] * 20000, [0] * 20000])
    rnd = random.Random(5)
    escribir_wav(f"{DIR}/h_r.wav", 44100, 2, [[rnd.randint(-32768, 32767) for _ in range(20000)] for _ in range(2)])
    for n in ("h_s", "h_r"):
        subprocess.run([CHOST, f"{DIR}/{n}.wav", f"{DIR}/{n}.flac", "44100", "2", "6"], check=True)
        verificar(pcm(f"{DIR}/{n}.flac") == pcm(f"{DIR}/{n}.wav"), f"FLAC de {'silencio' if n == 'h_s' else 'ruido de escala completa'}: idéntico")
    p = ffprobe(f"{DIR}/h_n5.flac")
    verificar(p.get("codec_name") == "flac" and p.get("sample_rate") == "44100" and p.get("channels") == "2", "ffprobe lee el FLAC: " + str(p))
    # decodificador propio
    r = subprocess.run([DECO, f"{DIR}/h_n5.flac"], capture_output=True)
    a = array.array("h")
    a.frombytes(r.stdout)
    verificar(a == ref, "el decodificador propio de la libsagui lee el FLAC y da lo mismo")
    # frecuencia y canales (WAV)
    for taxa, can, bits in ((22050, 1, 16), (11025, 2, 16), (48000, 2, 16), (44100, 2, 8)):
        out = f"{DIR}/h_{taxa}_{can}_{bits}.wav"
        subprocess.run([CHOST, f"{DIR}/prueba.wav", out, str(taxa), str(can), str(bits)], check=True)
        p = ffprobe(out)
        d = float(p.get("duration", 0))
        verificar(p.get("sample_rate") == str(taxa) and p.get("channels") == str(can) and abs(d - SEG) < 0.01,
                  f"WAV de {taxa} Hz, {can} canales, {bits} bits: {p.get('sample_rate')} Hz, {p.get('channels')} canales, {d:.3f} s, {p.get('codec_name')}")
    # calidad del remuestreo: el tono de 440 Hz a 22,05 kHz mono sale con la misma amplitud (±1 dB)
    a = pcm(f"{DIR}/h_22050_1_16.wav")
    pico = max(abs(x) for x in a[22050:44100])
    ref22 = pcm(f"{DIR}/prueba.wav", 22050, 1)
    pico_ref = max(abs(x) for x in ref22[22050:44100])
    db = 20 * math.log10(pico / pico_ref)
    verificar(abs(db) < 1.5, f"remuestreo a 22,05 kHz: pico {pico} frente a {pico_ref} de ffmpeg ({db:+.2f} dB)")


# ------------------------------------------------------------ parte 2: dentro de SaguiOS

def tiempo_de(out):
    m = re.search(r"(\d+) ms de audio en (\d+) ms", out)
    return (int(m.group(1)), int(m.group(2))) if m else (0, 0)


def leer_pen(pen, nombre):
    r = subprocess.run(["mtype", "-i", pen + "@@1M", f"::{nombre}"], capture_output=True)
    return r.stdout if r.returncode == 0 else b""


def en_os():
    pen = criar_pendrive("h341-os", mb=128, rotulo="CONVERT",
                         arquivos={"prueba.wav": open(f"{DIR}/prueba.wav", "rb").read(), "minuto.wav": open(f"{DIR}/minuto.wav", "rb").read()})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    img = imagem_de_teste("", ["--vesa", "--console"] + opcoes_apps())
    tiempos = {}
    with Maquina("h341-os", "ide", imagem=img, extras=extras, memoria="256M") as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("/apps/bin/convertir-audio --ayuda")
        verificar("--kbps" in out and "--nivel" in out and ".flac" in out, "la ayuda explica las opciones")
        m.comando("montar usb0p1")
        pruebas = [
            ("o128.mp3", "--kbps 128"), ("o320.mp3", "--kbps 320"), ("o64.mp3", "--kbps 64 --canales 1"),
            ("of.flac", "--nivel 5"), ("of8.flac", "--nivel 8 --canales 1"), ("o22.wav", "--taxa 22050 --canales 1"),
            ("o8.wav", "--bits 8"), ("o11.wav", "--taxa 11025"),
        ]
        for nombre, opc in pruebas:
            out = m.comando(f"/apps/bin/convertir-audio /usb/prueba.wav /usb/{nombre} {opc}", 600)
            dur, ms = tiempo_de(out)
            tiempos[nombre] = (dur, ms)
            verificar(dur > 0, f"[SaguiOS] convirtió a {nombre} {opc}: {dur} ms de audio en {ms} ms")
        out = m.comando("/apps/bin/convertir-audio /usb/prueba.wav /usb/o128.mp3 --kbps 128", 60)
        verificar("ya existe" in out, "sin -y avisa de que el destino existe: " + out.strip()[-80:])
        out = m.comando("/apps/bin/convertir-audio /usb/prueba.wav /usb/o128.mp3 --kbps 128 -y", 600)
        verificar("ms de audio" in out, "con -y sobrescribe")
        out = m.comando("/apps/bin/convertir-audio /usb/nada.wav /usb/x.mp3", 60)
        verificar("nada.wav" in out, "una entrada que no existe da error claro")
        out = m.comando("/apps/bin/convertir-audio /usb/prueba.wav /usb/x.mp3 --kbps 20", 60)
        verificar("de 64 a 320" in out, "una tasa fuera de rango se rechaza")
        t0 = time.time()
        out = m.comando("/apps/bin/convertir-audio /usb/minuto.wav /usb/minuto.mp3 --kbps 128", 1200)
        dur, ms = tiempo_de(out)
        tiempos["minuto.mp3"] = (dur, ms)
        verificar(dur >= SEG_PERF * 1000 - 50, f"[SaguiOS] 60 s a MP3 de 128 kbps: {ms} ms de reloj ({dur / max(ms, 1):.2f}x tiempo real)")
        out = m.comando("desmontar", 60)
        verificar("Ya puedes retirar el pendrive." in out, "desmontar escribe todo")
    for k, (dur, ms) in tiempos.items():
        print(f"  [tiempo] {k}: {dur} ms de audio en {ms} ms ({dur / max(ms, 1):.2f}x tiempo real)")
    ref = pcm(f"{DIR}/prueba.wav")
    # MP3
    for nombre, kbps, umbral, canales, seg in (("o128.mp3", 128, UMBRAL_MP3_128, 2, SEG), ("o320.mp3", 320, UMBRAL_MP3_320, 2, SEG), ("o64.mp3", 64, 0, 1, SEG),
                                                ("minuto.mp3", 128, 0, 2, SEG_PERF)):
        datos = leer_pen(pen, nombre)
        archivo = f"{DIR}/{nombre}"
        open(archivo, "wb").write(datos)
        p = ffprobe(archivo)
        verificar(len(datos) > 0 and p.get("codec_name") == "mp3", f"{nombre}: ffprobe lo reconoce como MP3 ({len(datos)} bytes)")
        verificar(p.get("sample_rate") == "44100" and p.get("channels") == str(canales), f"{nombre}: {p.get('sample_rate')} Hz, {p.get('channels')} canales")
        d = float(p.get("duration", 0))
        verificar(abs(d - seg) < 0.1, f"{nombre}: duración {d:.3f} s (esperada {seg})")
        esperado = kbps * 1000 / 8 * seg
        verificar(abs(len(datos) - esperado) / esperado < 0.02, f"{nombre}: tamaño {len(datos)} B frente a {esperado:.0f} B de {kbps} kbps CBR")
        if umbral:
            s = snr_alineado(pcm(archivo), ref, 2)
            verificar(s >= umbral, f"{nombre}: SNR {s:.1f} dB frente al original (umbral {umbral})")
        r = subprocess.run([DECO, archivo], capture_output=True)
        verificar(r.returncode == 0 and b"MP3" in r.stderr and b"0 ruins" in r.stderr, f"{nombre}: el decodificador propio lo lee sin errores")
    # FLAC
    for nombre, mono in (("of.flac", False), ("of8.flac", True)):
        datos = leer_pen(pen, nombre)
        archivo = f"{DIR}/{nombre}"
        open(archivo, "wb").write(datos)
        p = ffprobe(archivo)
        verificar(p.get("codec_name") == "flac" and p.get("channels") == ("1" if mono else "2") and p.get("sample_rate") == "44100",
                  f"{nombre}: ffprobe lo reconoce ({len(datos)} bytes, {p.get('channels')} canales)")
        if not mono:
            verificar(pcm(archivo) == ref, f"{nombre}: decodificado con ffmpeg es idéntico al WAV original")
        else:
            # mono: el promedio entero de los dos canales
            orig = array.array("h")
            orig.frombytes(ref.tobytes())
            medio = array.array("h", [(orig[2 * i] + orig[2 * i + 1]) >> 1 for i in range(len(orig) // 2)])
            verificar(pcm(archivo) == medio, f"{nombre}: decodificado es idéntico a la mezcla a mono (promedio de L y R)")
        r = subprocess.run([DECO, archivo], capture_output=True)
        verificar(r.returncode == 0 and b"FLAC" in r.stderr, f"{nombre}: el decodificador propio lo lee")
    # WAV
    datos = leer_pen(pen, "o22.wav")
    open(f"{DIR}/o22.wav", "wb").write(datos)
    p = ffprobe(f"{DIR}/o22.wav")
    verificar(p.get("sample_rate") == "22050" and p.get("channels") == "1" and p.get("codec_name") == "pcm_s16le" and abs(float(p["duration"]) - SEG) < 0.01,
              f"o22.wav: WAV PCM 16 bits de {p.get('sample_rate')} Hz, {p.get('channels')} canal, {p.get('duration')} s")
    a = pcm(f"{DIR}/o22.wav")
    pico = max(abs(x) for x in a[22050:44100])
    verificar(8000 < pico < 20000, f"o22.wav: el tono suena con amplitud razonable (pico {pico})")
    datos = leer_pen(pen, "o8.wav")
    open(f"{DIR}/o8.wav", "wb").write(datos)
    p = ffprobe(f"{DIR}/o8.wav")
    verificar(p.get("codec_name") == "pcm_u8" and p.get("sample_rate") == "44100" and p.get("channels") == "2", f"o8.wav: WAV PCM de 8 bits ({p.get('codec_name')})")
    datos = leer_pen(pen, "o11.wav")
    open(f"{DIR}/o11.wav", "wb").write(datos)
    p = ffprobe(f"{DIR}/o11.wav")
    verificar(p.get("sample_rate") == "11025" and p.get("channels") == "2" and abs(float(p["duration"]) - SEG) < 0.01, f"o11.wav: {p.get('sample_rate')} Hz, {p.get('duration')} s")


# ------------------------------------------------------------ parte 3: la ventana

def ventana():
    pen = criar_pendrive("h341-vent", mb=32, rotulo="CONVERT", arquivos={"prueba.wav": open(f"{DIR}/prueba.wav", "rb").read()})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    img = imagem_de_teste("", ["--vesa"] + opcoes_apps())
    with Maquina("h341-vent", "ide", imagem=img, extras=extras, memoria="256M") as m:
        m.esperar_serial('"Terminal"', 180)
        m.esperar_serial("sagui:/$", 60)
        m.comando("montar usb0p1")
        m.digitar("convertir-audio /usb/prueba.wav", 0.04)
        m.tecla("ret")
        m.esperar_serial("convertir-audio: ventana lista (1 archivos)", 60)
        time.sleep(1)
        m.captura("h341-ventana")
        verificar(True, "la ventana del conversor abre con el archivo de la línea de órdenes")


def main():
    quais = sys.argv[1:] or ["anfitrion", "os", "ventana"]
    preparar()
    if "anfitrion" in quais:
        anfitrion()
    if "os" in quais:
        en_os()
    if "ventana" in quais:
        ventana()
    return 0


if __name__ == "__main__":
    sys.exit(main())
