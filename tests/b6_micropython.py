#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""B6 - MicroPython dentro do SaguiOS.

Critério: python roda um arquivo .py (funções, classes, listas, dicionários,
inteiros grandes), roda código com -c, lê a entrada com input() (por um
pipe), o REPL avalia expressões e blocos de várias linhas, mostra o
traceback de uma exceção sem cair, sys.exit(n) devolve o código n ao shell
e Ctrl+D numa linha vazia fecha o REPL.

Ponto flutuante (D038): divisão verdadeira, potência fracionária, módulo
math, formatação com %, format e f-strings, o módulo time, os exemplos
mandelbrot.py e flotante.py, e dois python fazendo contas em double ao mesmo
tempo (um em segundo plano), cada um com o seu estado de FPU/SSE."""
import os
import sys
import time
import types

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402


def esperar_prompt(m, antes, texto=">>> ", tempo=30):
    fim = time.time() + tempo
    while m.serial().count(texto) <= antes:
        if time.time() > fim:
            raise Exception(f"o REPL não mostrou {texto!r}")
        time.sleep(0.2)


def linha_repl(m, texto, prompt=">>> "):
    antes = m.serial().count(prompt)
    m.digitar(texto + "\n", 0.04)
    esperar_prompt(m, antes, prompt)


def mandelbrot_esperado():
    """As linhas que /ejemplos/mandelbrot.py deve desenhar, calculadas aqui
    pelo CPython (as contas em double do IEEE 754 dão o mesmo resultado)."""
    caminho = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "rootfs", "ejemplos", "mandelbrot.py")
    linhas = []
    falso_time = types.ModuleType("time")
    falso_time.ticks_ms = lambda: 0
    falso_time.ticks_diff = lambda a, b: a - b
    antigo = sys.modules.get("time")
    sys.modules["time"] = falso_time
    try:
        exec(open(caminho).read(), {"__name__": "__main__", "print": lambda *a: linhas.append(" ".join(map(str, a)))})
    finally:
        sys.modules["time"] = antigo
    return linhas[:-1]   # a última é o tempo


def main():
    with Maquina("b6", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)

        out = m.comando("python /ejemplos/hola.py", tempo=90)
        verificar("Hola desde MicroPython en SaguiOS!" in out, "python /ejemplos/hola.py")
        verificar("Fibonacci: [0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89]" in out, "funções e listas por compreensão")
        verificar("2**100 = 1267650600228229401496703205376" in out, "inteiros grandes")
        verificar("El sagui Tito come bananas" in out, "classes")
        verificar("('sagui', 2)" in out, "dicionários e sorted")
        verificar("(python) terminou com codigo 0" in out, "o processo termina com código 0")
        m.captura("B6-arquivo")

        out = m.comando("python -c 'print(sum(range(101)), len(\"sagui\"))'")
        verificar("5050 5" in out, "python -c")
        m.comando("echo 'nomes = [\"tito\", \"ana\"]' > /tmp/b.py")
        m.comando("echo 'print(\" e \".join(n.upper() for n in nomes))' >> /tmp/b.py")
        out = m.comando("python /tmp/b.py")
        verificar("TITO e ANA" in out, "um .py gravado no sistema (echo >> arquivo)")
        out = m.comando("echo Tito | python -c 'print(\"oi, \" + input())'")
        verificar("oi, Tito" in out, "input() lê a entrada (pipe)")

        # REPL
        antes = m.serial().count(">>> ")
        m.digitar("python\n", 0.04)
        esperar_prompt(m, antes)
        linha_repl(m, "x = 6 * 7")
        linha_repl(m, "x")
        linha_repl(m, "for i in range(3):", "... ")
        linha_repl(m, "  print('linha', i * x)", "... ")
        linha_repl(m, "")
        linha_repl(m, "[1, 2][5]")
        s = m.serial()
        verificar("\n42" in s.replace("\r", ""), "o REPL mostra o valor de uma expressão")
        verificar("linha 0" in s and "linha 42" in s and "linha 84" in s, "bloco de várias linhas no REPL (...)")
        verificar("IndexError" in s and "Traceback" in s, "exceção mostra o traceback e o REPL continua")
        m.captura("B6-repl")
        m.tecla("ctrl-d")
        m.esperar_serial("(python) terminou com codigo 0", 20)
        verificar(True, "Ctrl+D numa linha vazia fecha o REPL")
        time.sleep(1)
        out = m.comando("python -c 'import sys; sys.exit(3)'")
        verificar("(python) terminou com codigo 3" in out, "sys.exit(3) devolve o código 3 ao shell")

        # Ponto flutuante (D038)
        out = m.comando("python -c 'print(1/3); print(2**0.5); print(7/2, -7//2, int(-3.99), round(2.5), float(\"1e3\"))'")
        verificar("0.3333333333333333" in out, "1/3 é divisão verdadeira (float)")
        verificar("1.414213562373095" in out, "2**0.5")
        verificar("3.5 -4 -3 2 1000.0" in out, "/, //, int(), round() e float()")
        out = m.comando("python -c 'import math; print(math.sqrt(2)); print(\"%.3f\" % math.pi); print(f\"{math.sin(math.pi/6):.4f}\")'")
        verificar("1.414213562373095" in out, "math.sqrt(2)")
        verificar("3.142" in out, "'%.3f' % math.pi")
        verificar("0.5000" in out, "f-string com math.sin(math.pi/6):.4f")
        out = m.comando("python -c 'import math; print(\"{:.2f}\".format(math.e), math.floor(-2.5), math.log2(8), math.isclose(0.1+0.2, 0.3), \"%.4f\" % math.atan2(1, 1))'")
        verificar("2.72 -3 3.0 True 0.7854" in out, "format, floor, log2, isclose e atan2")

        out = m.comando("python /ejemplos/mandelbrot.py", tempo=120)
        esperado = mandelbrot_esperado()
        iguais = sum(1 for l in esperado if l.rstrip() and l.rstrip() in out)
        verificar(len(esperado) == 24 and iguais == sum(1 for l in esperado if l.rstrip()),
                  f"mandelbrot.py desenha as 24 linhas esperadas ({iguais} conferidas)")
        verificar("Mandelbrot 78x24" in out and " ms" in out, "mandelbrot.py mede o tempo com time.ticks_ms")
        m.captura("B6-mandelbrot")
        out = m.comando("python /ejemplos/flotante.py", tempo=120)
        verificar("pi (Machin)                   = 3.14159265358979" in out, "flotante.py: pi pela fórmula de Machin")
        verificar("   45    0.7071    0.7071    1.0000" in out, "flotante.py: tabela de seno, cosseno e tangente")
        verificar("Bucle de 100000 operaciones float:" in out, "flotante.py: medida do bucle")

        # Dois processos com double ao mesmo tempo: o kernel guarda os
        # registradores da FPU/SSE de cada um (D032). O de segundo plano
        # demora mais que o de primeiro plano, para que rodem juntos.
        # D172: con KVM la CPU es tan rápida que 4 M de vueltas terminaban antes de que
        # el shell acabase de recibir el comando del primer plano (el prompt de fin del
        # segundo plano se tomaba por el del primero): con KVM, 10 veces más vueltas
        # (se compara la parte entera: el redondeo de la suma de 40 M de términos difiere en las últimas cifras).
        kvm = os.environ.get("SAGUI_KVM") == "1"
        n_bg, suma_bg = (40000000, "252982217556.") if kvm else (4000000, "8000001499.688")
        m.comando("echo 'import math; print(\"bg %%.3f\" %% sum(math.sqrt(i) * 1.5 for i in range(1, %d)))' > /tmp/bg.py" % (n_bg + 1))
        m.comando("echo 'import math; print(\"fg %.6f\" % sum(math.sin(i / 1000) for i in range(1, 200001)))' > /tmp/fg.py")
        m.comando("python /tmp/bg.py &")
        out = m.comando("python /tmp/fg.py", tempo=180)
        verificar("fg 512.375634" in out, "python em primeiro plano: soma de senos correta")
        verificar("bg " + suma_bg not in m.serial(), "os dois python rodaram ao mesmo tempo")
        m.esperar_serial("bg " + suma_bg, 300)
        verificar(True, "python em segundo plano: soma de raízes correta")
        time.sleep(1)
        out = m.comando("echo o shell segue vivo")
        verificar("o shell segue vivo" in out, "o shell continua funcionando")
    return 0


if __name__ == "__main__":
    sys.exit(main())
