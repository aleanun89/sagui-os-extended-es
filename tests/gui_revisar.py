#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Herramienta de depuración de la GUI (D300, docs/gui-revisar.md): abre una a una las aplicaciones
principales con el detector de problemas de la libsagui activado (SAGUI_GUI_REVISAR), saca una
captura de cada una y escribe build/gui-revisar/informe.md con los avisos agrupados por aplicación.

NO falla por los avisos: un aviso no es siempre un error (hay solapes a propósito). Sale con 0 y
resume «N avisos en M aplicaciones»; solo falla si algo no arranca (QEMU, el prompt, una aplicación
que no abre su ventana).

Uso: gui_revisar.py [--modo 1|2] [--solo app1,app2] [--kvm]
  --modo 2 (por defecto) dibuja además un borde rojo en lo que tiene aviso (se ve en las capturas).
"""
import os
import re
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import io
import math
import struct
import wave
import pdf_exemplos
from qemu import Maquina, FalhaTeste, criar_pendrive, pendrive_qemu, RAIZ, SAIDA  # noqa: E402

DIR = os.path.join(RAIZ, "build", "gui-revisar")
CAPS = os.path.join(DIR, "capturas")

# Pestañas de Configuración (título -> ancho = 8 px por letra + 10, separación 2, como h83).
TITULOS_CONFIG = ("Teclado", "Fecha y hora", "Pantalla", "Sonido", "Escritorio", "Capturas", "IA", "Fuentes", "Red",
                  "Escritorio remoto")


def pos_pestanas():
    x, pos = 10, []
    for t in TITULOS_CONFIG:
        l = len(t) * 8 + 10
        pos.append((x, l))
        x += l + 2
    return pos


# Cada aplicación: (nombre, orden, pasos). Pasos: ("esperar", s), ("captura", etiqueta), ("tecla", t),
# ("clic", dx, dy) relativo al área cliente de la última ventana, ("texto", t), ("pestana", i).
def tabla():
    pos = pos_pestanas()
    cfg = [("esperar", 1.0), ("captura", "teclado")]
    for i, t in enumerate(TITULOS_CONFIG[1:], 1):
        cfg += [("clic", pos[i][0] + pos[i][1] // 2, 24), ("esperar", 0.6), ("captura", t.lower().replace(" ", "-"))]
    return [
        ("configuracion", "configuracion &", cfg),
        ("arquivos", "arquivos &", [("esperar", 1.0), ("captura", "principal"), ("tecla", "ctrl-n"), ("esperar", 0.8), ("captura", "tecla-ctrl-n"), ("tecla", "esc")]),
        ("pluma", "pluma &", [("esperar", 1.0), ("captura", "principal"), ("tecla", "ctrl-o"), ("esperar", 1.5), ("captura", "dialogo-abrir"), ("tecla", "esc"),
                              ("tecla", "ctrl-f"), ("esperar", 1.0), ("captura", "buscar"), ("tecla", "esc")]),
        ("macaco", "macaco &", [("esperar", 1.0), ("captura", "principal"), ("tecla", "ctrl-s"), ("esperar", 1.5), ("captura", "dialogo-guardar"), ("tecla", "esc")]),
        ("gebramini", "gebramini /ejemplos/gebramini/parabola.gbm &", [("esperar", 1.8), ("captura", "principal"), ("tecla", "ctrl-o"), ("esperar", 1.5), ("captura", "dialogo-abrir"), ("tecla", "esc")]),
        ("planomini", "planomini /ejemplos/planomini/pieza.pmini &", [("esperar", 2.0), ("captura", "principal"), ("tecla", "ctrl-o"), ("esperar", 1.5), ("captura", "dialogo-abrir"), ("tecla", "esc")]),
        ("correo", "correo &", [("esperar", 1.5), ("captura", "principal")]),
        ("chatia", "chatia &", [("esperar", 1.5), ("captura", "principal")]),
        ("mico", "mico &", [("esperar", 1.5), ("captura", "principal"), ("tecla", "ctrl-o"), ("esperar", 1.2), ("captura", "dialogo-abrir"), ("tecla", "esc")]),
        ("recreativa", "recreativa &", [("esperar", 1.5), ("captura", "principal")]),
        ("vps", "python /lib/python/vps/disenador.py &", [("esperar", 3.0), ("captura", "principal")]),
        ("grabadora", "grabadora &", [("esperar", 1.5), ("captura", "principal")]),
        ("camara", "camara &", [("esperar", 1.5), ("captura", "principal")]),
        ("visorpdf", "visorpdf /usb/indice.pdf &", [("esperar", 1.5), ("captura", "principal")]),
        ("reproductor", "reproductor /usb/prueba.mpg &", [("esperar", 1.5), ("captura", "principal")]),
        ("musica", "musica /usb/tono.wav &", [("esperar", 1.5), ("captura", "principal")]),
        ("terminal", "terminal &", [("esperar", 1.5), ("captura", "principal")]),
        ("calculadora", "calculadora &", [("esperar", 1.0), ("captura", "principal")]),
        ("paint", "paint &", [("esperar", 1.5), ("captura", "principal"), ("tecla", "ctrl-o"), ("esperar", 1.2), ("captura", "dialogo-abrir"), ("tecla", "esc")]),
        ("infopc", "infopc &", [("esperar", 2.0), ("captura", "principal")]),
        ("discos", "discos --ventana &", [("esperar", 1.5), ("captura", "principal")]),
        ("particiones", "discos --ventana --particiones &", [("esperar", 1.5), ("captura", "principal")]),
    ]


def wav_prueba():
    """Un segundo de un la a 440 Hz, 8 kHz, 16 bits, mono."""
    b = io.BytesIO()
    w = wave.open(b, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(8000)
    w.writeframes(b"".join(struct.pack("<h", int(8000 * math.sin(2 * math.pi * 440 * i / 8000))) for i in range(8000)))
    w.close()
    return b.getvalue()


def video_prueba():
    """Un segundo de MPEG-1 con ffmpeg del anfitrión (b'' si no hay: se omite el Reproductor)."""
    import subprocess
    destino = os.path.join(SAIDA, "gui-revisar-prueba.mpg")
    try:
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc=size=160x120:rate=25",
                        "-t", "1", "-c:v", "mpeg1video", "-f", "mpeg", destino], check=True, timeout=60)
        with open(destino, "rb") as f:
            return f.read()
    except Exception:
        return b""


def focar_terminal(m):
    """Foco al terminal con su botón de la barra de tareas (el primero). Con el foco, ese botón minimiza:
    el registro del sistema lo dice y entonces se pulsa otra vez."""
    for _ in range(2):
        marca = len(m.serial_bruto())
        m.clique(170, 12)
        time.sleep(0.5)
        if "(minimizar)" not in m.serial_bruto()[marca:]:
            return
        time.sleep(0.3)


def ventanas(texto):
    return re.findall(r'criada por (\d+): "([^"]*)" (\d+)x(\d+) em \((\d+),(\d+)\)', texto)


def avisos(texto):
    return [l[l.index("[GUI-REVISAR]"):] for l in texto.replace("\r", "").splitlines()
            if "[GUI-REVISAR]" in l and "detector activo" not in l and "cobertura" not in l]


def main():
    modo = "2"
    solo = None
    args = sys.argv[1:]
    while args:
        a = args.pop(0)
        if a == "--modo":
            modo = args.pop(0)
        elif a == "--solo":
            solo = set(args.pop(0).split(","))
        elif a == "--kvm":
            os.environ["SAGUI_KVM"] = "1"
    os.makedirs(CAPS, exist_ok=True)
    for f in os.listdir(CAPS):
        os.remove(os.path.join(CAPS, f))
    fallos = []
    resultados = []   # (app, [avisos], [capturas], [ventanas])
    archivos = {"indice.pdf": pdf_exemplos.com_indice(), "tono.wav": wav_prueba()}
    mpg = video_prueba()
    if mpg:
        archivos["prueba.mpg"] = mpg
    pen = criar_pendrive("gui-revisar-pen", rotulo="REVISAR", arquivos=archivos)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("gui-revisar", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("montar usb0")
        m.comando("echo %s > /etc/gui-revisar.modo" % modo)
        for nombre, orden, pasos in tabla():
            if (solo and nombre not in solo) or (nombre == "reproductor" and not mpg):
                continue
            for intento in (1, 2):
                focar_terminal(m)
                marca = len(m.serial())
                m.digitar(orden + "\n", 0.03)
                fin = time.time() + 40
                while time.time() < fin and not ventanas(m.serial()[marca:]):
                    if m.proc.poll() is not None:
                        raise FalhaTeste("QEMU terminó con %s" % nombre)
                    time.sleep(0.3)
                if ventanas(m.serial()[marca:]):
                    break
            if not ventanas(m.serial()[marca:]):
                print("gui_revisar: %s no abrió ninguna ventana: %r" % (nombre, m.serial()[marca:][-300:]))
                fallos.append(nombre)
                continue
            caps = []
            for paso in pasos:
                if paso[0] == "esperar":
                    time.sleep(paso[1])
                elif paso[0] == "captura":
                    destino = "%s-%s.png" % (nombre, paso[1])
                    m.captura("gui-revisar-tmp", guardar=False)
                    shutil.move(os.path.join(SAIDA, "gui-revisar-tmp.png"), os.path.join(CAPS, destino))
                    caps.append(destino)
                elif paso[0] == "tecla":
                    m.tecla(paso[1], 0.2)
                    time.sleep(0.4)
                elif paso[0] == "texto":
                    m.digitar(paso[1].replace("\n", ""), 0.05)
                    if paso[1].endswith("\n"):
                        m.tecla("ret", 0.2)
                elif paso[0] == "clic":
                    v = ventanas(m.serial()[marca:])[-1]
                    m.clique(int(v[4]) + 2 + paso[1], int(v[5]) + 24 + paso[2])
            time.sleep(0.5)
            nuevo = m.serial()[marca:]
            vs = ventanas(nuevo)
            av = avisos(nuevo)
            resultados.append((nombre, sorted(set(av), key=av.index), caps, vs))
            # Se termina lo que se abrió: foco al terminal (clic en su barra de título, que ninguna
            # aplicación tapa) y «kill» de cada proceso; así no hay avisos de «¿guardar cambios?».
            focar_terminal(m)
            for pid in sorted({v[0] for v in vs}, key=int):
                try:
                    m.comando("kill %s" % pid, 15)
                except FalhaTeste:
                    print("gui_revisar: aviso: kill %s (%s) no volvió al prompt" % (pid, nombre))
            time.sleep(0.8)
        # El resumen se queda en el registro por si hace falta volver a mirarlo.
    escribir_informe(resultados, modo, fallos)
    total = sum(len(r[1]) for r in resultados)
    con = sum(1 for r in resultados if r[1])
    print("gui_revisar: %d avisos en %d aplicaciones (de %d abiertas); informe en %s" % (total, con, len(resultados), os.path.join(DIR, "informe.md")))
    return 1 if fallos else 0


def escribir_informe(resultados, modo, fallos=()):
    l = ["# Informe del detector de la GUI (D300)", "",
         "Generado por `tests/gui_revisar.py` (modo %s). Un aviso no es siempre un error: hay solapes intencionados." % modo, ""]
    total = sum(len(r[1]) for r in resultados)
    con = sum(1 for r in resultados if r[1])
    l += ["**%d avisos en %d aplicaciones** (de %d abiertas)." % (total, con, len(resultados)), "",
          "| Aplicación | Avisos | Ventanas |", "|---|---|---|"]
    if fallos:
        l[4:4] = ["**No abrieron ventana:** %s." % ", ".join(fallos), ""]
    for nombre, av, caps, vs in resultados:
        l.append("| %s | %d | %s |" % (nombre, len(av), ", ".join(sorted({v[1] for v in vs}))))
    for nombre, av, caps, vs in resultados:
        l += ["", "## %s" % nombre, ""]
        l += ["- %s" % a for a in av] if av else ["Sin avisos."]
        for c in caps:
            l += ["", "![%s](capturas/%s)" % (c, c)]
    with open(os.path.join(DIR, "informe.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(l) + "\n")


if __name__ == "__main__":
    sys.exit(main())
