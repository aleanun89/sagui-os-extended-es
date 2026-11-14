#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H40 - SSE2 nas rotas quentes de gráficos e memória (D042).

Critério:
  - o kernel mede no boot rep movsd x SSE2 e escolhe ([RAPIDO] no log);
  - /testes/sse2: memcpy, memmove, memset, preencher32, ampliar2x32 e
    escurecer32 com rep e com SSE2 dão o mesmo resultado que os laços em C,
    em muitos tamanhos e alinhamentos, sem escrever fora do lugar;
  - "sagui3d --medir 200" desenha exatamente os mesmos pixels que o Sagui 3D
    de antes do SSE2 (a soma é a mesma, com e sem SSE2);
  - dois processos com SSE2 (dois "simd estres", que conferem XMM0-XMM6 a
    cada troca de processo) não se corrompem enquanto o compositor do kernel
    usa SSE2 (fpu_kernel_inicio/fim): uma janela é arrastada e o Sagui 3D
    desenha centenas de quadros ao mesmo tempo;
  - numa CPU sem SSE2 (-cpu pentium2) a área de trabalho funciona, o kernel
    fica com rep movsd e a libsagui também, com os mesmos resultados.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402

# Soma dos pixels de "sagui3d --medir 200" medida com o Sagui 3D de antes do
# D042 (laços em C, byte a byte). Se a arte ou o mapa da fase 1 mudarem, ela
# muda: rode o comando num build sem SSE2 (-cpu pentium2) e atualize.
SOMA_SAGUI3D = "87eca5c0"


def medir_sagui3d(m):
    antes = m.serial().count("cuadros/s")
    m.comando("sagui3d --medir 200")
    fim = time.time() + 120
    while m.serial().count("cuadros/s") <= antes and time.time() < fim:
        time.sleep(0.3)
    linhas = re.findall(r"medida: 200 cuadros en \d+ ms = ([\d.]+) cuadros/s .*suma=([0-9a-f]+)", m.serial())
    verificar(linhas, "sagui3d --medir termina e mostra a medida")
    fechar_sagui3d(m)
    return linhas[-1]


def fechar_sagui3d(m):
    """O resultado fica na janela do Sagui 3D (em foco) até um Enter."""
    antes = m.serial().count("(sagui3d) terminou")
    time.sleep(0.5)
    m.tecla("ret")
    fim = time.time() + 20
    while m.serial().count("(sagui3d) terminou") <= antes and time.time() < fim:
        time.sleep(0.2)


def janela_terminal(m):
    a = re.search(r'janela (\d+) criada por \d+: "Terminal" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())
    return int(a.group(4)), int(a.group(5))


def main():
    with Maquina("h40", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        r = re.search(r"\[RAPIDO\] escolhido: copia (\S+), preencher (\S+), tela (\S+)", s)
        verificar(r is not None, f"o kernel mediu e escolheu os métodos no boot ({r.group(0)[20:] if r else '?'})")
        verificar("SSE2" in s, "a CPU padrão do QEMU tem SSE2")

        out = m.comando("/testes/sse2", tempo=120)
        verificar("la CPU tiene SSE2" in out, "sse2: a libsagui vê o SSE2 pelo CPUID")
        verificar(re.search(r"sse2: ok \(\d+ comparaciones, metodos rep(?:, \w+)*, sse2(?:, \w+)* y auto\)", out),
                  "sse2: rep e SSE2 iguais aos laços em C em todos os tamanhos e alinhamentos")

        fps, soma = medir_sagui3d(m)
        verificar(soma == SOMA_SAGUI3D, f"Sagui 3D: mesmos pixels de antes do SSE2 (soma {soma}), {fps} cuadros/s no QEMU")

        # Dois processos com XMM e o compositor do kernel com SSE2 ao mesmo
        # tempo. O de segundo plano escreve num arquivo (um programa em
        # segundo plano escrevendo no Terminal derruba o sagsh: ver D042).
        m.comando("simd estres 5 800 > /tmp/estres5.txt &")
        m.comando("sagui3d --medir 3000")
        pid3d = re.findall(r"executar /bin/sagui3d -> processo (\d+)", m.serial())[-1]
        x, y = janela_terminal(m)
        m.arrastar(x + 80, y + 10, x + 40, y + 60)    # o Terminal vem para a frente, vai e volta
        m.arrastar(x + 40, y + 60, x + 80, y + 10)
        out = m.comando("simd estres 9 200", tempo=120)
        verificar("estres 9: ok" in out, "estres 9 (primeiro plano): XMM intactos com o compositor usando SSE2")
        m.esperar_serial("medida: 3000 cuadros", 120)
        verificar(True, "o Sagui 3D desenhou 3000 quadros ao mesmo tempo (atrás do Terminal)")
        m.comando(f"kill {pid3d}")
        fim = time.time() + 60
        while m.serial().count("(simd) terminou") < 2 and time.time() < fim:
            time.sleep(0.5)
        out = m.comando("cat /tmp/estres5.txt")
        verificar("estres 5: ok" in out, "estres 5 (segundo plano): XMM intactos")
        verificar("FALLO" not in m.serial(), "nenhum FALLO no log")
        m.captura("H40-sse2-escritorio")

    with Maquina("h40-pentium2", "ide", extras=["-cpu", "pentium2"]) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("area de trabalho no ar" in s, "pentium2: a área de trabalho abre sem SSE2")
        verificar("[RAPIDO] sem SSE:" in s, "pentium2: o kernel fica com rep movsd/stosd")
        out = m.comando("/testes/sse2", tempo=120)
        verificar("la CPU no tiene SSE2" in out and re.search(r"sse2: ok \(\d+ comparaciones, metodos rep y auto\)", out),
                  "pentium2: a libsagui usa rep e os resultados são os mesmos")
        fps, soma = medir_sagui3d(m)
        verificar(soma == SOMA_SAGUI3D, f"pentium2: Sagui 3D com os mesmos pixels (soma {soma}), {fps} cuadros/s")
        m.captura("H40-pentium2-escritorio")
    return 0


if __name__ == "__main__":
    sys.exit(main())
