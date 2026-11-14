#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H170 - el malloc de la libsagui devuelve la memoria al sistema (D209, mejora 094 B).

Criterio:
  - /testes/memoria estres: 200.000 operaciones al azar de aloca/libera/realoca
    sin errores; tras liberarlo todo, la memoria usada del sistema vuelve casi a
    la de antes (lo grande vuelve al kernel); 64 MB en bloques de 1 MB, liberados,
    vuelven enteros;
  - Python: un bytearray de 30 MB, borrado y recogido, devuelve la memoria al
    sistema con Python todavía vivo;
  - mmap/munmap anónimos y mmap de un fichero (mismo contenido que leerlo).
Imprime las medidas para compararlas (docs/decisoes.md, D209).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402


def usada(out):
    return [int(x) for x in re.findall(r"Usada:\s+(\d+) KB", out)]


def main():
    medir_so = os.environ.get("SAGUI_MEDIR") == "1"   # solo medir (antes del cambio)
    with Maquina("h170", "ide", imagem=imagem_console(), memoria="512M") as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("/testes/memoria estres", tempo=300)
        print(out)
        verificar("estres: ok" in out, "estres de aloca/libera/realoca sin errores")
        g = re.search(r"fragmentacion (\d+) ms, pico (\d+) KB, tras liberar (\d+) KB", out)
        gr = re.search(r"grandes (\d+) ms, con 64 MB (\d+) KB, tras liberar (\d+) KB", out)
        if not medir_so:
            verificar(g and int(g.group(3)) < 3072, "tras la fragmentacion lo liberado vuelve (queda la cache: < 3 MB)")
            verificar(gr and int(gr.group(2)) >= 60000 and int(gr.group(3)) < 3072,
                      "64 MB en bloques de 1 MB vuelven al sistema al liberarlos")

        out = m.comando("python -c \"import os,gc;os.system('mem');b=bytearray(30000000);"
                        "os.system('mem');del b;gc.collect();os.system('mem')\"", tempo=120)
        print(out)
        u = usada(out)
        verificar(len(u) == 3, f"python: tres medidas de memoria {u}")
        if len(u) == 3:
            print(f"python: antes {u[0]} KB, con 30 MB {u[1]} KB, tras borrar {u[2]} KB")
            verificar(u[1] - u[0] > 25000, "python: el bytearray de 30 MB se nota")
            if not medir_so:
                verificar(u[2] - u[0] < 4096, "python: al borrarlo la memoria vuelve al sistema (Python sigue vivo)")

        if not medir_so:
            out = m.comando("/testes/memoria mmap", tempo=120)
            print(out)
            verificar("mmap: ok" in out, "mmap/munmap anonimos y de fichero")
    return 0


if __name__ == "__main__":
    sys.exit(main())
