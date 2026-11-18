#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H385 - prototipo del enlace dinámico: dlopen en espacio de usuario (D378, fase 1).

Criterio:
  1. En el PC: las bibliotecas de prueba de build/userland/testes/lib son
     ELF32 ET_DYN de i386 sin segmentos RWX; las buenas no tienen
     relocaciones de texto (TEXTREL) y libdlmal.so sí (a propósito).
  2. Dentro de SaguiOS, /testes/dlprueba abre libdldep.so (que arrastra a
     libdlbase.so por DT_NEEDED y DT_RUNPATH=$ORIGIN), llama a sus funciones
     por dlsym, comprueba relocaciones RELATIVE/GLOB_DAT/JMP_SLOT, constructores
     y destructores, la cuenta de referencias, dlerror, que libdlmal.so se
     rechaza, y que escribir en el código o en la RELRO mata al proceso (W^X).
     Acaba con "RESULTADO dlprueba: SUCESSO (0 falhas)" y sin excepciones del
     kernel.
  3. Imprime la medida del coste del código PIC (SHA-256 estático frente al de
     la .so, con y sin -Bsymbolic-functions); solo se informa, no se exige.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, BUILD  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 180 if KVM else 600
LIBS = os.path.join(BUILD, "userland", "testes", "lib")


def readelf(opc, f):
    return subprocess.run(["readelf", opc, "-W", f], capture_output=True, text=True).stdout


def no_pc():
    for nome in ("libdlbase.so", "libdldep.so", "libdlsha.so", "libdlsha_simb.so", "libdlmal.so"):
        f = os.path.join(LIBS, nome)
        verificar(os.path.exists(f), "%s compilada" % nome)
        if not os.path.exists(f):
            continue
        h = readelf("-h", f)
        seg = readelf("-l", f)
        din = readelf("-d", f)
        verificar("DYN (Shared object file)" in h and "Intel 80386" in h, "%s: ET_DYN de i386" % nome)
        rwx = [l for l in seg.splitlines() if l.strip().startswith("LOAD") and "RWE" in l]
        verificar(not rwx, "%s: ningún segmento RWX" % nome)
        textrel = "TEXTREL" in din
        if nome == "libdlmal.so":
            verificar(textrel, "libdlmal.so tiene TEXTREL (sin -fPIC, para la prueba de rechazo)")
        else:
            verificar(not textrel and "BIND_NOW" in din and "GNU_RELRO" in seg,
                      "%s: sin TEXTREL, con BIND_NOW y GNU_RELRO" % nome)


def main():
    no_pc()
    with Maquina("h385", "ide", imagem=imagem_de_teste("/apps/testes/dlprueba")) as m:
        m.esperar_serial("RESULTADO dlprueba:", TEMPO)
        s = m.serial().replace("\r", "")
    for l in s.split("\n"):
        if l.startswith("DLPRUEBA") or "[FALHA]" in l or "[ok]" in l or "EXCECAO" in l or "finalizado:" in l:
            print("    %s" % l.strip())
    verificar("RESULTADO dlprueba: SUCESSO (0 falhas)" in s, "/testes/dlprueba pasó todas sus pruebas")
    verificar("EXCECAO" not in s, "sin excepciones del kernel")
    m2 = re.search(r"DLPRUEBA PIC sha256 (\d+) MB: estatico (\d+) Mciclos, so (\d+) Mciclos, so-Bsymbolic (\d+) Mciclos", s)
    if m2:
        mb, te, tp, tb = (int(x) for x in m2.groups())
        if te:
            print("    coste PIC (%s): .so %+.1f %%, .so -Bsymbolic %+.1f %% (%d MB)" %
                  ("KVM" if KVM else "TCG", 100.0 * (tp - te) / te, 100.0 * (tb - te) / te, mb))
    return 0


if __name__ == "__main__":
    sys.exit(main())
