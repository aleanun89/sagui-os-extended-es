// SPDX-License-Identifier: GPL-3.0-only
/* widget-sistema - monitor pequeño del sistema (D365): CPU con su gráfica, memoria y red (bajada y subida
 * con la gráfica del último minuto). Una lectura por segundo; no repinta si los números no cambiaron. */
#include "widget.h"

#define L 224
#define A 232
#define HIST 60

static int hist_cpu[HIST], nhist;
static unsigned ult_chave[8];

static bool tique(jan_t *j)
{
    widget_datos_tique();
    unsigned cpu = widget_cpu_x10();
    unsigned mu, mt;
    widget_mem(&mu, &mt);
    unsigned rx, tx;
    const int *hrx, *htx;
    int nr;
    widget_rede(&rx, &tx, &hrx, &htx, &nr);
    if (nhist < HIST) hist_cpu[nhist++] = (int)cpu;
    else { for (int i = 1; i < HIST; i++) hist_cpu[i - 1] = hist_cpu[i]; hist_cpu[HIST - 1] = (int)cpu; }
    unsigned chave[8] = { cpu, mu, mt, rx, tx, (unsigned)nr, nr ? (unsigned)hrx[0] : 0, (unsigned)nhist };
    bool igual = true;
    for (int i = 0; i < 5; i++) if (chave[i] != ult_chave[i]) igual = false;
    if (igual && nhist >= HIST) return false;   /* nada cambió (la historia de CPU sigue llena de lo mismo) */
    for (int i = 0; i < 8; i++) ult_chave[i] = chave[i];

    widget_painel(j, 0, 0, L, A, 10);
    char t[64], a[24], b[24];
    widget_numero(a, sizeof a, cpu);
    formatar_texto(t, sizeof t, "CPU  %s%%  (%d)", a, widget_ncpu());
    g_texto(j, 10, 8, t, WCOR_TEXTO);
    widget_grafica(j, 10, 28, L - 20, 38, hist_cpu, nhist, 1000, WCOR_ACENTO);

    widget_kb(a, sizeof a, mu);
    widget_kb(b, sizeof b, mt);
    formatar_texto(t, sizeof t, "%s %s / %s", _("Memoria"), a, b);
    g_texto(j, 10, 74, t, WCOR_TEXTO);
    widget_barra(j, 10, 94, L - 20, 12, mt ? (int)((unsigned long long)mu * 100 / mt) : 0, WCOR_AZUL);

    widget_bytes(a, sizeof a, rx);
    formatar_texto(t, sizeof t, "%s %s/s", _("Bajada"), a);
    g_texto(j, 10, 116, t, WCOR_TEXTO);
    int max = 1024;
    for (int i = 0; i < nr; i++) { if (hrx[i] > max) max = hrx[i]; if (htx[i] > max) max = htx[i]; }
    widget_grafica(j, 10, 136, L - 20, 30, hrx, nr, max, WCOR_ACENTO);
    widget_bytes(b, sizeof b, tx);
    formatar_texto(t, sizeof t, "%s %s/s", _("Subida"), b);
    g_texto(j, 10, 172, t, WCOR_TEXTO);
    widget_grafica(j, 10, 192, L - 20, 30, htx, nr, max, WCOR_AMARELO);
    return true;
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    jan_t *j = widget_abrir("sistema", L, A, -20, 450, true, 100);
    if (!j) return 1;
    widget_ejecutar(j, "sistema", 1000, tique, NULL);
    return 0;
}
