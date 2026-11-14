// SPDX-License-Identifier: GPL-2.0-or-later
/* cal_logica.h - el calendario de cal (D232): días del mes, día de la semana (calendario gregoriano) y un mes dibujado. */
#ifndef CAL_LOGICA_H
#define CAL_LOGICA_H
#include "sagui.h"

static const char *const cal_meses[] = { "enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto",
                                         "septiembre", "octubre", "noviembre", "diciembre" };

static bool cal_bisiesto(int a) { return (a % 4 == 0 && a % 100 != 0) || a % 400 == 0; }

static int cal_dias_mes(int a, int m)
{
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return m == 2 && cal_bisiesto(a) ? 29 : d[m - 1];
}

/* 0 = domingo (algoritmo de Sakamoto) */
static int cal_dia_semana(int a, int m, int d)
{
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) a--;
    return (a + a / 4 - a / 100 + a / 400 + t[m - 1] + d) % 7;
}

/* Escribe un mes: título centrado en 20 columnas, cabecera de días y semanas (sin espacios a la derecha).

 * lunes: la semana empieza en lunes (lo habitual en España); con_anio: el título lleva el año. */
static void cal_mes(char *b, size_t max, int a, int m, bool lunes, int con_anio)
{
    static const char *const cab_dom = "do lu ma mi ju vi sá", *const cab_lun = "lu ma mi ju vi sá do";
    char titulo[40];
    if (con_anio) formatar_texto(titulo, sizeof titulo, "%s %d", cal_meses[m - 1], a);
    else copia_texto(titulo, cal_meses[m - 1], sizeof titulo);
    int lt = (int)tam_texto(titulo), n = 0, sangria = (20 - lt) / 2;
    if (sangria < 0) sangria = 0;
    n += formatar_texto(b + n, max - (size_t)n, "%*s%s\n%s\n", sangria, "", titulo, lunes ? cab_lun : cab_dom);
    int col = (cal_dia_semana(a, m, 1) + (lunes ? 6 : 0)) % 7;
    int dm = cal_dias_mes(a, m);
    for (int i = 0; i < col; i++) n += formatar_texto(b + n, max - (size_t)n, "   ");
    for (int d = 1; d <= dm; d++) {
        n += formatar_texto(b + n, max - (size_t)n, "%2d", d);
        col++;
        if (col == 7 || d == dm) { n += formatar_texto(b + n, max - (size_t)n, "\n"); col = 0; }
        else n += formatar_texto(b + n, max - (size_t)n, " ");
    }
}

#endif
