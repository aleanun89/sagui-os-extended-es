#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H302 (D302) - la Sound Blaster 16 (ISA): DMA 8237, detección, reproducción, mezclador y captura.

Con QEMU -device sb16 (puerto 220h, IRQ 5, DMA 1 y 5, DSP 4.05) y la salida en un .wav del
anfitrión (-audiodev wav, que el QEMU pasa a 48 kHz):
  - el arranque la encuentra (puerto, IRQ y DMA del mezclador, comprobados con una IRQ pedida al
    DSP y una transferencia corta) y la registra como tarjeta del mezclador común;
  - sonido, infopc, lspci (dispositivos ISA) y grabar --info dicen qué tarjeta hay y dónde;
  - reproducir de un WAV de 1 kHz (44,1 kHz, 16 bits, estéreo) sale con la misma frecuencia,
    nivel, duración y canales; un estéreo con un tono distinto en cada canal sale separado;
  - volumen 50 da ~1/4 del nivel y volumen silencio lo calla;
  - un tono de 10 s sale sin cortes (ninguna ventana de 10 ms baja de nivel);
  - la tarjeta interrumpe (IRQ de medio búfer, de 16 bits) y el mezclador CT1745 se cambia;
  - sin -device sb16 no se detecta nada, no aparece nada ISA y el sistema arranca igual.

La grabación no se puede comprobar en QEMU (su SB16 no emula el ADC): solo que grabar acaba sin
colgar nada y que la salida vuelve después (semidúplex, ver la D302).

Uso: h302_sb16.py [sb16] [sem]
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_pendrive, pendrive_qemu  # noqa: E402
import h70_sonido as h70  # noqa: E402

ARQUIVOS = {
    # nombre: (tasa, bits, canales, segundos, frecuencias, amplitud)
    "k1000.wav": (44100, 16, 2, 1.0, [1000, 1000], 0.5),
    "est.wav": (44100, 16, 2, 1.2, [600, 1500], 0.4),
    "v700.wav": (22050, 16, 1, 1.0, [700], 0.5),
    "s800.wav": (44100, 16, 1, 0.8, [800], 0.5),
    "r1200.wav": (44100, 16, 2, 1.0, [1200, 1200], 0.4),
}


def provar_sb16():
    arqs = {n: h70.gerar_wav(*p) for n, p in ARQUIVOS.items()}
    arqs["largo.wav"] = h70.gerar_wav_rapido(44100, 1, 10.0, 440, 0.4)
    pen = criar_pendrive("h302", rotulo="SB16", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h302-sb16", "ide", imagem=imagem_console(), extras=extras, som="sb16") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[ISA] DMA: 128 KB reservados" in s, "el arranque reserva 128 KB bajo 16 MB para el DMA ISA")
        verificar("[SB16] 220h: DSP 4.05, IRQ 5, DMA 1 y 5" in s, "detecta la SB16 en 220h con IRQ 5 y DMA 1 y 5")
        verificar("[AUDIO] sb16: Sound Blaster 16" in s, "la SB16 se registra en el mezclador común")
        verificar("Sonido: Sound Blaster 16, 44 kHz" in s, "la línea del sonido sale en el arranque")
        verificar("[ISAPNP] ninguna tarjeta ISA PnP" in s and "configurada por ISA PnP" not in s,
                  "el ISA PnP solo lista (la SB16 ya contestó en su puerto) y en QEMU no hay tarjetas PnP")

        out = m.comando("sonido")
        verificar("Sound Blaster 16: puerto 220h, IRQ 5, DMA 1 (8 bits) y 5 (16 bits), DSP 4.05" in out,
                  "sonido muestra puerto, IRQ, DMA y DSP")
        verificar("Mezclador CT1745: maestro 100, pcm 100" in out, "sonido muestra el mezclador CT1745")
        out = m.comando("lspci")
        verificar(re.search(r"ISA 220h Sound Blaster 16 \(Creative\), IRQ 5, DMA 1 y 5", out) is not None,
                  "lspci lista la SB16 entre los dispositivos ISA")
        out = m.comando("infopc -t pci")
        verificar("220h  IRQ 5  DMA 1/5  Sound Blaster 16" in out, "infopc -t pci la lista como dispositivo ISA")
        out = m.comando("infopc")
        verificar("Sound Blaster 16 (driver sb16), 44100 Hz" in out and "puerto 220h" in out,
                  "infopc muestra la tarjeta, el driver y el puerto")
        out = m.comando("volumen")
        verificar("Volumen: 100 %" in out and "Sound Blaster 16" in out, "volumen muestra la SB16")
        out = m.comando("grabar --info")
        verificar("Tarjeta: Sound Blaster 16, 44100 Hz nativos" in out and "Micr" in out,
                  "grabar --info ve la entrada de la SB16 (micrófono, línea, CD)")
        out = m.comando("sonido --mezclador cd 50")
        verificar("cd 50" in out, "sonido --mezclador cambia un canal del CT1745")
        m.comando("sonido --mezclador cd 77")

        m.comando("montar usb0p1")
        m.comando("mkdir /tmp/s")
        for nome in arqs:
            m.comando(f"cp /usb/{nome} /tmp/s/{nome}", 120)
        out = m.comando("reproducir /tmp/s/k1000.wav", 60)
        verificar("44100 Hz, 16 bits, est" in out, "reproducir describe el WAV de 44,1 kHz estéreo")
        m.comando("reproducir /tmp/s/est.wav", 60)
        m.comando("volumen 50")
        m.comando("reproducir /tmp/s/v700.wav", 60)
        m.comando("volumen silencio")
        m.comando("reproducir /tmp/s/s800.wav", 60)
        out = m.comando("volumen 100")
        verificar("Volumen: 100 %" in out, "volumen 100 quita el silencio")
        m.comando("reproducir /tmp/s/largo.wav", 60)
        # Grabar (semidúplex): el SB16 de QEMU no emula el ADC, pero la orden tiene que acabar sin colgar
        # nada y la salida tiene que volver a sonar después.
        out = m.comando("grabar -t 600 /tmp/g.wav", 60)
        verificar("sagui:/$" in m.serial()[-2000:] or out is not None, "grabar con la SB16 acaba (sin colgarse)")
        verificar("[SB16] grabando" in m.serial(), "grabar arranca la captura de la SB16")
        m.comando("reproducir /tmp/s/r1200.wav", 60)
        out = m.comando("infopc -t sistema")
        irq = re.search(r"IRQ 5 \((\d+) atendidas\)", out)
        verificar(irq and int(irq.group(1)) > 0, f"la SB16 interrumpe ({irq.group(1) if irq else 0} IRQ atendidas)")
        cortes = re.search(r"(\d+) cortes", out)
        print("  cortes del mezclador:", cortes.group(1) if cortes else "?")
        out = m.comando("sonido")
        n16 = re.search(r"de 16 bits (\d+)", out)
        verificar(n16 and int(n16.group(1)) > 0, f"son IRQ de 16 bits, de medio búfer ({n16.group(1) if n16 else 0})")
        time.sleep(1)
    esq, dir_ = h70.ler_gravacao(m.wav)
    tr = [h70.Trecho(esq, dir_, a, b) for a, b in h70.trechos(esq, dir_)]
    print("  trozos:", tr)

    def achar(f, canal="esq"):
        return [t for t in tr if h70.perto(getattr(t, "f_" + canal), f, 8)]

    k = achar(1000)
    verificar(len(k) == 1 and h70.perto(k[0].f_dir, 1000, 8), "1 kHz en los dos canales")
    k = k[0]
    verificar(h70.perto(k.segundos, 1.0, 0.08), f"duración del tono de 1,0 s: {k.segundos:.3f} s")
    esperado = h70.rms_esperado(0.5)
    verificar(h70.perto(k.rms_esq, esperado, 0.1 * esperado) and h70.perto(k.rms_dir, esperado, 0.1 * esperado),
              f"nivel igual al del archivo: RMS {k.rms_esq:.0f}/{k.rms_dir:.0f} (esperado {esperado:.0f})")
    e = achar(600)
    verificar(len(e) == 1 and h70.perto(e[0].f_dir, 1500, 8), "estéreo: 600 Hz a la izquierda y 1500 Hz a la derecha")
    v = achar(700)
    verificar(len(v) == 1, "700 Hz del WAV de 22,05 kHz mono")
    razao = v[0].rms_esq / esperado
    verificar(0.2 < razao < 0.3, f"volumen 50 da ~1/4 del nivel: {razao:.3f}")
    verificar(not achar(800), "con volumen silencio el tono de 800 Hz no sale")
    r = achar(1200)
    verificar(len(r) == 1 and h70.perto(r[0].segundos, 1.0, 0.08), "después de grabar, la salida vuelve a sonar (1200 Hz, 1 s)")
    lg = achar(440)
    verificar(len(lg) == 1 and h70.perto(lg[0].segundos, 10.0, 0.15),
              f"el tono de 10 s sale entero, en un solo trozo ({[round(t.segundos, 2) for t in lg]} s)")
    if lg:
        x = esq[lg[0].ini + 2400:lg[0].fim - 2400]
        ventanas = [h70.rms(x[i:i + 480]) for i in range(0, len(x) - 480, 480)]
        media = sum(ventanas) / len(ventanas)
        baixas = [i for i, r in enumerate(ventanas) if r < 0.7 * media]
        verificar(not baixas, f"sin cortes en 10 s: {len(baixas)} ventanas de 10 ms bajas (RMS medio {media:.0f})")


def sem_placa():
    with Maquina("h302-sem", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[SB16]" not in s and "[AUDIO] sb16" not in s, "sin SB16: no se detecta nada")
        verificar("Sonido:" not in s, "sin SB16: ninguna línea de sonido en el arranque")
        out = m.comando("lspci")
        verificar(re.search(r"^ISA [0-9a-f-]", out, re.M) is None, "sin SB16: lspci no lista dispositivos ISA")
        out = m.comando("sonido")
        verificar("no hay tarjeta HDA ni Sound Blaster 16" in out, "sin SB16: sonido lo dice")
        out = m.comando("echo vivo")
        verificar("vivo" in out, "sin SB16: el sistema sigue respondiendo")


def main():
    pedidos = sys.argv[1:] or ["sb16", "sem"]
    if "sb16" in pedidos:
        provar_sb16()
    if "sem" in pedidos:
        sem_placa()
    print("H302 OK")


if __name__ == "__main__":
    main()
