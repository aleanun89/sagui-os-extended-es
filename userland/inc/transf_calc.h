// SPDX-License-Identifier: GPL-3.0-only
/* transf_calc.h - velocidad y tiempo restante de una transferencia (D467).
 *
 * Solo enteros y sin dependencias (vale en la libsagui, sin coma flotante, y en el anfitrión para la prueba
 * tests/progreso_unidad.c). La velocidad es una media móvil de unos segundos: se guarda una muestra
 * (milisegundos, KB acumulados) cada TC_PERIODO_MS y se descartan las de más de TC_VENTANA_MS. */
#ifndef SAGUI_TRANSF_CALC_H
#define SAGUI_TRANSF_CALC_H
/* en el anfitrión (prueba) se usan los tipos de stdint.h; en SaguiOS ya los define sagui.h */
#if __STDC_HOSTED__
#include <stdint.h>
#endif

#define TC_MUESTRAS   32
#define TC_PERIODO_MS 250
#define TC_VENTANA_MS 6000
#define TC_MIN_MS     1000          /* menos de esto de historial: aún «Calculando...» */
#define TC_SIN_DATOS  0xFFFFFFFFu

typedef struct {
    uint32_t t[TC_MUESTRAS];        /* ms */
    uint32_t kb[TC_MUESTRAS];       /* KB acumulados */
    int n;
} tc_vel_t;

static inline void tc_reiniciar(tc_vel_t *v, uint32_t t, uint32_t kb)
{
    v->n = 1; v->t[0] = t; v->kb[0] = kb;
}

static inline void tc_quitar_primera(tc_vel_t *v)
{
    for (int i = 1; i < v->n; i++) { v->t[i - 1] = v->t[i]; v->kb[i - 1] = v->kb[i]; }
    v->n--;
}

/* Anota una muestra si pasó el periodo desde la última. */
static inline void tc_muestra(tc_vel_t *v, uint32_t t, uint32_t kb)
{
    if (v->n == 0) { tc_reiniciar(v, t, kb); return; }
    if (t - v->t[v->n - 1] < TC_PERIODO_MS) return;
    if (v->n == TC_MUESTRAS) tc_quitar_primera(v);
    v->t[v->n] = t; v->kb[v->n] = kb; v->n++;
    while (v->n > 2 && t - v->t[0] > TC_VENTANA_MS) tc_quitar_primera(v);
}

/* KB/s medios de la ventana, o TC_SIN_DATOS si aún no hay historial suficiente. */
static inline uint32_t tc_kbs(const tc_vel_t *v)
{
    if (v->n < 2) return TC_SIN_DATOS;
    uint32_t dt = v->t[v->n - 1] - v->t[0], dkb = v->kb[v->n - 1] - v->kb[0];
    if (dt < TC_MIN_MS) return TC_SIN_DATOS;
    return dkb < 4000000u ? dkb * 1000u / dt : (dkb / dt) * 1000u;
}

/* Segundos que faltan (redondeados hacia arriba), o TC_SIN_DATOS si la velocidad es 0 o desconocida. */
static inline uint32_t tc_segundos(uint32_t kb_restantes, uint32_t kbs)
{
    if (kbs == TC_SIN_DATOS || kbs == 0) return TC_SIN_DATOS;
    return (kb_restantes + kbs - 1) / kbs;
}

/* Cómo decirlo: «unos N segundos», «unos N minutos» o «H h M min». Los segundos se redondean de 5 en 5 desde 10. */
enum { TC_T_SEG, TC_T_MIN, TC_T_HORAS };
static inline int tc_clasificar(uint32_t s, uint32_t *a, uint32_t *b)
{
    if (s < 60) {
        if (s > 10) s = (s + 4) / 5 * 5;
        if (s == 0) s = 1;
        *a = s; *b = 0;
        return TC_T_SEG;
    }
    if (s < 90 * 60) {
        uint32_t m = (s + 30) / 60;
        if (m > 90) m = 90;
        *a = m; *b = 0;
        return TC_T_MIN;
    }
    *a = s / 3600; *b = (s % 3600 + 30) / 60;
    if (*b == 60) { (*a)++; *b = 0; }
    return TC_T_HORAS;
}

/* Porcentaje por bytes (0-100), sin desbordar: se trabaja en KB. */
static inline uint32_t tc_porcentaje(uint32_t kb_hechos, uint32_t kb_total)
{
    if (kb_total == 0) return 100;
    if (kb_hechos >= kb_total) return 100;
    return kb_hechos < 40000000u ? kb_hechos * 100u / kb_total : kb_hechos / (kb_total / 100u + 1);
}
#endif
