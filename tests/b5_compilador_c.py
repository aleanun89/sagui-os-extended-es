#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""B5 - compilador de C (cc) dentro do SaguiOS.

Critério: o cc compila os exemplos de /ejemplos e os executáveis gerados
rodam pelo sagsh (com argumentos); um programa digitado no próprio sistema
(cat > arquivo, até Ctrl+D) com struct, ponteiros, malloc, switch, recursão
e printf compila e dá a saída certa; o código de saída do main chega ao
shell; um erro de compilação mostra arquivo:linha e o cc termina com 1."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402

PROGRAMA = r'''#include <stdio.h>
#include <stdlib.h>
struct no { int v; struct no *prox; };
int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
char *nome(int k) {
  switch (k) { case 0: return "cero"; case 1: return "uno"; default: return "muchos"; }
}
int main(int argc, char **argv) {
  struct no *l = 0;
  for (int i = 1; i <= 4; i++) { struct no *n = malloc(sizeof(struct no)); n->v = i * i; n->prox = l; l = n; }
  int s = 0;
  for (struct no *p = l; p; p = p->prox) s += p->v;
  printf("suma=%d fib=%d %s %s\n", s, fib(15), nombre_ok(), nome(argc));
  char buf[32];
  sprintf(buf, "[%5d|%-3s|%x]", -42, "ok", 255);
  puts(buf);
  return 7;
}
int nombre_ok_impl = 1;
'''
# O protótipo vem depois do uso (função sem protótipo, como no C antigo).
PROGRAMA = PROGRAMA.replace("int nombre_ok_impl = 1;\n", "char *nombre_ok() { return \"bien\"; }\n")


def main():
    with Maquina("b5", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)

        out = m.comando("ayuda cc")
        verificar("Uso: cc" in out and "compilar" in out, "ajuda cc explica o compilador (apelido compilar)")

        out = m.comando("cc /ejemplos/hola.c -o /tmp/hola", tempo=90)
        verificar("(cc) terminou com codigo 0" in out, "cc /ejemplos/hola.c -o /tmp/hola compila sem erro")
        out = m.comando("/tmp/hola Tito")
        verificar("Hola, Tito! Compilado dentro de SaguiOS." in out and "5! = 120" in out,
                  "o executável gerado roda e recebe argumentos")
        m.captura("B5-hola")

        m.comando("cd /tmp")
        out = m.comando("compilar /ejemplos/primos.c", tempo=90)
        verificar("(cc) terminou com codigo 0" in out, "compilar (apelido) /ejemplos/primos.c gera ./primos na pasta atual")
        out = m.comando("./primos 30")
        verificar("2 3 5 7 11 13 17 19 23 29" in out and "10 primos hasta 30" in out, "./primos 30 (vetor global de 100 KB na BSS)")
        out = m.comando("ls -s")
        verificar(re.search(r"^primos\s+\d+ bytes", out, re.M), "ls mostra o executável primos")

        # Um programa digitado no próprio SaguiOS.
        m.digitar("cat > /tmp/prueba.c\n", 0.04)
        time.sleep(1)
        for linha in PROGRAMA.splitlines():
            m.digitar(linha + "\n", 0.03)
        m.tecla("ctrl-d")
        m.esperar_serial("(cat) terminou", 30)
        time.sleep(1)
        out = m.comando("wc /tmp/prueba.c")
        verificar(f"{len(PROGRAMA.splitlines())} líneas" in out, "o programa foi digitado inteiro (cat > arquivo)")
        out = m.comando("cc prueba.c -o prueba", tempo=90)
        verificar("(cc) terminou com codigo 0" in out, "cc compila o programa digitado")
        out = m.comando("./prueba")
        verificar("suma=30 fib=610 bien uno" in out, "struct, malloc, lista ligada, recursão, switch e função sem protótipo")
        verificar("[  -42|ok |ff]" in out, "printf/sprintf com largura, alinhamento e hexadecimal")
        verificar("(prueba) terminou com codigo 7" in out, "o valor devolvido pelo main é o código de saída")
        out = m.comando("./prueba a b")
        verificar("muchos" in out, "argc chega ao programa")
        m.captura("B5-programa-digitado")

        # Erro de compilação.
        m.comando("echo 'int main() { return x; }' > /tmp/mal.c")
        out = m.comando("cc mal.c")
        verificar("mal.c:1: error: 'x' no declarado" in out, "erro de compilação com arquivo:linha")
        verificar("(cc) terminou com codigo 1" in out, "cc termina com código 1 no erro")
        out = m.comando("ls -s")
        verificar(not re.search(r"^mal\s", out, re.M), "nenhum executável é gerado quando há erro")
        out = m.comando("echo o shell segue vivo")
        verificar("o shell segue vivo" in out, "o shell continua funcionando")
        m.captura("B5-erro")
    return 0


if __name__ == "__main__":
    sys.exit(main())
