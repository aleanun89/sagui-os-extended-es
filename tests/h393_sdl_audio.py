#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H393 (D393) - el audio de la SDL 2 del sistema va en un hilo propio (v8.5.5.3, 2026-11-22).

Hasta ahora el audio se movía desde el hilo principal (SDL_PollEvent, SDL_Delay, SDL_RenderPresent): un juego atascado 400 ms
en un cuadro (Brumacanto al hacer trozos nuevos) dejaba la tarjeta sin datos. Ahora SDL_PauseAudio(0) arranca un hilo de audio.
Todo se prueba DENTRO de SaguiOS (QEMU con tarjeta HDA y el audio de QEMU a un .wav) con /apps/bin/sdl2audio
(userland/libsdl2/sdl2audioprova.c), que se queda ocupado 400 ms cada segundo sin llamar a la SDL.

Partes (por defecto todas; o las que se pidan: h393_sdl_audio.py fuentes callback cola formato):
  fuentes    el hilo, la variable de vuelta atrás, los cerrojos del dispositivo y el cerrojo de SDL_mixer, la decisión D393.
  callback   seno por la llamada de vuelta con el principal ocupado 400 ms: con el hilo, el .wav es UN solo trozo (ningún
             silencio de más de 50 ms) y la llamada de vuelta corre en otro hilo; con SDL_SAGUI_AUDIO_HILO=0 (el modo de antes)
             el .wav se parte (los silencios de los bloqueos).
  cola       lo mismo por SDL_QueueAudio con ~700 ms de adelanto en la cola; la cola se vacía y SDL_ClearQueuedAudio la limpia.
  formato    F32 mono sin permitir cambios (como Brumacanto): el .wav suena a 880 Hz y con el nivel de 0,5 de escala, no a
             ruido (la SDL convierte al formato del sistema), con el hilo y sin él.
  cerrojo    SDL_LockAudioDevice excluye a la llamada de vuelta (ninguna vez la ve a medias); SDL_PauseAudio la calla y la
             devuelve; y sin tarjeta de sonido (mudo) la llamada de vuelta sigue yendo al ritmo del reloj, en el hilo.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste, RAIZ, APPS_RAIZ  # noqa: E402

TAXA = 48000


def leer(ruta):
    return open(os.path.join(RAIZ, ruta), encoding="utf-8").read()


def fuentes():
    s = leer("userland/libsdl2/sdl2_sagui.c")
    verificar("audio_hilo_fn" in s and "thread_create(audio_hilo_fn" in s, "la SDL 2 arranca un hilo de audio")
    verificar("SDL_SAGUI_AUDIO_HILO" in s and "SDL_SAGUI_AUDIO_PRIO" in s, "variables de vuelta atrás y de la prioridad")
    verificar("planificador_hilo_audio(1)" in s and "audio_prio_deseada" in s, "el hilo puede pedir la alta prioridad de audio (PLAN_AUDIO), opcional")
    verificar("void SDL_LockAudio(void) { if (audio_cerrojo)" in s, "SDL_LockAudio es un cerrojo de verdad")
    verificar("SDL_LockMutex(audio_cerrojo);" in s and "audio_spec.callback(audio_spec.userdata" in s,
              "la llamada de vuelta va con el cerrojo del dispositivo tomado")
    verificar("audio_conv" in s and "SDL_AUDIO_ALLOW_FORMAT_CHANGE" in s, "conversión de formato (F32, S32, U16, S8)")
    m = leer("userland/libsdl2/sdl2_mixer.c")
    verificar(m.count("SDL_LockAudio()") >= 15, "SDL_mixer toma el cerrojo del dispositivo al cambiar el estado")
    d = leer("docs/decisoes.md")
    verificar("hilo de audio de la SDL 2" in d, "apartado del hilo de audio en D393")
    verificar("tests/h393_sdl_audio.py" in d, "la decisión nombra la prueba")
    mk = leer("userland/libsdl2/teste.mk")
    verificar("sdl2audio" in mk, "el programa de prueba sdl2audio está en teste.mk")
    for ruta in ("userland/libsdl2/sdl2audioprova.c", "userland/libsdl2/sdl2_sagui.c", "userland/libsdl2/sdl2_mixer.c"):
        verificar(not re.search("[\u0370-\u04ff\u3000-\u9fff]", leer(ruta)), "sin otros alfabetos en " + os.path.basename(ruta))


def ejecutar(nombre, orden, tiempo, som="hda", entorno=""):
    """Arranca SaguiOS, ejecuta sdl2audio y devuelve (línea SDLAUDIO, serial, ruta del .wav)."""
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "sdl2audio")):
        raise FalhaTeste("la imagen no lleva sdl2audio (make)")
    img = imagem_de_teste("h393-" + nombre, ["--vesa", "--console", "--apps", APPS_RAIZ])
    with Maquina("h393-" + nombre, "ide", imagem=img, memoria="256M", som=som) as m:
        m.esperar_serial("sagui:/$", 150)
        antes = len(m.serial())
        out = m.comando("%s/apps/bin/sdl2audio %s" % (entorno, orden), tiempo)
        reg = m.serial()[antes:]
        linea = re.search(r"SDLAUDIO modo=\S+ .*", out + reg)
        verificar(linea is not None, "[%s] sdl2audio escribe su línea (%s)" % (nombre, out.strip()[-200:]))
        verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "[%s] sin excepciones del kernel" % nombre)
        return linea.group(0), reg, m.wav


def valores(linea):
    return {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", linea)}


def trozos(wav):
    from h70_sonido import ler_gravacao, trechos
    esq, dir_ = ler_gravacao(wav)
    return esq, dir_, trechos(esq, dir_)   # une los silencios de hasta 50 ms: uno mayor parte el trozo


def sonido(nombre, orden, seg, hilo, bloqueo):
    ent = "" if hilo else "SDL_SAGUI_AUDIO_HILO=0 "
    linea, reg, wav = ejecutar("%s-%d" % (nombre, hilo), orden, 60 + 3 * seg, entorno=ent)
    v = valores(linea)
    esq, dir_, tr = trozos(wav)
    etiqueta = "%s, %s" % (nombre, "con el hilo" if hilo else "sin el hilo (modo de antes)")
    print("  [%s] %s; trozos: %s" % (etiqueta, linea, [(round(a / TAXA, 2), round(b / TAXA, 2)) for a, b in tr]))
    verificar(v["bloqueos"] >= seg - 3, "[%s] el principal se atascó %d veces %d ms" % (etiqueta, v["bloqueos"], bloqueo))
    if hilo:
        verificar("audio: hilo propio" in reg, "[%s] la SDL arranca su hilo de audio" % etiqueta)
        verificar(re.search(r"audio: hilo parado; \d+ bloques, sin datos 0 veces", reg) is not None,
                  "[%s] el hilo termina limpio y el flujo no se quedó sin datos" % etiqueta)
        verificar(len(tr) == 1, "[%s] el .wav es un solo trozo (%d: ningún silencio de más de 50 ms)" % (etiqueta, len(tr)))
        dur = (tr[0][1] - tr[0][0]) / TAXA if tr else 0
        verificar(seg - 2 <= dur <= seg + 3, "[%s] el .wav suena %.1f s (toca %d s)" % (etiqueta, dur, seg))
    else:
        verificar("audio: hilo propio" not in reg, "[%s] sin hilo propio" % etiqueta)
        verificar(len(tr) >= 2, "[%s] el .wav se parte (%d trozos): los atascos del principal dejan la tarjeta sin datos" % (etiqueta, len(tr)))
    return v, esq, dir_, tr


def callback():
    v, _, _, _ = sonido("callback", "callback 8 400", 8, 1, 400)
    verificar(v["en_otro_hilo"] == 1, "la llamada de vuelta corre en otro hilo que el principal")
    verificar(v["llamadas"] > 8 * TAXA // 1024 * 0.6, "la llamada de vuelta se llamó al ritmo de la tarjeta (%d veces)" % v["llamadas"])
    sonido("callback", "callback 8 400", 8, 0, 400)


def cola():
    v, _, _, _ = sonido("cola", "cola 8 400", 8, 1, 400)
    verificar(v["cola_tras_limpiar"] == 0, "SDL_ClearQueuedAudio vacía la cola")
    sonido("cola", "cola 8 400", 8, 0, 400)


def formato():
    from h70_sonido import dominante, rms
    for hilo in (1, 0):
        ent = "" if hilo else "SDL_SAGUI_AUDIO_HILO=0 "
        linea, reg, wav = ejecutar("formato-%d" % hilo, "formato 6 0", 120, entorno=ent)
        esq, dir_, tr = trozos(wav)
        verificar(len(tr) >= 1, "[formato, hilo=%d] hay sonido en el .wav" % hilo)
        a, b = max(tr, key=lambda t: t[1] - t[0])
        centro = slice(a + (b - a) // 4, b - (b - a) // 4)
        f = dominante(esq[centro])
        nivel = rms(esq[centro])
        esperado = 0.5 * 32767 / 2 ** 0.5
        print("  [formato, hilo=%d] %d Hz, RMS %.0f (esperado %.0f)" % (hilo, f, nivel, esperado))
        verificar(abs(f - 880) <= 8, "[formato, hilo=%d] F32 mono suena a 880 Hz (%d Hz): convertido, no ruido" % (hilo, f))
        verificar(0.85 * esperado <= nivel <= 1.15 * esperado, "[formato, hilo=%d] con su nivel (RMS %.0f, esperado %.0f)" % (hilo, nivel, esperado))
        verificar("convertido desde el formato del programa" in reg, "[formato, hilo=%d] el registro dice que se convierte" % hilo)


def cerrojo():
    linea, reg, _ = ejecutar("cerrojo", "cerrojo 4", 90, som=None)
    v = valores(linea)
    verificar(v["difs"] == 0, "SDL_LockAudioDevice excluye a la llamada de vuelta (%d veces a medias)" % v["difs"])
    verificar(v["llamadas"] > 20, "la llamada de vuelta corre sin tarjeta al ritmo del reloj (%d llamadas en 4 s, mudo)" % v["llamadas"])
    verificar(v["en_otro_hilo"] == 1, "sin tarjeta también en el hilo de audio")
    # la alta prioridad (PLAN_AUDIO) es opcional: de serie no se pide (con ella Freeciv dejaba de contestar al servidor)
    verificar("[PLANIFICADOR]" not in reg or "alta prioridad de audio" not in reg, "de serie el hilo no pide la alta prioridad")
    linea, reg, _ = ejecutar("prio", "cerrojo 2", 90, som=None, entorno="SDL_SAGUI_AUDIO_PRIO=1 ")
    verificar(re.search(r"\[PLANIFICADOR\] hilo \d+ \(\S*\): alta prioridad de audio", reg) is not None,
              "SDL_SAGUI_AUDIO_PRIO=1 pide la alta prioridad de audio")
    linea, _, _ = ejecutar("pausa", "pausa", 90, som=None)
    verificar(valores(linea)["pausa_ok"] == 1, "SDL_PauseAudio(1) calla la llamada de vuelta y SDL_PauseAudio(0) la devuelve")


PARTES = {"fuentes": fuentes, "callback": callback, "cola": cola, "formato": formato, "cerrojo": cerrojo}

if __name__ == "__main__":
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        print("== " + p)
        PARTES[p]()
    print("H393 BIEN")
