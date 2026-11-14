// SPDX-License-Identifier: GPL-3.0-only
/* falha [tipo] - provoca un error a propósito para mostrar la protección: solo
 * este proceso es terminado por el kernel; el shell y el sistema siguen. */
#include "sagui.h"

int principal(int argc, char **argv)
{
    const char *tipo = argc > 1 ? argv[1] : "memoria";
    imprimir("falha: voy a provocar un error de tipo \"%s\" (proceso %d)...\n", tipo, meu_pid());
    if (compara_texto(tipo, "kernel") == 0) {
        *(volatile int *)0x00100000 = 1;           /* escribir en la memoria del kernel */
    } else if (compara_texto(tipo, "divisao") == 0) {
        /* Dividendo "grande" a propósito: con 1 / x el GCC cambia la división
         * por comparaciones (el resultado solo puede ser -1, 0 o 1) y la CPU nunca
         * llega a dividir. */
        volatile int zero = 0, dividendo = 12345;
        volatile int x = dividendo / zero;
        (void)x;
    } else if (compara_texto(tipo, "instrucao") == 0) {
        __asm__ volatile("cli");                   /* instrucción privilegiada */
    } else {
        *(volatile int *)0 = 1;                    /* puntero nulo */
    }
    imprimir("falha: ¿el error no se ha producido?\n");
    return 1;
}
