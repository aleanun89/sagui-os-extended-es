// SPDX-License-Identifier: GPL-2.0-or-later
/* ocio_efectos.c - los efectos del salvapantallas (D422). Ver ocio_efectos.h. Sin dependencias: lo incluye el
 * kernel y Configuración (la vista previa). */
#include "ocio_efectos.h"

typedef struct { unsigned *pix; int w, h, st; } oe_lienzo_t;

static unsigned oe_azar(unsigned *s)
{
    unsigned x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}

static void oe_rect(const oe_lienzo_t *c, int x, int y, int l, int a, unsigned cor)
{
    if (x < 0) { l += x; x = 0; }
    if (y < 0) { a += y; y = 0; }
    if (x + l > c->w) l = c->w - x;
    if (y + a > c->h) a = c->h - y;
    if (l <= 0 || a <= 0) return;
    for (int j = 0; j < a; j++) {
        unsigned *p = c->pix + (y + j) * c->st + x;
        for (int i = 0; i < l; i++) p[i] = cor;
    }
}

static unsigned oe_mezcla(unsigned cor, int pct)   /* pct % del color (0-100) sobre negro */
{
    unsigned r = ((cor >> 16) & 255) * (unsigned)pct / 100, g = ((cor >> 8) & 255) * (unsigned)pct / 100, b = (cor & 255) * (unsigned)pct / 100;
    return (r << 16) | (g << 8) | b;
}

/* La fuente de 5x7: dígitos, «:» y las letras de SAGUIOS. */
static const unsigned char oe_glifos[17][7] = {
    { 14, 17, 19, 21, 25, 17, 14 }, { 4, 12, 4, 4, 4, 4, 14 }, { 14, 17, 1, 2, 4, 8, 31 }, { 30, 1, 1, 14, 1, 1, 30 },
    { 2, 6, 10, 18, 31, 2, 2 }, { 31, 16, 30, 1, 1, 17, 14 }, { 6, 8, 16, 30, 17, 17, 14 }, { 31, 1, 2, 4, 8, 8, 8 },
    { 14, 17, 17, 14, 17, 17, 14 }, { 14, 17, 17, 15, 1, 2, 12 }, { 0, 4, 4, 0, 4, 4, 0 },
    { 15, 16, 16, 14, 1, 1, 30 },   /* S */ { 14, 17, 17, 31, 17, 17, 17 },   /* A */ { 14, 17, 16, 23, 17, 17, 15 },   /* G */
    { 17, 17, 17, 17, 17, 17, 14 },   /* U */ { 14, 4, 4, 4, 4, 4, 14 },   /* I */
};
static int oe_indice(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    switch (c) { case ':': return 10; case 'S': return 11; case 'A': return 12; case 'G': return 13; case 'U': return 14; case 'I': return 15; }
    return -1;   /* la O se dibuja como el cero (índice 0) */
}

static void oe_glifo(const oe_lienzo_t *c, int x, int y, char ch, int esc, unsigned cor)
{
    int k = ch == 'O' ? 0 : oe_indice(ch);
    if (k < 0) return;
    for (int f = 0; f < 7; f++)
        for (int b = 0; b < 5; b++)
            if (oe_glifos[k][f] & (16 >> b)) oe_rect(c, x + b * esc, y + f * esc, esc, esc, cor);
}

static int oe_texto_ancho(const char *t, int esc)
{
    int n = 0;
    while (t[n]) n++;
    return n ? (n * 6 - 1) * esc : 0;
}

static void oe_texto(const oe_lienzo_t *c, int x, int y, const char *t, int esc, unsigned cor)
{
    for (int i = 0; t[i]; i++) oe_glifo(c, x + i * 6 * esc, y, t[i], esc, cor);
}

/* ------------------------------------------------------------ estrellas */

static void oe_estrella_nueva(ocio_ef_t *e, int i, int lejos)
{
    e->ex[i] = (int)(oe_azar(&e->semilla) % 4096) - 2048;
    e->ey[i] = (int)(oe_azar(&e->semilla) % 4096) - 2048;
    e->ez[i] = lejos ? 1024 : (int)(oe_azar(&e->semilla) % 1020) + 4;
}

static void oe_estrellas(ocio_ef_t *e, const oe_lienzo_t *c, unsigned dt)
{
    int avance = (int)(dt * 30 / 100);   /* unidades de z por cuadro de dt ms */
    if (avance < 1) avance = 1;
    if (avance > 120) avance = 120;
    int esc = c->w > c->h ? c->w : c->h;
    for (int i = 0; i < OCIO_ESTRELLAS_MAX; i++) {
        e->ez[i] -= avance;
        if (e->ez[i] < 4) oe_estrella_nueva(e, i, 1);
        int z = e->ez[i];
        int sx = c->w / 2 + e->ex[i] * (esc / 2) / z / 4, sy = c->h / 2 + e->ey[i] * (esc / 2) / z / 4;
        if (sx < 0 || sy < 0 || sx >= c->w || sy >= c->h) { oe_estrella_nueva(e, i, 1); continue; }
        int br = 100 - z * 70 / 1100;
        int tam = z < 250 ? 4 : z < 700 ? 2 : 1;
        oe_rect(c, sx, sy, tam, tam, oe_mezcla(0xFFFFFF, br < 40 ? 40 : br));
    }
}

/* ------------------------------------------------------------ logotipo */

static int oe_rebote(unsigned t, int rango, int *vueltas)   /* onda triangular: 0..rango..0 */
{
    if (rango <= 0) { *vueltas = 0; return 0; }
    unsigned p = t % (unsigned)(2 * rango);
    *vueltas = (int)(t / (unsigned)rango);
    return p < (unsigned)rango ? (int)p : 2 * rango - (int)p;
}

static void oe_logotipo(ocio_ef_t *e, const oe_lienzo_t *c, unsigned ms)
{
    static const unsigned colores[6] = { 0x2E7D32, 0x1F5FAF, 0xC0261B, 0xB8860B, 0x7B3FA0, 0x00838F };
    int esc = c->w / 150;
    if (esc < 2) esc = 2;
    int l = oe_texto_ancho("SAGUIOS", esc) + 6 * esc, a = 7 * esc + 6 * esc;
    unsigned t = (ms - e->ms_inicio) / 12 * (unsigned)(esc > 4 ? 2 : 1);   /* píxeles recorridos */
    int vx, vy;
    int x = oe_rebote(t, c->w - l, &vx);
    int y = oe_rebote(t * 7 / 10, c->h - a, &vy);
    unsigned cor = colores[(unsigned)(vx + vy) % 6];
    oe_rect(c, x, y, l, a, cor);
    oe_rect(c, x, y, l, esc, oe_mezcla(0xFFFFFF, 40));
    oe_texto(c, x + 3 * esc, y + 3 * esc, "SAGUIOS", esc, 0xFFFFFF);
}

/* ------------------------------------------------------------ tuberías */

#define OE_CASILLA 18
#define OE_GROSOR  10

static void oe_tuberias_reiniciar(ocio_ef_t *e, const oe_lienzo_t *c)
{
    e->n_tubos = 0;
    int cw = c->w / OE_CASILLA, ch = c->h / OE_CASILLA;
    if (cw < 2) cw = 2;
    if (ch < 2) ch = 2;
    for (int k = 0; k < OCIO_TUBERIAS_N; k++) {
        e->tx[k] = (int)(oe_azar(&e->semilla) % (unsigned)cw);
        e->ty[k] = (int)(oe_azar(&e->semilla) % (unsigned)ch);
        e->td[k] = (int)(oe_azar(&e->semilla) % 4);
        e->tc[k] = (int)(oe_azar(&e->semilla) % 5);
    }
}

static void oe_tuberias_paso(ocio_ef_t *e, const oe_lienzo_t *c)
{
    static const int dx[4] = { 1, 0, -1, 0 }, dy[4] = { 0, 1, 0, -1 };
    int cw = c->w / OE_CASILLA, ch = c->h / OE_CASILLA;
    if (cw < 2) cw = 2;
    if (ch < 2) ch = 2;
    for (int k = 0; k < OCIO_TUBERIAS_N; k++) {
        if (e->n_tubos >= OCIO_TUBOS_MAX) { oe_tuberias_reiniciar(e, c); return; }
        if (oe_azar(&e->semilla) % 5 == 0) e->td[k] = (e->td[k] + (oe_azar(&e->semilla) & 1 ? 1 : 3)) & 3;
        int nx = e->tx[k] + dx[e->td[k]], ny = e->ty[k] + dy[e->td[k]];
        if (nx < 0 || ny < 0 || nx >= cw || ny >= ch) {   /* choca con el borde: gira */
            e->td[k] = (e->td[k] + (oe_azar(&e->semilla) & 1 ? 1 : 3)) & 3;
            nx = e->tx[k] + dx[e->td[k]]; ny = e->ty[k] + dy[e->td[k]];
            if (nx < 0 || ny < 0 || nx >= cw || ny >= ch) { e->tx[k] = cw / 2; e->ty[k] = ch / 2; continue; }
        }
        ocio_tubo_t *t = &e->tubos[e->n_tubos++];
        t->x = (short)nx; t->y = (short)ny; t->cor = (unsigned char)e->tc[k];
        t->cx = (unsigned char)((e->tx[k] - nx + 1) | ((e->ty[k] - ny + 1) << 2));   /* de dónde viene (desplazamiento + 1) */
        e->tx[k] = nx; e->ty[k] = ny;
    }
}

static void oe_tuberias(ocio_ef_t *e, const oe_lienzo_t *c, unsigned ms)
{
    static const unsigned colores[5] = { 0x2E9E4F, 0x2F6FD0, 0xD04A2F, 0xD0A82F, 0x9A4FD0 };
    if (e->n_tubos == 0 && e->ms_tubo == 0) oe_tuberias_reiniciar(e, c);
    unsigned dt = ms - e->ms_tubo;
    if (e->ms_tubo == 0 || dt > 2000) { e->ms_tubo = ms; dt = 0; }
    unsigned pasos = dt / 45;
    if (pasos > 40) pasos = 40;
    if (pasos) e->ms_tubo += pasos * 45;
    while (pasos--) oe_tuberias_paso(e, c);
    int off = (OE_CASILLA - OE_GROSOR) / 2;
    for (int i = 0; i < e->n_tubos; i++) {
        const ocio_tubo_t *t = &e->tubos[i];
        unsigned cor = colores[t->cor];
        int px = t->x * OE_CASILLA, py = t->y * OE_CASILLA;
        int ddx = (t->cx & 3) - 1, ddy = ((t->cx >> 2) & 3) - 1;   /* hacia la casilla de la que viene */
        oe_rect(c, px + off, py + off, OE_GROSOR, OE_GROSOR, cor);
        if (ddx) oe_rect(c, ddx > 0 ? px + off + OE_GROSOR : px - off, py + off, OE_CASILLA - OE_GROSOR, OE_GROSOR, cor);
        if (ddy) oe_rect(c, px + off, ddy > 0 ? py + off + OE_GROSOR : py - off, OE_GROSOR, OE_CASILLA - OE_GROSOR, cor);
        oe_rect(c, px + off + 1, py + off + 1, 3, OE_GROSOR - 2, oe_mezcla(0xFFFFFF, 45));   /* brillo */
    }
}

/* ------------------------------------------------------------ reloj */

static void oe_reloj(ocio_ef_t *e, const oe_lienzo_t *c, int hora, int min, int seg)
{
    char t[6] = { (char)('0' + hora / 10 % 10), (char)('0' + hora % 10), ':', (char)('0' + min / 10 % 10), (char)('0' + min % 10), 0 };
    int esc = c->w / 40;
    if (esc < 2) esc = 2;
    int l = oe_texto_ancho(t, esc), a = 7 * esc;
    unsigned s = e->semilla ^ (unsigned)(hora * 60 + min) * 2654435761u;   /* cambia de sitio cada minuto */
    int x = c->w > l ? (int)(oe_azar(&s) % (unsigned)(c->w - l)) : 0;
    int y = c->h > a + 40 ? (int)(oe_azar(&s) % (unsigned)(c->h - a - 40)) : 0;
    oe_texto(c, x, y, t, esc, 0xDDE6F0);
    int bl = (l * (seg < 0 ? 0 : seg > 59 ? 59 : seg)) / 59;
    oe_rect(c, x, y + a + esc * 2, l, 3, 0x203040);
    oe_rect(c, x, y + a + esc * 2, bl, 3, 0x2E7DD0);
}

/* ------------------------------------------------------------ la entrada */

void ocio_ef_iniciar(ocio_ef_t *e, int efecto, unsigned semilla, int w, int h, unsigned ms)
{
    unsigned char *p = (unsigned char *)e;
    for (unsigned i = 0; i < sizeof *e; i++) p[i] = 0;
    e->efecto = efecto;
    e->semilla = semilla ? semilla : 1;
    e->ms_ultimo = e->ms_inicio = ms;
    oe_azar(&e->semilla);
    for (int i = 0; i < OCIO_ESTRELLAS_MAX; i++) oe_estrella_nueva(e, i, 0);
    (void)w; (void)h;
}

void ocio_ef_cuadro(ocio_ef_t *e, unsigned *pix, int w, int h, int stride, unsigned ms, int limpiar,
                    int hora, int min, int seg)
{
    oe_lienzo_t c = { pix, w, h, stride };
    unsigned dt = ms - e->ms_ultimo;
    e->ms_ultimo = ms;
    if (limpiar) oe_rect(&c, 0, 0, w, h, 0);
    switch (e->efecto) {
    case OCIO_EF_ESTRELLAS: oe_estrellas(e, &c, dt); break;
    case OCIO_EF_LOGOTIPO:  oe_logotipo(e, &c, ms); break;
    case OCIO_EF_TUBERIAS:  oe_tuberias(e, &c, ms); break;
    case OCIO_EF_RELOJ:     oe_reloj(e, &c, hora, min, seg); break;
    default: break;
    }
}
