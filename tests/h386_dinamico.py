#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H386 - programas con bibliotecas compartidas (D378, fase 2).

Criterio:
  1. En el PC: /lib/ld-sagui.so.1 es un ELF estático en la zona del intérprete
     (0xD8000000); libsagui.so.1, libc.so.1 y libsagui_medios.so.1 (en /lib desde D462) son ET_DYN de
     i386 sin TEXTREL, con BIND_NOW y RELRO, sin segmentos RWX; los programas
     dinámicos (/testes/dinamico, /apps/bin/buscaminas) llevan PT_INTERP.
  2. ABI (D378, decisión 8a): los símbolos que exportan libsagui.so.1 y libc.so.1
     incluyen todos los de las listas userland/<so>.simbolos (quitar uno rompe los
     programas ya compilados: hace falta subir el número de soname). Los nuevos
     se informan (hay que añadirlos a la lista).
  3. Dentro de SaguiOS, /testes/dinamico (libc.so + libsagui.so, dlopen por el
     intérprete, W^X y RELRO) acaba con "RESULTADO dinamico: SUCESSO (0 falhas)",
     sin excepciones del kernel.
  4. Un programa estático sigue funcionando igual (/testes/dlprueba: h385).
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, BUILD, RAIZ  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 180 if KVM else 600


def readelf(opc, f):
    return subprocess.run(["readelf", opc, "-W", f], capture_output=True, text=True).stdout


def exportados(f):
    s = subprocess.run(["nm", "-D", "--defined-only", f], capture_output=True, text=True).stdout
    return {l.split()[2] for l in s.splitlines() if len(l.split()) == 3}


def no_pc():
    ld = os.path.join(BUILD, "lib", "ld-sagui.so.1")
    h = readelf("-h", ld)
    seg = readelf("-l", ld)
    cargas = [l.split() for l in seg.splitlines() if l.strip().startswith("LOAD")]
    verificar("EXEC (Executable file)" in h and cargas and all(int(c[2], 16) >= 0xD8000000 for c in cargas),
              "ld-sagui.so.1: ELF ejecutable en la zona del intérprete (0xD8000000)")
    for rel in ("lib/libsagui.so.1", "lib/libc.so.1", "lib/libsagui_medios.so.1"):
        f = os.path.join(BUILD, rel)
        h, seg, din = readelf("-h", f), readelf("-l", f), readelf("-d", f)
        rwx = [l for l in seg.splitlines() if l.strip().startswith("LOAD") and "RWE" in l]
        verificar("DYN (Shared object file)" in h and not rwx and "TEXTREL" not in din and "BIND_NOW" in din
                  and "GNU_RELRO" in seg, "%s: ET_DYN sin TEXTREL ni RWX, con BIND_NOW y RELRO" % rel)
    for rel in ("userland/testes/dinamico", "userland/bin/buscaminas"):
        seg = readelf("-l", os.path.join(BUILD, rel))
        verificar("/lib/ld-sagui.so.1" in seg, "%s lleva PT_INTERP = /lib/ld-sagui.so.1" % rel)
    for so in ("libsagui.so.1", "libc.so.1"):
        ref = os.path.join(RAIZ, "userland", so + ".simbolos")
        with open(ref) as h:
            esperados = {l.strip() for l in h if l.strip()}
        hoy = exportados(os.path.join(BUILD, "lib", so))
        faltan, nuevos = sorted(esperados - hoy), sorted(hoy - esperados)
        if faltan:
            print("    %s: FALTAN %s" % (so, " ".join(faltan[:20])))
        if nuevos:
            print("    %s: nuevos (añadirlos a userland/%s.simbolos): %s" % (so, so, " ".join(nuevos[:20])))
        verificar(not faltan, "ABI de %s: exporta los %d símbolos de su lista" % (so, len(esperados)))


def main():
    no_pc()
    with Maquina("h386", "ide", imagem=imagem_de_teste("/apps/testes/dinamico")) as m:
        m.esperar_serial("RESULTADO dinamico:", TEMPO)
        s = m.serial().replace("\r", "")
    for l in s.split("\n"):
        if l.startswith("DINAMICO") or "[FALHA]" in l or "[ok]" in l or "EXCECAO" in l or "finalizado:" in l or "ld-sagui" in l:
            print("    %s" % l.strip())
    verificar("RESULTADO dinamico: SUCESSO (0 falhas)" in s, "/testes/dinamico pasó todas sus pruebas")
    verificar("EXCECAO" not in s, "sin excepciones del kernel")
    return 0


if __name__ == "__main__":
    sys.exit(main())
