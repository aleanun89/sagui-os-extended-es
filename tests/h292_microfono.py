#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H292 (D292, mejora 075 punto 3) - micrófono y grabación, balance y latencia.

QEMU 8.2 no sabe inyectar audio de entrada: el backend `wav` solo graba la salida (la tarjeta
queda sin flujo de entrada y su posición no avanza) y `-audiodev none` hace que la tarjeta
emulada grabe silencio, pero avanzando de verdad. Por eso la prueba se reparte así:

  hda, ac97   Intel HDA (hda-duplex) y AC'97 con `-audiodev none`: el kernel registra la entrada;
              `grabar --info` lista las entradas; la grabación de la tarjeta avanza al ritmo del
              reloj (el flujo de DMA del HDA y del AC'97 funciona: ~24000 cuadros en 500 ms) y sale
              en silencio, bit a bit (md5 del WAV completo, con su cabecera, calculado aquí); la
              fuente de prueba del kernel (un tono de 1 kHz que pasa por la misma cola y la misma
              conversión) sale idéntica a lo esperado a 48 kHz estéreo (md5), a 8 kHz exacta
              muestra a muestra y a 44,1 kHz con la frecuencia y el nivel correctos; ganancia,
              silencio y entrada se cambian y se leen; otra aplicación no puede grabar a la vez
              (ERRO_OCUPADO) y, si el dueño muere, el micrófono queda libre; un servicio
              (cron) no puede grabar (como la cámara, D277).
  sin         sin tarjeta de sonido: `grabar` dice que no hay micrófono, la fuente de prueba sí
              sirve y nada se cuelga.
  balance     Intel HDA grabando la salida: `configuracion balance` atenúa el canal contrario (-100:
              el derecho calla; +50: el izquierdo a la mitad) y `configuracion latencia` se anota.
  conectores  `sonido --guardar` anota por modelo (códec + subsistema) lo que se prueba a mano y el arranque lo
              aplica (`sonido --aplicar`, desde `configuracion --aplicar`); la salida elegida se recuerda.
  ventana     la Grabadora (fuente de prueba: medidor y botón Grabar) y Configuración > Sonido
              (micrófono, balance, latencia) con capturas H292-*.png.

Lo que no se puede probar sin micrófono de verdad (nivel real, ganancia del códec, el micrófono
interno de un portátil) está en docs/microfono.md como lista de pruebas en hardware real.

Uso: h292_microfono.py [hda] [ac97] [sin] [balance] [conectores] [ventana]   (sin argumentos, todo)
"""
import hashlib
import math
import os
import re
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402
import h70_sonido as h70  # noqa: E402
import h83_configuracion as h83  # noqa: E402

TONO = [0, 1044, 2071, 3061, 4000, 4870, 5657, 6347, 6928, 7391, 7727, 7932, 8000, 7932, 7727, 7391, 6928, 6347,
        5657, 4870, 4000, 3061, 2071, 1044, 0, -1044, -2071, -3061, -4000, -4870, -5657, -6347, -6928, -7391,
        -7727, -7932, -8000, -7932, -7727, -7391, -6928, -6347, -5657, -4870, -4000, -3061, -2071, -1044]


def wav(taxa, canais, datos):
    cab = b"RIFF" + struct.pack("<I", 36 + len(datos)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, canais, taxa, taxa * canais * 2, canais * 2, 16)
    return cab + b"data" + struct.pack("<I", len(datos)) + datos


def extras_audio(placa):
    if placa == "hda":
        return ["-audiodev", "none,id=aud0", "-device", "intel-hda", "-device", "hda-duplex,audiodev=aud0"]
    return ["-audiodev", "none,id=aud0", "-device", "AC97,audiodev=aud0"]


def md5_guest(m, ruta):
    out = m.comando(f"md5sum {ruta}")
    r = re.search(r"([0-9a-f]{32})", out)
    return r.group(1) if r else None


def muestras_guest(m, ruta):
    """Las muestras de 16 bits de un WAV del invitado (cabecera de 44 bytes), por xxd -p."""
    out = m.comando(f"xxd -p {ruta}", 120)
    hexa = "".join(l.strip() for l in out.splitlines() if re.fullmatch(r"[0-9a-f]{2,60}", l.strip()) and len(l.strip()) % 2 == 0)
    datos = bytes.fromhex(hexa)[44:]
    return list(struct.unpack("<%dh" % (len(datos) // 2), datos[:len(datos) // 2 * 2]))


def goertzel(x, f, taxa):
    w = 2 * math.pi * f / taxa
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for v in x:
        s1, s2 = v + c * s1 - s2, s1
    return s1 * s1 + s2 * s2 - c * s1 * s2


def tarjeta(placa):
    with Maquina(f"h292-{placa}", "ide", imagem=imagem_console(), extras=extras_audio(placa)) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[CAPTURA] " in s and "entrada(s)" in s, f"[{placa}] el kernel registra la entrada de sonido")
        out = m.comando("grabar --info")
        verificar("Tarjeta:" in out and "Ganancia: 70 %" in out, f"[{placa}] grabar --info: tarjeta y ganancia")
        if placa == "hda":
            verificar("* 0: Entrada de l" in out, "[hda] el códec de QEMU ofrece su entrada de línea")
        else:
            verificar("* 0: Micr" in out and "1: Entrada de l" in out and "2: Auxiliar" in out, "[ac97] micrófono, línea y auxiliar")

        # fuente de prueba: 48 kHz estéreo, bit a bit
        n = 4800
        esp = wav(48000, 2, b"".join(struct.pack("<hh", TONO[i % 48], TONO[i % 48]) for i in range(n)))
        out = m.comando("grabar -p -r 48000 -e -t 100 /tmp/p48.wav")
        verificar("Listo" in out, f"[{placa}] grabar -p graba el tono de prueba ({out.strip()[-80:]})")
        verificar(md5_guest(m, "/tmp/p48.wav") == hashlib.md5(esp).hexdigest(),
                  f"[{placa}] 48 kHz estéreo: el WAV es idéntico al esperado (md5, {len(esp)} bytes)")
        # 8 kHz mono: paso entero (6 cuadros), muestra a muestra
        m.comando("grabar -p -r 8000 -t 200 /tmp/p8.wav")
        x = muestras_guest(m, "/tmp/p8.wav")
        verificar(len(x) == 1600 and all(x[k] == TONO[(6 * k) % 48] for k in range(len(x))),
                  f"[{placa}] 8 kHz mono: cada muestra es la 6.a del tono ({len(x)} muestras)")
        # 44,1 kHz mono: interpolación lineal
        m.comando("grabar -p -t 100 /tmp/p44.wav")
        x = muestras_guest(m, "/tmp/p44.wav")
        pico = max(abs(v) for v in x)
        rms = math.sqrt(sum(v * v for v in x) / len(x))
        verificar(len(x) == 4400 and 7800 <= pico <= 8100, f"[{placa}] 44,1 kHz: 4400 muestras, pico {pico}")
        verificar(5400 <= rms <= 5900, f"[{placa}] 44,1 kHz: nivel RMS {rms:.0f} (esperado 5657)")
        g = {f: goertzel(x, f, 44100) for f in (800, 900, 1000, 1100, 1200)}
        verificar(max(g, key=g.get) == 1000, f"[{placa}] 44,1 kHz: la frecuencia dominante es 1000 Hz")

        # la tarjeta de verdad: avanza y graba silencio
        antes = len(m.serial())
        out = m.comando("grabar /tmp/m.wav -r 48000 -t 500", 60)
        verificar("Listo" in out and "pico 0 %" in out, f"[{placa}] la grabación de la tarjeta termina ({out.strip()[-80:]})")
        mm = re.search(r"\[CAPTURA\] fechada; (\d+) cuadros, (\d+) perdidos", m.serial()[antes:])
        verificar(mm and 22000 <= int(mm.group(1)) <= 28000 and int(mm.group(2)) == 0,
                  f"[{placa}] el DMA de entrada avanza al ritmo del reloj ({mm.group(1) if mm else '?'} cuadros en 500 ms, sin pérdidas)")
        sil = wav(48000, 1, bytes(48000))
        verificar(md5_guest(m, "/tmp/m.wav") == hashlib.md5(sil).hexdigest(), f"[{placa}] -audiodev none graba silencio exacto (md5)")

        # entrada, ganancia y silencio
        out = m.comando("grabar -g 30 --silencio --info")
        verificar("Ganancia: 30 % (en silencio)" in out, f"[{placa}] ganancia 30 % y silencio")
        out = m.comando("grabar --activar --info")
        verificar("Ganancia: 30 %" in out and "(en silencio)" not in out, f"[{placa}] activar quita el silencio")
        if placa == "ac97":
            out = m.comando("grabar -i 2 --info")
            verificar("* 2: Auxiliar" in out, "[ac97] elegir la entrada auxiliar")
            m.comando("grabar -i 0")
        out = m.comando("grabar -i 9 --info")
        verificar("no existe la entrada 9" in out, f"[{placa}] una entrada inexistente se rechaza")
        m.comando("grabar -g 70")
        # silencio por software: con el micrófono en silencio, la fuente real graba ceros aunque entrara sonido
        # (aquí ya son ceros) y la fuente de prueba no se ve afectada por el silencio de la tarjeta
        # (el silencio vale para todo lo que sale por la cola, también la prueba)
        m.comando("grabar --silencio")
        m.comando("grabar -p -r 48000 -t 100 /tmp/ps.wav")
        x = muestras_guest(m, "/tmp/ps.wav")
        verificar(len(x) == 4800 and all(v == 0 for v in x), f"[{placa}] con el micrófono en silencio también la prueba sale a cero")
        m.comando("grabar --activar")

        # uso exclusivo y limpieza si el dueño muere
        m.comando("grabar -p /tmp/larga.wav 60 &")
        fim = time.time() + 20
        while "[CAPTURA] abierta" not in m.serial()[antes:] and time.time() < fim:
            time.sleep(0.3)
        time.sleep(1.0)
        out = m.comando("grabar /tmp/otra.wav 1")
        verificar("otro programa est" in out, f"[{placa}] una segunda grabación recibe «otro programa está grabando»")
        out = m.comando("ps")
        mp = re.search(r"^\s*(\d+)\s+\d+\s+\S+\s+\d+ KB\s+\d+\s+grabar\s*$", out, re.M)
        verificar(bool(mp), f"[{placa}] la grabación larga sigue en marcha")
        m.comando(f"kill {mp.group(1)}")
        time.sleep(1.0)
        out = m.comando("grabar -p -t 100 /tmp/x.wav")
        verificar("Listo" in out, f"[{placa}] al morir el dueño el micrófono queda libre")

        if placa == "hda":
            # un servicio (cron) no puede grabar
            m.comando("echo '* * * * * grabar -p -t 100 /tmp/c.wav > /tmp/cron.out 2>&1' > /tmp/tabla")
            out = m.comando("crontab /tmp/tabla")
            verificar("tabla instalada" in out, "[hda] crontab instala la tarea de grabar")
            fim = time.time() + 130
            r = ""
            while time.time() < fim:
                r = m.comando("cat /tmp/cron.out")
                if "servicios" in r:
                    break
                time.sleep(5)
            verificar("los servicios no pueden grabar" in r, f"[{placa}] un servicio (cron) no puede grabar ({r.strip()[-80:]})")
            m.comando("crontab -r")
        s = m.serial()
        verificar("Page fault" not in s and "PANIC" not in s.upper(), f"[{placa}] ningún fallo del kernel")


def sin_tarjeta():
    with Maquina("h292-sin", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("grabar --info")
        verificar("no hay entrada de sonido" in out, "sin tarjeta: grabar --info dice que no hay micrófono")
        out = m.comando("grabar /tmp/x.wav 1")
        verificar("no hay micr" in out, "sin tarjeta: grabar archivo.wav dice que no hay micrófono")
        out = m.comando("grabar -p -t 100 /tmp/x.wav")
        verificar("Listo" in out, "sin tarjeta, la fuente de prueba sí graba")
        out = m.comando("configuracion microfono silencio")
        verificar("no hay entrada de sonido" in out, "configuracion microfono sin tarjeta lo dice")
        verificar("sagui:" in m.serial()[-200:], "el sistema sigue respondiendo")


def balance():
    extras = []
    with Maquina("h292-bal", "ide", imagem=imagem_console(), som="hda", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("grabar -p -r 48000 -e -t 1500 /tmp/t.wav")
        m.comando("reproducir /tmp/t.wav", 60)
        time.sleep(0.5)
        out = m.comando("configuracion balance -100")
        verificar("[AUDIO] balance 0" in m.serial(), "configuracion balance -100 llega al kernel (0)")
        m.comando("reproducir /tmp/t.wav", 60)
        time.sleep(0.5)
        m.comando("configuracion balance 50")
        verificar("[AUDIO] balance 150" in m.serial(), "configuracion balance 50 llega al kernel (150)")
        m.comando("reproducir /tmp/t.wav", 60)
        time.sleep(0.5)
        out = m.comando("configuracion mostrar")
        verificar("Balance:   50" in out, f"configuracion mostrar da el balance ({out.strip()[-120:]})")
        m.comando("configuracion balance 0")
        m.comando("configuracion latencia 150")
        verificar("[AUDIO] latencia 150 ms" in m.serial(), "configuracion latencia 150 llega al kernel")
        out = m.comando("configuracion mostrar")
        verificar("latencia 150 ms" in out, "configuracion mostrar da la latencia")
        out = m.comando("infopc -t sistema")
        verificar("150" in out, "infopc ve la latencia nueva")
    esq, dir_ = h70.ler_gravacao(m.wav)
    tr = [h70.Trecho(esq, dir_, a, b) for a, b in h70.trechos(esq, dir_)]
    print("  trechos:", tr)
    verificar(len(tr) >= 3, f"tres reproducciones en el WAV de salida ({len(tr)})")
    a, b, c = tr[-3:]
    verificar(0.9 <= a.rms_esq / a.rms_dir <= 1.1, f"balance centrado: los dos canales iguales ({a.rms_esq:.0f} / {a.rms_dir:.0f})")
    verificar(b.rms_dir < 0.02 * b.rms_esq and b.rms_esq > 1000, f"balance -100: el derecho calla ({b.rms_esq:.0f} / {b.rms_dir:.0f})")
    verificar(0.45 <= c.rms_esq / c.rms_dir <= 0.55, f"balance +50: el izquierdo a la mitad ({c.rms_esq:.0f} / {c.rms_dir:.0f})")


def conectores():
    """Ajustes de conectores guardados por modelo (sonido --guardar / --aplicar) y la salida recordada."""
    img = h83.imagem_persistente("h292-conectores", False)
    with Maquina("h292k", "ide", imagem=img, snapshot=False, extras=extras_audio("hda")) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("sonido")
        verificar("Autosilencio: si" in out, "de serie el autosilencio está activo")
        out = m.comando("sonido -n --guardar --auto off")
        verificar("Guardado en /home/.config/sonido.conf" in out and "auto=off" in out, f"--guardar anota el ajuste ({out.strip()[-100:]})")
        m.comando("sonido -n --guardar --auto off")
        out = m.comando("sonido -n --guardar --pin 3 on")
        verificar("Guardado" in out, "--guardar con --pin")
        out = m.comando("cat /home/.config/sonido.conf")
        verificar(re.search(r"\[[0-9a-f]{8}:[0-9a-f]{8}:[0-9a-f]{8}\]", out) and out.count("auto=off") == 1 and "pin=3,on" in out,
                  "sonido.conf: sección del modelo, con cada orden una sola vez")
        out = m.comando("sonido --guardados")
        verificar("auto=off" in out and "pin=3,on" in out, "sonido --guardados lista lo guardado")
        out = m.comando("configuracion salida 0")
        r = m.comando("cat /home/.config/sagui.conf")
        verificar("salida=" in r, f"configuracion salida 0 se guarda ({r.strip()[-80:]})")
        h83.reiniciar(m)
    with Maquina("h292k2", "ide", imagem=img, snapshot=False, extras=extras_audio("hda")) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("sonido: 2 ajuste(s) aplicados" in s, "el arranque aplica los ajustes de conectores guardados")
        verificar("configuracion: salida 'Intel HDA" in s and "' -> 0" in s, "el arranque vuelve a elegir la salida guardada")
        out = m.comando("sonido")
        verificar("Autosilencio: no" in out, "tras reiniciar el autosilencio sigue apagado (aplicado desde sonido.conf)")
        m.comando("sonido --olvidar")
        out = m.comando("sonido --guardados")
        verificar("nada" in out, "sonido --olvidar borra lo guardado del modelo")


def ventana():
    img = h83.imagem_persistente("h292-grafica", True)
    extras = extras_audio("hda")
    with Maquina("h292v", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("grabadora -p &")
        fim = time.time() + 40
        while "grabadora: lista" not in m.serial() and time.time() < fim:
            time.sleep(0.5)
        verificar("grabadora: lista" in m.serial(), "la Grabadora abre con la fuente de prueba")
        time.sleep(2.0)
        j = h83.janela(m.serial(), "Grabadora")
        ox, oy = j["x"] + 2, j["y"] + 24
        m.captura("H292-grabadora-1-medidor")
        m.clique(ox + 12 + 46, oy + 12 + 14)                   # Grabar
        time.sleep(2.5)
        m.captura("H292-grabadora-2-grabando")
        m.clique(ox + 12 + 46, oy + 12 + 14)                   # Parar
        time.sleep(0.8)
        m.clique(ox + 112 + 46, oy + 12 + 14)                  # Reproducir
        time.sleep(1.0)
        m.captura("H292-grabadora-3-reproduciendo")
        s = m.serial()
        verificar("[AUDIO] fluxo" in s and "44100 Hz, 1 canal" in s, "Reproducir abre un flujo de audio a 44100 Hz mono")
        verificar("Page fault" not in s, "sin fallos del kernel con la Grabadora")
    img = h83.imagem_persistente("h292-grafica2", True)
    with Maquina("h292c", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("configuracion &")
        fim = time.time() + 20
        while time.time() < fim and '"Configuración"' not in m.serial():
            time.sleep(0.3)
        time.sleep(1.0)
        j = h83.janela(m.serial(), "Configuración")
        ox, oy = j["x"] + 2, j["y"] + 24
        pos = h83.pos_paginas()
        m.clique(ox + pos[3][0] + pos[3][1] // 2, oy + 24)
        time.sleep(1.2)
        verificar("[CAPTURA] abierta" in m.serial(), "la pestaña Sonido abre la captura para el medidor")
        m.captura("H292-config-sonido-1")
        m.clique(ox + 290, oy + 170 + 12)                      # balance: cuarto izquierdo
        time.sleep(0.6)
        verificar("[AUDIO] balance 50" in m.serial(), "un clic en la barra de balance lo cambia (50)")
        m.clique(ox + 540 + 108, oy + 50 + 146 + 13)           # Silenciar micrófono
        time.sleep(0.6)
        m.clique(ox + 722 + 12, oy + 50 + 70 + 12)             # ganancia +
        time.sleep(0.6)
        m.captura("H292-config-sonido-2")
        m.clique(150, 12)                                      # Terminal
        time.sleep(0.6)
        out = m.comando("grabar --info")
        verificar("Ganancia: 75 % (en silencio)" in out, f"la Configuración cambió ganancia y silencio ({out.strip()[-60:]})")
        out = m.comando("cat /home/.config/sagui.conf")
        verificar("balance=50" in out and "micro_ganancia=75" in out and "micro_silencio=1" in out and "micro_entrada=0" in out,
                  "sagui.conf guarda balance y micrófono")
        h83.reiniciar(m)
    with Maquina("h292d", "ide", imagem=img, snapshot=False, extras=extras) as m2:
        m2.esperar_serial("sagui:/$", 120)
        s = m2.serial()
        verificar("configuracion: balance 50 -> 0" in s and "configuracion: micro entrada 0 ganancia 75 silencio 1 -> 0" in s,
                  "el arranque aplica el balance y el micrófono guardados")
        out = m2.comando("grabar --info")
        verificar("Ganancia: 75 % (en silencio)" in out, "tras reiniciar, ganancia y silencio siguen")


if __name__ == "__main__":
    partes = sys.argv[1:] or ["hda", "ac97", "sin", "balance", "conectores", "ventana"]
    for p in partes:
        print(f"== {p}")
        if p in ("hda", "ac97"):
            tarjeta(p)
        else:
            {"sin": sin_tarjeta, "balance": balance, "conectores": conectores, "ventana": ventana}[p]()
    print("H292: todo bien")
