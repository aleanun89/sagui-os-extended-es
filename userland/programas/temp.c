// SPDX-License-Identifier: GPL-2.0-or-later
/* temp - la temperatura de la CPU en una línea (D092, mejora 050).
 *
 *   temp         "CPU: 45 °C (Núcleo 0 45 °C, Paquete 47 °C), máx. 100 °C"
  *   temp -c      una línea por segundo hasta Ctrl+C (-n N: sólo N líneas)
 *
 * Es el resumen de "sensores"; en una máquina virtual dice "no disponible". */
#include "sagui.h"
#include "sensores.h"

static void linha(const info_sensores_t *s)
{
    char g[24];
    if (!s->qtd_temp) {
        imprimir("CPU: temperatura no disponible (%s)\n", s->motivo[0] ? s->motivo : "sin sensor conocido");
        return;
    }
    imprimir("CPU: %s (", sensores_grau(g, sizeof g, sensores_temp_maior(s)));
    for (unsigned i = 0; i < s->qtd_temp && i < SENS_MAX_TEMP; i++)
        imprimir("%s%s %s", i ? ", " : "", s->temp[i].nome, sensores_grau(g, sizeof g, s->temp[i].mili_c));
    imprimir(")");
    if (s->max_mili_c) imprimir(", máx. %s", sensores_grau(g, sizeof g, s->max_mili_c));
    if (s->throttling & SENS_THR_TERMICO) imprimir(", LIMITANDO por temperatura");
    imprimir("\n");
}

int principal(int argc, char **argv)
{
    bool continuo = false;
    int vezes = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-c") == 0) continuo = true;
        else if (compara_texto(argv[i], "-n") == 0 && i + 1 < argc) vezes = texto_para_int(argv[++i]);
        else {
            imprimir("Uso: temp [-c [-n N]]   temperatura de la CPU (-c: cada segundo hasta Ctrl+C)\n");
            return compara_texto(argv[i], "-h") == 0 ? 0 : 1;
        }
    }
    info_sensores_t s;
    if (!sensores_ler(&s)) {
        imprimir_em(ERROS, "temp: el kernel no tiene el monitor de hardware\n");
        return 1;
    }
    if (!continuo) vezes = 1;
    for (int k = 0; vezes <= 0 || k < vezes; k++) {
        if (k) { dormir(1000); sensores_ler(&s); }
        linha(&s);
    }
    return 0;
}
