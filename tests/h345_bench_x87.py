#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H345 - benchmark: cargas de coma flotante con x87 en CPU sin SSE2 (D343).

Criterio:
  - `-cpu pentium2` (sin SSE2, TCG): `benchmark --mono --rapido` da OK en mandel y ncuerpos (estado=1,
    sin errores de comprobación, puntos > 0), marcadas x87=1, y el texto las llama «(x87)»;
  - `benchmark --sumas`: las dos sumas x87 coinciden con las de SSE2 (0x0271ec36 y 0x00028508);
  - con la CPU por defecto (con SSE2) las mismas cargas salen como siempre: estado=1 y x87=0, sin «(x87)».
No usa KVM: -cpu pentium2 no es posible con él.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

SUMAS = {"mandel": 0x0271ec36, "ncuerpos": 0x00028508}


def correr(nombre, args, final, cpu=None, espera=900):
    extras = ["-cpu", cpu] if cpu else []
    with Maquina(nombre, "ide", imagem=imagem_de_teste("/bin/benchmark " + args), extras=extras) as m:
        m.esperar_serial(final, espera)
        time.sleep(1.0)
        s = m.serial().replace("\r", "")
    verificar("EXCECAO" not in s, f"{nombre}: sin excepciones del kernel")
    return s


def cargas(s):
    r = {}
    for m in re.finditer(r"benchmark: carga (\S+) estado=(\d+) mono=(\d+) .*?x87=(\d)", s):
        r[m.group(1)] = dict(estado=int(m.group(2)), mono=int(m.group(3)), x87=int(m.group(4)))
    return r


def main():
    if os.environ.get("SAGUI_KVM") == "1":
        print("  SKIP: h345 usa -cpu pentium2 y TCG (sin SAGUI_KVM)")
        return 0
    # ---- sumas con la FPU x87
    s = correr("h345-sumas", "--sumas", "SUMA llamadas", "pentium2")
    for n, v in SUMAS.items():
        m = re.search(r"^SUMA %s \(x87\) 0x([0-9a-f]{8})" % n, s, re.M)
        verificar(m is not None, f"pentium2: --sumas informa {n} (x87)")
        verificar(int(m.group(1), 16) == v, f"pentium2: la suma x87 de {n} coincide con la de SSE2 (0x{m.group(1)})")
    # ---- medición completa de un núcleo sin SSE2
    s = correr("h345-p2", "--mono --rapido", "benchmark: total", "pentium2")
    c = cargas(s)
    for n in SUMAS:
        verificar(n in c and c[n]["estado"] == 1 and c[n]["mono"] > 0 and c[n]["x87"] == 1,
                  f"pentium2: {n} da OK con x87 ({c.get(n)})")
        verificar(f"{n} (x87)" in s, f"pentium2: el texto dice «{n} (x87)»")
    verificar("n/d" not in s and "ERROR" not in s, "pentium2: ninguna carga n/d ni ERROR")
    print("    pentium2 (x87): mandel %d, ncuerpos %d puntos" % (c["mandel"]["mono"], c["ncuerpos"]["mono"]))
    # ---- CPU por defecto: como antes
    s = correr("h345-sse2", "--mono --rapido", "benchmark: total")
    c = cargas(s)
    for n in SUMAS:
        verificar(n in c and c[n]["estado"] == 1 and c[n]["x87"] == 0, f"CPU por defecto: {n} sigue con SSE2 ({c.get(n)})")
    verificar("(x87)" not in s, "CPU por defecto: no aparece «(x87)»")
    print("    CPU por defecto (SSE2): mandel %d, ncuerpos %d puntos" % (c["mandel"]["mono"], c["ncuerpos"]["mono"]))
    print("OK h345")
    return 0


if __name__ == "__main__":
    sys.exit(main())
