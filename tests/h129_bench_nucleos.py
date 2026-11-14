#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H129 - a maquina de provas: 2 GB de memoria e varios nucleos (D151).

Critério:
  - a maquina arranca com 2 GB por omissao (todo o que o kernel sabe gerir,
    D049) e o log diz que sao 2047 MB utilizaveis;
  - o programa /testes/bench mede a carga com um processo e com varios, e
    diz o ganho (hoje 1: o SO ainda usa so um nucleo, o SMP e o que a D150
    prepara);
  - com --nucleos 4 a maquina tem quatro CPUs (o QEMU diz que sim) e o
    benchmark continua a correr: a prova e a base para quando o SMP entrar;
  - a pontuacao da partida (a do RAPIDO do kernel) sai no registo, para
    comparar entre maquinas.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def pontuacao(serial):
    """A pontuacao do RAPIDO (ciclos por 1024 bytes da copia e do preenchimento)."""
    # D208: la línea lista los métodos que tiene la CPU («copia rep N sse N ... sse3-nt N;
    # preencher rep N sse N sse2 N; tela ...»); se toman el rep y el SSE2 de cada una.
    m = re.search(r"\[RAPIDO\] ciclos/1024: copia([^;]*); preencher([^;]*)", serial)
    if not m:
        return None
    def valor(parte, nome):
        v = re.search(r"(?:^| )%s (\d+)" % re.escape(nome), parte)
        return int(v.group(1)) if v else None
    copia, preencher = m.group(1), m.group(2)
    r = (valor(copia, "rep"), valor(copia, "sse2"), valor(preencher, "rep"), valor(preencher, "sse2"))
    return r if r[0] is not None and r[2] is not None else None


def main():
    # ---- a maquina sozinha: 2 GB por omissao ----
    # D172: pide los 2 GB a proposito, por encima de SAGUI_MEM (la bateria paralela
    # baja los QEMU a 512M y la prueba comprueba justo que la maquina ve 2 GB).
    with Maquina("h129-1", "ide", imagem=imagem_de_teste("/testes/bench 4"), memoria="2G") as m:
        m.esperar_serial("bench: ganho", 180)
        s = m.serial().replace("\r", "")
        verificar("[SAGUI] memoria_mb=2047" in s or "memoria_mb=204" in s,
                  "a maquina ve 2 GB de memoria (%s)" %
                  (re.search(r"memoria_mb=(\d+)", s).group(1) if re.search(r"memoria_mb=(\d+)", s) else "?"))
        p1 = pontuacao(s)
        verificar(p1 is not None, "a pontuacao do RAPIDO saiu no registo")

        m1 = re.search(r"bench: 1 processo: (\d+) ms", s)
        mn = re.search(r"bench: (\d+) processos: (\d+) ms", s)
        g = re.search(r"bench: ganho (\d+)\.(\d+)x", s)
        verificar(m1 is not None, "a carga com um processo foi medida")
        verificar(mn is not None, "a carga com varios processos foi medida")
        if m1 and mn:
            print("    1 processo: %s ms | %s processos: %s ms | ganho %s" %
                  (m1.group(1), mn.group(1), mn.group(2), g.group(0) if g else "?"))
            verificar(int(mn.group(1)) == 4, "foram quatro processos em paralelo")

    # ---- a maquina com quatro CPUs ----
    with Maquina("h129-4", "ide", imagem=imagem_de_teste("/testes/bench 4"), nucleos=4) as m:
        m.esperar_serial("bench: ganho", 180)
        s = m.serial().replace("\r", "")
        p4 = pontuacao(s)
        verificar(p4 is not None, "a pontuacao do RAPIDO saiu com quatro CPUs")
        verificar("bench: 1 processo:" in s, "o benchmark correu com quatro CPUs")
        if p1 and p4:
            print("    RAPIDO com 1 CPU: %s" % (p1,))
            print("    RAPIDO com 4 CPUs: %s" % (p4,))
    return 0


if __name__ == "__main__":
    sys.exit(main())
