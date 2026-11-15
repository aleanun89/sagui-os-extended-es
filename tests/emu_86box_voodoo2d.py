#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""EMU 86Box - aceleración 2D de las 3dfx Voodoo3 (D305). A PETICIÓN, nunca en la batería.

QEMU no emula ninguna 3dfx: el driver acel_tdfx.c solo se puede probar en 86Box, que emula
el motor 2D de la Banshee/Voodoo3. Por cada máquina (p2-voodoo3 y p2-450-voodoo3500 si no se
pasan otras):

  escritorio  arranca la imagen con escritorio y comprueba por el serie que se reconoce la
              tarjeta (121a:0005), que la autoprueba completa del arranque da «IGUAL que la
              CPU» y que el 2D queda activo con el acelerador tdfx; luego, en el Terminal,
              «acel2d probar --detalle» (autoprueba completa, medida GPU frente a CPU y tandas)
              y «acel2d»;
  consola     arranca la imagen de consola (terminal a pantalla completa, donde el
              desplazamiento del texto lo hace la GPU: copias y rellenos) y, tras unos
              «ls /bin», «acel2d» tiene que contar copias y rellenos de la GPU y 0 fallos.

Capturas: docs/capturas/86box-voodoo2d-<máquina>-{escritorio,probar,consola}.png (las de la
primera máquina; las demás quedan en build/86box/). Informe con las medidas en
build/86box/informe-voodoo2d-<máquina>.txt.

Arrastrar una ventana con el ratón no se puede automatizar en el 86Box de Linux bajo Xvfb (el
ratón por XTEST no mueve el cursor del invitado, D305): el arrastre se prueba a mano con
./run_in_86box.sh --windows.

    tools/turno.sh python3 tests/emu_86box_voodoo2d.py [maquina ...] [--solo-escritorio]

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

MAQUINAS = ["p2-voodoo3", "p2-450-voodoo3500"]


def linhas(serial, padrao):
    return [l.strip() for l in re.findall(padrao, serial)]


def arrancar(m, espera):
    m.pasar_bios(tempo=60)
    m.esperar_serial("[ACEL2D] ", 150)
    fim = time.time() + 60
    while time.time() < fim and not re.search(r"\[ACEL2D\] (activo|CPU):", m.serial()):
        time.sleep(0.5)
    m.esperar_serial(espera, 150)
    m.esperar_serial("sagui:", 120)
    time.sleep(3)


def acelerador_activo(m, informe):
    s = m.serial().replace("\x00", "")
    for l in linhas(s, r"\[(?:ACEL2D|VIDEO)\][^\n]*"):
        informe.append(l)
    verificar("[ACEL2D] la pantalla (" in s and "es de la tarjeta 121a:0005" in s, "la pantalla es de la Voodoo3 (121a:0005)")
    verificar(re.search(r"\[ACEL2D\] autoprueba completa: \d+ ordenes .*IGUAL que la CPU", s) is not None,
              "la autoprueba completa del arranque da los mismos píxeles que la CPU")
    activo = re.search(r"\[ACEL2D\] activo: ([^\n]*)", s)
    verificar(activo is not None and "acelerador tdfx" in activo.group(1), "el 2D queda activo con el acelerador tdfx")
    vel = re.search(r"\[ACEL2D\] velocidad: ([^\n]*)", s)
    if vel:
        print("   ", vel.group(1).strip())


def escritorio(nombre, imagen, guardar, informe):
    with Maquina86Box(nombre, imagen, nome=f"{nombre}-voodoo2d") as m:
        try:
            arrancar(m, "area de trabalho no ar")
            acelerador_activo(m, informe)
            m.captura(f"86box-voodoo2d-{nombre}-escritorio", guardar=guardar)
            out = m.comando("acel2d probar --detalle", 300)
            informe.append("$ acel2d probar --detalle")
            informe.append(out.strip())
            verificar("acel2d probar: BIEN" in out, "«acel2d probar --detalle»: BIEN (autoprueba completa igual que la CPU)")
            med = re.search(r"acel2d probar: medida: GPU (\d+) us, CPU (\d+) us", out)
            verificar(med is not None, "«acel2d probar» da la medida GPU frente a CPU")
            g, c = int(med.group(1)), int(med.group(2))
            print(f"    medida (60 copias de la zona): GPU {g} us, CPU {c} us ({c / max(g, 1):.1f} veces)")
            time.sleep(2)
            m.captura(f"86box-voodoo2d-{nombre}-probar", guardar=guardar)
            s = m.serial().replace("\x00", "")
            tandas = linhas(s, r"\[ACEL2D\] tanda \d[^\n]*")
            for t in tandas:
                print("   ", t)
                informe.append(t)
            for t in tandas:
                mt = re.search(r"suma (\w+)\), CPU \d+ us \(suma (\w+)\)", t)
                verificar(mt is not None and mt.group(1) == mt.group(2), "tanda con la misma suma por la GPU y la CPU: " + t[9:40])
            det = linhas(s, r"\[ACEL2D\] (?:detalle|tdfx \()[^\n]*")
            for d in det:
                informe.append(d)
            verificar(any("detalle: copia" in d for d in det) and any("tdfx (" in d for d in det),
                      "--detalle vuelca los tiempos de cada copia y los registros de la tdfx al log")
            for d in det:
                if "detalle: copia" in d:
                    print("   ", d[9:])
            out = m.comando("acel2d", 60)
            informe.append("$ acel2d")
            informe.append(out.strip())
            verificar("Aceleracion 2D: tdfx" in out, "«acel2d»: el acelerador es tdfx")
            m.comando("infopc -t video", 120)
            time.sleep(1)   # la línea puede llegar al serie después del prompt
            verificar(re.search(r"Acelera\w+ 2D: +tdfx \(3dfx Voodoo3", m.serial()) is not None,
                      "infopc muestra el acelerador tdfx")
            verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones")
            return g, c
        except Exception:
            m.captura(f"86box-voodoo2d-{nombre}-fallo")
            informe.append("serie: " + m.serial().replace("\x00", "")[-2000:])
            raise


def consola(nombre, imagen, guardar, informe):
    with Maquina86Box(nombre, imagen, nome=f"{nombre}-voodoo2d-consola") as m:
        try:
            arrancar(m, "[SAGUI] boot ok")
            acelerador_activo(m, informe)
            for _ in range(3):
                m.comando("ls /bin", 120)
            out = m.comando("acel2d", 60)
            informe.append("$ acel2d (consola)")
            informe.append(out.strip())
            mc = re.search(r"copias (\d+), rellenos (\d+) por la GPU, fallos (\d+)", out)
            verificar(mc is not None and int(mc.group(1)) > 0 and int(mc.group(2)) > 0,
                      f"al desplazar el texto la GPU copia y rellena ({mc.group(0) if mc else '?'})")
            verificar(mc is not None and mc.group(3) == "0", "sin fallos de la GPU en la sesión")
            verificar("Aceleracion 2D: tdfx" in out, "el acelerador sigue activo tras desplazar el texto")
            time.sleep(1)
            m.captura(f"86box-voodoo2d-{nombre}-consola", guardar=guardar)
        except Exception:
            m.captura(f"86box-voodoo2d-{nombre}-consola-fallo")
            informe.append("serie: " + m.serial().replace("\x00", "")[-2000:])
            raise


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    solo_escritorio = "--solo-escritorio" in sys.argv
    maquinas = args or MAQUINAS
    verificar(emu86box.instalado(), "86Box instalado (bash tools/86box_instalar.sh)")
    img_escritorio = imagem_de_teste("", ["--vesa"])
    img_consola = imagem_de_teste("", ["--vesa", "--console"])
    os.makedirs(SALIDA, exist_ok=True)
    fallos = []
    for i, nombre in enumerate(maquinas):
        print(f"== {nombre}", flush=True)
        informe = [f"# Aceleración 2D de la 3dfx en {nombre} (D305)"]
        try:
            g, c = escritorio(nombre, img_escritorio, i == 0, informe)
            informe.append(f"medida: GPU {g} us, CPU {c} us")
            if not solo_escritorio:
                consola(nombre, img_consola, i == 0, informe)
        except (FalhaTeste, Exception) as e:  # noqa: BLE001 - se informa y se sigue con la máquina siguiente
            fallos.append(nombre)
            informe.append(f"FALLO: {e}")
            print(f"  FALLO en {nombre}: {e}", flush=True)
        finally:
            ruta = os.path.join(SALIDA, f"informe-voodoo2d-{nombre}.txt")
            with open(ruta, "w", encoding="utf-8") as f:
                f.write("\n".join(informe) + "\n")
            print(f"  informe: {ruta}", flush=True)
    if fallos:
        print("máquinas con fallos:", ", ".join(fallos))
        return 1
    print("EMU voodoo2d: BIEN en", ", ".join(maquinas))
    return 0


if __name__ == "__main__":
    sys.exit(main())
