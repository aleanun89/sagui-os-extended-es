// SPDX-License-Identifier: GPL-2.0-or-later
/* cal - calendario (D232): el mes de hoy, un mes cualquiera o un año entero.
 *   cal      cal 12 2026      cal 2027      cal -s (semana desde el domingo) */
#include "cal_logica.h"

static const char *AYUDA =
    "uso: cal [-s] [[MES] AÑO]\n"
    "  Sin argumentos muestra el mes actual; con AÑO, el año entero; con MES (1-12) y AÑO, ese mes.\n"
    "  También vale el nombre del mes: cal marzo 2027.   -s  la semana empieza el domingo (por defecto, el lunes)\n"
    "  -m  la semana empieza el lunes (es lo normal; se acepta por compatibilidad)   -y  el año actual entero\n"
    "Vale desde el año 1 al 9999 (calendario gregoriano).\n";

static int mes_por_nombre(const char *s)
{
    int n = (int)tam_texto(s);
    if (n < 3) return 0;
    for (int m = 0; m < 12; m++) {
        const char *c = cal_meses[m];
        int i = 0;
        while (i < n && c[i] && ((s[i] | 32) == c[i])) i++;
        if (i == n) return m + 1;
    }
    return 0;
}

int principal(int argc, char **argv)
{
    bool lunes = true, anio_hoy = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "smy", &o)) >= 0) {
        switch (r) {
        case 's': lunes = false; break;
        case 'm': lunes = true; break;
        case 'y': anio_hoy = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("cal", r, &o);
        }
    }
    int na = argc - o.indice, mes = 0, anio = 0;
    fuso_data_t hoy;
    agora_local(&hoy);
    if (na > 2) { imprimir_em(ERROS, "cal: demasiados argumentos (cal --help)\n"); return 2; }
    if (na == 0) { mes = hoy.mes; anio = hoy.ano; }
    else if (na == 1) {
        if (!texto_para_numero(argv[o.indice], &anio)) { imprimir_em(ERROS, "cal: %s: año no válido\n", argv[o.indice]); return 2; }
    } else {
        const char *m = argv[o.indice];
        if (!texto_para_numero(m, &mes)) mes = mes_por_nombre(m);
        if (mes < 1 || mes > 12) { imprimir_em(ERROS, "cal: %s: mes no válido (1-12 o su nombre)\n", m); return 2; }
        if (!texto_para_numero(argv[o.indice + 1], &anio)) { imprimir_em(ERROS, "cal: %s: año no válido\n", argv[o.indice + 1]); return 2; }
    }
    if (anio_hoy) { mes = 0; anio = hoy.ano; }
    if (anio < 1 || anio > 9999) { imprimir_em(ERROS, "cal: el año debe estar entre 1 y 9999\n"); return 2; }
    static char t[512];
    if (mes) {
        cal_mes(t, sizeof t, anio, mes, lunes, 1);
        escrever_texto(SAIDA, t);
        return 0;
    }
    imprimir("%*s%d\n", 8, "", anio);
    for (int m = 1; m <= 12; m++) {
        imprimir("\n");
        cal_mes(t, sizeof t, anio, m, lunes, 0);
        escrever_texto(SAIDA, t);
    }
    return 0;
}
