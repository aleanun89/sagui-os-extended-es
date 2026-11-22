#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H157 (D190) - reproductor de MIDI: sintetizador de la libsagui (audio_midi.c) y programa /bin/midi.

Los .mid de prueba son los de rootfs/ejemplos/musica/ (generados por tools/gerar_midis.py; nada
con derechos de terceros). Partes:

  anfitrion  los mismos .c de la libsagui en el anfitrión (gcc, tools/audio_decodificar.c):
             - la escala de La mayor (formato 0) dura lo esperado, no es silencio y cada nota tiene
               su frecuencia (La 440 Hz...) frente a la nota intermedia;
             - alegria.kar (formato 1, karaoke): título, autor y letra, el cambio de tempo (duración),
               percusión; gm_canales.mid: 16 canales sin pasar de la escala (limitador);
             - el salto (midi_buscar) cae en el instante pedido; la síntesis es determinista;
             - archivos dañados (cortados, formato 2, basura, 300 mutaciones al azar): error claro o
               audio, sin colgarse ni caerse;
  os         dentro de SaguiOS (QEMU, sin tarjeta): `midi --info`, `midi --a-wav` y
             `reproducir --volcar` dan el MISMO PCM que el anfitrión (bit a bit: es el mismo código en
             enteros), y `midi --medir` mide la CPU de la síntesis;
  sonido     con la tarjeta (Intel HDA y AC'97, grabando la salida en un .wav): `reproducir
             escala_la.mid` suena con cada nota en su frecuencia y la duración justa; la GUI:
             doble clic en un .kar en Archivos abre `midi`, toca, la vista de los 16 canales se enciende
             y cambia con la música, la letra avanza, pausa, salto y siguiente; capturas
             docs/capturas/H157-*.png;
  sinsonido  sin tarjeta: el programa avisa y no se cuelga.

Uso: h157_midi.py [anfitrion] [os] [sonido] [sinsonido]
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
from qemu import (Maquina, verificar, imagem_console, criar_pendrive, pendrive_qemu, mtools, ler_ppm,  # noqa: E402
                  reconhecer_texto, RAIZ, SAIDA, FalhaTeste)
import h70_sonido as h70  # noqa: E402

DIR = os.path.join(SAIDA, "h157")
DECO = os.path.join(DIR, "audio_decodificar")
MUSICA = os.path.join(RAIZ, "rootfs", "ejemplos", "musica")
BORDA, TITULO = 2, 24
TAXA = 44100


# ------------------------------------------------------------ utilidades

def goertzel(x, f, taxa):
    w = 2 * math.pi * f / taxa
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for v in x:
        s1, s2 = v + c * s1 - s2, s1
    return math.sqrt(max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / max(1, len(x)) * 2


def lado(pcm, c):
    a = array.array("h")
    a.frombytes(pcm[:len(pcm) // 4 * 4])
    return a[c::2]


def rms(x):
    return math.sqrt(sum(v * v for v in x) / max(1, len(x)))


NOTAS_ESCALA = [440.0 * 2 ** (k / 12) for k in (0, 2, 4, 5, 7, 9, 11, 12, 11, 9, 7, 5, 4, 2, 0)]


def notas_correctas(x, taxa, inicio, paso, nombre):
    """Cada nota i empieza en inicio + i*paso: su frecuencia pesa mucho más que las de un cuarto de
    tono por encima y por debajo."""
    malas = []
    for i, f in enumerate(NOTAS_ESCALA):
        a = int(inicio + i * paso + 0.10 * taxa)
        seg = x[a:a + int(0.30 * taxa)]
        e = goertzel(seg, f, taxa)
        v = max(goertzel(seg, f * 2 ** (1 / 24), taxa), goertzel(seg, f / 2 ** (1 / 24), taxa))
        if not (e > 6 * v and e > 150):
            malas.append((i, round(f, 1), round(e), round(v)))
    verificar(not malas, f"{nombre}: las 15 notas de la escala suenan en su frecuencia {malas[:3]}")


# ------------------------------------------------------------ anfitrión

def compilar():
    os.makedirs(DIR, exist_ok=True)
    fuentes = ["tools/audio_decodificar.c"] + [f"userland/libsagui/{f}" for f in
                                                ("audio_formato.c", "audio_midi.c", "audio_adpcm.c", "audio_flac.c", "audio_mp3.c",
                                                 "audio_mp3_tabelas.c")]
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DAUDIO_HOST", "-Itools", "-Iuserland/inc", "-Iuserland/libsagui", "-o", DECO,
                        *fuentes], cwd=RAIZ, capture_output=True)
    verificar(r.returncode == 0 and not r.stderr, "el sintetizador compila en el anfitrión sin avisos: " + r.stderr.decode()[:300])


def deco(archivo, *args, tiempo=60):
    r = subprocess.run([DECO, archivo, *args], capture_output=True, timeout=tiempo)
    return r.stdout, r.stderr.decode("latin-1"), r.returncode


def anfitrion():
    compilar()
    # ---- la escala de La
    pcm, err, rc = deco(f"{MUSICA}/escala_la.mid")
    verificar(rc == 0 and "MIDI (formato 0)" in err and "44100 Hz 2 canais" in err, f"escala_la.mid: formato 0, 44100 Hz estéreo ({err.splitlines()[0][:70]})")
    m = re.search(r"(\d+) quadros (\d+) ms", err)
    verificar(m and abs(int(m.group(2)) - 7458) <= 2, f"la duración sale de los tics: {m.group(2)} ms (7458 esperados)")
    seg = len(pcm) / 4 / TAXA
    verificar(7.4 < seg < 11.0, f"el audio dura {seg:.2f} s (la secuencia más la cola de la reverberación)")
    izq, der = lado(pcm, 0), lado(pcm, 1)
    verificar(rms(izq) > 500 and rms(der) > 500, f"no es silencio: RMS {rms(izq):.0f}/{rms(der):.0f}")
    verificar(max(abs(v) for v in pcm_i16(pcm)) <= 32767, "sin desbordes")
    notas_correctas(izq, TAXA, 0, 0.5 * TAXA, "escala_la.mid")
    verificar(rms(izq[int(8.5 * TAXA):]) < rms(izq[:TAXA * 7]) / 3, "tras la última nota la cola se apaga")
    # ---- karaoke con percusión, tempo y letra
    pcm, err, rc = deco(f"{MUSICA}/alegria.kar")
    verificar(rc == 0 and "MIDI karaoke" in err and "titulo='Alegria'" in err and "artista='Beethoven / SaguiOS'" in err,
              f"alegria.kar: karaoke, título y autor de los @T ({err.splitlines()[0][:90]})")
    d = int(re.search(r"(\d+) quadros (\d+) ms", err).group(2))
    verificar(38000 <= d <= 39000, f"el ritardando alarga la pieza: {d} ms (a tempo fijo de 112 serían ~36400)")
    izq, der = lado(pcm, 0), lado(pcm, 1)
    verificar(rms(izq) > 800 and rms(der) > 800, f"suena (RMS {rms(izq):.0f}/{rms(der):.0f})")
    # el piano a la izquierda (pan 40) y el bajo a la derecha (pan 80): los canales no son iguales
    verificar(abs(rms(izq) - rms(der)) > 20, "el panorama separa los canales")
    funciones()
    # ---- gm_canales: todos los canales
    pcm, err, rc = deco(f"{MUSICA}/gm_canales.mid")
    verificar(rc == 0 and "MIDI (formato 1)" in err, "gm_canales.mid: formato 1")
    pk = max(abs(v) for v in pcm_i16(pcm))
    verificar(pk <= 32767 and pk > 4000, f"16 canales a la vez: pico {pk} (el limitador sostiene la escala)")
    pcm, err, rc = deco(f"{MUSICA}/elisa.mid")
    verificar(rc == 0 and rms(lado(pcm, 0)) > 400, "elisa.mid suena")
    # ---- determinismo y salto
    a, _, _ = deco(f"{MUSICA}/alegria.kar")
    b, _, _ = deco(f"{MUSICA}/alegria.kar")
    verificar(a == b, "la síntesis es determinista (dos pasadas idénticas)")
    s, err, rc = deco(f"{MUSICA}/alegria.kar", "20000")
    verificar(rc == 0 and "posicion 882000" in err, f"el salto a 20 s deja la posición en 882000 cuadros ({err.splitlines()[1][:60]})")
    verificar(abs(len(a) - len(s) - 882000 * 4) <= 4096, f"tras el salto salen {len(s) // 4} cuadros: el resto de la pieza")
    verificar(rms(lado(s, 0)[:44100]) > 500, "y suena desde el instante pedido")
    # ---- archivos dañados
    base = open(f"{MUSICA}/alegria.kar", "rb").read()
    casos = {
        "cortado.mid": base[:len(base) // 2],
        "cortado_cab.mid": base[:10],
        "formato2.mid": base[:8] + struct.pack(">HHH", 2, 1, 480) + base[14:],
        "sin_pistas.mid": b"MThd" + struct.pack(">IHHH", 6, 1, 0, 480),
        "basura.mid": b"MThd" + bytes(random.Random(1).randrange(256) for _ in range(500)),
        "vlq_largo.mid": b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96) + b"MTrk" + struct.pack(">I", 12) + b"\xff\xff\xff\xff\xff\x90\x3c\x40\x00\xff\x2f\x00",
        "div_cero.mid": base[:12] + b"\x00\x00" + base[14:],
        "no_midi.mid": b"RIFFxxxxWAVE" + bytes(100),
    }
    for nombre, datos in casos.items():
        ruta = f"{DIR}/{nombre}"
        open(ruta, "wb").write(datos)
        try:
            pcm, err, rc = deco(ruta, tiempo=30)
            ok = rc in (0, 1)
        except subprocess.TimeoutExpired:
            ok = False
            err = "TIEMPO AGOTADO"
        verificar(ok, f"{nombre}: error claro o audio, sin colgarse ({err.strip().splitlines()[-1][:70] if err.strip() else 'ok'})")
    rnd = random.Random(157)
    malos = 0
    for k in range(300):
        d = bytearray(open(f"{MUSICA}/{rnd.choice(['escala_la.mid', 'elisa.mid', 'alegria.kar'])}", "rb").read())
        for _ in range(rnd.randrange(1, 12)):
            d[rnd.randrange(len(d))] = rnd.randrange(256)
        if rnd.random() < 0.3:
            d = d[:rnd.randrange(14, len(d))]
        ruta = f"{DIR}/mut.mid"
        open(ruta, "wb").write(d)
        try:
            r = subprocess.run([DECO, ruta], capture_output=True, timeout=30)
            if r.returncode not in (0, 1):
                malos += 1
                open(f"{DIR}/mut-malo-{k}.mid", "wb").write(d)
        except subprocess.TimeoutExpired:
            malos += 1
            open(f"{DIR}/mut-lento-{k}.mid", "wb").write(d)
    verificar(malos == 0, f"300 mutaciones al azar: ninguna se cuelga ni se cae ({malos} malas)")


def mini(nombre, construir, ppqn=480):
    """Un .mid de una pista hecho aquí con el escritor de tools/gerar_midis.py; devuelve el PCM sintetizado."""
    sys.path.insert(0, os.path.join(RAIZ, "tools"))
    import gerar_midis as G
    p = G.Pista()
    p.meta(0, 0x51, G.tempo(120))
    construir(p, G)
    ruta = f"{DIR}/{nombre}.mid"
    G.escribir(ruta, 0, ppqn, [p])
    pcm, err, rc = deco(ruta)
    verificar(rc == 0, f"{nombre}: sintetiza ({err.splitlines()[-1][:60]})")
    return pcm


def energia(x, f0, f1, paso, taxa=TAXA):
    return sum(goertzel(x, f, taxa) for f in range(f0, f1 + 1, paso))


def funciones():
    """Las funciones de General MIDI una a una, con archivos de una nota."""
    L = lambda pcm, a, b: lado(pcm, 0)[int(a * TAXA):int(b * TAXA)]   # noqa: E731
    # rueda de tono: La 440 un segundo, a la mitad la rueda sube 2 semitonos (rango por defecto)
    pcm = mini("f_bend", lambda p, G: (p.nota(0, 960, 0, 69, 100), p.add(480, [0xE0, 0x7F, 0x7F])))
    a1, a2 = L(pcm, 0.1, 0.4), L(pcm, 0.6, 0.9)
    verificar(goertzel(a1, 440, TAXA) > 8 * goertzel(a1, 494, TAXA) and goertzel(a2, 494, TAXA) > 8 * goertzel(a2, 440, TAXA),
              "rueda de tono: 440 Hz y, al subirla, 494 Hz (dos semitonos)")
    # rango de la rueda por RPN 0 (4 semitonos): La 440 -> 554
    def rango(p, G):
        for c, v in ((101, 0), (100, 0), (6, 4)):
            p.cc(0, 0, c, v)
        p.nota(0, 960, 0, 69, 100)
        p.add(480, [0xE0, 0x7F, 0x7F])
    pcm = mini("f_rpn", rango)
    verificar(goertzel(L(pcm, 0.6, 0.9), 554.4, TAXA) > 8 * goertzel(L(pcm, 0.6, 0.9), 494, TAXA), "el rango de la rueda sale del RPN 0 (4 semitonos: 554 Hz)")
    # pedal de sustain: la nota se suelta a 0,25 s pero el pedal la sostiene hasta 1,2 s
    con = mini("f_pedal", lambda p, G: (p.cc(0, 0, 64, 127), p.nota(0, 240, 0, 69, 100), p.cc(1152, 0, 64, 0), p.nota(2400, 100, 0, 40, 1)))
    sin = mini("f_nopedal", lambda p, G: (p.nota(0, 240, 0, 69, 100), p.nota(2400, 100, 0, 40, 1)))
    e_con, e_sin = goertzel(L(con, 0.7, 1.0), 440, TAXA), goertzel(L(sin, 0.7, 1.0), 440, TAXA)
    verificar(e_con > 5 * e_sin and e_con > 100, f"pedal de sustain: la nota sigue sonando ({e_con:.0f} contra {e_sin:.0f} sin pedal)")
    # volumen y expresión
    fuerte = mini("f_v127", lambda p, G: (p.cc(0, 0, 7, 127), p.nota(0, 480, 0, 69, 100)))
    bajo = mini("f_v40", lambda p, G: (p.cc(0, 0, 7, 40), p.nota(0, 480, 0, 69, 100)))
    r = rms(L(bajo, 0.05, 0.4)) / rms(L(fuerte, 0.05, 0.4))
    verificar(0.05 < r < 0.25, f"volumen (CC7 40 contra 127): amplitud x{r:.2f}")
    expr = mini("f_e40", lambda p, G: (p.cc(0, 0, 11, 40), p.nota(0, 480, 0, 69, 100)))
    r = rms(L(expr, 0.05, 0.4)) / rms(L(mini("f_e127", lambda p, G: p.nota(0, 480, 0, 69, 100)), 0.05, 0.4))
    verificar(0.05 < r < 0.25, f"expresión (CC11 40 contra 127): amplitud x{r:.2f}")
    # velocidad
    suave = mini("f_vel30", lambda p, G: p.nota(0, 480, 0, 69, 30))
    verificar(rms(L(suave, 0.05, 0.4)) < 0.4 * rms(L(fuerte, 0.05, 0.4)), "la velocidad controla la fuerza")
    # panorama
    izq = mini("f_pan0", lambda p, G: (p.cc(0, 0, 10, 0), p.nota(0, 480, 0, 69, 100)))
    der = mini("f_pan127", lambda p, G: (p.cc(0, 0, 10, 127), p.nota(0, 480, 0, 69, 100)))
    verificar(rms(lado(izq, 1)[2000:20000]) < 0.12 * rms(lado(izq, 0)[2000:20000]) and
              rms(lado(der, 0)[2000:20000]) < 0.12 * rms(lado(der, 1)[2000:20000]),
              "el panorama a los extremos deja casi todo en un canal (queda la reverberación)")
    # percusión en el canal 10: bombo (graves) y charles (agudos)
    bombo = mini("f_bombo", lambda p, G: p.nota(0, 200, 9, 36, 110))
    verificar(energia(L(bombo, 0, 0.15), 40, 160, 20) > 4 * energia(L(bombo, 0, 0.15), 2000, 6000, 400), "canal 10, nota 36: un bombo (graves)")
    charles = mini("f_charles", lambda p, G: p.nota(0, 100, 9, 42, 110))
    verificar(energia(L(charles, 0, 0.1), 4000, 9000, 500) > 4 * energia(L(charles, 0, 0.1), 60, 300, 20), "canal 10, nota 42: un charles (agudos)")
    caja = mini("f_caja", lambda p, G: p.nota(0, 200, 9, 38, 110))
    verificar(rms(L(caja, 0, 0.2)) > 300, "canal 10, nota 38: una caja")
    # el canal 10 no toca notas con altura: la misma nota 69 en el canal 1 sí
    verificar(goertzel(L(mini("f_nota69", lambda p, G: p.nota(0, 480, 0, 69, 100)), 0.1, 0.4), 440, TAXA) > 150, "el canal 1 toca alturas")
    # cambio de programa: el piano y la trompeta tienen espectros distintos, los dos con 440 Hz de fundamental
    piano = mini("f_prog0", lambda p, G: p.nota(0, 480, 0, 69, 100))
    trompeta = mini("f_prog56", lambda p, G: (p.prog(0, 0, 56), p.nota(0, 480, 0, 69, 100)))
    flauta = mini("f_prog73", lambda p, G: (p.prog(0, 0, 73), p.nota(0, 480, 0, 69, 100)))
    espec = lambda pcm: [goertzel(L(pcm, 0.15, 0.4), 440 * h, TAXA) / goertzel(L(pcm, 0.15, 0.4), 440, TAXA) for h in (2, 3, 4)]   # noqa: E731
    ep, et, ef = espec(piano), espec(trompeta), espec(flauta)
    verificar(max(abs(x - y) for x, y in zip(ep, et)) > 0.1 and ef[0] < 0.25 and ef[0] < ep[0] / 2,
              f"el cambio de programa cambia el timbre (armónicos 2-4: piano {[round(v, 2) for v in ep]}, trompeta {[round(v, 2) for v in et]}, flauta {[round(v, 2) for v in ef]})")
    # tempo: 120 bpm la primera negra, 60 bpm después: la nota de la negra 3 (tic 960) entra a 0,5 + 1,0 = 1,5 s
    def tempo(p, G):
        p.meta(480, 0x51, G.tempo(60))
        p.nota(960, 240, 0, 69, 100)
    pcm = mini("f_tempo", tempo)
    ini = next(i for i, v in enumerate(lado(pcm, 0)) if abs(v) > 300)
    verificar(abs(ini / TAXA - 1.5) < 0.03, f"un cambio de tempo (120 -> 60 bpm) retrasa la nota a {ini / TAXA:.3f} s (1,5 esperados)")
    # polifonía: 24 notas a la vez, sin desbordar
    pcm = mini("f_poli", lambda p, G: [p.nota(0, 960, 0, 48 + k, 100) for k in range(24)])
    verificar(max(abs(v) for v in pcm_i16(pcm)) <= 32767 and rms(lado(pcm, 0)[2000:20000]) > 500, "24 notas a la vez suenan sin desbordar")
    # los 128 programas suenan (nadie se queda en silencio)
    mudos = []
    for prog in range(128):
        pcm = mini(f"f_p{prog}", lambda p, G, prog=prog: (p.prog(0, 0, prog), p.nota(0, 480, 0, 60, 100)))
        if rms(lado(pcm, 0)[1000:20000]) < 100:
            mudos.append(prog)
    verificar(not mudos, f"los 128 programas de GM suenan {mudos}")


def pcm_i16(pcm):
    a = array.array("h")
    a.frombytes(pcm[:len(pcm) // 2 * 2])
    return a


# ------------------------------------------------------------ SaguiOS (sin tarjeta)

def os_():
    ref_escala, _, _ = deco(f"{MUSICA}/escala_la.mid")
    ref_alegria, _, _ = deco(f"{MUSICA}/alegria.kar")
    pen = criar_pendrive("h157-pen", rotulo="MIDI")
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h157-os", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("ls /ejemplos/musica")
        verificar(all(n in out for n in ("escala_la.mid", "alegria.kar", "elisa.mid", "gm_canales.mid")), "los ejemplos están en /ejemplos/musica")
        out = m.comando("midi --info /ejemplos/musica/alegria.kar")
        verificar("formato 1" in out and "(karaoke)" in out and "Alegria" in out, f"midi --info: {out.strip().splitlines()[-4:]}")
        m.comando("montar usb0p1")
        out = m.comando("midi --a-wav /ejemplos/musica/escala_la.mid /usb/escala.wav", 120)
        verificar("-> /usb/escala.wav" in out, f"midi --a-wav sintetiza a un WAV: {out.strip().splitlines()[-3:]}")
        out = m.comando("reproducir --volcar /ejemplos/musica/alegria.kar /usb/alegria.pcm", 300)
        verificar("MIDI karaoke" in out, "reproducir --volcar decodifica un .kar por audio_formato")
        out = m.comando("midi --medir /ejemplos/musica/alegria.kar /ejemplos/musica/gm_canales.mid", 300)
        print("   " + out.strip().replace("\n", "\n   "))
        medidas = re.findall(r"sintetizados en (\d+) ms \(x(\d+) tiempo real, (\d+)% de una CPU\)", out)
        verificar(len(medidas) == 2 and all(int(x[1]) >= 1 for x in medidas), f"sintetiza más deprisa que el tiempo real: {medidas}")
        m.comando("umount /usb")
    for nombre, ref in (("escala.wav", ref_escala), ("alegria.pcm", ref_alegria)):
        ruta = f"{DIR}/{nombre}"
        mtools(pen, "mcopy", "-n", f"p:/{nombre}", ruta)
        datos = open(ruta, "rb").read()
        if nombre.endswith(".wav"):
            verificar(datos[:4] == b"RIFF" and datos[8:16] == b"WAVEfmt ", "el WAV tiene cabecera RIFF/WAVE")
            tam = struct.unpack("<I", datos[40:44])[0]
            verificar(tam == len(datos) - 44, f"la cabecera lleva el tamaño de los datos ({tam})")
            datos = datos[44:]
        verificar(datos == ref, f"{nombre}: el PCM de SaguiOS es idéntico bit a bit al del anfitrión ({len(datos) // 4} cuadros)")


# ------------------------------------------------------------ con tarjeta

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
    ppm = os.path.join(SAIDA, "h157-tela.ppm")
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


CORES = [0xE0483E, 0xF0861E, 0xE6B91A, 0xA2C62A, 0x42B54C, 0x1FB38A, 0x1EA2CE, 0x2A74DC,
         0x4D54DC, 0x8C4FD6, 0xD0A04A, 0xC04BBE, 0xE04D8C, 0x7C8C96, 0x9A6B3E, 0x5FA8A0]
CAN_Y, CAN_H, TEC_X, TEC_L = 190, 20, 34 + 20 * 8 + 4, 88 * 5


def canais_acesos(t, jx, jy):
    """Canales cuya franja de teclado tiene píxeles de su color (una nota encendida)."""
    acesos = set()
    larg, _, px = t
    for c in range(16):
        y = jy + CAN_Y + c * CAN_H + 3
        cor = CORES[c]
        rgb = bytes(((cor >> 16) & 255, (cor >> 8) & 255, cor & 255))
        escuro = bytes((((cor >> 16) & 255) // 2 & 0x7F, ((cor >> 8) & 255) // 2 & 0x7F, (cor & 255) // 2 & 0x7F))
        fila = px[(y * larg + jx + TEC_X) * 3:(y * larg + jx + TEC_X + TEC_L) * 3]
        for i in range(0, len(fila), 3):
            if fila[i:i + 3] in (rgb, escuro):
                acesos.add(c)
                break
    return acesos


def inicio_sonido(x, limiar=400):
    for i, v in enumerate(x):
        if abs(v) > limiar:
            return i
    return None


def reproducir_escala(placa):
    """reproducir escala_la.mid con la tarjeta y analiza lo que grabó el QEMU (48 kHz)."""
    with Maquina(f"h157-{placa}", "ide", imagem=imagem_console(), som=placa) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("reproducir /ejemplos/musica/escala_la.mid", 120)
        verificar("MIDI" in out, f"reproducir muestra el formato: {out.strip()[:100]}")
    esq, dir_ = h70.ler_gravacao(m.wav)
    ini = inicio_sonido(esq)
    verificar(ini is not None, f"[{placa}] la tarjeta recibió sonido (empieza a los {ini / 48000 if ini else 0:.2f} s)")
    x = esq[ini:]
    verificar(abs(rms(esq) - rms(dir_)) < rms(esq) * 0.5, f"[{placa}] los dos canales suenan")
    malas = []
    for i, f in enumerate(NOTAS_ESCALA):
        a = int(i * 0.5 * 48000 + 0.10 * 48000)
        seg = x[a:a + int(0.30 * 48000)]
        e = goertzel(seg, f, 48000)
        v = max(goertzel(seg, f * 2 ** (1 / 24), 48000), goertzel(seg, f / 2 ** (1 / 24), 48000))
        if not (e > 4 * v and e > 60):
            malas.append((i, round(f, 1), round(e), round(v)))
    verificar(not malas, f"[{placa}] las 15 notas de la escala suenan en su frecuencia en la tarjeta {malas[:3]}")
    fin = len(x) - min((i for i, v in enumerate(reversed(x)) if abs(v) > 400), default=len(x))   # posición de la última muestra fuerte
    verificar(7.2 < fin / 48000 < 11.0, f"[{placa}] el sonido dura {fin / 48000:.2f} s")


def gui(placa):
    with Maquina(f"h157-gui-{placa}", "ide", som=placa) as m:
        m.esperar_serial("sagui:/", 120)
        ini = len(m.serial())
        m.comando("arquivos /ejemplos/musica")
        esperar(m, r"arquivos: pronto", 20, ini)
        time.sleep(0.5)
        a = janela(m.serial()[ini:], "Archivos")
        m.captura("h157-archivos", guardar=False)
        m.clique(a["x"] + 146 + 120, a["y"] + 98 + 2 + 0 * 20 + 8, duplo=True)
        esperar(m, r"arquivos: abrindo no midi /ejemplos/musica/alegria.kar", 30, ini)
        verificar(True, "doble clic en un .kar en Archivos abre /bin/midi")
        esperar(m, r"midi: pista 1 alegria.kar: karaoke 38499 ms 6 pistas \d+ notas \d+ letras titulo 'Alegria'", 30, ini)
        esperar(m, r"midi: reproduciendo alegria.kar", 15, ini)
        jm = janela(m.serial()[ini:], "MIDI")
        verificar(jm["l"] == 920 and jm["a"] == 612, f"ventana de {jm['l']}x{jm['a']}")
        time.sleep(3.0)
        t1 = tela(m)
        m.captura("H157-midi-karaoke")
        verificar("Alegria" in texto_em(t1, jm["x"] + 12, jm["y"] + 44, 40), "el título sale de los metaeventos")
        verificar("Beethoven / SaguiOS" in texto_em(t1, jm["x"] + 12, jm["y"] + 62, 40), "el autor sale de los @T del .kar")
        verificar("MIDI karaoke" in texto_em(t1, jm["x"] + 12, jm["y"] + 80, 60), "formato en la línea de datos")
        ac1 = canais_acesos(t1, jm["x"], jm["y"])
        print("   canales encendidos:", sorted(c + 1 for c in ac1))
        verificar(len(ac1) >= 1, "la vista de canales enciende teclas del color del canal")
        # la animación: en 2 s las notas encendidas cambian
        distintos = set()
        for _ in range(6):
            time.sleep(0.4)
            distintos.add(frozenset(canais_acesos(tela(m), jm["x"], jm["y"])))
        verificar(len(distintos) >= 2, f"las teclas cambian con la música ({len(distintos)} estados distintos)")
        verificar(canais_acesos(t1, jm["x"], jm["y"]) <= {0, 1, 2, 9}, "solo los canales 1, 2, 3 y 10 de alegria.kar")
        # la letra: la línea actual en la zona de letra
        ly = jm["y"] + CAN_Y + 16 * CAN_H + 8
        letra = texto_em(t1, jm["x"] + 12, ly, 90).strip()
        print(f"   letra: «{letra[:60]}»")
        # el espacio entre palabras va al principio de la sílaba: la línea registrada los conserva
        verificar(re.search(r"midi: letra: \[Una canci.n de paz para todos los ni.os hoy\]", m.serial()[ini:]) is not None,
                  "la letra del karaoke conserva los espacios entre palabras")
        # pausa
        marca = len(m.serial())
        m.tecla("spc")
        pausa = int(esperar(m, r"midi: pausa en (\d+) ms", 10, marca))
        verificar(1500 <= pausa <= 12000, f"pausa en {pausa} ms")
        time.sleep(0.6)
        a1 = frozenset(canais_acesos(tela(m), jm["x"], jm["y"]))
        time.sleep(0.8)
        a2 = frozenset(canais_acesos(tela(m), jm["x"], jm["y"]))
        verificar(a1 == a2, "en pausa la vista se queda quieta")
        m.captura("H157-midi-pausa")
        marca = len(m.serial())
        m.tecla("spc")
        esperar(m, r"midi: sigue en \d+ ms", 10, marca)
        # salto con un clic en la barra (al 50 %)
        marca = len(m.serial())
        m.clique(jm["x"] + 12 + (jm["l"] - 24) // 2, jm["y"] + 124 + 6)
        sal = int(esperar(m, r"midi: salto a (\d+) ms", 10, marca))
        verificar(abs(sal - 19250) < 600, f"un clic en la mitad de la barra salta a {sal} ms")
        time.sleep(2.0)
        m.captura("H157-midi-salto")
        # siguiente: la lista tiene las 4 pistas de la carpeta
        marca = len(m.serial())
        m.tecla("n")
        esperar(m, r"midi: pista 2 ", 15, marca)
        esperar(m, r"midi: reproduciendo ", 15, marca)
        time.sleep(2.0)
        m.captura("H157-midi-lista")
        # la demo de canales: 15 canales a la vez
        marca = len(m.serial())
        m.tecla("n")
        esperar(m, r"midi: pista 3 ", 15, marca)
        m.tecla("n")
        esperar(m, r"midi: pista 4 gm_canales.mid: formato 1", 15, marca)
        time.sleep(1.4)
        t4 = tela(m)
        ac4 = canais_acesos(t4, jm["x"], jm["y"])
        m.captura("H157-midi-canales")
        print("   gm_canales, canales encendidos:", sorted(c + 1 for c in ac4))
        verificar(len(ac4) >= 8, f"con gm_canales.mid se encienden {len(ac4)} de los 16 canales")
        nome = texto_em(t4, jm["x"] + 34, jm["y"] + CAN_Y + 4 * CAN_H + 2, 20)
        verificar("Bajo" in nome, f"el canal 5 muestra su instrumento: «{nome.strip()}»")
        nome = texto_em(t4, jm["x"] + 34, jm["y"] + CAN_Y + 9 * CAN_H + 2, 20)
        verificar("Percusi" in nome, f"el canal 10 es «{nome.strip()}»")
    esq, dir_ = h70.ler_gravacao(m.wav)
    verificar(rms(esq) > 300, f"[{placa}] la GUI sacó sonido por la tarjeta (RMS {rms(esq):.0f})")


def sinsonido():
    with Maquina("h157-sin", "ide") as m:
        m.esperar_serial("sagui:/", 120)
        ini = len(m.serial())
        m.comando("midi /ejemplos/musica/escala_la.mid &")
        esperar(m, r"midi: pista \d+ escala_la.mid", 30, ini)
        esperar(m, r"midi: sin sonido", 10, ini)
        time.sleep(1)
        verificar(True, "sin tarjeta: avisa y sigue vivo")
        jm = janela(m.serial()[ini:], "MIDI")
        t = tela(m)
        pie = texto_em(t, jm["x"] + 8, jm["y"] + jm["a"] - 22 + 3, 60)
        verificar("tarjeta" in pie.lower(), f"el pie dice «{pie.strip()}»")
        m.tecla("s")
        time.sleep(0.3)
        m.tecla("alt-f4")   # cierra la ventana: el teclado vuelve a la terminal
        time.sleep(1)
        out = m.comando("midi --info /ejemplos/musica/elisa.mid")
        verificar("Para Elisa" in out, "y la terminal sigue respondiendo")


def main():
    quais = sys.argv[1:] or ["anfitrion", "os", "sonido", "sinsonido"]
    os.makedirs(DIR, exist_ok=True)
    if "anfitrion" in quais:
        anfitrion()
    if "os" in quais:
        os_()
    if "sonido" in quais:
        for placa in ("hda", "ac97"):
            reproducir_escala(placa)
        gui("hda")
    if "sinsonido" in quais:
        sinsonido()
    print("H157 OK")


if __name__ == "__main__":
    main()
