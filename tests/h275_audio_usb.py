#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H275 (D275) - transferencias isócronas y audio USB (clase Audio 1.0).

QEMU con un altavoz USB (-device usb-audio, UAC 1, isócrono de salida) cuyo sonido
va a un .wav del anfitrión (-audiodev wav). Para cada controlador:
  xhci  - el altavoz en un xHCI (Isoch TRB): el kernel lo reconoce (descriptores de
          AudioControl/AudioStreaming), lo registra como tarjeta de sonido, `reproducir`
          toca un tono de 440 Hz y el .wav capturado trae esa frecuencia, su duración y
          el nivel del archivo; un estéreo de 44,1 kHz (600 Hz a la izquierda y 1500 a la
          derecha, con cambio de frecuencia en el mezclador); `volumen 50` da ~1/4 del
          nivel; `volumen silencio` calla (y manda el silencio a la unidad de función);
          `usb` y `volumen salida` informan de la tarjeta.
  companero - EHCI de ICH9 con UHCI compañeros: el altavoz de plena velocidad se cede al UHCI.
  uhci, ohci - lo mismo con el altavoz en un UHCI (TD isócronos en la lista de cuadros) y en un OHCI.
          (Con el EHCI no hay prueba en QEMU 8.2: el altavoz USB es de plena velocidad y el QEMU no
          lo deja colgar de un EHCI ni de un hub; el iTD de ehci.c queda por probar en hardware.)
  caliente - xHCI: se retira el altavoz con un WAV largo sonando (el sistema no se
          cuelga, `reproducir` termina, la tarjeta desaparece), se vuelve a enchufar y
          suena de nuevo.
  mezcla - HDA + altavoz USB a la vez: la salida es la USB (la última enchufada);
          `volumen salida N` pasa a la HDA sin cortar el flujo y después
          vuelve a la USB; cada tono sale en el .wav de su tarjeta.
Uso: h275_audio_usb.py [xhci] [uhci] [ohci] [companero] [caliente] [mezcla]   (sin argumentos, todo)
"""
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_pendrive, pendrive_qemu, SAIDA  # noqa: E402
from h70_sonido import (gerar_wav, ler_gravacao, trechos, Trecho, perto, rms_esperado,  # noqa: E402
                        goertzel, TAXA)

ARQUIVOS = {
    # nombre: (tasa, bits, canales, segundos, frecuencias, amplitud)
    "a440.wav": (48000, 16, 1, 1.0, [440], 0.5),
    "c_est.wav": (44100, 16, 2, 1.2, [600, 1500], 0.4),
    "d700.wav": (22050, 16, 1, 1.0, [700], 0.5),
    "e800.wav": (48000, 16, 1, 0.8, [800], 0.5),
    "h300.wav": (16000, 16, 1, 8.0, [300], 0.4),
    "f900.wav": (48000, 16, 1, 1.0, [900], 0.5),
}


def pen():
    arqs = {n: gerar_wav(*p) for n, p in ARQUIVOS.items()}
    return criar_pendrive("h275-pen", rotulo="USBAUDIO", arquivos=arqs)


def wav_de(nome):
    return os.path.join(SAIDA, f"h275-{nome}.wav")


def audiodev(nome):
    caminho = wav_de(nome)
    if os.path.exists(caminho):
        os.remove(caminho)
    return ["-audiodev", f"wav,id=ad-{nome},path={caminho},out.frequency=48000"]


def preparar(m):
    m.esperar_serial("sagui:/$", 180)
    m.comando("montar usb0p1")
    m.comando("mkdir /tmp/s")
    for nome in ARQUIVOS:
        m.comando(f"cp /usb/{nome} /tmp/s/{nome}", 120)


def trechos_de(caminho):
    esq, dir_ = ler_gravacao(caminho)
    return esq, dir_, [Trecho(esq, dir_, a, b) for a, b in trechos(esq, dir_)]


def reconhecer(m, etiqueta, ctl):
    s = m.serial()
    verificar(re.search(r"\[UAC\] .*salida PCM 16 bits estereo a 48000 Hz, endpoint 1", s),
              f"[{etiqueta}] el kernel reconoce el altavoz USB (UAC 1, 16 bits, 48 kHz)")
    verificar("[AUDIO] salida: " in s and "(usb)" in s, f"[{etiqueta}] el mezclador lo toma como salida de sonido")
    out = m.comando("volumen")
    verificar("Volumen: 100 %" in out and "(usb)" in out, f"[{etiqueta}] volumen muestra la tarjeta USB")
    out = m.comando("volumen salida")
    verificar(re.search(r"\* 0: .* \(usb\)", out), f"[{etiqueta}] volumen salida lista el altavoz USB como actual")
    out = m.comando("usb")
    verificar("Audio USB:" in out and ctl in out, f"[{etiqueta}] usb informa del audio por el {ctl}")


def tonos(m, etiqueta, canal):
    """El guion de reproducciones comun a xHCI y EHCI."""
    out = m.comando("reproducir /tmp/s/a440.wav", 60)
    verificar("48000 Hz, 16 bits, mono, 1,0 s" in out, f"[{etiqueta}] reproducir describe el WAV de 48 kHz")
    m.comando("reproducir /tmp/s/c_est.wav", 60)
    m.comando("volumen 50")
    m.comando("reproducir /tmp/s/d700.wav", 60)
    m.comando("volumen silencio")
    m.comando("reproducir /tmp/s/e800.wav", 60)
    out = m.comando("volumen 100")
    verificar("Volumen: 100 %" in out, f"[{etiqueta}] volumen 100 quita el silencio")
    s = m.serial()
    verificar("[UAC] silencio del hardware activado" in s and "[UAC] silencio del hardware quitado" in s,
              f"[{etiqueta}] el silencio también va a la unidad de función del altavoz")
    out = m.comando("usb")
    r = re.search(r"Audio USB: .*?; (\d+) paquetes, (\d+) completados, (\d+) huecos, (\d+) errores", out)
    verificar(r and int(r.group(1)) > 3000 and int(r.group(2)) > 3000 and int(r.group(4)) == 0,
              f"[{etiqueta}] el flujo isócrono: {r.groups() if r else out[-200:]} (paquetes, completados, huecos, errores)")


def comprobar_tonos(etiqueta, caminho):
    esq, dir_, tr = trechos_de(caminho)
    print(f"  [{etiqueta}] trechos:", tr)

    def achar(f, canal="esq"):
        return [t for t in tr if perto(getattr(t, "f_" + canal), f, 8)]

    a = achar(440)
    verificar(len(a) == 1 and perto(a[0].f_dir, 440, 8), f"[{etiqueta}] 440 Hz por el altavoz USB en los dos canales")
    a = a[0]
    verificar(perto(a.segundos, 1.0, 0.1), f"[{etiqueta}] duración del tono de 1,0 s: {a.segundos:.3f} s")
    esperado = rms_esperado(0.5)
    verificar(perto(a.rms_esq, esperado, 0.12 * esperado),
              f"[{etiqueta}] nivel igual al del archivo: RMS {a.rms_esq:.0f} (esperado {esperado:.0f})")
    c = achar(600)
    verificar(len(c) == 1 and perto(c[0].f_dir, 1500, 8),
              f"[{etiqueta}] estéreo de 44,1 kHz: 600 Hz a la izquierda y 1500 Hz a la derecha")
    verificar(perto(c[0].segundos, 1.2, 0.1), f"[{etiqueta}] duración de 1,2 s: {c[0].segundos:.3f} s")
    d = achar(700)
    verificar(len(d) == 1, f"[{etiqueta}] 700 Hz del WAV de 22,05 kHz")
    razao = d[0].rms_esq / esperado
    verificar(0.2 < razao < 0.3, f"[{etiqueta}] volumen 50 da ~1/4 del nivel: {razao:.3f}")
    verificar(not achar(800), f"[{etiqueta}] con volumen silencio el tono de 800 Hz no sale")


def parte_controlador(etiqueta, ctl, extras_ctl):
    caminho = wav_de(etiqueta)
    extras = audiodev(etiqueta) + extras_ctl + pendrive_qemu(pen())
    with Maquina(f"h275-{etiqueta}", "ide", imagem=imagem_console(), extras=extras) as m:
        preparar(m)
        reconhecer(m, etiqueta, ctl)
        tonos(m, etiqueta, None)
        time.sleep(1)
    comprobar_tonos(etiqueta, caminho)


def parte_xhci():
    parte_controlador("xhci", "xHCI", ["-device", "qemu-xhci,id=xhci",
                                      "-device", "usb-audio,id=ua0,bus=xhci.0,audiodev=ad-xhci",
                                      "-device", "usb-ehci,id=ehci"])


def parte_uhci():
    # piix3-usb-uhci: el altavoz (plena velocidad) y el pendrive en los dos puertos; el pendrive de
    # las WAV va por el EHCI de siempre (la imagen de la prueba lo espera).
    parte_controlador("uhci", "UHCI", ["-device", "piix3-usb-uhci,id=uhci",
                                      "-device", "usb-audio,id=ua0,bus=uhci.0,port=1,audiodev=ad-uhci",
                                      "-device", "usb-ehci,id=ehci"])


def parte_ohci():
    parte_controlador("ohci", "OHCI", ["-device", "pci-ohci,id=ohci",
                                      "-device", "usb-audio,id=ua0,bus=ohci.0,port=1,audiodev=ad-ohci",
                                      "-device", "usb-ehci,id=ehci"])


def parte_companero():
    # EHCI de ICH9 con sus UHCI compañeros: el altavoz de plena velocidad se cede al UHCI y suena por él
    parte_controlador("companero", "UHCI", [
        "-device", "ich9-usb-ehci1,id=ehci,addr=1d.7,multifunction=on",
        "-device", "ich9-usb-uhci1,masterbus=ehci.0,firstport=0,addr=1d.0,multifunction=on",
        "-device", "ich9-usb-uhci2,masterbus=ehci.0,firstport=2,addr=1d.1",
        "-device", "ich9-usb-uhci3,masterbus=ehci.0,firstport=4,addr=1d.2",
        "-device", "usb-audio,id=ua0,bus=ehci.0,port=1,audiodev=ad-companero"])


def parte_caliente():
    caminho1, caminho2 = wav_de("caliente1"), wav_de("caliente2")
    extras = (audiodev("caliente1") + audiodev("caliente2") +
              ["-device", "qemu-xhci,id=xhci", "-device", "usb-audio,id=ua0,bus=xhci.0,audiodev=ad-caliente1",
               "-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen()))
    with Maquina("h275-caliente", "ide", imagem=imagem_console(), extras=extras) as m:
        preparar(m)
        verificar("[UAC] " in m.serial(), "[caliente] el altavoz del arranque se reconoce")
        marca = len(m.serial())
        m.comando("reproducir /tmp/s/h300.wav &")
        time.sleep(2.5)
        m.monitor("device_del ua0")
        fim = time.time() + 20
        while "tarjeta de audio USB retirada" not in m.serial()[marca:] and time.time() < fim:
            time.sleep(0.3)
        s = m.serial()[marca:]
        verificar("[UAC] tarjeta de audio USB retirada" in s, "[caliente] el kernel nota que se retiró el altavoz")
        verificar("sin tarjeta de sonido" in s, "[caliente] el mezclador se queda sin tarjeta")
        # el reproducir en segundo plano no se queda colgado: termina enseguida (se tiran sus datos)
        t0 = time.time()
        try:
            out = m.comando("echo vivo", 30)
        except Exception:
            print("  --- serie tras retirar el altavoz ---\n" + m.serial()[marca:][-2500:])
            raise
        verificar("vivo" in out, "[caliente] el sistema sigue respondiendo")
        fim = time.time() + 30
        while "fluxo" not in m.serial()[marca:] or "fechado" not in m.serial()[marca:]:
            if time.time() > fim:
                break
            time.sleep(0.5)
        verificar("fechado" in m.serial()[marca:], f"[caliente] el reproducir de fondo cierra su flujo sin colgarse ({time.time() - t0:.1f} s)")
        out = m.comando("volumen")
        verificar("no hay tarjeta de sonido" in out, "[caliente] volumen dice que no hay tarjeta")
        m.comando("reproducir /tmp/s/a440.wav", 30)   # sin tarjeta: avisa y vuelve al prompt (no se cuelga)
        marca = len(m.serial())
        m.monitor("device_add usb-audio,id=ua1,bus=xhci.0,audiodev=ad-caliente2")
        fim = time.time() + 40
        while "salida PCM 16 bits" not in m.serial()[marca:] and time.time() < fim:
            time.sleep(0.5)
        verificar("salida PCM 16 bits" in m.serial()[marca:], "[caliente] el altavoz vuelve a reconocerse al enchufarlo")
        m.comando("reproducir /tmp/s/f900.wav", 60)
        time.sleep(1)
    esq, dir_, tr1 = trechos_de(caminho1)
    verificar(any(perto(t.f_esq, 300, 8) for t in tr1), "[caliente] antes de retirarlo sonaban los 300 Hz")
    largos = [t for t in tr1 if perto(t.f_esq, 300, 8)]
    verificar(largos[0].segundos < 6.5, f"[caliente] el sonido paró al retirar el altavoz ({largos[0].segundos:.2f} s de 8)")
    esq, dir_, tr2 = trechos_de(caminho2)
    verificar(any(perto(t.f_esq, 900, 8) for t in tr2), "[caliente] tras volver a enchufarlo suenan los 900 Hz")


def parte_mezcla():
    caminho_usb = wav_de("mezcla-usb")
    extras = (audiodev("mezcla-usb") + ["-device", "qemu-xhci,id=xhci", "-device", "usb-audio,id=ua0,bus=xhci.0,audiodev=ad-mezcla-usb",
                                        "-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen()))
    with Maquina("h275-mezcla", "ide", imagem=imagem_console(), extras=extras, som="hda") as m:
        preparar(m)
        out = m.comando("volumen salida")
        idx = {d: int(i) for i, d in re.findall(r"(\d): .*? \((usb|hda)\)", out)}
        verificar(re.search(r"\* %d: .*\(usb\)" % idx.get("usb", -1), out), "[mezcla] la salida actual es la USB (la última enchufada)")
        verificar("hda" in idx, "[mezcla] la tarjeta HDA también está en la lista")
        iu, ih = idx["usb"], idx["hda"]
        m.comando("reproducir /tmp/s/a440.wav", 60)
        out = m.comando(f"volumen salida {ih}")
        verificar(re.search(r"\* %d: .*\(hda\)" % ih, out), "[mezcla] volumen salida N pasa a la HDA")
        m.comando("reproducir /tmp/s/e800.wav", 60)
        # cambiar de salida con el flujo sonando: no se corta
        m.comando("reproducir /tmp/s/h300.wav &")
        time.sleep(1.5)
        m.comando(f"volumen salida {iu}")
        time.sleep(2.0)
        m.comando(f"volumen salida {ih}")
        time.sleep(1.0)
        m.comando("kill %1")
        m.comando(f"volumen salida {iu}")
        m.comando("reproducir /tmp/s/f900.wav", 60)
        time.sleep(1)
    esq_u, dir_u, tr_u = trechos_de(caminho_usb)
    esq_h, dir_h, tr_h = trechos_de(m.wav)
    print("  [mezcla] USB:", tr_u)
    print("  [mezcla] HDA:", tr_h)
    f_u = sorted(round(t.f_esq) for t in tr_u)
    f_h = sorted(round(t.f_esq) for t in tr_h)
    verificar(any(perto(f, 440, 8) for f in f_u) and not any(perto(f, 440, 8) for f in f_h),
              "[mezcla] el tono de 440 Hz salió por el altavoz USB y no por la HDA")
    verificar(any(perto(f, 800, 8) for f in f_h) and not any(perto(f, 800, 8) for f in f_u),
              "[mezcla] el de 800 Hz, tras cambiar a la HDA, salió por la HDA y no por el USB")
    verificar(any(perto(f, 900, 8) for f in f_u), "[mezcla] al volver a la USB vuelve a sonar por el USB")
    # el WAV de 300 Hz cambió de salida con el flujo abierto: hay 300 Hz en las dos
    verificar(any(perto(f, 300, 8) for f in f_u) and any(perto(f, 300, 8) for f in f_h),
              "[mezcla] el flujo abierto siguió sonando al cambiar de salida (300 Hz en las dos)")


def main():
    partes = {"xhci": parte_xhci, "uhci": parte_uhci, "ohci": parte_ohci, "companero": parte_companero, "caliente": parte_caliente, "mezcla": parte_mezcla}
    quais = sys.argv[1:] or list(partes)
    for q in quais:
        partes[q]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
