#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""EMU 86Box - 2D por la GPU en las gráficas de la época: ATI Mach64 y S3 Trio64/ViRGE (D308).
A PETICIÓN, nunca en la batería.

QEMU no emula ninguna de estas tarjetas: acel_mach64.c y acel_s3.c solo se pueden probar en 86Box.
Por cada máquina (p2-mach64, p2-s3virge y p2-s3trio64 si no se pasan otras):

  escritorio  arranca la imagen con escritorio y comprueba por el serie que la pantalla es de la
              tarjeta esperada, que la autoprueba completa del arranque da «IGUAL que la CPU» y que
              el 2D queda activo con el acelerador (mach64 o s3); luego, en el Terminal,
              «acel2d probar --detalle» (autoprueba completa, medida GPU frente a CPU, tandas con la
              misma suma y el volcado de registros) y «acel2d»;
  consola     arranca la imagen de consola (el desplazamiento del texto lo hacen la GPU: copias y
              rellenos) y, tras unos «ls /bin», «acel2d» tiene que contar copias y rellenos de la GPU
              y 0 fallos;
  16 bits     (con --bpp16) lo mismo que «escritorio» con la imagen de 16 bits por píxel (RGB565),
              que prueba el otro formato de los dos drivers.

Capturas: docs/capturas/86box-mach64-{escritorio,probar,consola}.png y
86box-s3-{virge,trio64}-{escritorio,probar,consola}.png. Informe con las medidas en
build/86box/informe-acel2d-<máquina>.txt.

Arrastrar una ventana con el ratón no se puede automatizar en el 86Box de Linux bajo Xvfb (el ratón
por XTEST no mueve el cursor del invitado): el arrastre se prueba a mano con
./run_in_86box.sh --windows p2-mach64 (o p2-s3virge, p2-s3trio64).

    tools/turno.sh python3 tests/emu_86box_acel2d_epoca.py [maquina ...] [--solo-escritorio] [--bpp16]

No lo recogen rodar_todos.py ni rodar_paralelo.py: solo buscan h<dos dígitos>*.py y b<dígito>_*.py.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu86box  # noqa: E402
from emu86box import Maquina86Box, verificar, FalhaTeste, SALIDA  # noqa: E402
from qemu import imagem_de_teste  # noqa: E402

# máquina: (identificador PCI, acelerador, nombre de las capturas)
MAQUINAS = {
    "p2-mach64": ("1002:5654", "mach64", "86box-mach64"),
    "p2-s3virge": ("5333:8a01", "s3", "86box-s3-virge"),
    "p2-s3trio64": ("5333:8901", "s3", "86box-s3-trio64"),
}


def linhas(serial, padrao):
    return [l.strip() for l in re.findall(padrao, serial)]


def arrancar(m, espera):
    m.pasar_bios(tempo=60)
    m.esperar_serial("[ACEL2D] ", 150)
    fim = time.time() + 90
    while time.time() < fim and not re.search(r"\[ACEL2D\] (activo|CPU):", m.serial()):
        time.sleep(0.5)
    m.esperar_serial(espera, 150)
    m.esperar_serial("sagui:", 120)
    time.sleep(3)


def acelerador_activo(m, pci, acel, informe):
    s = m.serial().replace("\x00", "")
    for l in linhas(s, r"\[(?:ACEL2D|VIDEO)\][^\n]*"):
        informe.append(l)
    verificar(f"es de la tarjeta {pci}" in s, f"la pantalla es de la tarjeta {pci}")
    verificar(re.search(r"\[ACEL2D\] autoprueba completa: \d+ ordenes .*IGUAL que la CPU", s) is not None,
              "la autoprueba completa del arranque da los mismos píxeles que la CPU")
    activo = re.search(r"\[ACEL2D\] activo: ([^\n]*)", s)
    verificar(activo is not None and f"acelerador {acel}" in activo.group(1), f"el 2D queda activo con el acelerador {acel}")
    vel = re.search(r"\[ACEL2D\] velocidad: ([^\n]*)", s)
    if vel:
        print("   ", vel.group(1).strip())
        informe.append("velocidad del arranque: " + vel.group(1).strip())


def escritorio(nombre, imagen, guardar, informe, sufijo=""):
    pci, acel, cap = MAQUINAS[nombre]
    with Maquina86Box(nombre, imagen, nome=f"{nombre}-acel2d{sufijo}") as m:
        try:
            arrancar(m, "area de trabalho no ar")
            acelerador_activo(m, pci, acel, informe)
            m.captura(f"{cap}-escritorio{sufijo}", guardar=guardar)
            out = m.comando("acel2d probar --detalle", 300)
            informe.append("$ acel2d probar --detalle")
            informe.append(out.strip())
            verificar("acel2d probar: BIEN" in out, "«acel2d probar --detalle»: BIEN (autoprueba completa igual que la CPU)")
            med = re.search(r"acel2d probar: medida: GPU (\d+) us, CPU (\d+) us", out)
            verificar(med is not None, "«acel2d probar» da la medida GPU frente a CPU")
            g, c = int(med.group(1)), int(med.group(2))
            print(f"    medida (60 copias de la zona): GPU {g} us, CPU {c} us ({c / max(g, 1):.1f} veces)")
            time.sleep(2)
            m.captura(f"{cap}-probar{sufijo}", guardar=guardar)
            s = m.serial().replace("\x00", "")
            tandas = linhas(s, r"\[ACEL2D\] tanda \d[^\n]*")
            for t in tandas:
                print("   ", t)
                informe.append(t)
            verificar(len(tandas) >= 3, "las tres tandas de la medida")
            for t in tandas:
                mt = re.search(r"suma (\w+)\), CPU \d+ us \(suma (\w+)\)", t)
                verificar(mt is not None and mt.group(1) == mt.group(2), "tanda con la misma suma por la GPU y la CPU: " + t[9:40])
            det = linhas(s, r"\[ACEL2D\] (?:detalle|%s \()[^\n]*" % acel)
            informe.extend(det)
            verificar(any("detalle: copia" in d for d in det) and any(f"{acel} (" in d for d in det),
                      f"--detalle vuelca los tiempos de cada copia y los registros de {acel} al log")
            out = m.comando("acel2d", 60)
            informe.append("$ acel2d")
            informe.append(out.strip())
            verificar(f"Aceleracion 2D: {acel}" in out, f"«acel2d»: el acelerador es {acel}")
            return g, c
        except Exception:
            m.captura(f"{cap}-fallo{sufijo}")
            informe.append("serie: " + m.serial().replace("\x00", "")[-3000:])
            raise


def consola(nombre, imagen, guardar, informe):
    pci, acel, cap = MAQUINAS[nombre]
    with Maquina86Box(nombre, imagen, nome=f"{nombre}-acel2d-consola") as m:
        try:
            arrancar(m, "[SAGUI] boot ok")
            acelerador_activo(m, pci, acel, informe)
            for _ in range(3):
                m.comando("ls /bin", 120)
            out = m.comando("acel2d", 60)
            informe.append("$ acel2d (consola)")
            informe.append(out.strip())
            mc = re.search(r"copias (\d+), rellenos (\d+) por la GPU, fallos (\d+)", out)
            verificar(mc is not None and int(mc.group(1)) > 0 and int(mc.group(2)) > 0,
                      f"al desplazar el texto la GPU copia y rellena ({mc.group(0) if mc else '?'})")
            verificar(mc is not None and mc.group(3) == "0", "sin fallos de la GPU en la sesión")
            verificar(f"Aceleracion 2D: {acel}" in out, "el acelerador sigue activo tras desplazar el texto")
            time.sleep(1)
            m.captura(f"{cap}-consola", guardar=guardar)
        except Exception:
            m.captura(f"{cap}-consola-fallo")
            informe.append("serie: " + m.serial().replace("\x00", "")[-3000:])
            raise


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    solo_escritorio = "--solo-escritorio" in sys.argv
    bpp16 = "--bpp16" in sys.argv
    maquinas = args or list(MAQUINAS)
    for n in maquinas:
        verificar(n in MAQUINAS, f"máquina conocida: {n} (una de {', '.join(MAQUINAS)})")
    verificar(emu86box.instalado(), "86Box instalado (bash tools/86box_instalar.sh)")
    img_escritorio = imagem_de_teste("", ["--vesa"])
    img_consola = imagem_de_teste("", ["--vesa", "--console"])
    img_16 = imagem_de_teste("", ["--vesa", "--bpp", "16"]) if bpp16 else None
    os.makedirs(SALIDA, exist_ok=True)
    fallos, medidas = [], []
    for nombre in maquinas:
        print(f"== {nombre}", flush=True)
        informe = [f"# 2D por la GPU en {nombre} (D308)"]
        try:
            g, c = escritorio(nombre, img_escritorio, True, informe)
            informe.append(f"medida: GPU {g} us, CPU {c} us")
            medidas.append(f"{nombre}: GPU {g} us, CPU {c} us")
            if not solo_escritorio:
                consola(nombre, img_consola, True, informe)
            if bpp16:
                print("  -- 16 bits", flush=True)
                g, c = escritorio(nombre, img_16, False, informe, "-16bits")
                informe.append(f"medida en 16 bits: GPU {g} us, CPU {c} us")
                medidas.append(f"{nombre} (16 bits): GPU {g} us, CPU {c} us")
        except (FalhaTeste, Exception) as e:  # noqa: BLE001 - se informa y se sigue con la máquina siguiente
            fallos.append(nombre)
            informe.append(f"FALLO: {e}")
            print(f"  FALLO en {nombre}: {e}", flush=True)
        finally:
            ruta = os.path.join(SALIDA, f"informe-acel2d-{nombre}.txt")
            with open(ruta, "w", encoding="utf-8") as f:
                f.write("\n".join(informe) + "\n")
            print(f"  informe: {ruta}", flush=True)
    for l in medidas:
        print("  medida:", l)
    if fallos:
        print("máquinas con fallos:", ", ".join(fallos))
        return 1
    print("EMU acel2d de la época: BIEN en", ", ".join(maquinas))
    return 0


if __name__ == "__main__":
    sys.exit(main())
