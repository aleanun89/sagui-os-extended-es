#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H126 (D150) - as primitivas do SMP e da sincronização.

Duas partes, porque as primitivas estão em dois sítios:

  1. as do kernel (kernel/inc/atomicas.h, kernel/inc/spinlock.h e
     kernel/inc/cpu_dados.h): a autoprueba "smp" do kernel (o cabeçalho de
     arranque diz que prueba se execute). Com um só processador não se pode
     comprobar que dois núcleos compitam pelo mesmo dado, mas sim que as
     primitivas fazem o que têm de fazer: a comparação e a troca, a soma e
     a subtração devolvem o valor anterior, o cerrojo de espera ativa se
     toma e se solta, e os dados por CPU estão alinhados a 64 bytes;

  2. as do userland (userland/libsagui/sincronia.c): o programa
     /testes/smp, que exercita o mutex, o semáforo e um contador
     compartilhado com atómicas, e imprime valores exatos.

Critério:
  - a autoprueba do kernel diz "[TESTE] smp ok" e os valores que imprime
    são os esperados (comparação e troca, soma, cerrojo, CPUs);
  - o programa /testes/smp imprime os valores exatos: o contador com
    atómicas e o protegido pelo mutex suman 400 (4 "hilos" por 100
    rondas), el semáforo acaba a zero y no hay errores;
  - el programa acaba com "RESULTADO smp: SUCESSO (0 falhas)".
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def linhas(serial):
    """Las líneas del log de serie, sin los \\r."""
    return [l.strip() for l in serial.replace("\r", "").splitlines()]


def main():
    # ---- 1. as primitivas do kernel ----
    with Maquina("h126-kernel", "ide", imagem=imagem_de_teste("smp")) as m:
        m.esperar_serial("[TESTE] smp ok", 180)
        s = m.serial()
        for l in linhas(s):
            if "[TESTE]" in l:
                print("   ", l)
        verificar("[TESTE] smp ok" in s, "as primitivas atómicas do kernel passam a autoprueba")

        # os valores exatos das atómicas de 32 bits
        a = re.findall(r"\[TESTE\] atomicas: cas=(\d+) falhou=(\d+) add=(\d+) sub=(\d+) "
                       r"inc=(\d+) dec=(\d+) final=(\d+)", s)
        verificar(bool(a), "o kernel imprime os valores das atómicas de 32 bits")
        if a:
            cas, falhou, add, sub, inc, dec, final = a[-1]
            verificar(cas == "1", "a comparação e troca muda o valor")
            verificar(falhou == "0", "a comparação e troca falha quando o esperado está errado")
            verificar(add == "7" and sub == "10", "a soma e a subtração devolvem o valor anterior")
            verificar(inc == "6" and dec == "7", "o incremento e o decremento devolvem o valor anterior")
            verificar(final == "6", "o valor final é o que tem que ser")

        # as de 64 bits (só se a CPU tem cmpxchg8b)
        b = re.findall(r"\[TESTE\] atomicas64: tem=(\d+) add=(\d+) cas=(\d+) final=(\d+)", s)
        verificar(bool(b), "o kernel diz se a CPU tem cmpxchg8b")
        if b:
            tem, add64, cas64, final64 = b[-1]
            if tem == "1":
                verificar(add64 == "100", "a soma de 64 bits devolve o valor anterior")
                verificar(cas64 == "1", "a comparação e troca de 64 bits")
                verificar(final64 == "200", "o valor de 64 bits final é o que tem que ser")
            else:
                verificar(add64 == "0" and final64 == "0", "sem cmpxchg8b as operações de 64 bits não tocam nada")

        # o cerrojo de espera ativa
        c = re.findall(r"\[TESTE\] spinlock: primeiro=(\d+) segundo=(\d+) terceiro=(\d+)", s)
        verificar(bool(c), "o kernel imprime o estado do cerrojo")
        if c:
            primeiro, segundo, terceiro = c[-1]
            verificar(primeiro == "1", "o cerrojo livre se toma")
            verificar(segundo == "0", "o cerrojo tomado não se pode tomar otra vez")
            verificar(terceiro == "1", "o cerrojo fica livre depois de soltar")

        # os dados por CPU
        d = re.findall(r"\[TESTE\] cpu: atual=(\d+) numero=(\d+) pilha=(\d+)", s)
        verificar(bool(d), "o kernel imprime os dados por CPU")
        if d:
            atual, numero, pilha = d[-1]
            verificar(atual == "0", "a CPU atual é a 0 (só há um processador)")
            verificar(int(numero) >= 1, "a máquina diz ter pelo menos um processador")
            verificar(int(pilha) > 0, "cada CPU tem a sua pilha de kernel")

    # ---- 2. as primitivas do userland ----
    with Maquina("h126-userland", "ide", imagem=imagem_de_teste("/testes/smp")) as m:
        m.esperar_serial("[TESTE] programa terminou", 180)
        s = m.serial()
        for l in linhas(s):
            if l.startswith(("SMP ", "RESULTADO ", "[ok]", "[FALHA]")):
                print("   ", l)
        verificar("SMP atomicas: cas=1 falhou=0 add=7 sub=10 inc=6 dec=7 final=6" in s,
                  "as atómicas de 32 bits do programa dão os valores esperados")
        verificar(re.search(r"SMP atomicas64: tem=\d+ add=\d+ cas=\d+ falhou=\d+ final=\d+", s) is not None,
                  "o programa diz o que dá das atómicas de 64 bits")
        verificar("SMP contador atomico: 400" in s,
                  "o contador partilhado com atómicas soma 400 (4 hilos por 100 rondas)")
        verificar("SMP contador mutex: 400" in s,
                  "o contador protegido pelo mutex soma 400 (4 hilos por 100 rondas)")
        verificar("SMP semaforo: 0" in s, "o semáforo fica a zero")
        verificar("SMP erros: 0" in s, "não houve erros no teste dos hilos simulados")
        verificar("RESULTADO smp: SUCESSO (0 falhas)" in s, "o programa do SMP passou")
        verificar("[FALHA]" not in s, "nenhuma das verificações do programa falhou")

    return 0


if __name__ == "__main__":
    sys.exit(main())
