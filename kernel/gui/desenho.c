// SPDX-License-Identifier: GPL-3.0-only
/* desenho.c - rutinas de dibujo con recorte, usadas por el escritorio
 * y por el gestor de ventanas. Todo se recorta contra un rectángulo, para
 * que redibujar sólo un pedazo de la pantalla nunca pinte fuera de él.
 * Los rectángulos y las copias de líneas usan rapido.c (rep movsd o SSE2, D042). */
#include "desenho.h"
#include "rapido.h"

extern const unsigned char fonte_sagui[256][16];

void sup_iniciar(superficie_t *s, uint32_t *px, int larg, int alt)
{
    s->px = px; s->larg = larg; s->alt = alt;
    sup_recorte(s, 0, 0, 0, 0);
}

void sup_recorte(superficie_t *s, int x, int y, int l, int a)
{
    if (l <= 0 || a <= 0) { s->cx0 = 0; s->cy0 = 0; s->cx1 = s->larg; s->cy1 = s->alt; return; }
    s->cx0 = x < 0 ? 0 : x;
    s->cy0 = y < 0 ? 0 : y;
    s->cx1 = x + l > s->larg ? s->larg : x + l;
    s->cy1 = y + a > s->alt ? s->alt : y + a;
}

void sup_retangulo(superficie_t *s, int x, int y, int l, int a, uint32_t cor)
{
    int x0 = x < s->cx0 ? s->cx0 : x, y0 = y < s->cy0 ? s->cy0 : y;
    int x1 = x + l > s->cx1 ? s->cx1 : x + l, y1 = y + a > s->cy1 ? s->cy1 : y + a;
    if (x0 >= x1) return;
    for (int j = y0; j < y1; j++)
        rapido_preencher32(s->px + j * s->larg + x0, cor, (size_t)(x1 - x0));
}

void sup_moldura(superficie_t *s, int x, int y, int l, int a, uint32_t cor)
{
    sup_retangulo(s, x, y, l, 1, cor);
    sup_retangulo(s, x, y + a - 1, l, 1, cor);
    sup_retangulo(s, x, y, 1, a, cor);
    sup_retangulo(s, x + l - 1, y, 1, a, cor);
}

static inline void ponto(superficie_t *s, int x, int y, uint32_t cor)
{
    if (x >= s->cx0 && x < s->cx1 && y >= s->cy0 && y < s->cy1)
        s->px[y * s->larg + x] = cor;
}

int texto_largura(const char *t)
{
    int n = 0;
    while (t[n]) n++;
    return n * 8;
}

void sup_texto(superficie_t *s, int x, int y, const char *t, uint32_t cor)
{
    for (; *t; t++, x += 8) {
        if (x >= s->cx1) break;
        if (x + 8 <= s->cx0) continue;
        const unsigned char *g = fonte_sagui[(uint8_t)*t];
        for (int j = 0; j < 16; j++)
            for (int i = 0; i < 8; i++)
                if (g[j] & (0x80 >> i)) ponto(s, x + i, y + j, cor);
    }
}

void sup_desenho(superficie_t *s, int x, int y, const desenho_t *d, int escala)
{
    for (int j = 0; j < d->altura; j++)
        for (int i = 0; i < d->largura; i++) {
            uint32_t cor;
            if (arte_cor(d->linhas[j][i], &cor))
                sup_retangulo(s, x + i * escala, y + j * escala, escala, escala, cor);
        }
}

/* Para los botones de la barra y los menús (D065): un icono de 32x32 cabe en
 * 16x16 tomando un píxel de cada cuadrado de 2x2. Prefiere el píxel opaco del
 * cuadrado, para que los contornos finos no desaparezcan. */
void sup_desenho_metade(superficie_t *s, int x, int y, const desenho_t *d)
{
    int f = d->largura > 16 ? d->largura / 16 : 1;
    for (int j = 0; j < d->altura / f; j++)
        for (int i = 0; i < d->largura / f; i++) {
            uint32_t cor;
            bool achou = false;
            for (int k = 0; k < f * f && !achou; k++)
                achou = arte_cor(d->linhas[j * f + k / f][i * f + k % f], &cor);
            if (achou) ponto(s, x + i, y + j, cor);
        }
}

void sup_copiar(superficie_t *s, int x, int y, const uint32_t *origem, int larg, int alt, int passo)
{
    int x0 = x < s->cx0 ? s->cx0 : x, y0 = y < s->cy0 ? s->cy0 : y;
    int x1 = x + larg > s->cx1 ? s->cx1 : x + larg, y1 = y + alt > s->cy1 ? s->cy1 : y + alt;
    if (x0 >= x1) return;
    for (int j = y0; j < y1; j++)
        rapido_copiar(s->px + j * s->larg + x0, origem + (j - y) * passo + (x0 - x), (size_t)(x1 - x0) * 4);
}

/* D365: mezcla pct % del origen sobre el destino, canal a canal (sin SSE: solo se usa en widgets translúcidos). */
void sup_misturar(superficie_t *s, int x, int y, const uint32_t *origem, int larg, int alt, int passo, int pct, bool clave)
{
    int x0 = x < s->cx0 ? s->cx0 : x, y0 = y < s->cy0 ? s->cy0 : y;
    int x1 = x + larg > s->cx1 ? s->cx1 : x + larg, y1 = y + alt > s->cy1 ? s->cy1 : y + alt;
    if (x0 >= x1) return;
    uint32_t a = (uint32_t)pct * 256 / 100, b = 256 - a;
    for (int j = y0; j < y1; j++) {
        uint32_t *d = s->px + j * s->larg + x0;
        const uint32_t *o = origem + (j - y) * passo + (x0 - x);
        for (int i = 0; i < x1 - x0; i++) {
            uint32_t p = o[i], q = d[i];
            if (clave && (p >> 24) == 0xFF) continue;
            uint32_t rb = (((p & 0xFF00FF) * a + (q & 0xFF00FF) * b) >> 8) & 0xFF00FF;
            uint32_t g = (((p & 0x00FF00) * a + (q & 0x00FF00) * b) >> 8) & 0x00FF00;
            d[i] = rb | g;
        }
    }
}
