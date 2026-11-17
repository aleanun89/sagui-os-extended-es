// SPDX-License-Identifier: GPL-3.0-only
/* widget-relogio - el reloj de escritorio (D365): analógico o digital (un clic cambia de modo; la
 * elección se guarda en relogio.modo de widgets.conf). Se repinta solo cuando cambia el segundo. */
#include "widget.h"

#define L 168
#define A 196

static bool analogico = true;
static int ultimo = -1;
static uint32_t ultimo_dia;

static void ponto(jan_t *j, int x, int y, int t, uint32_t cor) { g_retangulo(j, x, y, t, t, cor); }

/* Aguja desde el centro: grados desde las 12 en punto, hacia la derecha; largo y grosor en píxeles. */
static void agulha(jan_t *j, int cx, int cy, int graus, int largo, int grosor, uint32_t cor)
{
    int x1 = cx + widget_seno(graus) * largo / 1000, y1 = cy - widget_coseno(graus) * largo / 1000;
    for (int d = -(grosor / 2); d <= grosor / 2; d++) {
        g_linha(j, cx + d, cy, x1 + d, y1, cor);
        g_linha(j, cx, cy + d, x1, y1 + d, cor);
    }
}

static void desenhar(jan_t *j, const fuso_data_t *d)
{
    widget_painel(j, 0, 0, L, A, 10);
    char t[40];
    if (analogico) {
        int cx = L / 2, cy = 88, r = 70;
        for (int y = -r; y <= r; y++) {   /* esfera: disco con un filete de acento */
            int w = 0;
            while ((w + 1) * (w + 1) + y * y <= r * r) w++;
            g_retangulo(j, cx - w, cy + y, 2 * w + 1, 1, 0x0E1812);
        }
        for (int g = 0; g < 360; g++) {
            int x = cx + widget_seno(g) * r / 1000, y = cy - widget_coseno(g) * r / 1000;
            g_retangulo(j, x, y, 1, 1, WCOR_ACENTO);
            if (g % 6 == 0) { int x2 = cx + widget_seno(g) * (r - 3) / 1000, y2 = cy - widget_coseno(g) * (r - 3) / 1000; ponto(j, x2, y2, 1, WCOR_FRACO); }
        }
        for (int h = 0; h < 12; h++) {
            int g = h * 30;
            int xa = cx + widget_seno(g) * (r - 4) / 1000, ya = cy - widget_coseno(g) * (r - 4) / 1000;
            int xb = cx + widget_seno(g) * (r - 11) / 1000, yb = cy - widget_coseno(g) * (r - 11) / 1000;
            g_linha(j, xa, ya, xb, yb, WCOR_TEXTO);
            g_linha(j, xa + 1, ya, xb + 1, yb, WCOR_TEXTO);
        }
        int hm = (d->hora % 12) * 30 + d->minuto / 2, mm = d->minuto * 6 + d->segundo / 10, ss = d->segundo * 6;
        agulha(j, cx, cy, hm, 38, 3, WCOR_TEXTO);
        agulha(j, cx, cy, mm, 55, 2, WCOR_TEXTO);
        agulha(j, cx, cy, ss, 60, 1, WCOR_AMARELO);
        ponto(j, cx - 2, cy - 2, 5, WCOR_ACENTO);
        formatar_data(t, sizeof t, "%a %d/%m", d);
        g_texto(j, (L - (int)tam_texto(t) * LETRA_L) / 2, 168, t, WCOR_FRACO);
    } else {
        formatar_texto(t, sizeof t, "%02d:%02d", d->hora, d->minuto);
        widget_texto(j, (L - 5 * LETRA_L * 3) / 2, 40, t, WCOR_TEXTO, 3);
        formatar_texto(t, sizeof t, ":%02d", d->segundo);
        widget_texto(j, (L - 3 * LETRA_L * 2) / 2, 92, t, WCOR_ACENTO, 2);
        formatar_data(t, sizeof t, "%A", d);
        g_texto(j, (L - (int)tam_texto(t) * LETRA_L) / 2, 140, t, WCOR_FRACO);
        formatar_data(t, sizeof t, "%d/%m/%Y", d);
        g_texto(j, (L - (int)tam_texto(t) * LETRA_L) / 2, 160, t, WCOR_TEXTO);
    }
}

static bool tique(jan_t *j)
{
    fuso_data_t d;
    agora_local(&d);
    int chave = d.segundo + 60 * d.minuto + 3600 * d.hora;
    uint32_t dia = (uint32_t)d.ano * 400 + d.dia_ano;
    if (chave == ultimo && dia == ultimo_dia) return false;   /* nada cambió */
    ultimo = chave; ultimo_dia = dia;
    desenhar(j, &d);
    return true;
}

static bool evento(jan_t *j, const evento_t *e)
{
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        analogico = !analogico;
        widget_gravar("relogio.modo", analogico ? "analogico" : "digital");
        ultimo = -1;
        return tique(j);
    }
    return false;
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    char modo[16];
    if (widget_ler("relogio.modo", modo, sizeof modo) == 0) analogico = compara_texto(modo, "digital") != 0;
    jan_t *j = widget_abrir("relogio", L, A, -20, 40, true, 100);
    if (!j) return 1;
    widget_ejecutar(j, "relogio", 1000, tique, evento);
    return 0;
}
