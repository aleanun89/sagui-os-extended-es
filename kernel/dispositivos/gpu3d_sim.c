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
 * regla de arriba-izquierda, color, especular y textura interpolados sin
 * corrección de perspectiva (rhw no se usa), Z de 24 bits y mezcla. */
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

typedef struct { int32_t x, y, z, u, v; uint8_t c[4], e[3]; } sv_t;   /* x, y con 4 bits; z de 24 bits; u, v con 16 bits */

static void converter(const gpu3d_vert_t *g, sv_t *s)
{
    s->x = g3d_fixo_de_float(g->x, 4);
    s->y = g3d_fixo_de_float(g->y, 4);
    int32_t z = g3d_fixo_de_float(g->z, 24);
    s->z = z < 0 ? 0 : z > 0xFFFFFF ? 0xFFFFFF : z;
    s->u = g3d_fixo_de_float(g->u, 16);
    s->v = g3d_fixo_de_float(g->v, 16);
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

static void triangulo(const gpu3d_estado_t *e, sv_t *v0, sv_t *v1, sv_t *v2)
{
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
            for (int k = 0; k < 4; k++) {
                int32_t q = mistura3((int32_t)v0->c[k] << 8, (int32_t)v1->c[k] << 8, (int32_t)v2->c[k] << 8, l1, l2);
                c[k] = (uint32_t)(q < 0 ? 0 : (q + 128) >> 8);
                if (c[k] > 255) c[k] = 255;
            }
            if (tpx) {
                int32_t u = mistura3(v0->u, v1->u, v2->u, l1, l2), w = mistura3(v0->v, v1->v, v2->v, l1, l2);
                int tx = (int)(((int64_t)u * tl) >> 16), ty = (int)(((int64_t)w * ta) >> 16);
                if (e->flags & G3D_TEX_REPETE_S) { tx %= tl; if (tx < 0) tx += tl; } else { if (tx < 0) tx = 0; else if (tx >= tl) tx = tl - 1; }
                if (e->flags & G3D_TEX_REPETE_T) { ty %= ta; if (ty < 0) ty += ta; } else { if (ty < 0) ty = 0; else if (ty >= ta) ty = ta - 1; }
                uint32_t t = tpx[ty * tl + tx];
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
            cor[i] = (c[0] << 16) | (c[1] << 8) | c[2];
        }
}

static int s_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *va)
{
    if (!pronto || !cor) return G3D_FALHA;
    for (int i = 0; i + 2 < n; i += 3) {
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
    for (int y = 0; y < A; y++) memcpy(dest + y * passo_px, cor + y * L, (uint32_t)L * 4);
    return G3D_OK;
}

static void s_detalle(const char *fase) { klog("[GPU3D] simulado %s: destino %dx%d, vallas %u\n", fase, L, A, vala); }
static void s_memoria(uint32_t *total_kb, uint32_t *livre_kb) { *total_kb = 16384; *livre_kb = 16384 - usados_kb; }

const gpu3d_driver_t gpu3d_sim = {
    "simulado", s_iniciar, s_alvo, s_limpar, s_textura, s_desenhar, s_esperar, s_ler, s_desligar, s_detalle, s_memoria,
};
