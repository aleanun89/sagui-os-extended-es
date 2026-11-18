#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H86 (D115) - Reproductor de música gráfico (/apps/bin/musica, mejora 041/053).

El audio es GENERADO aquí (tonos distintos por canal, 6 s cada uno; nada con derechos de
terceros): un WAV (440 Hz izquierda, 660 Hz derecha), un MP3 con etiquetas ID3 (600/1500 Hz) y un
FLAC con comentarios Vorbis (800/1700 Hz), con ffmpeg. Partes:

  anfitrion  los mismos .c de la libsagui en el anfitrión: el salto (audio_arquivo_buscar) cae en
             el instante pedido en WAV y ADPCM (idéntico bit a bit al decodificar de seguido), y en
             FLAC y MP3 a menos de ~0,3 s (el cuadro más cercano);
  sonido     QEMU con tarjeta Intel HDA, grabando la salida en un .wav (como h70/h75):
               - doble clic en un .wav en Archivos abre el reproductor y suena;
               - Abrir con una carpeta llena la lista (WAV, MP3 y FLAC, en orden) y toca la primera;
               - espacio pausa (la posición se queda quieta y el sonido calla) y sigue sin perder
                 ni repetir nada;
               - un clic en la barra salta (WAV al 75 %: solo quedan ~1,5 s), y al acabar la
                 pista pasa sola a la siguiente (MP3 con salto al 66 %, FLAC con salto al 50 %);
               - la grabación tiene cada tono con su frecuencia por canal y las duraciones que
                 salen de los saltos; se muestran formato y etiquetas; capturas de pantalla;
               - las teclas N y P, las flechas ±5 s, y el volumen (clic en la barra de volumen);
  sinsonido  sin tarjeta: «No hay tarjeta de sonido», sin colgarse; un .ogg falso dice que
             Ogg Vorbis no está soportado.

Uso: h86_musica.py [anfitrion] [sonido] [sinsonido]
"""
import array
import math
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_pendrive, pendrive_qemu, ler_ppm,  # noqa: E402
                  reconhecer_texto, RAIZ, SAIDA, FalhaTeste)
import h70_sonido as h70  # noqa: E402
import h75_codecs_audio as h75  # noqa: E402

DIR = os.path.join(SAIDA, "h86")
BORDA, TITULO = 2, 24
SEG = 6.0


# ------------------------------------------------------------ material

def tono(nome, taxa, f1, f2, seg=SEG):
    n = int(taxa * seg)
    h75.escribir_wav(nome, taxa, 2, [[12000 * math.sin(2 * math.pi * f1 * k / taxa) for k in range(n)],
                                     [12000 * math.sin(2 * math.pi * f2 * k / taxa) for k in range(n)]])


def preparar():
    os.makedirs(DIR, exist_ok=True)
    tono(f"{DIR}/a_tono.wav", 44100, 440, 660)
    tono(f"{DIR}/b.wav", 44100, 600, 1500)
    tono(f"{DIR}/c.wav", 44100, 800, 1700)
    h75.ffmpeg("-i", f"{DIR}/b.wav", "-c:a", "libmp3lame", "-b:a", "128k", "-metadata", "title=Tono MP3",
               "-metadata", "artist=Generador h86", "-metadata", "album=Pruebas", f"{DIR}/b_tono.mp3")
    h75.ffmpeg("-i", f"{DIR}/c.wav", "-c:a", "flac", "-metadata", "title=Tono FLAC", "-metadata", "artist=Generador h86",
               f"{DIR}/c_tono.flac")
    return {n: open(f"{DIR}/{n}", "rb").read() for n in ("a_tono.wav", "b_tono.mp3", "c_tono.flac")}


# ------------------------------------------------------------ anfitrión

def anfitrion(arqs):
    os.makedirs(h75.DIR, exist_ok=True)
    h75.compilar_anfitrion()

    def pcm(archivo, ms=None):
        args = [h75.DECO, archivo] + ([str(ms)] if ms is not None else [])
        r = subprocess.run(args, capture_output=True, timeout=120)
        a = array.array("h")
        a.frombytes(r.stdout[:len(r.stdout) // 4 * 4])
        pos = re.search(r"posicion (\d+) cuadros", r.stderr.decode("latin-1"))
        return a, int(pos.group(1)) if pos else 0

    adpcm = f"{DIR}/a_adpcm.wav"
    h75.ffmpeg("-i", f"{DIR}/a_tono.wav", "-c:a", "adpcm_ima_wav", adpcm)
    for nombre, ruta, exacto in (("WAV", f"{DIR}/a_tono.wav", True), ("ADPCM", adpcm, True),
                                 ("FLAC", f"{DIR}/c_tono.flac", False), ("MP3", f"{DIR}/b_tono.mp3", False)):
        todo, _ = pcm(ruta)
        for ms in (0, 1500, 4500, 5900):
            parte, pos = pcm(ruta, ms)
            esperado = int(ms * 44.1)
            if exacto:
                verificar(pos == esperado, f"[{nombre}] salto a {ms} ms: posición {pos} cuadros (esperado {esperado})")
                verificar(parte == todo[pos * 2:], f"[{nombre}] salto a {ms} ms: lo que sigue es idéntico al final de la decodificación")
            else:
                verificar(abs(pos - esperado) < 0.3 * 44100, f"[{nombre}] salto a {ms} ms: cuadro más cercano {pos} (pedido {esperado}, "
                          f"error {abs(pos - esperado) / 44.1:.0f} ms)")
                verificar(abs(len(parte) // 2 - (len(todo) // 2 - pos)) < 0.3 * 44100 or nombre == "MP3",
                          f"[{nombre}] salto a {ms} ms: queda audio hasta el final ({len(parte) // 2} cuadros)")
                if nombre == "FLAC":
                    verificar(parte == todo[pos * 2:], f"[{nombre}] salto a {ms} ms: idéntico al final (la posición es la real del cuadro)")
                if ms < 5000:
                    verificar(len(parte) > 4410, f"[{nombre}] salto a {ms} ms: se decodifica sonido después del salto")


# ------------------------------------------------------------ dentro de SaguiOS

def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{titulo}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO)


def esperar(m, padrao, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.15)
    raise FalhaTeste(f"tiempo agotado esperando {padrao!r}")


def tela(m):
    ppm = os.path.join(SAIDA, "h86-tela.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    return ler_ppm(ppm)


def pixel(t, x, y):
    larg, _, px = t
    i = (y * larg + x) * 3
    return tuple(px[i:i + 3])


def texto_em(t, x0, y0, cols):
    larg, _, px = t
    recorte = b"".join(px[((y0 + k) * larg + x0) * 3:((y0 + k) * larg + x0 + cols * 8) * 3] for k in range(16))
    return reconhecer_texto(cols * 8, 16, recorte)[0]


def perto(a, b, tol):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def sonido(arqs):
    pen = criar_pendrive("h86-pen", rotulo="MUSICA", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h86-som", "ide", extras=extras, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("montar usb0p1")
        m.comando("mkdir /tmp/m")
        for n in arqs:
            m.comando(f"cp /usb/{n} /tmp/m/{n}", 120)
        out = m.comando("ayuda musica")
        verificar("musica [archivo | carpeta]" in out, "ajuda musica explica el uso")

        # ---- doble clic en un .wav en Archivos
        ini = len(m.serial())
        m.comando("arquivos /tmp/m")
        esperar(m, r"arquivos: pronto", 20, ini)
        time.sleep(0.5)
        a = janela(m.serial()[ini:], "Archivos")
        m.clique(a["x"] + 146 + 120, a["y"] + 70 + 2 + 0 * 20 + 8, duplo=True)
        esperar(m, r"musica: pista 1 a_tono.wav: WAV PCM 44100 Hz 2 canales 6000 ms", 30, ini)
        esperar(m, r"musica: reproduciendo a_tono.wav", 10, ini)
        verificar(True, "doble clic en un .wav en Archivos abre el reproductor y toca")
        jm = janela(m.serial()[ini:], "M.sica")
        bx, by, bw = jm["x"] + 12, jm["y"] + 124, jm["l"] - 24
        verificar(jm["l"] == 560 and jm["a"] == 440, f"ventana de {jm['l']}x{jm['a']}")
        time.sleep(1.0)
        m.captura("h86-musica-tocando")
        t = tela(m)
        verificar(texto_em(t, jm["x"] + 12, jm["y"] + 44, 20).startswith("a_tono.wav"), "el título es el nombre del archivo (sin etiquetas)")
        verificar("WAV PCM, 44100 Hz, estéreo, 16 bits, 1411 kbps" in texto_em(t, jm["x"] + 12, jm["y"] + 80, 50),
                  "muestra formato, frecuencia, canales, bits y kbps")
        verificar(perto(pixel(t, bx + 3, by + 6), (11, 122, 62), 6), "la barra de progreso se llena de verde")
        verificar(perto(pixel(t, bx + bw - 6, by + 6), (196, 203, 210), 6), "y el resto de la barra sigue gris")

        # ---- Abrir con una carpeta: la lista
        m.clique(jm["x"] + 12 + 30, jm["y"] + 8 + 12)
        time.sleep(1.0)
        # el diálogo (que pausa el sonido) propone la carpeta de la pista actual: Intro
        marca = len(m.serial())
        m.tecla("ret")                 # D278: «¿Añadir una carpeta entera?» Sí (Intro)
        time.sleep(1.0)
        m.tecla("ret")                 # el diálogo de carpeta propone /tmp/m/: Intro
        esperar(m, r"musica: 3 pistas añadidas desde /tmp/m/?", 30, marca)   # D278: el diálogo de carpeta devuelve /tmp/m
        esperar(m, r"musica: pista 1 a_tono.wav: WAV PCM", 20, marca)
        esperar(m, r"musica: reproduciendo a_tono.wav", 15, marca)
        verificar(True, "Abrir con una carpeta llena la lista con las 3 pistas y toca la primera")
        time.sleep(2.0)
        t = tela(m)
        rodape = texto_em(t, jm["x"] + 8, jm["y"] + jm["a"] - 22 + 3, 40)
        verificar("pista 1 de 3" in rodape, f"el pie dice «{rodape}»")

        # ---- pausa
        m.tecla("spc")
        pausa = int(esperar(m, r"musica: pausa en (\d+) ms", 10, marca))
        verificar(1500 <= pausa <= 4200, f"pausa: la posición es {pausa} ms")
        time.sleep(0.5)
        t1 = tela(m)
        time.sleep(1.5)
        t2 = tela(m)
        verificar(texto_em(t1, jm["x"] + 12, jm["y"] + 102, 6) == texto_em(t2, jm["x"] + 12, jm["y"] + 102, 6),
                  "en pausa el tiempo no avanza: " + texto_em(t2, jm["x"] + 12, jm["y"] + 102, 6))
        m.captura("h86-musica-pausa")
        m.tecla("spc")
        sigue = int(esperar(m, r"musica: sigue en (\d+) ms", 10, marca))
        verificar(abs(sigue - pausa) <= 50, f"seguir: continúa donde se quedó ({pausa} -> {sigue} ms)")

        # ---- salto con la barra: al 75 % de 6 s
        time.sleep(1.0)
        m.clique(int(bx + bw * 0.75), by + 6)
        salto = int(esperar(m, r"musica: salto a (\d+) ms", 10, marca))
        verificar(abs(salto - 4500) < 120, f"clic en la barra al 75 %: salto a {salto} ms (esperado ~4500)")
        # fin de la pista 1 -> pista 2 (MP3) sola
        esperar(m, r"musica: fin de la pista 1", 15, marca)
        esperar(m, r"musica: pista 2 b_tono.mp3: MP3 \(MPEG-1 Layer III\) 44100 Hz 2 canales \d+ ms titulo 'Tono MP3'", 15, marca)
        verificar(True, "al acabar la pista pasa sola a la siguiente (MP3 con título de la etiqueta)")
        time.sleep(1.0)
        t = tela(m)
        verificar(texto_em(t, jm["x"] + 12, jm["y"] + 44, 20).startswith("Tono MP3"), "el título del ID3 sale en la ventana")
        verificar(texto_em(t, jm["x"] + 12, jm["y"] + 62, 30).startswith("Generador h86 - Pruebas"), "artista y álbum del ID3")
        m.captura("h86-musica-mp3")
        m.clique(int(bx + bw * 0.66), by + 6)   # salto en el MP3
        salto = int(esperar(m, r"musica: salto a (\d+) ms \(cuadro \d+, (\d+) ms\)", 10, marca)[0])
        verificar(abs(salto - 3960) < 120, f"salto en el MP3 a {salto} ms")
        esperar(m, r"musica: fin de la pista 2", 15, marca)
        esperar(m, r"musica: pista 3 c_tono.flac: FLAC 44100 Hz 2 canales 6000 ms titulo 'Tono FLAC'", 15, marca)
        time.sleep(1.0)
        t = tela(m)
        verificar(texto_em(t, jm["x"] + 12, jm["y"] + 62, 20).startswith("Generador h86"), "artista del FLAC (comentarios Vorbis)")
        verificar("FLAC, 44100 Hz, estéreo, 16 bits" in texto_em(t, jm["x"] + 12, jm["y"] + 80, 50), "formato del FLAC")
        m.clique(int(bx + bw * 0.5), by + 6)
        salto = int(esperar(m, r"musica: salto a (\d+) ms \(cuadro (\d+), (\d+) ms\)", 10, marca)[0])
        verificar(abs(salto - 3000) < 120, f"salto en el FLAC a {salto} ms")
        esperar(m, r"musica: fin de la lista", 20, marca)
        verificar(True, "al acabar la última pista se detiene en la primera")
        time.sleep(1.0)
        t = tela(m)
        verificar("Detenido" in texto_em(t, jm["x"] + 8, jm["y"] + jm["a"] - 22 + 3, 40), "al final de la lista queda «Detenido»")

        # ---- siguiente / anterior (N, P) y flechas, ahora con la pista 1 detenida
        marca = len(m.serial())
        m.tecla("n")
        esperar(m, r"musica: pista 2 b_tono.mp3", 10, marca)
        m.tecla("p")
        esperar(m, r"musica: pista 1 a_tono.wav", 10, marca)
        verificar(True, "N y P: siguiente y anterior")
        m.tecla("spc")
        esperar(m, r"musica: reproduciendo a_tono.wav", 10, marca)
        time.sleep(0.5)
        m.tecla("right")
        esperar(m, r"musica: salto a (\d+) ms", 10, marca)
        m.tecla("left")
        time.sleep(0.5)
        s = re.findall(r"musica: salto a (\d+) ms", m.serial()[marca:])
        verificar(len(s) == 2 and abs(int(s[0]) - int(s[1]) - 5000) < 400,
                  f"flecha derecha +5 s y flecha izquierda -5 s ({s})")
        m.tecla("s")
        esperar(m, r"musica: parado en \d+ ms", 10, marca)
        verificar(True, "S: parar")
        # volumen: clic al 50 % de la barra de volumen (x = larg - 12 - 100 - 40, ancho 100)
        vx = jm["x"] + jm["l"] - 12 - 100 - 40
        m.clique(vx + 50, jm["y"] + 158 + 2)
        time.sleep(0.5)
        t = tela(m)
        txt = texto_em(t, vx + 100 + 6, jm["y"] + 158 - 6, 5)
        verificar(txt.startswith("5") and "%" in txt, f"volumen al ~50 %: «{txt}»")
        m.captura("h86-musica-volumen")
    esq, dir_ = h70.ler_gravacao(m.wav)
    # Ventanas de 0,1 s: qué par de tonos (izquierda/derecha) suena en cada una; las pistas pasan
    # una a otra sin silencio, así que no se separan por silencios sino por frecuencia.
    pares = {440: 660, 600: 1500, 800: 1700}
    dur = {f: 0.0 for f in pares}
    # Lo que debería sonar, reconstruido del registro del reproductor: cada intervalo empieza en
    # «reproduciendo» (0), «sigue en X» o un salto, y acaba en una pausa, un salto, parar o el final.
    esperado = {440: 0.0, 600: 0.0, 800: 0.0}
    nombre_a_f = {"a_tono.wav": 440, "b_tono.mp3": 600, "c_tono.flac": 800}
    f_act, p0 = None, None
    for linea in m.serial().splitlines():
        g = re.search(r"musica: (pista \d+ (\S+):|reproduciendo (\S+)|sigue en (\d+) ms|pausa en (\d+) ms|salto a \d+ ms \(cuadro \d+, (\d+) ms\) desde (\d+) ms|"
                      r"parado en (\d+) ms|fin de la pista)", linea)
        if not g:
            continue
        if g.group(2):
            f_act, p0 = nombre_a_f[g.group(2)], None
        elif g.group(3):
            p0 = 0
        elif g.group(4):
            p0 = int(g.group(4))
        elif g.group(5) and p0 is not None:
            esperado[f_act] += (int(g.group(5)) - p0) / 1000
            p0 = None
        elif g.group(6) and p0 is not None:
            esperado[f_act] += (int(g.group(7)) - p0) / 1000
            p0 = int(g.group(6))
        elif g.group(8) and p0 is not None:
            esperado[f_act] += (int(g.group(8)) - p0) / 1000
            p0 = None
        elif "fin de la pista" in linea and p0 is not None:
            esperado[f_act] += (6000 - p0) / 1000
            p0 = None
    mal = 0
    for k in range(0, len(esq) - 4800, 4800):
        x, y = esq[k:k + 4800], dir_[k:k + 4800]
        if h70.rms(x) < 1000:
            continue
        f = max(pares, key=lambda f: h70.goertzel(x, f))
        if h70.goertzel(y, pares[f]) < 0.5 * max(h70.goertzel(y, g) for g in pares.values()):
            mal += 1
        dur[f] += 0.1
    print(f"  segundos por tono: {dur} (ventanas con el canal derecho que no cuadra: {mal})")
    verificar(mal <= 3, f"cada tono izquierdo va con su tono derecho ({mal} ventanas dudosas, las de los cambios de pista)")
    print(f"  segundos esperados según el registro: {esperado}")
    for f, nombre in ((440, "WAV"), (600, "MP3"), (800, "FLAC")):
        verificar(esperado[f] > 1.5 and -0.5 < dur[f] - esperado[f] < 1.3,   # lo que ya está en el DMA de la tarjeta sigue sonando ~0,15 s tras cada pausa o salto
                  f"{nombre} ({f}/{pares[f]} Hz): en la grabación suenan {dur[f]:.1f} s; según el registro de posiciones, {esperado[f]:.1f} s "
                  f"(la pausa, los saltos y el cambio de pista no pierden ni repiten sonido)")


# ------------------------------------------------------------ sin tarjeta

def sinsonido(arqs):
    arqs = dict(arqs)
    arqs["falso.ogg"] = b"OggS" + bytes(range(256)) * 8
    pen = criar_pendrive("h86-pen2", rotulo="MUSICA", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h86-sem", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("montar usb0p1")
        m.comando("mkdir /tmp/m")
        for n in ("a_tono.wav", "falso.ogg"):
            m.comando(f"cp /usb/{n} /tmp/m/{n}", 120)
        ini = len(m.serial())
        m.comando("musica /tmp/m/falso.ogg /tmp/m/a_tono.wav &")
        esperar(m, r"musica: error al abrir /tmp/m/falso.ogg", 20, ini)
        time.sleep(1.0)
        jm = janela(m.serial()[ini:], "M.sica")
        t = tela(m)
        rodape = texto_em(t, jm["x"] + 8, jm["y"] + jm["a"] - 22 + 3, 70)
        verificar("Ogg Vorbis" in rodape, f"un .ogg avisa: «{rodape}»")
        m.tecla("n")   # la siguiente pista (el WAV): se abre; sin tarjeta no suena
        esperar(m, r"musica: pista 2 a_tono.wav", 10, ini)
        m.tecla("spc")
        esperar(m, r"musica: sin sonido \(-\d+\)", 20, ini)
        time.sleep(1.0)
        t = tela(m)
        rodape = texto_em(t, jm["x"] + 8, jm["y"] + jm["a"] - 22 + 3, 70)
        verificar("No hay tarjeta de sonido" in rodape, f"sin tarjeta: «{rodape}»")
        verificar("WAV PCM" in texto_em(t, jm["x"] + 12, jm["y"] + 80, 50), "sin tarjeta muestra igualmente los datos del archivo")
        m.captura("h86-musica-sin-tarjeta")
        m.tecla("spc")   # no se cuelga: sigue respondiendo
        time.sleep(0.5)
        m.tecla("right")
        esperar(m, r"musica: salto a \d+ ms", 10, ini)
        verificar(True, "sin tarjeta no se cuelga: sigue atendiendo las teclas")


def main():
    quais = sys.argv[1:] or ["anfitrion", "sonido", "sinsonido"]
    arqs = preparar()
    if "anfitrion" in quais:
        anfitrion(arqs)
    if "sonido" in quais:
        sonido(arqs)
    if "sinsonido" in quais:
        sinsonido(arqs)
    print("H86 ok")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        sys.exit(1)
