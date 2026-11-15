// SPDX-License-Identifier: GPL-2.0-or-later
/* latencia [segundos] - mide cuánto tarda en volver el sistema (D281). Duerme 10 ms una y
 * otra vez; cada vuelta mide el tiempo real y anota el mayor retraso sobre esos 10 ms. Si el disco
 * congela el sistema (PIO con las interrupciones apagadas), el mayor retraso crece a cientos de
 * milisegundos. Escribe "latencia: N vueltas, maximo M ms, medio P ms" por el registro y la consola. */
#include "sagui.h"

int principal(int argc, char **argv)
{
    unsigned seg = argc > 1 ? (unsigned)texto_para_int(argv[1]) : 5;
    unsigned ini = tempo_ms(), max = 0, vueltas = 0, soma = 0;
    while (tempo_ms() - ini < seg * 1000) {
        unsigned t0 = tempo_ms();
        dormir(10);
        unsigned dt = tempo_ms() - t0;
        if (dt > max) max = dt;
        soma += dt;
        vueltas++;
    }
    imprimir("latencia: %u vueltas, maximo %u ms, medio %u ms\n", vueltas, max, vueltas ? soma / vueltas : 0);
    registrar("latencia: %u vueltas, maximo %u ms, medio %u ms\n", vueltas, max, vueltas ? soma / vueltas : 0);
    return 0;
}
