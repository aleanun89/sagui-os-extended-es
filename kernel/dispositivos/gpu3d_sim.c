// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_sim.c - un driver 3D de mentira, por la CPU, para probar la capa común (D285).
 *
 * QEMU no emula ninguna GPU con el 3D de los drivers reales. Con
 * «mkimagem.py --gpu3d=simulado» la capa usa este driver: así se prueban en
 * QEMU el camino entero (SaguiGL -> llamada -> capa -> driver -> cuadro), la
 * autoprueba, el dueño del motor, el vigilante y la vuelta al software. No se
 * elige nunca solo y no sirve para ir más rápido.
 *
 * Es un rasterizador pequeño con enteros (el kernel no usa coma flotante):
 * regla de arriba-izquierda, color, especular y textura interpolados, Z de 24
 * bits y mezcla. D404: con rhw distinto en los tres vértices, color, especular
 * y textura con corrección de perspectiva (como una GPU); con G3D_TEX_LINEAR,
 * textura bilineal. Con rhw igual y sin LINEAR, el mismo píxel de antes.
 *
 * D390: para probar en QEMU el diagnóstico común (sentinelas, análisis del destino,
 * «gpu3d probar --ver» y /sistema/conf/gpu3d-simulado.conf) tiene dos «registros» de
 * mentira, 0x1000-0x10FC: 0x1000 hace XOR del color que escribe cada dibujo, y 0x1004
 * distinto de 0 hace que no escriba nada (como una GPU que ejecuta sin tocar la memoria). */
#include "gpu3d.h"
#include "console.h"
#include "memoria.h"
#include "sensores_conv.h"
#include "texto.h"

static uint32_t *cor, *zb;
static int L, A;
static struct { uint32_t *px; int larg, alt; } tex[GPU3D_TEXTURAS];
static bool pronto;
static g3d_vala_t vala;
static uint32_t usados_kb;
static uint32_t regs[0x40];                 /* D390: 0x1000-0x10FC */
#define R_XOR     regs[0]
#define R_MUDO    regs[1]

static void ajustes(int quando)
{
    for (int i = 0; i < gpu3d_ajustes.n; i++)
        if (gpu3d_ajustes.quando[i] == quando) regs[(gpu3d_ajustes.reg[i] - 0x1000u) >> 2 & 0x3Fu] = gpu3d_ajustes.val[i];
}

static void soltar_alvo(void)
{
    if (cor) kfree(cor);
    if (zb) kfree(zb);
    cor = zb = NULL; L = A = 0;
}

static void s_desligar(void)
{
    soltar_alvo();
    for (int i = 0; i < GPU3D_TEXTURAS; i++) if (tex[i].px) { kfree(tex[i].px); tex[i].px = NULL; }
    pronto = false;
}

static int s_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *m, int nm)
{
    (void)d; (void)geracao; (void)t; (void)m; (void)nm;
    s_desligar();
    pronto = true; vala = 0; usados_kb = 0;
    memset(regs, 0, sizeof regs);
    ajustes(G3D_AJ_INICIO);
    klog("[GPU3D] simulado: driver de pruebas por la CPU (no es una GPU)%s\n",
         variante & GPU3D_CAB_GTT ? "; RAM en el GTT pedida (D358): en el simulado no cambia nada" : "");
    return G3D_OK;
}

static int s_alvo(int larg, int alt, int zbits)
{
    (void)zbits;
    if (!pronto) return G3D_FALHA;
    if (larg < 8 || alt < 8 || larg > 1024 || alt > 1024) return G3D_NAO;
    soltar_alvo();
    cor = kmalloc((uint32_t)(larg * alt) * 4);
    zb = kmalloc((uint32_t)(larg * alt) * 4);
    if (!cor || !zb) { soltar_alvo(); return G3D_NAO; }
    L = larg; A = alt;
    usados_kb = (uint32_t)(larg * alt * 8) >> 10;
    return G3D_OK;
}

static int s_limpar(uint32_t mascara, uint32_t c)
{
    if (!pronto || !cor) return G3D_FALHA;
    ajustes(G3D_AJ_DIBUJO);
    if (R_MUDO) return G3D_OK;
    for (int i = 0; i < L * A; i++) {
        if (mascara & 1) cor[i] = c & 0x00FFFFFFu;
        if (mascara & 2) zb[i] = 0x00FFFFFFu;
    }
    return G3D_OK;
}

static int s_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!pronto) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    if (tex[vaga].px) { kfree(tex[vaga].px); tex[vaga].px = NULL; }
    if (!argb) return G3D_OK;
    if (larg < 1 || alt < 1 || larg > 512 || alt > 512) return G3D_NAO;
    tex[vaga].px = kmalloc((uint32_t)(larg * alt) * 4);
    if (!tex[vaga].px) return G3D_NAO;
    memcpy(tex[vaga].px, argb, (uint32_t)(larg * alt) * 4);
    tex[vaga].larg = larg; tex[vaga].alt = alt;
    return G3D_OK;
}

typedef struct { int32_t x, y, z, u, v, q; uint8_t c[4], e[3]; } sv_t;   /* x, y con 4 bits; z de 24 bits; u, v, q (rhw) con 16 bits */

static void converter(const gpu3d_vert_t *g, sv_t *s)
{
    s->x = g3d_fixo_de_float(g->x, 4);
    s->y = g3d_fixo_de_float(g->y, 4);
    int32_t z = g3d_fixo_de_float(g->z, 24);
    s->z = z < 0 ? 0 : z > 0xFFFFFF ? 0xFFFFFF : z;
    s->u = g3d_fixo_de_float(g->u, 16);
    s->v = g3d_fixo_de_float(g->v, 16);
    int32_t q = g3d_fixo_de_float(g->rhw, 16);
    s->q = q < 1 ? 1 : q > 0x7FFFFFF ? 0x7FFFFFF : q;
    s->c[0] = (uint8_t)(g->cor >> 16); s->c[1] = (uint8_t)(g->cor >> 8); s->c[2] = (uint8_t)g->cor; s->c[3] = (uint8_t)(g->cor >> 24);
    s->e[0] = (uint8_t)(g->espec >> 16); s->e[1] = (uint8_t)(g->espec >> 8); s->e[2] = (uint8_t)g->espec;
}

static inline int64_t aresta(const sv_t *a, const sv_t *b, int32_t x, int32_t y)
{
    return (int64_t)(b->x - a->x) * (y - a->y) - (int64_t)(b->y - a->y) * (x - a->x);
}

/* Con el área positiva (sentido horario en pantalla): arista de arriba (horizontal,
 * hacia la derecha) o de la izquierda (sube). */
static inline int cima_esquerda(const sv_t *a, const sv_t *b)
{
    int32_t dx = b->x - a->x, dy = b->y - a->y;
    return dy < 0 || (dy == 0 && dx > 0);
}

static inline int32_t mistura3(int32_t a, int32_t b, int32_t c, int32_t l1, int32_t l2)
{
    return a + (int32_t)(((int64_t)(b - a) * l1 + (int64_t)(c - a) * l2) >> 16);
}

static inline uint32_t fator(uint8_t f, uint32_t alfa)
{
    return f == G3D_F_ZERO ? 0 : f == G3D_F_ALFA ? alfa : f == G3D_F_UM_MENOS_ALFA ? 255 - alfa : 255;
}

/* D404: un texel con la repetición o el borde de la textura. */
static inline uint32_t texel_sim(const uint32_t *tpx, int tl, int ta, int tx, int ty, uint32_t flags)
{
    if (flags & G3D_TEX_REPETE_S) { tx %= tl; if (tx < 0) tx += tl; } else { if (tx < 0) tx = 0; else if (tx >= tl) tx = tl - 1; }
    if (flags & G3D_TEX_REPETE_T) { ty %= ta; if (ty < 0) ty += ta; } else { if (ty < 0) ty = 0; else if (ty >= ta) ty = ta - 1; }
    return tpx[ty * tl + tx];
}

/* D404: pesos con corrección de perspectiva: (l1, l2) de pantalla -> del triángulo, en 16 bits. */
static void perspectiva(int32_t l1, int32_t l2, const sv_t *v0, const sv_t *v1, const sv_t *v2, int32_t *p1, int32_t *p2)
{
    int64_t w0 = 65536 - (int64_t)l1 - l2, w1 = l1, w2 = l2;
    if (w0 < 0) w0 = 0;
    if (w1 < 0) w1 = 0;
    if (w2 < 0) w2 = 0;
    uint64_t b0 = (uint64_t)w0 * (uint32_t)v0->q, b1 = (uint64_t)w1 * (uint32_t)v1->q, b2 = (uint64_t)w2 * (uint32_t)v2->q;
    uint64_t soma = b0 + b1 + b2;
    if (!soma) { *p1 = l1; *p2 = l2; return; }
    int k = 0;
    while ((soma >> k) > 0x7FFFFFFFu) k++;
    uint32_t d = (uint32_t)(soma >> k);
    *p1 = (int32_t)sconv_div64((b1 >> k) << 16, d);
    *p2 = (int32_t)sconv_div64((b2 >> k) << 16, d);
}

static void triangulo(const gpu3d_estado_t *e, sv_t *v0, sv_t *v1, sv_t *v2)
{
    int persp = v0->q != v1->q || v0->q != v2->q;
    int64_t area = aresta(v0, v1, v2->x, v2->y);
    if (area == 0) return;
    if (area < 0) { sv_t *t = v1; v1 = v2; v2 = t; area = -area; }
    if (area >= 0x7FFFFFFF) return;
    int32_t minx = v0->x, maxx = v0->x, miny = v0->y, maxy = v0->y;
    if (v1->x < minx) minx = v1->x;
    if (v2->x < minx) minx = v2->x;
    if (v1->x > maxx) maxx = v1->x;
    if (v2->x > maxx) maxx = v2->x;
    if (v1->y < miny) miny = v1->y;
    if (v2->y < miny) miny = v2->y;
    if (v1->y > maxy) maxy = v1->y;
    if (v2->y > maxy) maxy = v2->y;
    int x0 = minx >> 4, x1 = (maxx + 15) >> 4, y0 = miny >> 4, y1 = (maxy + 15) >> 4;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > L) x1 = L;
    if (y1 > A) y1 = A;
    int b0 = cima_esquerda(v1, v2) ? 0 : -1, b1 = cima_esquerda(v2, v0) ? 0 : -1, b2 = cima_esquerda(v0, v1) ? 0 : -1;
    const uint32_t *tpx = NULL;
    int tl = 0, ta = 0;
    if ((e->flags & G3D_TEXTURA) && e->textura < GPU3D_TEXTURAS && tex[e->textura].px) {
        tpx = tex[e->textura].px; tl = tex[e->textura].larg; ta = tex[e->textura].alt;
    }
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int32_t px = x * 16 + 8, py = y * 16 + 8;
            int64_t w0 = aresta(v1, v2, px, py), w1 = aresta(v2, v0, px, py), w2 = aresta(v0, v1, px, py);
            if (w0 + b0 < 0 || w1 + b1 < 0 || w2 + b2 < 0) continue;
            int32_t l1 = (int32_t)sconv_div64((uint64_t)w1 << 16, (uint32_t)area);
            int32_t l2 = (int32_t)sconv_div64((uint64_t)w2 << 16, (uint32_t)area);
            int i = y * L + x;
            if (e->flags & G3D_Z_TESTE) {
                uint32_t z = (uint32_t)mistura3(v0->z, v1->z, v2->z, l1, l2);
                if (e->func_z == G3D_Z_MENOR ? !(z < zb[i]) : e->func_z == G3D_Z_MENOR_IGUAL ? !(z <= zb[i]) : 0) continue;
                if (e->flags & G3D_Z_ESCRITA) zb[i] = z;
            }
            uint32_t c[4];
            if (persp) perspectiva(l1, l2, v0, v1, v2, &l1, &l2);
            for (int k = 0; k < 4; k++) {
                int32_t q = mistura3((int32_t)v0->c[k] << 8, (int32_t)v1->c[k] << 8, (int32_t)v2->c[k] << 8, l1, l2);
                c[k] = (uint32_t)(q < 0 ? 0 : (q + 128) >> 8);
                if (c[k] > 255) c[k] = 255;
            }
            if (tpx) {
                int32_t u = mistura3(v0->u, v1->u, v2->u, l1, l2), w = mistura3(v0->v, v1->v, v2->v, l1, l2);
                uint32_t t;
                if (e->flags & G3D_TEX_LINEAR) {
                    /* D404: bilineal, con los centros de los texels en .5 */
                    int64_t sx = (int64_t)u * tl - 32768, sy = (int64_t)w * ta - 32768;
                    int tx = (int)(sx >> 16), ty = (int)(sy >> 16);
                    uint32_t fx = (uint32_t)(sx & 0xFFFF) >> 8, fy = (uint32_t)(sy & 0xFFFF) >> 8;
                    uint32_t a = texel_sim(tpx, tl, ta, tx, ty, e->flags), b = texel_sim(tpx, tl, ta, tx + 1, ty, e->flags);
                    uint32_t cc = texel_sim(tpx, tl, ta, tx, ty + 1, e->flags), d = texel_sim(tpx, tl, ta, tx + 1, ty + 1, e->flags);
                    t = 0;
                    for (int k = 0; k < 32; k += 8) {
                        uint32_t cima = ((a >> k) & 0xFF) * (256 - fx) + ((b >> k) & 0xFF) * fx;
                        uint32_t baixo = ((cc >> k) & 0xFF) * (256 - fx) + ((d >> k) & 0xFF) * fx;
                        t |= ((cima * (256 - fy) + baixo * fy + 32768) >> 16) << k;
                    }
                } else {
                    int tx = (int)(((int64_t)u * tl) >> 16), ty = (int)(((int64_t)w * ta) >> 16);
                    t = texel_sim(tpx, tl, ta, tx, ty, e->flags);
                }
                c[0] = (c[0] * ((t >> 16) & 0xFF) + 127) / 255;
                c[1] = (c[1] * ((t >> 8) & 0xFF) + 127) / 255;
                c[2] = (c[2] * (t & 0xFF) + 127) / 255;
                c[3] = (c[3] * (t >> 24) + 127) / 255;
            }
            for (int k = 0; k < 3; k++) {
                int32_t q = mistura3((int32_t)v0->e[k] << 8, (int32_t)v1->e[k] << 8, (int32_t)v2->e[k] << 8, l1, l2);
                c[k] += (uint32_t)(q < 0 ? 0 : (q + 128) >> 8);
                if (c[k] > 255) c[k] = 255;
            }
            if (e->flags & G3D_MEZCLA) {
                uint32_t d = cor[i], fo = fator(e->mez_orig, c[3]), fd = fator(e->mez_dest, c[3]);
                uint32_t dc[3] = { (d >> 16) & 0xFF, (d >> 8) & 0xFF, d & 0xFF };
                for (int k = 0; k < 3; k++) {
                    c[k] = (c[k] * fo + dc[k] * fd + 127) / 255;
                    if (c[k] > 255) c[k] = 255;
                }
            }
            cor[i] = ((c[0] << 16) | (c[1] << 8) | c[2]) ^ (R_XOR & 0x00FFFFFFu);
        }
}

static int s_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *va)
{
    if (!pronto || !cor) return G3D_FALHA;
    ajustes(G3D_AJ_DIBUJO);
    for (int i = 0; i + 2 < n && !R_MUDO; i += 3) {
        sv_t a, b, c;
        converter(&v[i], &a); converter(&v[i + 1], &b); converter(&v[i + 2], &c);
        triangulo(e, &a, &b, &c);
    }
    vala++;
    if (va) *va = vala;
    return G3D_OK;
}

static int s_esperar(g3d_vala_t va, uint32_t ms) { (void)va; (void)ms; return pronto ? G3D_OK : G3D_FALHA; }

static int s_ler(uint32_t *dest, int passo_px)
{
    if (!pronto || !cor) return G3D_FALHA;
    /* D391: con la GPU simulada del volcado (--volcado-gpu-simulado), el cuadro vuelve por su GART de
     * software, como lo haría el motor 2D de una R5xx; sin ella (o si no se puede), como siempre. */
    acel_origem_t o = { 0, cor, (uint32_t)L * 4u, 0 };
    if (acel_retornar(&o, L, A, dest, passo_px)) return G3D_OK;
    for (int y = 0; y < A; y++) memcpy(dest + y * passo_px, cor + y * L, (uint32_t)L * 4);
    return G3D_OK;
}

static void s_detalle(const char *fase) { klog("[GPU3D] simulado %s: destino %dx%d, vallas %u\n", fase, L, A, vala); }
static void s_memoria(uint32_t *total_kb, uint32_t *livre_kb) { *total_kb = 16384; *livre_kb = 16384 - usados_kb; }

static const g3d_faixa_t s_faixas[] = { { 0x1000u, 0x10FCu } };

static void s_vistas(g3d_vista_t *c, g3d_vista_t *z)
{
    if (!pronto || !cor) return;
    *c = (g3d_vista_t){ (volatile uint8_t *)cor, (uint32_t)L * 4u, L, A, 32, 0, 0, false };
    *z = (g3d_vista_t){ (volatile uint8_t *)zb, (uint32_t)L * 4u, L, A, 32, 24, 0, false };
}

const gpu3d_driver_t gpu3d_sim = {
    "simulado", s_iniciar, s_alvo, s_limpar, s_textura, s_desenhar, s_esperar, s_ler, s_desligar, s_detalle, s_memoria,
    "simulado", s_faixas, 1, s_vistas,
};
