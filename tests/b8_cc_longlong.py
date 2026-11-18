#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""B8 - long long no compilador de C (cc) dentro do SaguiOS (D128).

Critério: dentro do SaguiOS, o cc compila um programa com long long e
unsigned long long (aritmética, divisões com e sem sinal, deslocamentos,
comparações, conversões com double, estruturas, vetores, funções,
printf %lld/%llu/%llx/%llo e números de 64 bits escritos à mão) e a saída é
idêntica à do mesmo fonte compilado com GCC no computador de desenvolvimento,
tanto com SSE2 (por padrão) como com cc -x87; um programa sem long long não
cresce (a biblioteca de 64 bits só entra quando é preciso); e um erro de
tipos mostra arquivo:linha."""
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, RAIZ  # noqa: E402

ROOTFS = os.path.join(RAIZ, "rootfs")


def saida_gcc(fonte, *args):
    """Compila o fonte do rootfs com GCC (no computador de desenvolvimento)
    e devolve a sua saída."""
    with tempfile.TemporaryDirectory() as d:
        exe = os.path.join(d, "prog")
        subprocess.run(["gcc", "-w", "-O0", "-o", exe, os.path.join(ROOTFS, fonte)],
                       check=True)
        return subprocess.run([exe, *args], capture_output=True, text=True, check=True).stdout


def saida_programa(out):
    """As linhas que escreveu o programa: sem o eco do comando, os avisos do
    kernel nem o prompt."""
    linhas = out.split("\n")[1:]
    return [l for l in linhas if l and not l.startswith("[SAGUI]") and not l.startswith("sagui:")]


def comparar(m, comando, fonte, nome, *args):
    out = m.comando(comando, tempo=150)
    nossa = saida_programa(out)
    esperada = [l for l in saida_gcc(fonte, *args).splitlines() if l]
    if nossa != esperada:
        for a, b in zip(nossa + [""] * len(esperada), esperada + [""] * len(nuestra)):
            if a != b:
                print(f"    SaguiOS: {a!r}\n    GCC:     {b!r}")
                break
    verificar(nossa == esperada, nome)
    return out


def main():
    with Maquina("b8", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("cd /tmp")

        out = m.comando("cc /testes/cc_longlong.c -o ll", tempo=150)
        verificar("(cc) terminou com codigo 0" in out, "cc compila um programa com long long (SSE2)")
        comparar(m, "./ll", "testes/cc_longlong.c",
                 "long long: aritmética, divisões, deslocamentos, comparações, double, structs e vetores = GCC (SSE2)")
        m.captura("B8-longlong")

        out = m.comando("cc -x87 /testes/cc_longlong.c -o ll87", tempo=150)
        verificar("(cc) terminou com codigo 0" in out, "cc -x87 compila o mesmo programa")
        comparar(m, "./ll87", "testes/cc_longlong.c",
                 "com a FPU x87 (cc -x87) a saída do long long também é a de GCC")

        # Um programa sem long long não leva a biblioteca de 64 bits.
        out = m.comando("cc /ejemplos/hola.c -o hola", tempo=150)
        verificar("(cc) terminou com codigo 0" in out, "cc compila /ejemplos/hola.c")
        out = m.comando("ls -s")
        tam = re.search(r"^hola\s+(\d+) bytes", out, re.M)
        print(f"    hola: {tam.group(1) if tam else '?'} bytes")
        verificar(tam and int(tam.group(1)) < 8000, "hola.c continua pequeno (a biblioteca de 64 bits só entra se for preciso)")

        # Erro de tipos: arquivo e linha.
        m.comando("echo 'int main() { long long a = 1; return a[0]; }' > /tmp/mal.c")
        out = m.comando("cc mal.c")
        verificar("mal.c:1: error:" in out, "erro de compilação com arquivo:linha")
        verificar("(cc) terminou com codigo 1" in out, "cc termina com código 1 no erro")
        out = m.comando("echo o shell segue vivo")
        verificar("o shell segue vivo" in out, "o shell continua funcionando")
    return 0


if __name__ == "__main__":
    sys.exit(main())
