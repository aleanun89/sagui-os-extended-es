#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H191 (D191) - lo que va dentro del SaguiOS no ve las cabeceras del anfitrión.

GCC añade CPATH y C_INCLUDE_PATH incluso con -nostdinc, y el env.sh del entorno
local define CPATH con la glibc del anfitrión. El Makefile los anula. Esta prueba:

  1. con CPATH y C_INCLUDE_PATH apuntando a una carpeta trampa, una receta de
     make no los ve en su entorno (se pide a make que imprima `env`);
  2. toma todas las órdenes de compilación de C del build (`make -Bn`, con y sin
     SEM_GPL), con el mismo entorno que les da make, y pide a GCC sus
     dependencias (-M): ninguna puede salir de fuera del repositorio ni del
     include propio de GCC.

No compila nada ni usa QEMU. Uso: h191_cpath.py
"""
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import verificar, RAIZ  # noqa: E402

CABECERAS = ["stdint.h", "stdlib.h", "stdio.h", "string.h", "time.h", "math.h",
             "stddef.h", "stdarg.h", "limits.h", "features.h", "mm_malloc.h", "sys/types.h"]


def main():
    with tempfile.TemporaryDirectory() as trampa:
        for h in CABECERAS:
            ruta = os.path.join(trampa, h)
            os.makedirs(os.path.dirname(ruta), exist_ok=True)
            with open(ruta, "w") as f:
                f.write("#error cabecera del anfitrion\n")
        env = dict(os.environ, CPATH=trampa, C_INCLUDE_PATH=trampa)
        # 1. el entorno de las recetas
        mk = os.path.join(trampa, "env.mk")
        with open(mk, "w") as f:
            f.write("include %s/Makefile\nenv-h191:\n\t@env\n" % RAIZ)
        r = subprocess.run(["make", "-f", mk, "env-h191"], cwd=RAIZ, capture_output=True, text=True, env=env)
        verificar(r.returncode == 0, "make env-h191 " + r.stderr[-300:])
        verificar(not re.search(r"^(C_INCLUDE_PATH|CPATH)=", r.stdout, re.M),
                  "las recetas de make no reciben CPATH ni C_INCLUDE_PATH")
        # 2. dependencias reales, con el entorno que tienen las recetas
        env = {k: v for k, v in os.environ.items() if k not in ("CPATH", "C_INCLUDE_PATH")}
        gcc_inc = subprocess.run(["gcc", "-print-file-name=include"], capture_output=True,
                                 text=True, env=env).stdout.strip()
        n = 0
        malas = []
        for extra in ([], ["SEM_GPL=1"]):
            r = subprocess.run(["make", "-Bn"] + extra, cwd=RAIZ, capture_output=True, text=True, env=env)
            verificar(r.returncode == 0, "make -Bn " + " ".join(extra) + " " + r.stderr[-300:])
            for linea in r.stdout.splitlines():
                m = re.search(r" -c -o (\S+) (\S+)$", linea)
                # solo las órdenes de gcc para el SaguiOS (las del anfitrión no llevan -m32)
                if not m or not re.match(r"(?:\w+=\S* )*gcc ", linea) or " -m32" not in linea:
                    continue
                if not os.path.exists(os.path.join(RAIZ, m.group(2))):
                    continue  # fuente generada por el build, aún no existe
                orden = linea.replace(" -c -o " + m.group(1), " -M -MG -o /dev/stdout")
                # la línea puede traer su propio CPATH= (QuickJS): se mantiene
                p = subprocess.run(orden, shell=True, cwd=RAIZ, capture_output=True, text=True, env=env)
                if p.returncode != 0:
                    verificar(False, "gcc -M " + m.group(2) + " " + p.stderr[-300:])
                n += 1
                for d in re.findall(r"(\S+\.h)", p.stdout.replace("\\\n", " ")):
                    if os.path.isabs(d) and not d.startswith(RAIZ) and not d.startswith(gcc_inc):
                        malas.append((m.group(2), d))
        verificar(n > 500, "se revisaron las órdenes de compilación (%d)" % n)
        verificar(not malas, "ninguna cabecera sale del anfitrión: %s" % malas[:5])
    print("H191 OK (%d compilaciones revisadas)" % n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
