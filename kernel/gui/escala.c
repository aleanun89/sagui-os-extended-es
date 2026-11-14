// SPDX-License-Identifier: GPL-2.0-or-later
/* escala.c - copia ampliada o reducida de píxeles (pantalla completa, D044).
 *
 * Vecino más cercano, en punto fijo 16.16: cada píxel del destino toma el
 * píxel del origen que cae debajo de él. Respeta el recorte de la superficie. */
#include "desenho.h"
#include "rapido.h"

void sup_escalar(superficie_t *s, int x, int y, int l, int a, const uint32_t *origem, int ol, int oa)
{
    if (l <= 0 || a <= 0 || ol <= 0 || oa <= 0) return;
    int x0 = x < s->cx0 ? s->cx0 : x, y0 = y < s->cy0 ? s->cy0 : y;
    int x1 = x + l > s->cx1 ? s->cx1 : x + l, y1 = y + a > s->cy1 ? s->cy1 : y + a;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t passo_x = ((uint32_t)ol << 16) / (uint32_t)l, passo_y = ((uint32_t)oa << 16) / (uint32_t)a;
    /* D245: ampliación por enteros (x2, x3...): cada fila del origen se expande una vez y las
     * demás filas iguales se copian con rapido_copiar (SSE2 si el arranque lo eligió). */
    int kx = (l % ol == 0) ? l / ol : 0;
    int sy_ant = -1;
    uint32_t *d_ant = 0;
    for (int j = y0; j < y1; j++) {
        uint32_t sy = ((uint32_t)(j - y) * passo_y) >> 16;
        if (sy >= (uint32_t)oa) sy = (uint32_t)oa - 1;
        uint32_t *d = s->px + j * s->larg;
        if ((int)sy == sy_ant) {
            rapido_copiar(d + x0, d_ant + x0, (size_t)(x1 - x0) * 4);
            continue;
        }
        sy_ant = (int)sy; d_ant = d;
        const uint32_t *o = origem + sy * (uint32_t)ol;
        if (kx == 2 || kx == 3) {
            int sx = (x0 - x) / kx, ph = (x0 - x) % kx;   /* píxel de origen y fase dentro de él */
            for (int i = x0; i < x1; ) {
                uint32_t v = o[sx];
                int n = kx - ph;
                if (n > x1 - i) n = x1 - i;
                for (int q = 0; q < n; q++) d[i + q] = v;
                i += n; sx++; ph = 0;
            }
            continue;
        }
        uint32_t fx = (uint32_t)(x0 - x) * passo_x;
        for (int i = x0; i < x1; i++, fx += passo_x) {
            uint32_t sx = fx >> 16;
            d[i] = o[sx < (uint32_t)ol ? sx : (uint32_t)ol - 1];
        }
    }
}
