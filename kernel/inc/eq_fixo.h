// SPDX-License-Identifier: GPL-2.0-or-later
/* eq_fixo.h - el filtro del ecualizador en punto fijo (D201). Lo usan el mezclador del
 * kernel (audio.c) y la prueba del PC (tests/ecualizador_unidad.c), por eso solo usa
 * int/long long y no depende de nada más que de abi.h (eq_audio_t).
 *
 * Cascada de biquads en forma directa I, coeficientes Q28. Las muestras se escalan 8 bits
 * (x << 8) para que el estado tenga precisión de sobra con los polos cerca de 1 (60 Hz a
 * 48 kHz); el acumulador es de 64 bits. */
#ifndef EQ_FIXO_H
#define EQ_FIXO_H
#include "abi.h"

typedef struct { int x1, x2, y1, y2; } eq_estado_t;

/* Una muestra de 16 bits por la cascada de un canal (st: EQ_ETAPAS estados). */
static inline int eq_filtrar(const eq_audio_t *eq, eq_estado_t *st, int entrada)
{
    int x = entrada * 256;
    for (unsigned k = 0; k < eq->etapas; k++) {
        const int *c = eq->coef[k];
        eq_estado_t *e = &st[k];
        long long acc = (long long)c[0] * x + (long long)c[1] * e->x1 + (long long)c[2] * e->x2
                      - (long long)c[3] * e->y1 - (long long)c[4] * e->y2;
        acc += 1 << 27;
        acc >>= 28;
        int y = acc > 8388607 ? 8388607 : acc < -8388608 ? -8388608 : (int)acc;
        e->x2 = e->x1; e->x1 = x;
        e->y2 = e->y1; e->y1 = y;
        x = y;
    }
    x = (x + 128) >> 8;
    return x > 32767 ? 32767 : x < -32768 ? -32768 : x;
}

/* Lo que manda el userland: etapas <= 5, cada biquad estable (|a2| < 1, |a1| < 1 + a2, en Q28)
 * y realces dentro de +-12. Con int en Q28 el acumulador de 64 bits no desborda. */
static inline int eq_valido(const eq_audio_t *e)
{
    if (e->etapas > EQ_ETAPAS) return 0;
    for (unsigned k = 0; k < e->etapas; k++) {
        const int *c = e->coef[k];
        long long a1 = c[3], a2 = c[4], um = 1 << 28;
        if (a2 <= -um || a2 >= um) return 0;
        if (a1 <= -(um + a2) || a1 >= um + a2) return 0;
    }
    for (int j = 0; j < EQ_BANDAS; j++)
        if (e->db[j] < -12 || e->db[j] > 12) return 0;
    return 1;
}
#endif
