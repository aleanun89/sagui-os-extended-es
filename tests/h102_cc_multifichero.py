#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H102 (D133) - el `cc` compila varios ficheros de origen y tiene más biblioteca.

Criterio:
  - `cc a.c b.c -o prog` compila dos ficheros en un solo programa y el
    ejecutable corre;
  - una función definida en el segundo fichero se puede llamar desde el
    primero (son unidades de traducción distintas, pero el programa es uno);
  - un `static` con el mismo nombre en los dos ficheros no choca: cada
    unidad tiene su ámbito (el `static` del C);
  - un `static` de un fichero **no** se ve desde el otro: al usarlo da el
    error de «no declarado»;
  - una función sin `static` repetida en los dos ficheros da error;
  - un programa de un solo fichero sigue compilando igual que antes;
  - la biblioteca nueva: ficheros (`fopen`, `fwrite`, `fread`, `fclose`),
    `qsort` con su comparador, `strtol` y el reloj (`time`, `clock`).
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console  # noqa: E402

# El primer fichero: define un `static` (una función y una variable) que no
# deben verse desde el otro fichero, y llama a una función del segundo.
PRIMERO = r'''#include <stdio.h>
static int comun(void) { return 1; }
static int secreto = 7;
int desde_el_segundo(void);
int main(void) {
  printf("primero=%d segundo=%d\n", comun(), desde_el_segundo());
  return 0;
}
'''

# El segundo fichero: un `static` con el MISMO nombre que en el primero (no
# deben chocar) y una función pública que el primero llama.
SEGUNDO = r'''static int comun(void) { return 2; }
int desde_el_segundo(void) { return comun() + 10; }
'''

# La biblioteca nueva: ficheros, qsort, strtol y el reloj.
BIBLIOTECA = r'''#include <stdio.h>
static int compara(const void *a, const void *b) {
  int x = *(const int *)a, y = *(const int *)b;
  if (x < y) return -1;
  if (x > y) return 1;
  return 0;
}
int main(void) {
  int v[5];
  v[0] = 5; v[1] = 3; v[2] = 9; v[3] = 1; v[4] = 7;
  qsort(v, 5, sizeof(int), compara);
  for (int i = 0; i < 5; i++) printf("%d ", v[i]);
  printf("\n");
  long n = strtol("123abc", 0, 10);
  printf("strtol=%ld\n", n);
  FILE *f = fopen("/tmp/cc_fichero.txt", "w");
  if (!f) { printf("sin fichero\n"); return 1; }
  fprintf(f, "hola %d\n", 42);
  fclose(f);
  f = fopen("/tmp/cc_fichero.txt", "r");
  if (!f) { printf("sem abrir\n"); return 1; }
  char buf[32];
  int nread = fread(buf, 1, 31, f);
  buf[nread] = 0;
  fclose(f);
  printf("leido=%s", buf);
  time_t t = time(0);
  printf("reloj=%ld\n", (long)t);
  return 0;
}
'''


def escrever(m, caminho, texto):
    """Escribe un fichero en el SaguiOS con `cat >` (como hace la b5)."""
    m.digitar(f"cat > {caminho}\n", 0.04)
    time.sleep(1)
    for linha in texto.splitlines():
        m.digitar(linha + "\n", 0.03)
    m.tecla("ctrl-d")
    m.esperar_serial("(cat) terminou", 30)


def main():
    with Maquina("h102", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)

        # ---- dos ficheiros, um so programa
        escrever(m, "/tmp/primeiro.c", PRIMERO)
        escrever(m, "/tmp/segundo.c", SEGUNDO)
        out = m.comando("cc /tmp/primeiro.c /tmp/segundo.c -o /tmp/duo", tempo=90)
        verificar("(cc) terminou com codigo 0" in out, "cc compila dois ficheiros de origem")
        out = m.comando("/tmp/duo")
        verificar("primero=1 segundo=12" in out,
                  f"os dois ficheiros formam um programa so (saida: {out.strip()})")

        # ---- o static de um ficheiro nao se ve no outro
        m.comando("echo 'int desde_el_segundo(void) { return secreto; }' > /tmp/usa_secreto.c")
        out = m.comando("cc /tmp/primeiro.c /tmp/usa_secreto.c -o /tmp/erro", tempo=90)
        verificar("'secreto' no declarado" in out,
                  f"um static de um ficheiro nao se ve no outro (saida: {out.strip()})")
        verificar("(cc) terminou com codigo 1" in out, "o cc termina com codigo 1 no erro")

        # ---- a mesma funcao publica em dois ficheiros da erro
        m.comando("echo 'int desde_el_segundo(void) { return 1; }' > /tmp/repetida.c")
        out = m.comando("cc /tmp/primeiro.c /tmp/segundo.c /tmp/repetida.c -o /tmp/erro2", tempo=90)
        verificar("(cc) terminou com codigo 1" in out,
                  f"uma funcao sem static repetida da erro (saida: {out.strip()})")

        # ---- a biblioteca nova
        escrever(m, "/tmp/biblioteca.c", BIBLIOTECA)
        out = m.comando("cc /tmp/biblioteca.c -o /tmp/biblioteca", tempo=90)
        if "(cc) terminou com codigo 0" not in out:
            print("  (o cc nao compila o programa da biblioteca:", out.strip(), ")")
        verificar("(cc) terminou com codigo 0" in out,
                  "cc compila um programa com ficheiros, qsort, strtol e o relogio")
        out = m.comando("/tmp/biblioteca")
        verificar("1 3 5 7 9" in out, f"qsort ordena (saida: {out.strip()})")
        verificar("strtol=123" in out, "strtol le um numero")
        verificar("leido=hola 42" in out, "fopen/fwrite/fread le o que se escreveu")
        verificar("reloj=" in out, "time devolve a hora")

        # ---- um ficheiro so continua a funcionar
        out = m.comando("cc /ejemplos/hola.c -o /tmp/hola", tempo=90)
        verificar("(cc) terminou com codigo 0" in out, "um programa de um so ficheiro compila igual")
        out = m.comando("/tmp/hola Tito")
        verificar("Hola, Tito!" in out, "o executavel de um so ficheiro roda")
    print("H102 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
