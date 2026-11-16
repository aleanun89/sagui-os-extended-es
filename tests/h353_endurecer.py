#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H353 - endurecimiento tras la revisión de la D349 (D351).

Criterio:
  1. La barrera del espacio compartido: /testes/endurecer (dos hilos leen sin
     parar trozos de 256 KB del disco, uno en el área de datos y otro en su
     pila sin tocar, mientras el principal quita y devuelve la escritura del
     buffer y lo devuelve y reasigna) termina sin excepción del kernel, con
     una CPU y con cuatro (--smp), y la lectura final trae los mismos datos.
     Antes de la D351, el kernel fallaba al copiar en el buffer protegido o
     devuelto mientras la llamada dormía en el disco.
  2. W^X: ningún ELF de build/userland ni de build/apps/raiz tiene un
     segmento cargable con escritura y ejecución a la vez (antes, los que
     tenían .init_array: C++, NetSurf, gprolog...).
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, BUILD  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 240 if KVM else 600


def elfs_rwx():
    """(ELF ejecutables mirados, los que tienen un PT_LOAD RWE)."""
    vistos, rwx = 0, []
    for raiz in (os.path.join(BUILD, "userland"), os.path.join(BUILD, "apps", "raiz")):
        for d, _, fs in os.walk(raiz):
            for f in fs:
                c = os.path.join(d, f)
                try:
                    with open(c, "rb") as h:
                        cab = h.read(18)
                except OSError:
                    continue
                if len(cab) < 18 or cab[:4] != b"\x7fELF" or cab[16] != 2:   # ET_EXEC
                    continue
                vistos += 1
                s = subprocess.run(["readelf", "-lW", c], capture_output=True, text=True).stdout
                if any(l.split()[0] == "LOAD" and "RWE" in l for l in s.splitlines() if l.strip()):
                    rwx.append(os.path.relpath(c, BUILD))
    return vistos, rwx


def segmentos(rel):
    s = subprocess.run(["readelf", "-lW", os.path.join(BUILD, rel)], capture_output=True, text=True).stdout
    return [" ".join(l.split()[1:]) for l in s.splitlines() if l.strip().startswith("LOAD")]


def correr(nome, extras, nucleos):
    with Maquina(nome, "ide", imagem=imagem_de_teste("/testes/endurecer", extras), nucleos=nucleos) as m:
        m.esperar_serial("RESULTADO endurecer:", TEMPO)
        s = m.serial().replace("\r", "")
        for l in s.split("\n"):
            if l.startswith("ENDURECER") or "[FALHA]" in l or "EXCECAO" in l:
                print("    %s" % l.strip())
        verificar("RESULTADO endurecer: SUCESSO (0 falhas)" in s, "%s: /testes/endurecer paso todas sus pruebas" % nome)
        verificar("EXCECAO" not in s, "%s: sin excepciones del kernel" % nome)
        return s


def main():
    vistos, rwx = elfs_rwx()
    for r in rwx:
        print("    RWX: %s" % r)
    for rel in ("userland/bin/dosbox", "userland/bin/netsurf", "userland/bin/gprolog", "userland/testes/ctors"):
        if os.path.exists(os.path.join(BUILD, rel)):
            print("    %s: %s" % (rel, " | ".join(segmentos(rel))))
    verificar(vistos > 100 and not rwx, "W^X: ningun segmento RWX en %d ELF de build/" % vistos)

    correr("h353-1", [], 1)
    s = correr("h353-smp", ["--smp"], 4)
    verificar("CPU 3 arrancou" in s, "con --smp arrancan los secundarios")
    return 0


if __name__ == "__main__":
    sys.exit(main())
