// SPDX-License-Identifier: GPL-3.0-only
/* gui_selcor.c - selector de color (D454): paleta, cuadro de saturación/brillo, tono, alfa y tres deslizadores RGB.
 *
 * Todo en enteros: el tono va de 0 a 359 y la saturación y el brillo de 0 a 255. seletorcor_t se puede
 * incrustar en cualquier ventana (mide SELCOR_L x SELCOR_A); seletor_cor_dialogo lo pone en una ventana modal con
 * Aceptar y Cancelar (el inspector de propiedades lo usa así). */
#include "gui.h"

#define PAL_COLS 10
#define PAL_FILAS 3
#define PAL_CEL 16
#define SV_X 0
#define SV_Y 56
#define SV_L 150
#define SV_A 96
#define TONO_X 158
#define ALFA_X 180
#define BARRA_L 16
#define SL_X 14
#define SL_L 190
#define SL_Y0 160
#define SL_PASO 16

static const uint32_t edpaleta_cores[PAL_COLS * PAL_FILAS] = {
    0x000000, 0x404040, 0x808080, 0xC0C0C0, 0xFFFFFF, 0x800000, 0xFF0000, 0xFF8000, 0xFFFF00, 0x80FF00,
    0x00FF00, 0x00FF80, 0x00FFFF, 0x0080FF, 0x0000FF, 0x8000FF, 0xFF00FF, 0xFF0080, 0x804000, 0x008040,
    0x400000, 0x004000, 0x000040, 0x408080, 0x804080, 0xFFC0C0, 0xC0FFC0, 0xC0C0FF, 0xFFE0A0, 0xA0E0FF,
};

void cor_rgb_para_hsv(int r, int g, int b, int *h, int *s, int *v)
{
    int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    int d = mx - mn;
    *v = mx;
    *s = mx ? d * 255 / mx : 0;
    int hh = 0;
    if (d) {
        if (mx == r) hh = 60 * (g - b) / d;
        else if (mx == g) hh = 120 + 60 * (b - r) / d;
        else hh = 240 + 60 * (r - g) / d;
        if (hh < 0) hh += 360;
        if (hh >= 360) hh -= 360;
    }
    *h = hh;
}

void cor_hsv_para_rgb(int h, int s, int v, int *r, int *g, int *b)
{
    h %= 360; if (h < 0) h += 360;
    int reg = h / 60, rem = h % 60;
    int p = v * (255 - s) / 255;
    int q = v * (255 - s * rem / 60) / 255;
    int t = v * (255 - s * (60 - rem) / 60) / 255;
    switch (reg) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
    }
}

static int lim(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

void seletorcor_definir(seletorcor_t *c, uint32_t argb)
{
    c->alfa = c->com_alfa ? (int)(argb >> 24) & 255 : 255;
    c->r = (int)(argb >> 16) & 255; c->g = (int)(argb >> 8) & 255; c->b = (int)argb & 255;
    int h, s, v;
    cor_rgb_para_hsv(c->r, c->g, c->b, &h, &s, &v);
    if (s == 0) h = c->h;            /* sin saturación el tono no se sabe: se conserva */
    c->h = h; c->s = s; c->v = v;
}

void seletorcor_iniciar(seletorcor_t *c, int x, int y, uint32_t argb, bool com_alfa)
{
    memset(c, 0, sizeof *c);
    c->x = x; c->y = y; c->com_alfa = com_alfa;
    seletorcor_definir(c, com_alfa && !(argb >> 24) ? argb | 0xFF000000u : argb);
}

uint32_t seletorcor_cor(const seletorcor_t *c)
{
    return ((uint32_t)c->alfa << 24) | ((uint32_t)c->r << 16) | ((uint32_t)c->g << 8) | (uint32_t)c->b;
}

static void de_hsv(seletorcor_t *c)
{
    cor_hsv_para_rgb(c->h, c->s, c->v, &c->r, &c->g, &c->b);
}

static void de_rgb(seletorcor_t *c)
{
    int h, s, v;
    cor_rgb_para_hsv(c->r, c->g, c->b, &h, &s, &v);
    if (s != 0 && v != 0) c->h = h;
    c->s = s; c->v = v;
}

static uint32_t rgb3(int r, int g, int b) { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b; }

/* Cuadros de ajedrez para ver la transparencia. */
static void tablero(jan_t *j, int x, int y, int l, int a)
{
    for (int yy = 0; yy < a; yy += 6)
        for (int xx = 0; xx < l; xx += 6) {
            int w = l - xx < 6 ? l - xx : 6, h = a - yy < 6 ? a - yy : 6;
            g_retangulo(j, x + xx, y + yy, w, h, (((xx / 6) + (yy / 6)) & 1) ? 0xB8B8B8 : 0xF0F0F0);
        }
}

static void mostra(jan_t *j, int x, int y, int l, int a, uint32_t rgb, int alfa)
{
    if (alfa >= 255) { g_retangulo(j, x, y, l, a, rgb); return; }
    tablero(j, x, y, l, a);
    for (int yy = 0; yy < a; yy += 6)
        for (int xx = 0; xx < l; xx += 6) {
            int w = l - xx < 6 ? l - xx : 6, h = a - yy < 6 ? a - yy : 6;
            uint32_t fondo = (((xx / 6) + (yy / 6)) & 1) ? 0xB8B8B8 : 0xF0F0F0;
            g_retangulo(j, x + xx, y + yy, w, h, mod_mezclar(fondo, rgb, (unsigned)alfa));
        }
}

static void barra_valor(jan_t *j, int x, int y, int l, int a, int pos, int tam)   /* marca del deslizador */
{
    (void)tam;
    int px = x + pos * (l - 1) / 255;
    g_retangulo(j, px - 1, y - 1, 3, a + 2, 0xFFFFFF);
    g_moldura(j, px - 2, y - 2, 5, a + 4, 0x202020);
}

void seletorcor_desenhar(jan_t *j, seletorcor_t *c)
{
    int x = c->x, y = c->y;
    g_retangulo(j, x, y, SELCOR_L, SELCOR_A, COR_FUNDO);
    /* paleta */
    for (int i = 0; i < PAL_COLS * PAL_FILAS; i++) {
        int cx = x + (i % PAL_COLS) * PAL_CEL, cy = y + (i / PAL_COLS) * PAL_CEL;
        g_retangulo(j, cx, cy, PAL_CEL - 1, PAL_CEL - 1, edpaleta_cores[i]);
        if (edpaleta_cores[i] == rgb3(c->r, c->g, c->b)) g_moldura(j, cx, cy, PAL_CEL - 1, PAL_CEL - 1, 0xFFFFFF), g_moldura(j, cx + 1, cy + 1, PAL_CEL - 3, PAL_CEL - 3, 0x000000);
    }
    /* vista previa y hexadecimal */
    mostra(j, x + 168, y, 60, 40, rgb3(c->r, c->g, c->b), c->alfa);
    g_moldura(j, x + 168, y, 60, 40, mod_tema.borde_forte);
    char t[24];
    formatar_texto(t, sizeof t, "#%02x%02x%02x", c->r, c->g, c->b);
    g_texto(j, x + 168, y + 44, t, COR_TEXTO);
    /* cuadro saturación/brillo (celdas de 2x2) */
    for (int yy = 0; yy < SV_A; yy += 2)
        for (int xx = 0; xx < SV_L; xx += 2) {
            int r, g, b;
            cor_hsv_para_rgb(c->h, xx * 255 / (SV_L - 1), 255 - yy * 255 / (SV_A - 1), &r, &g, &b);
            g_retangulo(j, x + SV_X + xx, y + SV_Y + yy, 2, 2, rgb3(r, g, b));
        }
    g_moldura(j, x + SV_X - 1, y + SV_Y - 1, SV_L + 2, SV_A + 2, mod_tema.borde_forte);
    int mx = x + SV_X + c->s * (SV_L - 1) / 255, my = y + SV_Y + (255 - c->v) * (SV_A - 1) / 255;
    g_moldura(j, mx - 3, my - 3, 7, 7, 0x000000);
    g_moldura(j, mx - 2, my - 2, 5, 5, 0xFFFFFF);
    /* barra de tono */
    for (int yy = 0; yy < SV_A; yy++) {
        int r, g, b;
        cor_hsv_para_rgb(yy * 359 / (SV_A - 1), 255, 255, &r, &g, &b);
        g_retangulo(j, x + TONO_X, y + SV_Y + yy, BARRA_L, 1, rgb3(r, g, b));
    }
    g_moldura(j, x + TONO_X - 1, y + SV_Y - 1, BARRA_L + 2, SV_A + 2, mod_tema.borde_forte);
    int hy = y + SV_Y + c->h * (SV_A - 1) / 359;
    g_retangulo(j, x + TONO_X - 2, hy - 1, BARRA_L + 4, 3, 0xFFFFFF);
    g_moldura(j, x + TONO_X - 2, hy - 2, BARRA_L + 4, 5, 0x202020);
    /* barra de alfa */
    if (c->com_alfa) {
        for (int yy = 0; yy < SV_A; yy += 2) {
            tablero(j, x + ALFA_X, y + SV_Y + yy, BARRA_L, 2);
            g_retangulo(j, x + ALFA_X, y + SV_Y + yy, BARRA_L, 2,
                        mod_mezclar(((yy / 6) & 1) ? 0xB8B8B8 : 0xF0F0F0, rgb3(c->r, c->g, c->b), (unsigned)(255 - yy * 255 / (SV_A - 1))));
        }
        g_moldura(j, x + ALFA_X - 1, y + SV_Y - 1, BARRA_L + 2, SV_A + 2, mod_tema.borde_forte);
        int ay = y + SV_Y + (255 - c->alfa) * (SV_A - 1) / 255;
        g_retangulo(j, x + ALFA_X - 2, ay - 1, BARRA_L + 4, 3, 0xFFFFFF);
        g_moldura(j, x + ALFA_X - 2, ay - 2, BARRA_L + 4, 5, 0x202020);
        formatar_texto(t, sizeof t, "A %d", c->alfa);
        g_texto(j, x + 204, y + SV_Y, t, COR_TEXTO);
    }
    /* deslizadores R, G, B */
    static const char eti[3] = { 'R', 'G', 'B' };
    int val[3] = { c->r, c->g, c->b };
    for (int k = 0; k < 3; k++) {
        int sy = y + SL_Y0 + k * SL_PASO;
        char e[2] = { eti[k], 0 };
        g_texto(j, x, sy - 2, e, COR_TEXTO);
        for (int xx = 0; xx < SL_L; xx += 2) {
            int v = xx * 255 / (SL_L - 1);
            int rr = k == 0 ? v : c->r, gg = k == 1 ? v : c->g, bb = k == 2 ? v : c->b;
            g_retangulo(j, x + SL_X + xx, sy, 2, 12, rgb3(rr, gg, bb));
        }
        g_moldura(j, x + SL_X - 1, sy - 1, SL_L + 2, 14, mod_tema.borde_forte);
        barra_valor(j, x + SL_X, sy, SL_L, 12, val[k], 255);
        formatar_texto(t, sizeof t, "%d", val[k]);
        g_texto(j, x + SL_X + SL_L + 8, sy - 2, t, COR_TEXTO);
    }
    jan_mostrar_area(j, x, y, SELCOR_L, SELCOR_A);
}

/* Aplica la posición del ratón al control que se arrastra. */
static bool aplicar(seletorcor_t *c, int ex, int ey)
{
    int x = ex - c->x, y = ey - c->y;
    seletorcor_t antes = *c;
    switch (c->arrasto) {
    case 1:
        c->s = lim((x - SV_X) * 255 / (SV_L - 1), 0, 255);
        c->v = 255 - lim((y - SV_Y) * 255 / (SV_A - 1), 0, 255);
        de_hsv(c);
        break;
    case 2:
        c->h = lim((y - SV_Y) * 359 / (SV_A - 1), 0, 359);
        de_hsv(c);
        break;
    case 3:
        c->alfa = 255 - lim((y - SV_Y) * 255 / (SV_A - 1), 0, 255);
        break;
    case 4: case 5: case 6: {
        int v = lim((x - SL_X) * 255 / (SL_L - 1), 0, 255);
        if (c->arrasto == 4) c->r = v; else if (c->arrasto == 5) c->g = v; else c->b = v;
        de_rgb(c);
        break;
    }
    default: return false;
    }
    return memcmp(&antes, c, sizeof *c) != 0;
}

int seletorcor_tratar(jan_t *j, seletorcor_t *c, const evento_t *e)
{
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        int x = e->x - c->x, y = e->y - c->y;
        if (x < 0 || y < 0 || x >= SELCOR_L || y >= SELCOR_A) return SELCOR_NADA;
        c->arrasto = 0;
        if (y < PAL_FILAS * PAL_CEL && x < PAL_COLS * PAL_CEL) {
            uint32_t col = edpaleta_cores[(y / PAL_CEL) * PAL_COLS + x / PAL_CEL];
            c->r = (int)(col >> 16); c->g = (int)(col >> 8) & 255; c->b = (int)col & 255;
            de_rgb(c);
            seletorcor_desenhar(j, c);
            return SELCOR_MUDOU;
        }
        if (y >= SV_Y && y < SV_Y + SV_A) {
            if (x >= SV_X && x < SV_X + SV_L) c->arrasto = 1;
            else if (x >= TONO_X - 2 && x < TONO_X + BARRA_L + 2) c->arrasto = 2;
            else if (c->com_alfa && x >= ALFA_X - 2 && x < ALFA_X + BARRA_L + 2) c->arrasto = 3;
        }
        for (int k = 0; k < 3; k++)
            if (y >= SL_Y0 + k * SL_PASO - 2 && y < SL_Y0 + k * SL_PASO + 14 && x >= SL_X - 4 && x < SL_X + SL_L + 4) c->arrasto = 4 + k;
        if (!c->arrasto) return SELCOR_NADA;
        bool m = aplicar(c, e->x, e->y);
        seletorcor_desenhar(j, c);
        return m ? SELCOR_MUDOU : SELCOR_NADA;
    }
    if (e->tipo == EV_MOVER && c->arrasto && (e->botoes & 1)) {
        if (!aplicar(c, e->x, e->y)) return SELCOR_NADA;
        seletorcor_desenhar(j, c);
        return SELCOR_MUDOU;
    }
    if (e->tipo == EV_SOLTOU && c->arrasto) { c->arrasto = 0; return SELCOR_NADA; }
    return SELCOR_NADA;
}

bool seletor_cor_dialogo(const char *titulo, uint32_t *argb, bool com_alfa)
{
    titulo = idioma_traducir_sl(titulo);
    int larg = SELCOR_L + 24, alt = SELCOR_A + 24 + 44;
    jan_t *j = jan_abrir(titulo, larg, alt);
    if (!j) return false;
    jan_estilo(j, JAN_DIALOGO, 0, 0);
    g_retangulo(j, 0, 0, larg, alt, COR_FUNDO);
    seletorcor_t c;
    seletorcor_iniciar(&c, 12, 12, *argb, com_alfa);
    mbotao_t ok = { .x = larg - 12 - 92, .y = alt - 40, .l = 92, .a = 28, .variante = MB_PRIMARIO, .texto = N_("Aceptar"), .fundo = COR_FUNDO };
    mbotao_t no = { .x = larg - 12 - 92 - 100, .y = alt - 40, .l = 92, .a = 28, .variante = MB_SECUNDARIO, .texto = N_("Cancelar"), .fundo = COR_FUNDO };
    seletorcor_desenhar(j, &c);
    mbotao_desenhar(j, &ok);
    mbotao_desenhar(j, &no);
    jan_mostrar(j);
    registrar("selcor: abierto\n");
    bool aceptado = false;
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TEMA) { g_retangulo(j, 0, 0, larg, alt, COR_FUNDO); seletorcor_desenhar(j, &c); mbotao_desenhar(j, &ok); mbotao_desenhar(j, &no); jan_mostrar(j); }
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (e.tipo == EV_TECLA && e.tecla == '\n') { aceptado = true; break; }
        if (seletorcor_tratar(j, &c, &e) == SELCOR_MUDOU) registrar("selcor: color %08x\n", seletorcor_cor(&c));
        if (mbotao_tratar(j, &ok, &e)) { aceptado = true; break; }
        if (mbotao_tratar(j, &no, &e)) break;
    }
    if (aceptado) *argb = seletorcor_cor(&c);
    jan_fechar(j);
    return aceptado;
}
