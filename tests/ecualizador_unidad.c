// SPDX-License-Identifier: GPL-2.0-or-later
/* ecualizador_unidad.c - prueba del ecualizador (D201) en el PC.
 *
 * Compila el MISMO ecualizador.c de la libsagui (coeficientes, x87) y el MISMO eq_fixo.h del
 * kernel (filtro en punto fijo), y compara con un filtro de coma flotante de doble precisión:
 *   - plano = identidad exacta (etapas = 0 y, aun filtrando, x sale igual);
 *   - un seno a la frecuencia de cada banda con +6 dB sale con la ganancia teórica (peaking: +6;
 *     shelf: +3 en su frecuencia de corte y +6 lejos de ella), +-0,5 dB, y el filtro entero
 *     coincide con el de coma flotante (error de unos pocos LSB);
 *   - con todas las bandas a +12 dB y el margen automático, un barrido a casi plena escala no satura;
 *   - eq_valido rechaza lo inestable.
 * Sale con 0 si todo bien. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eq_fixo.h"

int eq_calcular(const int *db, unsigned taxa, eq_audio_t *e);
int eq_calcular_m(const int *db, unsigned taxa, eq_audio_t *e, int margen);
static const int FS = 48000;
static int falhas;

#define CHECA(c, ...) do { if (!(c)) { falhas++; printf("FALLA: " __VA_ARGS__); printf("\n"); } } while (0)

/* Ganancia medida (dB) de un seno de amplitud 10000 por el filtro en punto fijo; max_dif = el mayor
 * error contra el de coma flotante, en LSB. */
static double medir(const eq_audio_t *e, double f, double amp, double *max_dif, int *recortes)
{
    eq_estado_t st[EQ_ETAPAS];
    double z[EQ_ETAPAS][4];
    memset(st, 0, sizeof st);
    memset(z, 0, sizeof z);
    int n = FS, ini = FS / 2;
    double ei = 0, eo = 0, md = 0;
    for (int i = 0; i < n; i++) {
        int x = (int)lrint(amp * sin(2 * M_PI * f * i / FS));
        int y = eq_filtrar(e, st, x);
        double r = x;
        for (unsigned k = 0; k < e->etapas; k++) {
            const int *c = e->coef[k];
            double o = (c[0] * r + c[1] * z[k][0] + c[2] * z[k][1] - c[3] * z[k][2] - c[4] * z[k][3]) / 268435456.0;
            z[k][1] = z[k][0]; z[k][0] = r; z[k][3] = z[k][2]; z[k][2] = o;
            r = o;
        }
        if (recortes && (y >= 32767 || y <= -32768)) (*recortes)++;
        if (fabs(y - r) > md && fabs(r) < 32767) md = fabs(y - r);
        if (i >= ini) { ei += (double)x * x; eo += (double)y * y; }
    }
    if (max_dif) *max_dif = md;
    return 10 * log10(eo / ei);
}

int main(void)
{
    eq_audio_t e;
    int db[EQ_BANDAS] = { 0 };
    static const int freq[EQ_BANDAS] = { 60, 250, 1000, 4000, 12000 };

    /* 1. plano = identidad exacta */
    CHECA(eq_calcular(db, FS, &e) == 0 && e.etapas == 0, "plano debe dar 0 etapas");
    eq_estado_t st[EQ_ETAPAS];
    memset(st, 0, sizeof st);
    int mal = 0;
    for (int x = -32768; x <= 32767; x++) mal += eq_filtrar(&e, st, x) != x;
    CHECA(mal == 0, "plano no es identidad (%d muestras distintas)", mal);

    /* 2. +6 dB en cada banda, sin margen para ver la ganancia pura */
    for (int b = 0; b < EQ_BANDAS; b++) {
        memset(db, 0, sizeof db);
        db[b] = 6;
        eq_calcular_m(db, FS, &e, 0);
        CHECA(eq_valido(&e) && e.etapas == 1, "banda %d: no válida", b);
        double dif, g = medir(&e, freq[b], 10000, &dif, 0);
        int borde = b == 0 || b == EQ_BANDAS - 1;
        double esp = borde ? 3.0 : 6.0;     /* el shelf pasa por la mitad en su frecuencia */
        printf("banda %d (%d Hz) +6 dB: ganancia %.2f dB (esperado %.1f), dif. punto fijo/flotante %.2f LSB\n", b, freq[b], g, esp, dif);
        CHECA(fabs(g - esp) <= 0.5, "banda %d: %.2f dB, se esperaba %.1f +-0,5", b, g, esp);
        CHECA(dif <= 4.0, "banda %d: el punto fijo se aparta %.2f LSB del flotante", b, dif);
        double lejos = medir(&e, b == 0 ? 20 : 18000, 10000, 0, 0);
        if (borde) CHECA(fabs(lejos - 6.0) <= 1.0, "banda %d: lejos del corte %.2f dB, se esperaba +6 +-1", b, lejos);
        /* y -6 dB: simétrico */
        db[b] = -6;
        eq_calcular_m(db, FS, &e, 0);
        g = medir(&e, freq[b], 10000, 0, 0);
        CHECA(fabs(g + esp) <= 0.5, "banda %d: -6 dB da %.2f, se esperaba %.1f", b, g, -esp);
    }

    /* 3. todas a +12 dB con margen automático: nada de saturación con un barrido casi a plena escala */
    for (int b = 0; b < EQ_BANDAS; b++) db[b] = 12;
    eq_calcular(db, FS, &e);
    CHECA(eq_valido(&e) && e.etapas == 5, "todas a +12: no válida");
    int recortes = 0;
    double pico_db = -99;
    for (int k = 0; k < 120; k++) {
        double f = 20 * pow(1000.0, k / 119.0);     /* 20 Hz a 20 kHz */
        if (f > 21000) f = 21000;
        double g = medir(&e, f, 32000, 0, &recortes);
        if (g > pico_db) pico_db = g;
    }
    printf("todas +12 dB con margen: pico de la respuesta %.2f dB, muestras saturadas %d\n", pico_db, recortes);
    CHECA(recortes == 0, "todas a +12 dB satura %d muestras", recortes);
    CHECA(pico_db <= 0.3, "el margen deja un pico de %.2f dB", pico_db);

    /* 4. los preajustes del userland son válidos y filtran como el flotante, con ruido */
    static const int pre[4][EQ_BANDAS] = { { 0, 0, 0, 0, 0 }, { 6, 4, 0, 0, 0 }, { -3, -1, 3, 3, -2 }, { 0, 0, 0, 4, 6 } };
    for (int p = 0; p < 4; p++) {
        eq_calcular(pre[p], FS, &e);
        CHECA(eq_valido(&e), "preajuste %d no válido", p);
        double md = 0;
        for (int k = 0; k < 8; k++) { double dif; medir(&e, 80 + 700 * k, 20000, &dif, 0); if (dif > md) md = dif; }
        CHECA(md <= 4.0, "preajuste %d: punto fijo a %.2f LSB del flotante", p, md);
    }

    /* 5. otras tasas: 44,1 kHz, 96 kHz y 8 kHz (12 kHz y 4 kHz por encima de Nyquist se saltan) */
    for (int ti = 0; ti < 3; ti++) {
        static const unsigned tasas[3] = { 44100, 96000, 8000 };
        for (int b = 0; b < EQ_BANDAS; b++) db[b] = 5;
        CHECA(eq_calcular(db, tasas[ti], &e) == 0 && eq_valido(&e), "tasa %u no válida", tasas[ti]);
    }

    /* 6. eq_valido rechaza lo inestable o fuera de rango */
    for (int b = 0; b < EQ_BANDAS; b++) db[b] = 3;
    eq_calcular(db, FS, &e);
    eq_audio_t m = e; m.coef[0][4] = 1 << 28;
    CHECA(!eq_valido(&m), "a2 = 1 debería rechazarse");
    m = e; m.coef[1][3] = 3 << 28;
    CHECA(!eq_valido(&m), "a1 = 3 debería rechazarse");
    m = e; m.etapas = 6;
    CHECA(!eq_valido(&m), "6 etapas debería rechazarse");
    m = e; m.db[2] = 13;
    CHECA(!eq_valido(&m), "13 dB debería rechazarse");
    db[1] = 13;
    CHECA(eq_calcular(db, FS, &e) < 0, "eq_calcular con 13 dB debería fallar");

    printf(falhas ? "ECUALIZADOR: %d fallos\n" : "ECUALIZADOR: todo bien\n", falhas);
    return falhas != 0;
}
