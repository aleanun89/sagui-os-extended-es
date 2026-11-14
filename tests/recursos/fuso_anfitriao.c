// SPDX-License-Identifier: GPL-2.0-or-later
/* fuso_anfitriao.c - roda o kernel/lib/fuso.c no anfitrião para o h67.
 *
 * Lê comandos da entrada, um por linha, e responde um por linha:
 *   L <tz> <utc>     -> "ano mes dia hora min seg dsem diaano desvio verao abrev"
 *   U <tz> <local>   -> "utc situacao"   (local = segundos da hora de parede, como se fosse UTC)
 *   C <ano> <mes> <dia> <h> <m> <s> -> "segundos"
 *   P <tz>           -> "ok|inv"
 * A cadeia TZ vai sem espaços (como sempre). */
#include <stdio.h>
#include <string.h>
#include "fuso.h"

int main(void)
{
    char linha[512], cmd[4], tz[256];
    while (fgets(linha, sizeof linha, stdin)) {
        unsigned long v;
        fuso_t f;
        if (sscanf(linha, "%3s", cmd) != 1) continue;
        if (cmd[0] == 'L' && sscanf(linha, "%*s %255s %lu", tz, &v) == 2) {
            fuso_data_t d;
            fuso_ler(tz, &f);
            fuso_local(&f, (uint32_t)v, &d);
            printf("%u %u %u %u %u %u %u %u %d %d %s\n", d.ano, d.mes, d.dia, d.hora, d.minuto, d.segundo,
                   d.dia_semana, d.dia_ano, d.desvio, d.verao, d.abrev);
        } else if (cmd[0] == 'U' && sscanf(linha, "%*s %255s %lu", tz, &v) == 2) {
            int s;
            fuso_ler(tz, &f);
            uint32_t u = fuso_para_utc(&f, (uint32_t)v, &s);
            printf("%u %d\n", u, s);
        } else if (cmd[0] == 'C') {
            int a, m, d, h, mi, s;
            sscanf(linha, "%*s %d %d %d %d %d %d", &a, &m, &d, &h, &mi, &s);
            printf("%u\n", fuso_compor(a, m, d, h, mi, s));
        } else if (cmd[0] == 'P' && sscanf(linha, "%*s %255s", tz) == 1) {
            printf("%s\n", fuso_ler(tz, &f) ? "ok" : "inv");
        } else printf("?\n");
        fflush(stdout);
    }
    return 0;
}
