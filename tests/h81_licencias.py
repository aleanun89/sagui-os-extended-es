#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H81 (D109, mejora 057) - licencias: auditoría y compilación sin GPL.

  1. compila con `make SEM_GPL=1` en una carpeta propia (SAIDA/h81/build; no toca
     build/) y comprueba que no se compiló nada de kernel/terceros/linux/;
  2. tools/auditar_licencias.py: el árbol entero cuadra con docs/terceros.md, lo GPL
     está aislado, la lista real de fuentes de SEM_GPL no tiene nada GPL y el
     kernel.elf recién compilado no contiene ningún símbolo ni cadena de lo GPL;
  3. la imagen sin GPL arranca hasta el escritorio y hasta la consola, dice su
     variante (arranque, infopc) y el monitor de sensores sigue funcionando;
  4. la imagen normal (con GPL) sí contiene lo de Linux y lo dice (la auditoría
     del binario no es vacía: detecta las marcas en build/kernel.elf).

Uso: h81_licencias.py
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA  # noqa: E402

DIR = os.path.join(SAIDA, "h81")
B = os.path.join(DIR, "build")


def imagem_console_sem_gpl():
    destino = os.path.join(DIR, "sem-gpl-console.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(B, "boot", "mbr.bin"), "--estagio2", os.path.join(B, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(B, "kernel.bin"), "--kernel-elf", os.path.join(B, "kernel.elf"),
                    "--ramdisk", os.path.join(B, "ramdisk.sgfs"), "--saida", destino, "--vesa", "--console"],
                   check=True, stdout=subprocess.DEVNULL)
    return destino


def main():
    os.makedirs(DIR, exist_ok=True)
    jobs = os.environ.get("SAGUI_JOBS", "4")
    r = subprocess.run(["make", "-j" + jobs, "SEM_GPL=1", "B=" + B, B + "/saguios.img"], cwd=RAIZ,
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "make SEM_GPL=1 compila la imagen " + r.stderr[-400:])
    verificar(os.path.exists(os.path.join(B, "saguios.img")), "saguios.img sin GPL generada")
    objs = [os.path.join(d, f) for d, _, fs in os.walk(B) for f in fs if "terceros/linux" in os.path.join(d, f)]
    verificar(not objs, f"nada de kernel/terceros/linux/ en la carpeta de compilación {objs[:3]}")

    r = subprocess.run(["python3", os.path.join(RAIZ, "tools", "auditar_licencias.py"), "--elf",
                        os.path.join(B, "kernel.elf")], capture_output=True, text=True)
    print(r.stdout)
    verificar(r.returncode == 0 and "todo cuadra" in r.stdout, "auditoría de licencias (árbol, variante y binario)")
    verificar("variante sin GPL:" in r.stdout and "sin ninguna de las" in r.stdout,
              "la auditoría llegó a la variante y al binario")

    # El binario con GPL tiene que contener las marcas (si no, la comprobación del binario no sirve).
    elf = os.path.join(RAIZ, "build", "kernel.elf")
    if os.path.exists(elf):
        r = subprocess.run(["python3", os.path.join(RAIZ, "tools", "auditar_licencias.py"), "--sin-make", "--elf", elf],
                           capture_output=True, text=True)
        verificar(r.returncode != 0 and "contiene" in r.stdout,
                  "la imagen con GPL SÍ contiene lo de Linux (la comprobación del binario detecta)")

    # Arranque de la imagen sin GPL: escritorio.
    img = os.path.join(B, "saguios.img")
    with Maquina("h81e", "ide", imagem=img) as m:
        m.esperar_serial("area de trabalho no ar", 120)
        s = m.serial()
        verificar("[SAGUI] compilacion: sin GPL (permisiva)" in s, "el arranque dice «sin GPL (permisiva)»")
        verificar("EXCECAO" not in s, "arranca hasta el escritorio sin errores del kernel")
        m.captura("H81-sin-gpl-escritorio", guardar=False)

    # Consola: infopc, sensores y red.
    with Maquina("h81c", "ide", imagem=imagem_console_sem_gpl()) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("infopc -t sistema", 60)
        verificar(re.search(r"^Compilación:\s+sin GPL \(permisiva\)", out, re.M), "infopc: «Compilación: sin GPL (permisiva)»")
        verificar("terminou com codigo 0" in out, "infopc termina con código 0")
        out = m.comando("sensores", 60)
        verificar("terminou com codigo 0" in out and re.search(r"^Temperatura:\s+no disponible", out, re.M),
                  "sensores sigue funcionando (no disponible en la máquina virtual)")
        out = m.comando("echo sigo vivo", 20)
        verificar("sigo vivo" in out and "EXCECAO" not in m.serial(), "el sistema sigue funcionando")


if __name__ == "__main__":
    main()
    print("H81 OK")
