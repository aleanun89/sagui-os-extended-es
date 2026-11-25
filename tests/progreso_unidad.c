// SPDX-License-Identifier: GPL-3.0-only
/* progreso_unidad.c - prueba en el anfitrión del cálculo de velocidad y tiempo restante (D467, h467). */
#include "../userland/inc/transf_calc.h"
#include <stdio.h>

static int fallos;
#define COMPROBAR(c, ...) do { if (!(c)) { printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); fallos++; } } while (0)

int main(void)
{
    tc_vel_t v;
    /* 10 MB/s constantes durante 3 s: 10240 KB/s, con muestras cada 250 ms */
    tc_reiniciar(&v, 0, 0);
    COMPROBAR(tc_kbs(&v) == TC_SIN_DATOS, "sin historial debe ser desconocido");
    for (uint32_t t = 250; t <= 3000; t += 250) tc_muestra(&v, t, t * 10240 / 1000);
    uint32_t k = tc_kbs(&v);
    COMPROBAR(k >= 10200 && k <= 10280, "velocidad %u KB/s (esperaba ~10240)", k);
    COMPROBAR(tc_segundos(102400, k) == 10, "102400 KB a %u KB/s deben ser 10 s (%u)", k, tc_segundos(102400, k));
    /* la ventana descarta lo viejo: tras 20 s a 1 MB/s la media sale de lo último */
    for (uint32_t t = 3250; t <= 23000; t += 250) tc_muestra(&v, t, 30720 + (t - 3000) * 1024 / 1000);
    k = tc_kbs(&v);
    COMPROBAR(k >= 1000 && k <= 1050, "tras el cambio de ritmo %u KB/s (esperaba ~1024)", k);
    COMPROBAR(v.n <= TC_MUESTRAS, "el anillo no desborda (%d)", v.n);
    /* parado: velocidad 0 -> sin estimación */
    tc_reiniciar(&v, 0, 100);
    for (uint32_t t = 250; t <= 2000; t += 250) tc_muestra(&v, t, 100);
    COMPROBAR(tc_kbs(&v) == 0 && tc_segundos(5000, 0) == TC_SIN_DATOS, "parado no estima");
    /* clasificación del tiempo */
    uint32_t a, b;
    COMPROBAR(tc_clasificar(1, &a, &b) == TC_T_SEG && a == 1, "1 s");
    COMPROBAR(tc_clasificar(7, &a, &b) == TC_T_SEG && a == 7, "7 s");
    COMPROBAR(tc_clasificar(13, &a, &b) == TC_T_SEG && a == 15, "13 s -> 15");
    COMPROBAR(tc_clasificar(59, &a, &b) == TC_T_SEG && a == 60, "59 s -> 60");
    COMPROBAR(tc_clasificar(60, &a, &b) == TC_T_MIN && a == 1, "60 s -> 1 min");
    COMPROBAR(tc_clasificar(120, &a, &b) == TC_T_MIN && a == 2, "120 s -> 2 min");
    COMPROBAR(tc_clasificar(3600 + 40 * 60, &a, &b) == TC_T_HORAS && a == 1 && b == 40, "1 h 40 min");
    /* porcentaje por bytes */
    COMPROBAR(tc_porcentaje(0, 1000) == 0 && tc_porcentaje(500, 1000) == 50 && tc_porcentaje(1000, 1000) == 100, "porcentaje");
    COMPROBAR(tc_porcentaje(3000000, 4000000) == 75, "porcentaje grande");
    COMPROBAR(tc_porcentaje(1, 0) == 100, "total 0");
    if (fallos) { printf("%d fallos\n", fallos); return 1; }
    printf("progreso_unidad: todo bien\n");
    return 0;
}
