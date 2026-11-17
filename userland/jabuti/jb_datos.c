// SPDX-License-Identifier: GPL-3.0-or-later
/* jb_datos.c - la paleta, la biblioteca de personajes y fondos (dibujados aquí, pixel art propio;
 * el sagui sale del arte de la mascota del escritorio) y las tablas de seno (D377). */
#include "jb.h"

const uint32_t jb_paleta[32] = {
    0xFF000000,   /* 0 transparente */
    0x141210, 0x3C2D23, 0x6E553C, 0x968C7D, 0xBEB4A5, 0xF5F5F0, 0xAA7864,
    0xC8202A, 0x7A1218, 0xF07828, 0xFFD52E, 0xD9A400, 0x8CD23C, 0x3CAA3C, 0x0B7A3E,
    0x1EB4A0, 0x7EC8E3, 0x2A6FD8, 0x1E3A8C, 0x8C46C8, 0xE646B4, 0xFF9EC4, 0xE8C8A0,
    0x5A564E, 0xA0522D, 0xCFA36B, 0x9ADCC0, 0xFFE8A0, 0x6A737C, 0xD0D0D0, 0x303030,
};
const char jb_alfabeto[33] = ".0123456789abcdefghijklmnopqrstu";

/* Los colores con nombre (los de «¿tocando el color?» y el lápiz). */
static const struct { const char *n; int i; } colores[] = {
    { "rojo", 8 }, { "naranja", 10 }, { "amarillo", 11 }, { "verde", 14 }, { "turquesa", 16 }, { "azul", 18 },
    { "morado", 20 }, { "rosa", 22 }, { "negro", 1 }, { "blanco", 6 }, { "gris", 4 }, { "marron", 3 },
};
const char *jb_nombre_color(int i)
{
    for (unsigned k = 0; k < sizeof colores / sizeof colores[0]; k++) if (colores[k].i == i) return colores[k].n;
    return NULL;
}
int jb_color_por_nombre(const char *n)
{
    for (unsigned k = 0; k < sizeof colores / sizeof colores[0]; k++) if (compara_texto(colores[k].n, n) == 0) return colores[k].i;
    return -1;
}
int jb_colores_n(void) { return (int)(sizeof colores / sizeof colores[0]); }
const char *jb_color_nombre_n(int k) { return colores[k].n; }

/* ---- trigonometría entera ---- */
static const short tseno[91] = {
    0, 175, 349, 523, 698, 872, 1045, 1219, 1392, 1564,
    1736, 1908, 2079, 2250, 2419, 2588, 2756, 2924, 3090, 3256,
    3420, 3584, 3746, 3907, 4067, 4226, 4384, 4540, 4695, 4848,
    5000, 5150, 5299, 5446, 5592, 5736, 5878, 6018, 6157, 6293,
    6428, 6561, 6691, 6820, 6947, 7071, 7193, 7314, 7431, 7547,
    7660, 7771, 7880, 7986, 8090, 8192, 8290, 8387, 8480, 8572,
    8660, 8746, 8829, 8910, 8988, 9063, 9135, 9205, 9272, 9336,
    9397, 9455, 9511, 9563, 9613, 9659, 9703, 9744, 9781, 9816,
    9848, 9877, 9903, 9925, 9945, 9962, 9976, 9986, 9994, 9998,
    10000
};
int jb_seno(int g)
{
    g %= 360000;
    if (g < 0) g += 360000;
    int neg = 0;
    if (g >= 180000) { g -= 180000; neg = 1; }
    if (g > 90000) g = 180000 - g;
    int i = g / 1000, f = g % 1000;
    int a = tseno[i], b = i < 90 ? tseno[i + 1] : tseno[90];
    int v = a + (b - a) * f / 1000;
    return neg ? -v : v;
}
int jb_coseno(int g) { return jb_seno(g + 90000); }
int jb_raiz(int n)
{
    if (n <= 0) return 0;
    unsigned r = 0, bit = 1u << 30, x = (unsigned)n;
    while (bit > x) bit >>= 2;
    while (bit) {
        if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (int)r;
}

/* ---- primitivas para dibujar los trajes (rejilla de ancho l) ---- */
typedef struct { uint8_t *g; int l, a; } rej_t;
static void p_px(rej_t *r, int x, int y, int c) { if (x >= 0 && y >= 0 && x < r->l && y < r->a) r->g[y * r->l + x] = (uint8_t)c; }
static void p_rect(rej_t *r, int x, int y, int w, int h, int c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) p_px(r, x + i, y + j, c); }
/* disco/elipse con centro (cx2, cy2) y radios (rx2, ry2) en medios píxeles */
static void p_elipse(rej_t *r, int cx2, int cy2, int rx2, int ry2, int c)
{
    for (int y = 0; y < r->a; y++)
        for (int x = 0; x < r->l; x++) {
            long dx = 2 * x + 1 - cx2, dy = 2 * y + 1 - cy2;
            if (dx * dx * ry2 * ry2 + dy * dy * rx2 * rx2 <= (long)rx2 * rx2 * ry2 * ry2) p_px(r, x, y, c);
        }
}
static int cruz(int ax, int ay, int bx, int by, int cx, int cy) { return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax); }
static void p_tri(rej_t *r, int x0, int y0, int x1, int y1, int x2, int y2, int c)
{
    for (int y = 0; y < r->a; y++)
        for (int x = 0; x < r->l; x++) {
            int a = cruz(x0, y0, x1, y1, x, y), b = cruz(x1, y1, x2, y2, x, y), d = cruz(x2, y2, x0, y0, x, y);
            if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) p_px(r, x, y, c);
        }
}
static void p_linea(rej_t *r, int x0, int y0, int x1, int y1, int c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx - dy;
    for (;;) {
        p_px(r, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 > -dy) { e -= dy; x0 += sx; }
        if (e2 < dx) { e += dx; y0 += sy; }
    }
}

/* El sagui: el dibujo de la mascota del escritorio (arte_sagui, 32x32). */
static const char *const arte_sagui[32] = {
    "................................",
    "...........EEEEEEEEEE...........",
    ".........EEEEEEEEEEEEEE.........",
    "..BBB...EEEEEBBBBBEEEEEE..BBB...",
    ".BBBBB.EEEEEEEBBBEEEEEEEEBBBBB..",
    "BBBBBBBEEEEEEEEBEEEEEEEEEBBBBBB.",
    "BBBBBBEEERRRRRRRRRRRRRREEEBBBBB.",
    ".BBBBBEERRPPPRRRRRRPPPRREEBBBBB.",
    "..BBBBEERRPbPRRRRRRPbPRREEBBBB..",
    "...BBBEERRPPPRRRRRRPPPRREEBBB...",
    ".....EEERRRRRRRRRRRRRRRREEE.....",
    "......EERRRRRRPPPRRRRRRREE......",
    "......EEERRRRRRRRRRRRRREEE......",
    ".......EEERRRRPPPPRRRREEE.......",
    "........EEEERRRRRRRREEEE........",
    ".........EEEEEEEEEEEEEE.........",
    "........WWWWWWWWWWWWWWWW........",
    "....wWWWWWCCCCCCCCCCCCWWWWWw....",
    "...wWWMMMCCCCCCCCCCCCCCMMMWWw...",
    "..wWWMMMCCCCCccccccCCCCCMMMWWWw.",
    "..wWWMMCCCCCccccccccCCCCCMMWWw..",
    ".wWWMMMCCCCccccccccccCCCCMMMWWw.",
    ".wWWMMCCCCCccccccccccCCCCCMMWWw.",
    ".wWWMMCCCCCCccccccccCCCCCCMMWWwE",
    ".wWWMMMCCCCCCCCCCCCCCCCCCMMM..EC",
    ".wWWWMMMCCCCCCCCCCCCCCCCMMM..EC.",
    ".wWWWWMMMMCCCCCCCCCCCCMMMM..CE..",
    ".wWWWEEMMMMMMMMMMMMMMMMMMEE.EC..",
    "wwWWEEEE.MMMM......MMMM.EEECE...",
    "...EEEE..EEE........EEE..CECE...",
    "...........................EC...",
    "................................",
};
static int letra_sagui(char c)
{
    switch (c) {
    case 'P': return 1; case 'E': return 2; case 'M': return 3; case 'C': return 4; case 'c': return 5;
    case 'B': return 6; case 'R': return 7; case 'b': return 6; case 'W': return 8; case 'w': return 9;
    default: return 0;
    }
}

static void traje_sagui(rej_t *r, int k)
{
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            int c = letra_sagui(arte_sagui[y][x]);
            if (k == 1 && y >= 27) c = 0;     /* segundo traje: agachado (sin las patas) y con los ojos cerrados */
            if (k == 1 && (y == 8 || y == 9) && c == 1) c = 7;
            p_px(r, x, y, c);
        }
    if (k == 1) { p_rect(r, 4, 25, 24, 3, 3); p_rect(r, 5, 28, 6, 2, 2); p_rect(r, 20, 28, 6, 2, 2); }
}
static void traje_pelota(rej_t *r, int k)
{
    int c = k ? 18 : 8, osc = k ? 19 : 9;
    p_elipse(r, 32, 32, 28, 28, osc);
    p_elipse(r, 31, 30, 26, 26, c);
    p_elipse(r, 22, 20, 8, 6, 6);
    p_rect(r, 4, 15, 24, 2, 6);
}
static void traje_cohete(rej_t *r, int k)
{
    p_tri(r, 16, 1, 9, 12, 23, 12, 8);
    p_rect(r, 9, 12, 15, 12, 6);
    p_rect(r, 9, 12, 2, 12, 5); p_rect(r, 22, 12, 2, 12, 4);
    p_elipse(r, 32, 32, 8, 8, 17); p_elipse(r, 32, 32, 5, 5, 18);
    p_tri(r, 9, 17, 3, 26, 9, 24, 8);
    p_tri(r, 23, 17, 29, 26, 23, 24, 8);
    p_rect(r, 11, 24, 11, 2, 29);
    if (k) { p_tri(r, 11, 26, 21, 26, 16, 31, 11); p_tri(r, 13, 26, 19, 26, 16, 29, 10); }
}
static void traje_estrella(rej_t *r, int k)
{
    int c = k ? 10 : 11, osc = 12;
    int px[10] = { 16, 20, 30, 23, 25, 16, 7, 9, 2, 12 };
    int py[10] = { 1, 11, 12, 18, 29, 23, 29, 18, 12, 11 };
    for (int i = 0; i < 10; i++) { int j = (i + 1) % 10; p_tri(r, 16, 16, px[i], py[i], px[j], py[j], c); }
    for (int i = 0; i < 10; i++) { int j = (i + 1) % 10; p_linea(r, px[i], py[i], px[j], py[j], osc); }
    p_px(r, 13, 13, 1); p_px(r, 18, 13, 1); p_px(r, 14, 18, 1); p_px(r, 15, 19, 1); p_px(r, 16, 19, 1); p_px(r, 17, 18, 1);
}
static void traje_manzana(rej_t *r, int k)
{
    int c = k ? 11 : 8, osc = k ? 12 : 9;
    p_elipse(r, 22, 36, 18, 22, osc); p_elipse(r, 42, 36, 18, 22, osc);
    p_elipse(r, 22, 35, 16, 20, c); p_elipse(r, 42, 35, 16, 20, c);
    p_rect(r, 15, 5, 2, 7, 3);
    p_elipse(r, 40, 12, 8, 4, 14);
    p_elipse(r, 20, 24, 3, 4, 6);
}
static void traje_cesta(rej_t *r, int k)
{
    (void)k;
    p_tri(r, 2, 12, 30, 12, 6, 28, 25); p_tri(r, 30, 12, 26, 28, 6, 28, 25);
    p_rect(r, 3, 11, 26, 3, 3);
    for (int x = 7; x < 28; x += 5) p_linea(r, x, 14, x - 1 + (x > 16 ? -0 : 0), 27, 3);
    p_rect(r, 6, 19, 20, 1, 3);
    p_linea(r, 4, 11, 10, 4, 3); p_linea(r, 10, 4, 22, 4, 3); p_linea(r, 22, 4, 28, 11, 3);
}
static void traje_pez(rej_t *r, int k)
{
    int c = k ? 17 : 10, osc = k ? 18 : 12;
    p_tri(r, 24, 16, 31, 6, 31, 26, osc);
    p_elipse(r, 28, 32, 24, 14, osc); p_elipse(r, 28, 32, 22, 12, c);
    p_tri(r, 12, 7, 20, 7, 16, 12, osc);
    p_elipse(r, 17, 27, 5, 5, 6); p_px(r, 8, 13, 1); p_px(r, 9, 13, 1); p_px(r, 8, 14, 1); p_px(r, 9, 14, 1);
    p_linea(r, 21, 12, 21, 21, osc);
}
static void traje_flor(rej_t *r, int k)
{
    int c = k ? 17 : 22, osc = k ? 18 : 21;
    p_rect(r, 15, 16, 2, 15, 14); p_elipse(r, 44, 50, 10, 4, 14); p_elipse(r, 20, 44, 10, 4, 14);
    static const int dx[5] = { 0, 9, 5, -5, -9 }, dy[5] = { -9, -3, 8, 8, -3 };
    for (int i = 0; i < 5; i++) { p_elipse(r, 32 + dx[i] * 2, 18 + dy[i] * 2, 12, 12, osc); p_elipse(r, 32 + dx[i] * 2, 18 + dy[i] * 2, 10, 10, c); }
    p_elipse(r, 32, 18, 8, 8, 11); p_elipse(r, 32, 18, 5, 5, 12);
}

typedef struct { const char *clave, *titulo; int n; void (*f)(rej_t *, int); } bib_t;
static const bib_t bib[] = {
    { "sagui", N_("Sagui"), 2, traje_sagui }, { "pelota", N_("Pelota"), 2, traje_pelota }, { "cohete", N_("Cohete"), 2, traje_cohete },
    { "estrella", N_("Estrella"), 2, traje_estrella }, { "manzana", N_("Manzana"), 2, traje_manzana }, { "cesta", N_("Cesta"), 1, traje_cesta },
    { "pez", N_("Pez"), 2, traje_pez }, { "flor", N_("Flor"), 2, traje_flor },
};
#define NBIB ((int)(sizeof bib / sizeof bib[0]))
int jb_biblioteca_n(void) { return NBIB; }
const char *jb_biblioteca_nombre(int i) { return bib[i].clave; }
const char *jb_biblioteca_titulo(int i) { return bib[i].titulo; }
int jb_biblioteca_trajes(int i) { return bib[i].n; }
void jb_biblioteca_traje(int i, int k, traje_t *t)
{
    rej_t r = { t->px, TRAJE_L, TRAJE_L };
    memset(t->px, 0, sizeof t->px);
    bib[i].f(&r, k);
    formatar_texto(t->nombre, sizeof t->nombre, "%s%d", bib[i].clave, k + 1);
}
int jb_buscar_biblioteca(const char *c)
{
    for (int i = 0; i < NBIB; i++) if (compara_texto(bib[i].clave, c) == 0) return i;
    return -1;
}

/* ---- fondos ---- */
static void fondo_blanco(rej_t *r) { p_rect(r, 0, 0, FONDO_L, FONDO_A, 6); }
static void fondo_cielo(rej_t *r)
{
    p_rect(r, 0, 0, FONDO_L, 26, 17);
    p_rect(r, 0, 26, FONDO_L, 10, 14);
    p_rect(r, 0, 26, FONDO_L, 1, 15);
    p_elipse(r, 20, 12, 16, 8, 6); p_elipse(r, 30, 10, 12, 8, 6); p_elipse(r, 80, 18, 20, 8, 6); p_elipse(r, 90, 16, 12, 8, 6);
    p_elipse(r, 80, 8, 6, 6, 11);
}
static void fondo_pasto(rej_t *r) { p_rect(r, 0, 0, FONDO_L, FONDO_A, 14); for (int i = 0; i < 40; i++) p_px(r, (i * 17) % FONDO_L, (i * 7) % FONDO_A, 13); }
static void fondo_noche(rej_t *r)
{
    p_rect(r, 0, 0, FONDO_L, FONDO_A, 19);
    for (int i = 0; i < 30; i++) p_px(r, (i * 29 + 5) % FONDO_L, (i * 13 + 3) % 28, i % 3 ? 6 : 11);
    p_elipse(r, 80, 14, 10, 10, 28);
    p_rect(r, 0, 30, FONDO_L, 6, 15);
}
static void fondo_espacio(rej_t *r)
{
    p_rect(r, 0, 0, FONDO_L, FONDO_A, 1);
    for (int i = 0; i < 45; i++) p_px(r, (i * 31 + 7) % FONDO_L, (i * 11 + 2) % FONDO_A, i % 4 ? 5 : 11);
    p_elipse(r, 70, 60, 18, 18, 20); p_elipse(r, 66, 56, 8, 8, 21);
}
static void fondo_cancha(rej_t *r)
{
    p_rect(r, 0, 0, FONDO_L, FONDO_A, 14);
    p_rect(r, 1, 1, FONDO_L - 2, 1, 6); p_rect(r, 1, FONDO_A - 2, FONDO_L - 2, 1, 6);
    p_rect(r, 1, 1, 1, FONDO_A - 2, 6); p_rect(r, FONDO_L - 2, 1, 1, FONDO_A - 2, 6);
    p_rect(r, FONDO_L / 2, 1, 1, FONDO_A - 2, 6);
    p_elipse(r, FONDO_L, FONDO_A, 12, 12, 6); p_elipse(r, FONDO_L, FONDO_A, 10, 10, 14);
}
static void fondo_mar(rej_t *r)
{
    p_rect(r, 0, 0, FONDO_L, 8, 17); p_rect(r, 0, 8, FONDO_L, FONDO_A - 8, 18);
    for (int y = 10; y < FONDO_A; y += 5) for (int x = (y / 5) % 2 * 3; x < FONDO_L; x += 6) p_rect(r, x, y, 2, 1, 17);
    p_rect(r, 0, 32, FONDO_L, 4, 26);
}
typedef struct { const char *clave, *titulo; void (*f)(rej_t *); } fbib_t;
static const fbib_t fb[] = {
    { "blanco", N_("Blanco"), fondo_blanco }, { "cielo", N_("Cielo"), fondo_cielo }, { "pasto", N_("Pasto"), fondo_pasto },
    { "noche", N_("Noche"), fondo_noche }, { "espacio", N_("Espacio"), fondo_espacio }, { "cancha", N_("Cancha"), fondo_cancha },
    { "mar", N_("Mar"), fondo_mar },
};
#define NFB ((int)(sizeof fb / sizeof fb[0]))
int jb_fondos_n(void) { return NFB; }
const char *jb_fondo_nombre(int i) { return fb[i].clave; }
const char *jb_fondo_titulo(int i) { return fb[i].titulo; }
void jb_fondo_biblioteca(int i, fondo_t *f)
{
    rej_t r = { f->px, FONDO_L, FONDO_A };
    memset(f->px, 0, sizeof f->px);
    fb[i].f(&r);
    copia_texto(f->nombre, fb[i].clave, sizeof f->nombre);
}
int jb_buscar_fondo(const char *c)
{
    for (int i = 0; i < NFB; i++) if (compara_texto(fb[i].clave, c) == 0) return i;
    return -1;
}
