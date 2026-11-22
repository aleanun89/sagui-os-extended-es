#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H431 - AoEN: el audio no depende de los FPS (D431, v8.5.5.2).

Partes (por defecto todas; o las que se pidan: h431_aoen_audio.py mezcla juego fuentes):
  mezcla   en el PC (tests/aoen_audio.c con userland/juegos/aoen/mezcla.c): un hilo de audio al ritmo del reloj y un hilo de
           «juego» que dispara un efecto cada 250 ms con cuadros de 5, 100 y 300 ms y una parada de hasta 800 ms. La música
           (un seno continuo) sale idéntica a la referencia, sin huecos ni trozos repetidos; cada efecto dura exactamente lo
           suyo (a su velocidad, aunque el juego vaya a 3 fps), no se pierde ninguno y suena en el bloque siguiente a su disparo.
  juego    en el PC: el propio AoEN (escena «audio», datos sintéticos, SDL con audio y vídeo «dummy») con una música MIDI
           sintética, con AOEN_FPS_LENTO=150 (cuadros de 150 ms ocupados) y sin ello: el WAV grabado dura lo que toca, la
           música no se corta (ninguna ventana de 50 ms en silencio), los efectos llegan todos y el hueco entre mezclas es
           corto. Datos del juego original: nunca.
  saguios  DENTRO de SaguiOS (QEMU con tarjeta HDA y el audio de QEMU a un .wav, DRS y MIDI sintéticos): la escena «audio» de AoEN,
           sin freno y con AOEN_FPS_LENTO=150 AOEN_FPS_OCUPADO=1, en una CPU y con --smp: el hilo propio abre la tarjeta y
           escribe sin errores, el .wav dura lo que toca y es un solo trozo (ningún silencio de más de 50 ms), la línea final
           da «sin datos 0» y el juego sale limpio (el hilo termina y vuelve el indicador).
  gpu      D436 (audio con el dibujo por GPU), DENTRO de SaguiOS con el driver simulado como una GPU LENTA («inicio 100c 96», 150 ms: los valores van en hexadecimal
           en gpu3d-simulado.conf: cada espera, lectura y presentación tarda 150 ms, más que lo que el kernel mezcla por
           delante de la tarjeta): AoEN en partida con el dibujo por GPU a pocos fps. Las esperas ceden la CPU
           (gpu3d_respirar): la tarjeta no corta («0 cortes»), el flujo de AoEN no se queda vacío («sin datos 0») y su hilo
           de audio tiene la alta prioridad (PLAN_AUDIO, de serie con el dibujo por GPU; AOEN_AUDIO_PRIO=1 la pide sin él).
  fuentes  el audio ya no usa SDL_QueueAudio ni cerrojos del dispositivo; hay mezcla.c/h, D431 y la documentación.
"""
import math
import os
import re
import struct
import subprocess
import sys
import tempfile
import wave

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
AOEN = os.path.join(RAIZ, "userland", "juegos", "aoen")
RATE = 44100


def leer_wav(ruta):
    w = wave.open(ruta)
    n = w.getnframes()
    d = struct.unpack("<%dh" % (n * 2), w.readframes(n))
    return n, d[0::2], d[1::2]


def mezcla():
    from qemu import verificar
    tmp = tempfile.mkdtemp(prefix="h431-")
    exe = os.path.join(tmp, "aoen_audio")
    subprocess.run(["gcc", "-O2", "-Wall", "-o", exe, os.path.join(RAIZ, "tests", "aoen_audio.c"), os.path.join(AOEN, "mezcla.c"), "-lm", "-lpthread"], check=True)
    for seg, cuadro, parada in ((6, 5, 0), (8, 100, 600), (8, 300, 800)):
        wav = os.path.join(tmp, "s_%d_%d.wav" % (cuadro, parada))
        r = subprocess.run([exe, wav, str(seg), str(cuadro), str(parada)], capture_output=True, text=True, timeout=120)
        verificar(r.returncode == 0, "FPS %.1f (cuadro de %d ms, parada %d ms): el arnés termina" % (1000.0 / (cuadro + 1), cuadro, parada))
        eventos = [int(x.split()[1]) for x in r.stdout.splitlines() if x.startswith("evento")]
        resumen = re.search(r"cuadros (\d+) bloques (\d+) eventos (\d+) perdidos (\d+)", r.stdout)
        verificar(resumen is not None, "el arnés da su resumen")
        n, izq, der = leer_wav(wav)
        verificar(abs(n - seg * RATE) < 0.06 * seg * RATE + 8192, "duración %.2f s (esperada %d s: la marca el reloj de audio, no los cuadros)" % (n / RATE, seg))
        verificar(izq == der, "los dos canales son iguales (efectos sin paneo)")
        # música continua: lo que no es efecto es el seno de referencia, muestra a muestra
        res = [izq[i] - int(round(8000.0 * math.sin(2.0 * math.pi * 440.0 * i / RATE))) for i in range(n)]
        tramos, ini = [], None
        for i, v in enumerate(res):
            if v != 0 and ini is None: ini = i
            elif v == 0 and ini is not None: tramos.append((ini, i - ini)); ini = None
        if ini is not None: tramos.append((ini, n - ini))
        verificar(all(abs(v) in (0, 3000) for v in res), "fuera de los efectos, la música es el seno de referencia sin un hueco ni una repetición")
        completos = [t for t in tramos if t[0] + t[1] < n]
        verificar(all(t[1] == 4410 for t in completos), "cada efecto dura exactamente 4410 muestras (0,1 s a su velocidad): %d efectos" % len(completos))
        enviados = int(resumen.group(3))
        verificar(int(resumen.group(4)) == 0 and enviados == len(eventos), "no se pierde ningún efecto (%d disparados, %d recibidos)" % (len(eventos), enviados))
        verificar(len(tramos) == len(eventos) or len(tramos) == len(eventos) - 1, "suena un efecto por cada disparo (%d tramos, %d disparos)" % (len(tramos), len(eventos)))
        # cada efecto empieza en el bloque siguiente a su disparo (1024 cuadros), no cuando acabe el cuadro del juego
        tarde = [t[0] - e for t, e in zip(tramos, eventos) if t[0] - e > 2048 or t[0] - e < -8]
        verificar(not tarde, "cada efecto empieza dentro de un bloque de su disparo (%d fuera)" % len(tarde))


def midi_sintetico(ruta, segundos=40):
    """Una sola nota larga (órgano, 120 pulsos por minuto): suena sin parar mientras dura y se repite en bucle."""
    ticks = segundos * 2 * 480

    def vlq(v):
        b = [v & 127]
        v >>= 7
        while v:
            b.append((v & 127) | 128)
            v >>= 7
        return bytes(reversed(b))
    pista = b"\x00\xc0\x10" + b"\x00\x90\x3c\x64" + vlq(ticks) + b"\x80\x3c\x40" + b"\x00\xff\x2f\x00"
    open(ruta, "wb").write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(pista)) + pista)


def juego():
    from qemu import verificar
    tmp = tempfile.mkdtemp(prefix="h431j-")
    exe = os.path.join(tmp, "aoen")
    fuentes = [os.path.join(AOEN, f) for f in sorted(os.listdir(AOEN)) if f.endswith(".c") and f not in ("aoen_config.c", "puente.c")]
    sdl = subprocess.run(["sdl2-config", "--cflags"], capture_output=True, text=True).stdout.split()
    libs = subprocess.run(["sdl2-config", "--libs"], capture_output=True, text=True).stdout.split()
    subprocess.run(["gcc", "-std=gnu99", "-O2", "-w", "-I", AOEN, "-I", os.path.join(RAIZ, "userland", "terceros", "stbimage")] + sdl + ["-o", exe] + fuentes + libs + ["-lm", "-lpthread"], check=True)
    datos = os.path.join(tmp, "DATA")
    os.makedirs(datos)
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), datos], check=True, stdout=subprocess.DEVNULL)
    musica = os.path.join(tmp, "musica")
    os.makedirs(musica)
    midi_sintetico(os.path.join(musica, "open.mid"))
    seg = 8
    for lento in (0, 150):
        wav = os.path.join(tmp, "juego_%d.wav" % lento)
        env = dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy", AOEN_SCENE="audio", AOEN_AUDIO_SEG=str(seg), AOEN_AUDIO_WAV=wav,
                   AOEN_MUSIC=musica, AOEN_FPS_LENTO=str(lento), AOEN_FPS_OCUPADO="1", HOME=tmp, XDG_CONFIG_HOME=tmp)
        r = subprocess.run([exe, datos], capture_output=True, text=True, timeout=120, env=env)
        reg = r.stderr
        etiqueta = "con cuadros de %d ms ocupados" % lento if lento else "sin freno"
        if "escena audio" not in reg: print(reg[-600:])
        verificar("escena audio" in reg, "AoEN (%s): la escena de audio corre" % etiqueta)
        m = re.search(r"audio: (\d+) cuadros en (\d+) bloques, (\d+) efectos, (\d+) perdidos, hueco máximo (\d+) ms", reg)
        verificar(m is not None, "AoEN (%s): informa de lo mezclado" % etiqueta)
        frames, bloques, efectos, perdidos, hueco = [int(x) for x in m.groups()]
        verificar(abs(frames - seg * RATE) < 0.15 * seg * RATE, "AoEN (%s): se mezclaron %.1f s de audio (toca %d s, por el reloj)" % (etiqueta, frames / RATE, seg))
        verificar(efectos >= seg - 2 and perdidos == 0, "AoEN (%s): llegaron %d alarmas de las ~%d disparadas, sin perder ninguna" % (etiqueta, efectos, seg))
        verificar(hueco < 400, "AoEN (%s): el mayor hueco entre mezclas fue de %d ms (la parada de 400 ms de «carga» no corta el audio)" % (etiqueta, hueco))
        n, izq, _ = leer_wav(wav)
        verificar(n >= 5 * RATE, "AoEN (%s): el WAV grabado dura %.1f s" % (etiqueta, n / RATE))
        # música continua: en ninguna ventana de 50 ms (salvo la primera) el volumen se va a cero
        mudas = 0
        v = RATE // 20
        for i in range(v * 4, n - v, v):
            e = sum(abs(x) for x in izq[i:i + v]) / v
            if e < 50: mudas += 1
        verificar(mudas == 0, "AoEN (%s): la música no se corta (%d ventanas de 50 ms en silencio)" % (etiqueta, mudas))


def saguios():
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    from h70_sonido import ler_gravacao, trechos
    import h136_opentyrian as h136
    import time
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta «saguios»")
        return
    jogos = tempfile.mkdtemp(prefix="h431-jogos-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")], check=True, stdout=subprocess.DEVNULL)
    os.makedirs(os.path.join(jogos, "AOE", "MUSIC"))
    midi_sintetico(os.path.join(jogos, "AOE", "MUSIC", "open.mid"))
    kvm = os.environ.get("SAGUI_KVM") == "1"
    seg = 8
    patron = r"\[AOEN\] audio: (\d+) cuadros en (\d+) bloques, (\d+) efectos, (\d+) perdidos, hueco m\S+ximo (\d+) ms, sin datos (\d+) veces"
    for nombre, cpus, lento in (("1 CPU, sin freno", 1, 0), ("1 CPU, cuadros de 150 ms", 1, 150), ("4 CPU, cuadros de 150 ms", 4, 150)):
        opciones = ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"] + (["--smp"] if cpus > 1 else [])
        img = imagem_de_teste("h431-%d-%d" % (cpus, lento), opciones)
        with Maquina("h431-%d-%d" % (cpus, lento), "ide", imagem=img, memoria="512M", nucleos=cpus, som="hda") as m:
            esperar(m, r"sagui:/\$", 200)
            time.sleep(4)
            h136.focar_terminal(m)
            marca = len(m.serial())
            m.digitar("AOEN_SCENE=audio AOEN_AUDIO_SEG=%d AOEN_MUSIC=/jogos/AOE/MUSIC AOEN_FPS_LENTO=%d AOEN_FPS_OCUPADO=1 /apps/bin/aoen; echo SALIDA-$?" % (seg, lento), 0.03)
            m.tecla("ret")
            g = esperar(m, patron, (200 if kvm else 600), desde=marca)
            reg = m.serial()[marca:]
            sal = esperar(m, r"SALIDA-(\d+)", 60, desde=marca)   # el juego sale: el hilo de audio termina y vuelve el indicador
            verificar(sal == "0", "SaguiOS (%s): AoEN sale limpio (código 0, el hilo de audio termina)" % nombre)
            time.sleep(2)
            frames, bloques, efectos, perdidos, hueco, vacios = [int(x) for x in g]
            etiqueta = "SaguiOS (%s)" % nombre
            verificar("hilo propio, con tarjeta" in reg, "%s: el hilo propio abre la tarjeta (audio_abrir) y escribe sin errores" % etiqueta)
            verificar("EXCECAO" not in m.serial(), "%s: sin excepciones del kernel" % etiqueta)
            verificar(abs(frames - seg * 44100) < 0.2 * seg * 44100, "%s: se mezclaron %.1f s de audio (toca %d s)" % (etiqueta, frames / 44100.0, seg))
            verificar(efectos >= seg - 3 and perdidos == 0, "%s: llegaron %d alarmas, sin perder ninguna" % (etiqueta, efectos))
            verificar(vacios == 0, "%s: la tarjeta no se quedó sin datos (%d veces; hueco máximo %d ms)" % (etiqueta, vacios, hueco))
        esq, dir_ = ler_gravacao(m.wav)
        tr = trechos(esq, dir_)   # une los huecos de hasta 50 ms: un hueco mayor parte el trozo
        print("  trechos:", [(a / 48000.0, b / 48000.0) for a, b in tr])
        verificar(len(tr) == 1, "%s: el .wav de QEMU es un solo trozo, sin silencios de más de 50 ms (%d trozos)" % (etiqueta, len(tr)))
        dur = (tr[0][1] - tr[0][0]) / 48000.0 if tr else 0
        verificar(seg - 1.5 <= dur <= seg + 3, "%s: el .wav suena %.1f s (toca %d s)" % (etiqueta, dur, seg))


def gpu():
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    import h436_aoen_gpu as h436
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta «gpu»")
        return
    jogos = h436.jogos_sinteticos()
    os.makedirs(os.path.join(jogos, "AOE", "MUSIC"))
    midi_sintetico(os.path.join(jogos, "AOE", "MUSIC", "open.mid"))
    kvm = os.environ.get("SAGUI_KVM") == "1"
    img = imagem_de_teste("h431-gpu", ["--vesa", "--gpu3d=simulado", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h431-gpu", "ide", imagem=img, memoria="512M", som="hda") as m:
        h436.arrancar(m)
        m.comando("echo 'inicio 100c 96' > /sistema/conf/gpu3d-simulado.conf")
        # Amilo, 2026-11-21: ceder dentro de una copia con SSE del kernel daba #NM (pánico). «inicio 1010 1»: cada lectura del
        # cuadro copia con SSE2 llamando a gpu3d_respirar en medio, forzado a ceder: no debe ceder ahí (ni caerse)
        m.comando("echo 'inicio 1010 1' >> /sistema/conf/gpu3d-simulado.conf")
        cuadros = 40 if kvm else 25
        g, s = h436.correr(m, "AOEN_FULL=1 AOEN_SCENE=game AOEN_PLAYERS=2 AOEN_SIZE=2 AOEN_SEED=7 AOEN_GPU=1 SDL_SAGUI_GPU=1 SDL_SAGUI_GPU_TIEMPOS=1 "
                           "AOEN_TEX=/jogos/AOE/TEX AOEN_MUSIC=/jogos/AOE/MUSIC AOEN_BENCH=%d /apps/bin/aoen" % cuadros,
                           r"\[AOEN\] audio: kernel: latencia (\d+) ms, (\d+) cortes de la tarjeta; por delante (\d+) ms; alta prioridad (\S+)",
                           900)
        verificar("[GPU3D] simulado: GPU lenta de mentira, 150 ms por espera" in m.serial(), "el driver simulado hace de GPU lenta")
        verificar("[AOEN] dibujo por GPU (D436)" in s and "dibujo por GPU apagado" not in s, "AoEN dibuja por la GPU (simulada y lenta)")
        b = re.search(r"\[AOEN\] bench \d+x\d+ cuadros=(\d+) ms=(\d+) fps=([\d.]+)", s)
        verificar(b is not None, "la partida termina (banco de %d cuadros)" % cuadros)
        print("    fps con la GPU lenta: %s" % b.group(3))
        verificar(float(b.group(3)) < 8.0, "pocos fps: la GPU lenta manda (%s fps)" % b.group(3))
        a = re.search(r"\[AOEN\] audio: (\d+) cuadros en (\d+) bloques, (\d+) efectos, (\d+) perdidos, hueco m\S+ximo (\d+) ms, sin datos (\d+) veces", s)
        verificar(a is not None, "AoEN da el resumen del audio")
        lat, cortes, delante, prio = int(g[0]), int(g[1]), int(g[2]), g[3]
        print("    audio: %s; kernel: latencia %d ms, %d cortes, por delante %d ms, alta prioridad %s" % (a.group(0), lat, cortes, delante, prio))
        verificar(cortes == 0 and lat == 100, "la tarjeta no corta con esperas de la GPU de 150 ms (%d cortes, latencia %d ms)" % (cortes, lat))
        verificar(int(a.group(6)) == 0, "el flujo de AoEN no se queda sin datos (%s veces; hueco máximo %s ms)" % (a.group(6), a.group(5)))
        verificar(prio.startswith("s"), "el hilo de audio tiene la alta prioridad (de serie con el dibujo por GPU)")
        verificar(re.search(r"\[PLANIFICADOR\] hilo \d+ \(\S*\): alta prioridad de audio", m.serial()), "el kernel da la alta prioridad de audio")
        c = re.search(r"las esperas de la GPU cedieron la CPU (\d+) veces", s)
        verificar(c is not None and int(c.group(1)) > 0, "las esperas de la GPU ceden la CPU (%s veces)" % (c.group(1) if c else "?"))
        verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial() and "Coprocesador" not in m.serial(), "sin excepciones ni pánicos")
        f = re.search(r"(\d+) dentro de una copia con SSE sin ceder", s)
        verificar("una espera dentro de una copia con SSE: no se cede la CPU" in m.serial() and f and int(f.group(1)) > 0,
                  "dentro de una copia con SSE del kernel no se cede la CPU (%s veces; con lecturas forzadas)" % (f.group(1) if f else "?"))
        # sin el dibujo por GPU también se puede pedir (AOEN_AUDIO_PRIO=1, como la casilla de Opciones > Sonido)
        g2, s2 = h436.correr(m, "AOEN_SCENE=audio AOEN_AUDIO_SEG=4 AOEN_AUDIO_PRIO=1 /apps/bin/aoen", r"\[AOEN\] audio: kernel: latencia (\d+) ms, (\d+) cortes", 300)
        verificar("alta prioridad del hilo encendida" in s2, "AOEN_AUDIO_PRIO=1 la enciende también por la CPU")


def fuentes():
    from qemu import verificar
    leer = lambda p: open(os.path.join(RAIZ, p), encoding="utf-8").read()
    a = leer("userland/juegos/aoen/audio.c")
    verificar("SDL_QueueAudio" not in a and "SDL_LockAudioDevice" not in a, "audio.c no empuja audio por cuadro ni cierra el dispositivo")
    verificar("mezcla_voz" in a and "audio_escrever" in a and "mezcla_render" in a, "audio.c dispara por la cola y mezcla en su hilo")
    m = leer("userland/juegos/aoen/music.c")
    verificar("audio_lock" not in m and "enviar_pista" in m, "la música cambia de pista por un buzón, sin cerrojos")
    verificar(os.path.exists(os.path.join(AOEN, "mezcla.h")) and "SDL" not in leer("userland/juegos/aoen/mezcla.c"), "mezcla.c no depende de la SDL")
    d = leer("docs/decisoes.md")
    verificar("## D431." in d, "D431 está en docs/decisoes.md")
    doc = leer("docs/aoen.md")
    verificar("AOEN_FPS_LENTO" in doc and "AOEN_AUDIO_WAV" in doc, "docs/aoen.md documenta AOEN_FPS_LENTO y AOEN_AUDIO_WAV")


if __name__ == "__main__":
    partes = sys.argv[1:] or ["mezcla", "juego", "saguios", "gpu", "fuentes"]
    for p in partes:
        print("== " + p)
        globals()[p]()
    print("H431 ok")
