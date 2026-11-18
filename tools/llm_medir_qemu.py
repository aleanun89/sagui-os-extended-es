#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mide el motor `llm` (D380) con un modelo REAL dentro de SaguiOS en QEMU (a mano, nunca en la
batería: el modelo no es libre y no va a git ni a las imágenes). El modelo va en un pendrive
virtual FAT32 que se crea en build/testes (y se borra al acabar).

  python3 tools/llm_medir_qemu.py MODELO.gguf [--contexto 12k] [--kvm] [-n 64]

Escribe la salida de `llm` (con los tok/s de lectura y de generación) y la deja en
build/testes/llm-medida.txt. Con --kvm hace falta el grupo kvm (sg kvm -c "...")."""
import os
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))

PREGUNTA = ("Explica en tres frases qué es un sistema operativo y por qué un ordenador antiguo "
            "todavía puede ser útil hoy en una escuela")


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    if "--kvm" in args:
        os.environ["SAGUI_KVM"] = "1"
        args.remove("--kvm")
    ctx, n = "12k", "64"
    if "--contexto" in args:
        i = args.index("--contexto"); ctx = args[i + 1]; del args[i:i + 2]
    if "-n" in args:
        i = args.index("-n"); n = args[i + 1]; del args[i:i + 2]
    modelo = os.path.abspath(args[0])
    from qemu import Maquina, criar_pendrive, pendrive_qemu, SAIDA  # noqa: E402
    mb = os.path.getsize(modelo) // (1 << 20) + 128
    print("creando el pendrive de %d MB..." % mb)
    pen = criar_pendrive("llm-medida-pen", mb=mb, rotulo="MODELOS", arquivos={})
    from qemu import mtools  # noqa: E402
    mtools(pen, "mcopy", "-o", modelo, "p:/modelo.gguf")
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    saida = []
    try:
        with Maquina("llm-medida", "ide", memoria="2G", extras=extras) as m:
            m.esperar_serial("sagui:/$", 240)
            time.sleep(1)
            saida.append(m.comando("montar usb0", 60))
            saida.append(m.comando("llm --info -m /usb/modelo.gguf --contexto %s" % ctx, 120))
            t0 = time.time()
            saida.append(m.comando("llm -m /usb/modelo.gguf --contexto %s -n %s --temp 0 -p '%s'" % (ctx, n, PREGUNTA), 3600))
            saida.append("(tiempo total del comando, con la carga del modelo: %.0f s)\n" % (time.time() - t0))
            m.comando("umount /usb", 60)
    finally:
        os.remove(pen)
    texto = "".join(saida)
    print(texto)
    with open(os.path.join(SAIDA, "llm-medida.txt"), "w", encoding="latin-1", errors="replace") as f:
        f.write(texto)
    return 0


if __name__ == "__main__":
    sys.exit(main())
