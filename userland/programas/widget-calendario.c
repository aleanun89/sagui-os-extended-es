// SPDX-License-Identifier: GPL-3.0-only
/* widget-calendario - el mes en curso, con el día de hoy marcado (D365). Las flechas del título pasan
 * de mes; un clic en el título vuelve al actual. Comprueba el cambio de día cada 30 s y no repinta si no cambió. */
#include "widget.h"

#define CEL_L 26
#define L (7 * CEL_L + 16)
#define A 196

static const char *const meses[12] = { N_("enero"), N_("febrero"), N_("marzo"), N_("abril"), N_("mayo"), N_("junio"),
                                       N_("julio"), N_("agosto"), N_("septiembre"), N_("octubre"), N_("noviembre"), N_("diciembre") };
static const char *const dias[7] = { N_("L"), N_("M"), N_("X"), N_("J"), N_("V"), N_("S"), N_("D") };

static int mes_off;          /* meses respecto al actual */
static uint32_t visto = 0xFFFFFFFF;

static int dia_semana(int a, int m, int d)   /* 0 = domingo (Sakamoto) */
{
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) a--;
    return (a + a / 4 - a / 100 + a / 400 + t[m - 1] + d) % 7;
}

static int dias_mes(int a, int m)
{
    static const int n[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && ((a % 4 == 0 && a % 100 != 0) || a % 400 == 0)) return 29;
    return n[m - 1];
}

static bool tique(jan_t *j)
{
    fuso_data_t hoje;
    agora_local(&hoje);
    uint32_t estado = (uint32_t)hoje.ano * 10000 + hoje.mes * 100 + hoje.dia + (uint32_t)(mes_off + 1000) * 100000000u;
    if (estado == visto) return false;
    visto = estado;
    int m0 = hoje.mes - 1 + mes_off, ano = hoje.ano + (m0 >= 0 ? m0 / 12 : -((11 - m0) / 12));
    int m = ((m0 % 12) + 12) % 12 + 1;
    widget_painel(j, 0, 0, L, A, 10);
    char t[40];
    formatar_texto(t, sizeof t, "%s %d", _(meses[m - 1]), ano);
    g_texto(j, (L - (int)tam_texto(t) * LETRA_L) / 2, 8, t, WCOR_TEXTO);
    g_texto(j, 10, 8, "<", WCOR_ACENTO);
    g_texto(j, L - 18, 8, ">", WCOR_ACENTO);
    for (int i = 0; i < 7; i++)
        g_texto(j, 8 + i * CEL_L + (CEL_L - LETRA_L) / 2, 32, _(dias[i]), i >= 5 ? WCOR_AMARELO : WCOR_FRACO);
    g_retangulo(j, 8, 52, 7 * CEL_L, 1, WCOR_BORDA);
    int col = (dia_semana(ano, m, 1) + 6) % 7;   /* lunes primero */
    int total = dias_mes(ano, m);
    for (int d = 1; d <= total; d++) {
        int fila = (col + d - 1) / 7, c = (col + d - 1) % 7;
        int x = 8 + c * CEL_L, y = 58 + fila * 22;
        bool eh_hoje = mes_off == 0 && d == hoje.dia;
        if (eh_hoje) {
            g_retangulo(j, x + 1, y - 1, CEL_L - 2, 20, WCOR_ACENTO);
        }
        formatar_texto(t, sizeof t, "%d", d);
        g_texto(j, x + (CEL_L - (int)tam_texto(t) * LETRA_L) / 2, y + 1, t, eh_hoje ? 0x0C150F : (c >= 5 ? WCOR_AMARELO : WCOR_TEXTO));
    }
    return true;
}

static bool evento(jan_t *j, const evento_t *e)
{
    if (e->tipo != EV_CLIQUE || !(e->botoes & 1) || e->y > 28) return false;
    if (e->x < 36) mes_off--;
    else if (e->x > L - 36) mes_off++;
    else mes_off = 0;
    visto = 0xFFFFFFFF;
    return tique(j);
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    jan_t *j = widget_abrir("calendario", L, A, -20, 250, true, 100);
    if (!j) return 1;
    widget_ejecutar(j, "calendario", 30000, tique, evento);
    return 0;
}
