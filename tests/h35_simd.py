#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H35 - FPU x87, MMX, SSE e SSE2 nos programas (D032).

Critério: o kernel detecta a CPU pelo CPUID e liga a FPU/SSE; o programa
`simd` faz contas com x87, MMX, SSE e SSE2 e acerta; dois processos `simd
estres` rodando juntos (cada um com um padrão diferente em XMM0-XMM6, e
dormindo 1 ms a cada volta para forçar a troca de processo) terminam com
os registradores intactos, o que prova que o kernel salva o estado de cada
processo; uma exceção de ponto flutuante desmascarada (SSE ou x87) encerra
só o programa. Numa CPU sem SSE (-cpu pentium2: i686 com MMX e FXSR; o
kernel é compilado para i686 e não roda num Pentium) o sistema dá boot, o
x87 e o MMX funcionam, o `simd` diz que não há SSE e o `python` usa a
variante x87 (D082).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402


def main():
    with Maquina("h35", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[CPU] " in s and "SSE2" in s, "o kernel detecta a CPU e lista SSE2 no log")

        out = m.comando("simd")
        for nome in ("x87", "MMX", "SSE", "SSE2"):
            verificar(any(l.split()[:2] == [nome, "ok"] for l in out.splitlines()), f"simd: {nome} ok")
        verificar("sqrt(2) * 1000 = 1414" in out, "x87: fsqrt")
        verificar("paddw = 11 22 33 44" in out, "MMX: paddw")
        verificar("mulps = 3 5 7 9" in out, "SSE: mulps")
        verificar("pi (double) = 3.1415" in out, "SSE2: double compilado pelo GCC")
        verificar("simd: todo correcto" in out, "simd: todo correcto")
        m.captura("H35-simd")

        m.comando("simd estres 7 300 &")
        out = m.comando("simd estres 9 300", tempo=120)
        verificar("estres 9: ok" in out, "estres 9 (primeiro plano): XMM intactos")
        m.esperar_serial("estres 7: ok", 120)
        verificar("estres 7: FALLO" not in m.serial(), "estres 7 (segundo plano): XMM intactos")

        out = m.comando("simd excecao")
        # O QEMU sem KVM não gera o #XM do SSE; o simd então tenta a FPU x87 (#MF).
        verificar("Error de coma flotante (SIMD)" in out or "Error de coma flotante (x87)" in out,
                  "exceção de ponto flutuante desmascarada encerra só o programa")
        out = m.comando("simd")
        verificar("simd: todo correcto" in out, "depois da exceção o shell e o SIMD seguem funcionando")

    with Maquina("h35", "ide", imagem=imagem_console(), extras=["-cpu", "pentium2"]) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("simd")
        verificar("x87    ok" in out and "MMX    ok" in out, "pentium2: x87 e MMX ok")
        verificar("SSE    no disponible" in out, "pentium2: sem SSE, e o simd avisa")
        out = m.comando("simd estres 1 5")
        verificar("no tiene SSE2" in out, "pentium2: simd estres recusa sem SSE2")
        # O python (MicroPython com double pelo SSE2, D038) confere o CPUID
        # antes de qualquer conta e, sem SSE2, executa a variante x87 (D082).
        out = m.comando("python -c 'print(1/3)'")
        verificar("executar /apps/bin/python-x87" in out and "0.3333333333333333" in out and "terminou com codigo 0" in out,
                  "pentium2: sem SSE2, python usa /apps/bin/python-x87 e calcula 1/3")
    return 0


if __name__ == "__main__":
    sys.exit(main())
