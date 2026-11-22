#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H419 - el Python del sistema con módulos al estilo CPython (D419): socket completo, subprocess,
signal, os (fork/exec/pipe/waitpid), pwd, grp, resource y selectors.

Criterio (con una CPU y con cuatro CPU y --smp):
  - /testes/python_posix.py pasa todas sus pruebas ("RESULTADO python_posix: 0 fallos"):
      * socket: TCP servidor y cliente (en un hilo y en un proceso hijo con fork), no bloqueante
        (connect_ex, EAGAIN, select, SO_ERROR), connect rechazado, timeout, UDP (sendto,
        recvfrom, recvfrom_into), getaddrinfo y compañía, selectors;
      * subprocess: run/Popen/communicate/check_output... de ls y echo con salida capturada y
        código de retorno, shell=True, tuberías, 200 KB de ida y vuelta, timeouts, terminate/kill;
      * os: fork, pipe, dup2, execv, waitpid, kill, os.open/read/write, devnull;
      * signal: alarm con tratador (dentro de un bucle, de time.sleep y de waitpid), excepciones
        desde el tratador, raise_signal, SIG_IGN, señales de otro proceso;
      * pwd, grp y resource con las cuentas de la imagen;
  - las pruebas de Python que ya existían (threading h141, red h62, biblioteca h61, b6) siguen
    pasando: se lanzan aparte (ver el informe de la rama);
  - ninguna excepción del kernel.

SAGUI_KVM=1 usa KVM.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 300 if KVM else 900
MINIMO_OK = 80   # pruebas "ok" que tiene que haber como poco (para que no pase por no ejecutar nada)


def maquina(nome, extras, nucleos):
    imagem = imagem_de_teste("", ["--vesa", "--console"] + extras)
    with Maquina(nome, "ide", imagem=imagem, nucleos=nucleos) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("python /testes/python_posix.py", tempo=TEMPO)
        for l in out.split("\n"):
            if "FALLO" in l or l.startswith("Traceback") or l.startswith("  File"):
                print("    %s" % l.strip())
        oks = len(re.findall(r"^POSIXPY ok ", out, re.M))
        verificar("RESULTADO python_posix: 0 fallos" in out, "%s: python_posix.py pasa todas sus pruebas (%d ok)" % (nome, oks))
        verificar("POSIXPY FALLO" not in out, "%s: ninguna prueba falla" % nome)
        verificar(oks >= MINIMO_OK, "%s: se ejecutaron al menos %d comprobaciones (%d)" % (nome, MINIMO_OK, oks))
        # el programa debe haber acabado limpio, y sin hijos huérfanos que dejen el prompt colgado
        verificar("terminou com codigo 0" in out, "%s: python acaba con codigo 0" % nome)
        verificar("EXCECAO" not in m.serial(), "%s: sin excepciones del kernel" % nome)


def main():
    maquina("h419-1", [], 1)
    maquina("h419-smp", ["--smp"], 4)
    return 0


if __name__ == "__main__":
    sys.exit(main())
