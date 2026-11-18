/* SPDX-License-Identifier: GPL-3.0-only */
/* tcc_libc_extra.c - lo que TCC pide a la libc y la libc común de SaguiOS no tiene (D384).
 * Solo se enlaza en /apps/bin/tcc. Las dos son de long double (80 bits, la x87):
 *   - ldexpl: por potencias de 2, exacta;
 *   - strtold: con el strtod exacto de la libc (D210); un literal «long double» pierde las cifras
 *     que no caben en un double (TCC solo la usa para los literales con sufijo L). */
#include <stdlib.h>

long double ldexpl(long double x, int e)
{
    /* por potencias de 2 (exacto); en C puro, para que TCC también lo compile (autocompilación) */
    long double f = e < 0 ? 0.5L : 2.0L;
    unsigned n = e < 0 ? 0u - (unsigned)e : (unsigned)e;
    while (n) {
        if (n & 1)
            x *= f;
        n >>= 1;
        if (n)
            f *= f;
    }
    return x;
}

long double strtold(const char *texto, char **fim)
{
    return (long double)strtod(texto, fim);
}
