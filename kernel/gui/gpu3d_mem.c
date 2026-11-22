// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_mem.c - piezas comunes de los drivers 3D: memoria de GPU por bloques,
 * búferes y coma flotante con enteros (D285).
 *
 * No toca hardware ni usa nada del kernel salvo memset: se compila también en
 * el PC para la prueba de unidad (tests/gen3_3d_unidad.c).
 *
 * Memoria de GPU: una zona (g3d_arena_t) con su dirección de GPU, repartida en
 * bloques seguidos, libres u ocupados; pedir es buscar el primer bloque libre
 * donde quepa con la alineación pedida y partirlo; soltar es marcarlo libre y
 * unirlo con sus vecinos libres. Son pocas piezas grandes (destino, Z,
 * texturas, lote), así que una lista corta basta. */
#include "gpu3d.h"

void *memset(void *d, int c, size_t n);

/* Coma fija con «frac» bits -> bits de un float (redondeando hacia cero). */
uint32_t g3d_float_de_fixo(int32_t v, int frac)
{
    if (!v) return 0;
    uint32_t sinal = v < 0 ? 0x80000000u : 0, m = v < 0 ? (uint32_t)-v : (uint32_t)v;
    int e = 31;
    while (!(m & 0x80000000u)) { m <<= 1; e--; }
    return sinal | ((uint32_t)(127 + e - frac) << 23) | ((m >> 8) & 0x7FFFFFu);
}

/* Bits de un float -> coma fija con «frac» bits (saturando; NaN e infinito dan el tope). */
int32_t g3d_fixo_de_float(uint32_t bits, int frac)
{
    int e = (int)((bits >> 23) & 0xFF) - 127 + frac;      /* exponente del resultado */
    uint32_t m = (bits & 0x7FFFFFu) | 0x800000u;          /* 1.23 */
    uint32_t r;
    if (((bits >> 23) & 0xFF) == 0) return 0;
    if (e >= 31) r = 0x7FFFFFFFu;
    else if (e >= 23) r = m << (e - 23);
    else if (e >= -1) r = m >> (23 - e);
    else r = 0;
    if (r > 0x7FFFFFFFu) r = 0x7FFFFFFFu;
    return bits & 0x80000000u ? -(int32_t)r : (int32_t)r;
}

/* ------------------------------------------------------------ memoria de GPU y búferes */

void g3d_arena_iniciar(g3d_arena_t *a, uint64_t gpu, uint8_t *cpu, uint64_t bytes)
{
    memset(a, 0, sizeof *a);
    a->gpu = gpu; a->cpu = cpu; a->bytes = bytes;
    a->b[0].off = 0; a->b[0].bytes = bytes; a->b[0].usado = false;
    a->n = 1;
}

uint64_t g3d_arena_livre(const g3d_arena_t *a)
{
    uint64_t t = 0;
    for (int i = 0; i < a->n; i++) if (!a->b[i].usado) t += a->b[i].bytes;
    return t;
}

static void arena_inserir(g3d_arena_t *a, int i, uint64_t off, uint64_t bytes, bool usado)
{
    for (int k = a->n; k > i; k--) a->b[k] = a->b[k - 1];
    a->b[i].off = off; a->b[i].bytes = bytes; a->b[i].usado = usado;
    a->n++;
}

bool g3d_buffer_criar(g3d_arena_t *a, g3d_buffer_t *b, uint8_t tipo, uint64_t bytes, uint32_t alinhamento)
{
    if (!bytes || !alinhamento || (alinhamento & (alinhamento - 1))) return false;
    bytes = (bytes + alinhamento - 1) & ~(uint64_t)(alinhamento - 1);
    for (int i = 0; i < a->n; i++) {
        g3d_bloco_t *k = &a->b[i];
        if (k->usado) continue;
        uint64_t ini = (a->gpu + k->off + alinhamento - 1) & ~(uint64_t)(alinhamento - 1);
        uint64_t off = ini - a->gpu, fim = k->off + k->bytes;
        if (off + bytes > fim) continue;
        if (a->n + 2 > G3D_BLOCOS) return false;
        uint64_t antes = off - k->off, depois = fim - (off + bytes);
        k->off = off; k->bytes = bytes; k->usado = true;
        if (depois) arena_inserir(a, i + 1, off + bytes, depois, false);
        if (antes) arena_inserir(a, i, off - antes, antes, false);
        memset(b, 0, sizeof *b);
        b->usado = true; b->tipo = tipo; b->gpu = a->gpu + off; b->bytes = bytes;
        b->cpu = a->cpu ? a->cpu + (uint32_t)off : NULL;
        return true;
    }
    return false;
}

void g3d_buffer_soltar(g3d_arena_t *a, g3d_buffer_t *b)
{
    if (!b->usado) return;
    uint64_t off = b->gpu - a->gpu;
    for (int i = 0; i < a->n; i++) {
        if (a->b[i].off != off || !a->b[i].usado) continue;
        a->b[i].usado = false;
        if (i + 1 < a->n && !a->b[i + 1].usado) {
            a->b[i].bytes += a->b[i + 1].bytes;
            for (int k = i + 1; k + 1 < a->n; k++) a->b[k] = a->b[k + 1];
            a->n--;
        }
        if (i > 0 && !a->b[i - 1].usado) {
            a->b[i - 1].bytes += a->b[i].bytes;
            for (int k = i; k + 1 < a->n; k++) a->b[k] = a->b[k + 1];
            a->n--;
        }
        break;
    }
    memset(b, 0, sizeof *b);
}

/* ------------------------------------------------------------ presentar (D432) */

int g3d_apres_bandas(const g3d_apres_t *p, int larg, int alt, int r[4][4])
{
    int n = 0;
    int x0 = p->dx, y0 = p->dy, x1 = p->dx + p->dl, y1 = p->dy + p->da;
    if (y0 > 0)    { r[n][0] = 0;  r[n][1] = 0;  r[n][2] = larg;      r[n][3] = y0;       n++; }
    if (y1 < alt)  { r[n][0] = 0;  r[n][1] = y1; r[n][2] = larg;      r[n][3] = alt - y1; n++; }
    if (x0 > 0)    { r[n][0] = 0;  r[n][1] = y0; r[n][2] = x0;        r[n][3] = y1 - y0;  n++; }
    if (x1 < larg) { r[n][0] = x1; r[n][1] = y0; r[n][2] = larg - x1; r[n][3] = y1 - y0;  n++; }
    return n;
}

static void quad6(gpu3d_vert_t q[6], const uint32_t x[2], const uint32_t y[2], const uint32_t u[2], const uint32_t v[2], uint32_t cor)
{
    static const uint8_t ix[6] = { 0, 1, 1, 0, 1, 0 }, iy[6] = { 0, 0, 1, 0, 1, 1 };
    for (int i = 0; i < 6; i++) {
        q[i].x = x[ix[i]]; q[i].y = y[iy[i]];
        q[i].z = 0x3F000000u;        /* 0,5 */
        q[i].rhw = 0x3F800000u;      /* 1 */
        q[i].cor = cor; q[i].espec = 0;
        q[i].u = u[ix[i]]; q[i].v = v[iy[i]];
    }
}

void g3d_apres_quad(const g3d_apres_t *p, int fl, int fa, gpu3d_vert_t q[6])
{
    uint32_t x[2] = { g3d_float_de_fixo(p->dx, 0), g3d_float_de_fixo(p->dx + p->dl, 0) };
    uint32_t y[2] = { g3d_float_de_fixo(p->dy, 0), g3d_float_de_fixo(p->dy + p->da, 0) };
    /* Fracción del búfer con 18 bits (lados de hasta 8191 sin desbordar 32 bits). */
    uint32_t u[2] = { g3d_float_de_fixo((int32_t)(((uint32_t)p->ox << 18) / (uint32_t)fl), 18),
                      g3d_float_de_fixo((int32_t)(((uint32_t)(p->ox + p->ol) << 18) / (uint32_t)fl), 18) };
    uint32_t v[2] = { g3d_float_de_fixo((int32_t)(((uint32_t)p->oy << 18) / (uint32_t)fa), 18),
                      g3d_float_de_fixo((int32_t)(((uint32_t)(p->oy + p->oa) << 18) / (uint32_t)fa), 18) };
    quad6(q, x, y, u, v, 0xFFFFFFFFu);
}

void g3d_quad_cor(int x, int y, int l, int a, uint32_t cor, gpu3d_vert_t q[6])
{
    uint32_t xs[2] = { g3d_float_de_fixo(x, 0), g3d_float_de_fixo(x + l, 0) };
    uint32_t ys[2] = { g3d_float_de_fixo(y, 0), g3d_float_de_fixo(y + a, 0) };
    uint32_t z[2] = { 0, 0 };
    quad6(q, xs, ys, z, z, cor);
}
