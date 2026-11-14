#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H208 - SSE, SSE3 y SSSE3 opcionales, elegidos por CPUID (D208).

Criterio, con tres CPU de QEMU (TCG: la CPU es la que se pide, no la del anfitrión):
  - -cpu pentium2 (sin SSE): el escritorio arranca, el kernel registra "[RAPIDO] sem SSE",
    /testes/sse2 da lo mismo con rep, infopc dice "kernel (gráficos): rep movsd/stosd";
  - -cpu pentium3 (SSE sin SSE2): el kernel mide y elige una variante SSE (no SSE2), la libsagui
    solo usa SSE, /testes/sse2 prueba rep y sse contra los bucles en C, infopc lo cuenta;
  - -cpu core2duo (SSE3 y SSSE3): /testes/sse2 prueba rep, sse, sse2, sse3 y ssse3 (copias,
    rellenos, ampliar, trocar_rb32 con pshufb, misturar32) contra los bucles en C;
  - -cpu max (moderna): arranca y el infopc lista las variantes;
  - en todas el escritorio funciona y la captura es igual a la de la CPU moderna (la barra
    superior, con el reloj, queda fuera de la comparación).
Los programas que de verdad necesitan SSE2 ya avisaban (h65): aquí se vigila además que el
sgldemo diga su mensaje en pentium3 en vez de caer con #UD.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402
from h65_compat_antiguos import tela_ppm, parecidas, area_de_trabalho  # noqa: E402

CPUS = [
    # (cpu, rótulo, [RAPIDO] esperado, variantes de /testes/sse2, variantes que infopc no debe nombrar)
    ("pentium2", "p2", r"\[RAPIDO\] sem SSE:", "rep y auto", ()),
    ("pentium3", "p3", r"\[RAPIDO\] escolhido: copia (rep|sse|sse-nt),", "rep, sse y auto", ("SSE2", "SSE3", "SSSE3")),
    ("core2duo", "c2", r"\[RAPIDO\] escolhido: copia (rep|sse|sse-nt|sse2|sse2-nt|sse3|sse3-nt),",
     "rep, sse, sse2, sse3, ssse3 y auto", ()),
    ("max", "mx", r"\[RAPIDO\] escolhido: copia ", None, ()),
]


def probar(cpu, rot, rapido, variantes, prohibidas, ref):
    with Maquina(f"h208{rot}", "ide", extras=["-cpu", cpu]) as m:
        area_de_trabalho(m, cpu)
        img = tela_ppm(m, "pantalla")   # antes de ejecutar nada: el terminal aún está igual en todas
        s = m.serial()
        verificar(re.search(rapido, s), f"[{cpu}] el kernel registra su elección ([RAPIDO])")
        verificar("EXCECAO" not in s, f"[{cpu}] ninguna excepción en el arranque")
        if variantes:
            out = m.comando("/testes/sse2", tempo=1500)
            verificar(re.search(r"sse2: ok \(\d+ comparaciones, metodos " + variantes + r"\)", out),
                      f"[{cpu}] /testes/sse2: {variantes} iguales a los bucles en C")
        out = m.comando("infopc -t cpu", tempo=120)
        verificar("Rutas rápidas:" in out and "kernel (gráficos):" in out and "programas:" in out,
                  f"[{cpu}] infopc dice qué variantes eligió")
        linha = " ".join(l for l in out.splitlines() if "kernel (gráficos)" in l or "programas:" in l)
        print("    ", linha.strip())
        for p in prohibidas:
            verificar(p not in linha.replace("rep/C", ""), f"[{cpu}] infopc no nombra {p} (la CPU no lo tiene)")
        if rot == "p2":
            verificar("rep movsd/stosd" in linha, f"[{cpu}] el kernel sigue con rep movsd/stosd")
        if rot == "c2":
            verificar(re.search(r"^  .*\bSSSE3\b", out, re.M) and "SSE3" in out,
                      f"[{cpu}] infopc ve SSE3 y SSSE3 (la variante pshufb se prueba arriba; la elige la medida, y en TCG gana SSE2)")
        if rot == "p3":
            out = m.comando("sgldemo")
            verificar("necesita una CPU con SSE2" in out, f"[{cpu}] sgldemo avisa en vez de caer con #UD")
        out = m.comando("echo fin")
        verificar("fin" in out, f"[{cpu}] el sistema sigue respondiendo")
        if ref is None:
            return img
        similar = parecidas(img, ref, (24, 24, 24))
        verificar(similar > 0.99, f"[{cpu}] la captura es igual a la de la CPU moderna ({similar:.3f})")
        return img


def main():
    ref = None
    # Primero la moderna, para tener la captura de referencia.
    for cpu, rot, rapido, variantes, prohibidas in sorted(CPUS, key=lambda c: c[0] != "max"):
        img = probar(cpu, rot, rapido, variantes, prohibidas, ref)
        if cpu == "max":
            ref = img
    print("H208: todo correcto")


if __name__ == "__main__":
    main()
