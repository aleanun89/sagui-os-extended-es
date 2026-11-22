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
#include "video.h"
#include "g3d_v2_provas.h"
#include "prazo.h"
#include "fpu.h"
#include "sse2.h"

static uint32_t *cor, *zb;
static int L, A;
static struct { uint32_t *px; int larg, alt; int niveis; uint32_t *nv[12]; } tex[GPU3D_TEXTURAS];   /* D426: nv[1..]: mipmaps */
static bool pronto;
static g3d_vala_t vala;
static uint32_t usados_kb;
static uint32_t regs[0x40];                 /* D390: 0x1000-0x10FC */
#define R_XOR     regs[0]
#define R_MUDO    regs[1]
/* D426: dibujando en una textura (gpu3d v2): cor/zb/L/A son los de la textura y los del principal quedan aquí. */
static int dest_v = -1, princ_L, princ_A;
static uint32_t *princ_cor, *princ_zb, *dest_zb;
static uint32_t n_v2;
static void voltar_principal(void)
{
    if (dest_v < 0) return;
    cor = princ_cor; zb = princ_zb; L = princ_L; A = princ_A;
    if (dest_zb) { kfree(dest_zb); dest_zb = NULL; }
    dest_v = -1;
}

static void ajustes(int quando)
{
    for (int i = 0; i < gpu3d_ajustes.n; i++)
        if (gpu3d_ajustes.quando[i] == quando) regs[(gpu3d_ajustes.reg[i] - 0x1000u) >> 2 & 0x3Fu] = gpu3d_ajustes.val[i];
}

static void soltar_alvo(void)
{
    voltar_principal();
    if (cor) kfree(cor);
    if (zb) kfree(zb);
    cor = zb = NULL; L = A = 0;
}

static void soltar_niveis(int vaga)
{
    for (int k = 1; k < 12; k++) if (tex[vaga].nv[k]) { kfree(tex[vaga].nv[k]); tex[vaga].nv[k] = NULL; }
    tex[vaga].niveis = 1;
}

static void s_vbuf_soltar_todos(void);

static void s_desligar(void)
{
    soltar_alvo();
    s_vbuf_soltar_todos();
    for (int i = 0; i < GPU3D_TEXTURAS; i++) { if (tex[i].px) { kfree(tex[i].px); tex[i].px = NULL; } soltar_niveis(i); }
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
        if (mascara & 1) cor[i] = (c & 0x00FFFFFFu) | (dest_v >= 0 ? 0xFF000000u : 0);   /* D426: una textura guarda el alfa */
        if (mascara & 2) zb[i] = (zb[i] & 0xFF000000u) | 0x00FFFFFFu;                   /* el esténcil (v2) va arriba */
    }
    return G3D_OK;
}

static int s_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!pronto) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    if (tex[vaga].px) { kfree(tex[vaga].px); tex[vaga].px = NULL; }
    soltar_niveis(vaga);
    if (!argb) return G3D_OK;
    if (larg < 1 || alt < 1 || larg > GPU3D_MAX_LADO || alt > GPU3D_MAX_LADO) return G3D_NAO;
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
                uint32_t zv = zb[i] & 0x00FFFFFFu;   /* D426: el esténcil de v2 en los 8 bits altos */
                if (e->func_z == G3D_Z_MENOR ? !(z < zv) : e->func_z == G3D_Z_MENOR_IGUAL ? !(z <= zv) : 0) continue;
                if (e->flags & G3D_Z_ESCRITA) zb[i] = (zb[i] & 0xFF000000u) | z;
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
            cor[i] = (((c[0] << 16) | (c[1] << 8) | c[2]) ^ (R_XOR & 0x00FFFFFFu)) | (dest_v >= 0 ? c[3] << 24 : 0);
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
        gpu3d_respirar();   /* D436: rasterizar aquí es «el tiempo de la GPU»; una GPU de verdad no para la CPU */
    }
    vala++;
    if (va) *va = vala;
    return G3D_OK;
}

/* D436 (audio): una GPU lenta de mentira. «inicio 100c N» en gpu3d-simulado.conf (N en hexadecimal) : cada espera, lectura del cuadro y
 * presentación tarda N ms (hasta 500), girando como un driver real que espera una valla, con gpu3d_respirar en cada
 * vuelta. Así se prueba en QEMU que el audio no se corta con el dibujo por GPU y pocos fps. */
#define R_LENTO regs[3]
static uint32_t n_lento;
/* D436 (Amilo: #NM en sse3_copiar al ceder dentro de una copia con SSE). «inicio 1010 1»: cada lectura del cuadro se
 * copia por trozos con SSE2 DENTRO de un fpu_kernel_inicio/fim llamando a gpu3d_respirar entre trozo y trozo, y
 * gpu3d_respirar cede en cada llamada: si cediera ahí, la siguiente instrucción SSE sería una excepción del kernel.
 * h431 «gpu» lo usa para comprobar que no cede (y que lo dice en el registro). */
#define R_SSE_CEDER regs[4]
static void s_ler_sse(uint32_t *dest, int passo_px)
{
    if (!R_SSE_CEDER || !(fpu_recursos() & CPU_SSE2) || !fpu_kernel_inicio()) return;
    gpu3d_respirar_forcar(true);
    static uint32_t fila[GPU3D_MAX_LADO];
    for (int y = 0; y < A; y++) {
        sse2_copiar(fila, cor + y * L, (unsigned)L * 4u);
        gpu3d_respirar();
        sse2_copiar(dest + y * passo_px, fila, (unsigned)L * 4u);
    }
    gpu3d_respirar_forcar(false);
    fpu_kernel_fim();
}

static void s_lento(void)
{
    uint32_t ms = R_LENTO > 500 ? 500 : R_LENTO;
    if (!ms) return;
    if (!n_lento++) klog("[GPU3D] simulado: GPU lenta de mentira, %u ms por espera (D436)\n", ms);
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) gpu3d_respirar();
}

static int s_esperar(g3d_vala_t va, uint32_t ms) { (void)va; (void)ms; if (pronto) s_lento(); return pronto ? G3D_OK : G3D_FALHA; }

static int s_ler(uint32_t *dest, int passo_px)
{
    if (!pronto || !cor) return G3D_FALHA;
    s_lento();
    if (dest_v < 0) s_ler_sse(dest, passo_px);   /* D436: la comprobación de «inicio 1010 1» (lo de abajo lo vuelve a copiar) */
    if (dest_v >= 0) {   /* D426: la textura en la que se dibuja, sin el alfa */
        for (int y = 0; y < A; y++) for (int x = 0; x < L; x++) dest[y * passo_px + x] = cor[y * L + x] & 0x00FFFFFFu;
        return G3D_OK;
    }
    /* D391: con la GPU simulada del volcado (--volcado-gpu-simulado), el cuadro vuelve por su GART de
     * software, como lo haría el motor 2D de una R5xx; sin ella (o si no se puede), como siempre. */
    acel_origem_t o = { 0, cor, (uint32_t)L * 4u, 0 };
    gpu3d_respirar_bloquear(true);   /* D436: como en el R5xx, la vuelta por la GART no cede */
    bool por_gart = acel_retornar(&o, L, A, dest, passo_px);
    gpu3d_respirar_bloquear(false);
    if (por_gart) return G3D_OK;
    for (int y = 0; y < A; y++) memcpy(dest + y * passo_px, cor + y * L, (uint32_t)L * 4);
    return G3D_OK;
}

static uint32_t n_v3_detalle(void);
static void s_detalle(const char *fase) { klog("[GPU3D] simulado %s: destino %dx%d, vallas %u, dibujos v2 %u, v3 %u\n", fase, L, A, vala, n_v2, n_v3_detalle()); }
static void s_memoria(uint32_t *total_kb, uint32_t *livre_kb) { *total_kb = 16384; *livre_kb = 16384 - usados_kb; }

/* ============================================================ gpu3d v2 (D426)
 *
 * El simulado también entiende v2, para recorrer en QEMU el camino entero (SaguiGL ES 1.1 -> capa -> driver):
 * la tubería de docs/gpu3d.md «gpu3d v2» con enteros (canales de 0 a 255): hasta 4 unidades con su combinación
 * (COMBINE, DOT3, escala), especular, niebla, prueba alfa, tijera, esténcil de 8 bits (en los 8 bits altos del
 * Z), las 8 funciones del Z, mezcla separada y con resta, operación lógica, máscara de color, texturas con
 * repetición, borde o espejo (vecino o bilineal, cualquier lado), DXT1/3/5 (se descomprimen al subirlas) y
 * dibujar en una textura. Sin mipmaps, cubos, anisótropo ni coordenadas proyectivas. Lo que anuncia lo
 * decide g3d_v2_provas.c, como en las Radeon: así esas pruebas también se recorren en QEMU. */


typedef struct { int32_t x, y, z, q; uint8_t c[4], e[4]; int32_t tc[4][2]; } sv2_t;

static void converter2(const gpu3d_vert2_t *g, sv2_t *s)
{
    s->x = g3d_fixo_de_float(g->x, 4);
    s->y = g3d_fixo_de_float(g->y, 4);
    int32_t z = g3d_fixo_de_float(g->z, 24);
    s->z = z < 0 ? 0 : z > 0xFFFFFF ? 0xFFFFFF : z;
    int32_t q = g3d_fixo_de_float(g->rhw, 16);
    s->q = q < 1 ? 1 : q > 0x7FFFFFF ? 0x7FFFFFF : q;
    for (int k = 0; k < 4; k++) { s->c[k] = (uint8_t)(g->cor >> (k == 3 ? 24 : 16 - 8 * k)); s->e[k] = (uint8_t)(g->espec >> (k == 3 ? 24 : 16 - 8 * k)); }
    for (int u = 0; u < 4; u++) { s->tc[u][0] = g3d_fixo_de_float(g->tc[u][0], 16); s->tc[u][1] = g3d_fixo_de_float(g->tc[u][1], 16); }
}

/* Un eje de la textura con su envoltura (coordenada en texels). */
static inline int envolver(int t, int n, uint8_t env)
{
    if (env == G3D_ENV_REPETIR) { t %= n; return t < 0 ? t + n : t; }
    if (env == G3D_ENV_ESPELHO) { int p = 2 * n; t %= p; if (t < 0) t += p; return t >= n ? p - 1 - t : t; }
    return t < 0 ? 0 : t >= n ? n - 1 : t;
}

static uint32_t amostra_nivel(const gpu3d_unidade_t *u, int k, bool lin, int32_t s, int32_t t)
{
    const uint32_t *px = k ? tex[u->vaga].nv[k] : tex[u->vaga].px;
    int tl = tex[u->vaga].larg >> k, ta = tex[u->vaga].alt >> k;
    if (tl < 1) tl = 1;
    if (ta < 1) ta = 1;
    if (!px) return 0xFFFFFFFFu;
    if (!lin) {
        int tx = (int)(((int64_t)s * tl) >> 16), ty = (int)(((int64_t)t * ta) >> 16);
        return px[envolver(ty, ta, u->env_t) * tl + envolver(tx, tl, u->env_s)];
    }
    int64_t sx = (int64_t)s * tl - 32768, sy = (int64_t)t * ta - 32768;
    int tx = (int)(sx >> 16), ty = (int)(sy >> 16);
    uint32_t fx = (uint32_t)(sx & 0xFFFF) >> 8, fy = (uint32_t)(sy & 0xFFFF) >> 8;
    int x0 = envolver(tx, tl, u->env_s), x1 = envolver(tx + 1, tl, u->env_s), y0 = envolver(ty, ta, u->env_t), y1 = envolver(ty + 1, ta, u->env_t);
    uint32_t a = px[y0 * tl + x0], b = px[y0 * tl + x1], c = px[y1 * tl + x0], d = px[y1 * tl + x1], r = 0;
    for (int k = 0; k < 32; k += 8) {
        uint32_t cima = ((a >> k) & 0xFF) * (256 - fx) + ((b >> k) & 0xFF) * fx;
        uint32_t baixo = ((c >> k) & 0xFF) * (256 - fx) + ((d >> k) & 0xFF) * fx;
        r |= ((cima * (256 - fy) + baixo * fy + 32768) >> 16) << k;
    }
    return r;
}

/* rho2 = el mayor de |d(s,t)/dx|^2 y |d(s,t)/dy|^2 en texels^2 (coma fija de 16 bits): nivel de detalle en
 * 1/256, como log2(rho2) / 2 de SaguiGL (aproximado por tramos). Negativo: se amplía. */
static int lod256(uint64_t rho2)
{
    if (rho2 <= 65536u) return -256;
    int e = 0;
    while ((rho2 >> e) >= 2u * 65536u) e++;
    uint32_t m = (uint32_t)((rho2 >> e) - 65536u) >> 8;            /* fracción de la mantisa, de 0 a 255 */
    int l2 = e * 256 + (int)((m * (345 - (89 * m >> 8))) >> 8);      /* log2 en 1/256 (m(1,3466 - 0,3466 m)) */
    return l2 / 2;
}

/* Una textura con su filtro y, si la unidad los pide, sus mipmaps (como amostra7 de SaguiGL). */
static uint32_t amostra(const gpu3d_unidade_t *u, int32_t s, int32_t t, int lod)
{
    bool mip = (u->filtro & (G3D_FIL_MIP_VECINO | G3D_FIL_MIP_LINEAR)) && tex[u->vaga].niveis > 1;
    bool min = lod > 0;
    bool lin = (u->filtro & (min ? G3D_FIL_LINEAR_MIN : G3D_FIL_LINEAR_MAG)) != 0;
    if (!mip || !min) return amostra_nivel(u, 0, lin, s, t);
    int ult = tex[u->vaga].niveis - 1;
    if (u->filtro & G3D_FIL_MIP_VECINO) {
        int k = lod <= 128 ? 0 : (lod + 128) >> 8;
        return amostra_nivel(u, k > ult ? ult : k, lin, s, t);
    }
    int k = lod >> 8, fr = lod & 255;
    if (k >= ult) return amostra_nivel(u, ult, lin, s, t);
    uint32_t a = amostra_nivel(u, k, lin, s, t), b = amostra_nivel(u, k + 1, lin, s, t), r = 0;
    for (int i = 0; i < 32; i += 8) r |= ((((a >> i) & 0xFF) * (uint32_t)(256 - fr) + ((b >> i) & 0xFF) * (uint32_t)fr + 128) >> 8) << i;
    return r;
}

static inline int sat255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static inline int canal_de(uint32_t argb, int k) { return (int)((argb >> (k == 3 ? 24 : 16 - 8 * k)) & 0xFF); }

/* func(a0, a1, a2) en 0..255 (los argumentos ya con su operando). */
static int combinar(int f, int a0, int a1, int a2)
{
    switch (f) {
    case G3D_CB_MODULAR:     return (a0 * a1 + 127) / 255;
    case G3D_CB_SOMAR:       return a0 + a1;
    case G3D_CB_SOMAR_SINAL: return a0 + a1 - 128;
    case G3D_CB_INTERPOLAR:  return (a0 * a2 + a1 * (255 - a2) + 127) / 255;
    case G3D_CB_SUBTRAIR:    return a0 - a1;
    default:                 return a0;
    }
}

static bool cmp8(int f, int a, int b)
{
    switch (f & 7) { case 0: return false; case 1: return a < b; case 2: return a == b; case 3: return a <= b; case 4: return a > b;
    case 5: return a != b; case 6: return a >= b; default: return true; }
}

static uint32_t est_op(int op, uint32_t v, uint32_t ref)
{
    switch (op & 7) { case 1: return 0; case 2: return ref; case 3: return v == 255 ? 255 : v + 1; case 4: return v ? v - 1 : 0;
    case 5: return ~v & 0xFF; case 6: return (v + 1) & 0xFF; case 7: return (v - 1) & 0xFF; default: return v; }
}

static int fator2(int f, const int s[4], const int d[4], int k)
{
    switch (f) {
    case G3D_F_ZERO: return 0;
    case G3D_F_ALFA: return s[3];
    case G3D_F_UM_MENOS_ALFA: return 255 - s[3];
    case G3D_F_COR_ORIG: return s[k];
    case G3D_F_UM_MENOS_COR_ORIG: return 255 - s[k];
    case G3D_F_COR_DEST: return d[k];
    case G3D_F_UM_MENOS_COR_DEST: return 255 - d[k];
    case G3D_F_ALFA_DEST: return d[3];
    case G3D_F_UM_MENOS_ALFA_DEST: return 255 - d[3];
    case G3D_F_ALFA_SATURADO: return k == 3 ? 255 : (s[3] < 255 - d[3] ? s[3] : 255 - d[3]);
    default: return 255;
    }
}

static uint32_t logica(int op, uint32_t s, uint32_t d)
{
    switch (op & 15) { case 0: return 0; case 1: return s & d; case 2: return s & ~d; case 3: return s; case 4: return ~s & d; case 5: return d;
    case 6: return s ^ d; case 7: return s | d; case 8: return ~(s | d); case 9: return ~(s ^ d); case 10: return ~d; case 11: return s | ~d;
    case 12: return ~s; case 13: return ~s | d; case 14: return ~(s & d); default: return 0xFFFFFFFFu; }
}

/* El color de un píxel: unidades, especular y niebla. */
static void fragmento(const gpu3d_estado2_t *e, const int c0[4], const int es[4], const int32_t tc[4][2], const int lod[4], int out[4])
{
    int ant[4] = { c0[0], c0[1], c0[2], c0[3] };
    int nu = e->unidades > 4 ? 4 : e->unidades;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        if (!x->ativa) continue;
        int co = x->coord > 3 ? u : x->coord;
        uint32_t tx = amostra(x, tc[co][0], tc[co][1], lod[u]);
        int r[4];
        int arg[3][4];
        for (int k = 0; k < 3; k++) {
            for (int ch = 0; ch < 4; ch++) {
                bool alfa = ch == 3;
                int ft = alfa ? x->alfa_fonte[k] : x->rgb_fonte[k], opd = alfa ? x->alfa_opd[k] : x->rgb_opd[k];
                int src[4];
                for (int i = 0; i < 4; i++)
                    src[i] = ft == G3D_FT_TEXTURA ? canal_de(tx, i) : ft == G3D_FT_CONSTANTE ? canal_de(x->constante, i) :
                             ft == G3D_FT_PRIMARIA ? c0[i] : ant[i];
                int v = (opd == G3D_OPD_COR || opd == G3D_OPD_UM_MENOS_COR) && !alfa ? src[ch] : src[3];
                if (opd == G3D_OPD_UM_MENOS_COR || opd == G3D_OPD_UM_MENOS_ALFA) v = 255 - v;
                arg[k][ch] = v;
            }
        }
        if (x->rgb_func == G3D_CB_DOT3_RGB || x->rgb_func == G3D_CB_DOT3_RGBA) {
            int d = 0;
            for (int ch = 0; ch < 3; ch++) d += (2 * arg[0][ch] - 255) * (2 * arg[1][ch] - 255);
            d = sat255((d / 255) * x->rgb_escala);
            r[0] = r[1] = r[2] = d;
            r[3] = x->rgb_func == G3D_CB_DOT3_RGBA ? d : sat255(combinar(x->alfa_func, arg[0][3], arg[1][3], arg[2][3]) * x->alfa_escala);
        } else {
            for (int ch = 0; ch < 3; ch++) r[ch] = sat255(combinar(x->rgb_func, arg[0][ch], arg[1][ch], arg[2][ch]) * x->rgb_escala);
            r[3] = sat255(combinar(x->alfa_func, arg[0][3], arg[1][3], arg[2][3]) * x->alfa_escala);
        }
        for (int i = 0; i < 4; i++) ant[i] = r[i];
    }
    for (int ch = 0; ch < 3; ch++) out[ch] = sat255(ant[ch] + es[ch]);
    out[3] = ant[3];
    if (e->flags & G3D2_NEBLINA) {
        int f = es[3];
        for (int ch = 0; ch < 3; ch++) out[ch] = (out[ch] * f + canal_de(e->cor_neblina, ch) * (255 - f) + 127) / 255;
    }
}

/* D426: un peso baricéntrico con signo (16 bits de fracción) y su corrección de perspectiva sin recortar a 0
 * (para las diferencias del nivel de mipmap, que miran el píxel de al lado). */
static int32_t peso_s(int64_t w, int64_t area)
{
    uint64_t m = (uint64_t)(w < 0 ? -w : w) << 16;
    int64_t r = (int64_t)sconv_div64(m, (uint32_t)area);
    if (r > 0x3FFFFFFF) r = 0x3FFFFFFF;
    return (int32_t)(w < 0 ? -r : r);
}

static void perspectiva_s(int32_t l1, int32_t l2, const sv_t *v0, const sv_t *v1, const sv_t *v2, int32_t *p1, int32_t *p2)
{
    int64_t b0 = (65536 - (int64_t)l1 - l2) * v0->q, b1 = (int64_t)l1 * v1->q, b2 = (int64_t)l2 * v2->q;
    int64_t soma = b0 + b1 + b2;
    if (soma <= 0) { *p1 = l1; *p2 = l2; return; }
    int k = 0;
    while ((soma >> k) > 0x7FFFFFFF) k++;
    uint32_t d = (uint32_t)(soma >> k);
    int64_t n1 = (b1 >> k) * 65536, n2 = (b2 >> k) * 65536;
    *p1 = (int32_t)(n1 < 0 ? -(int64_t)sconv_div64((uint64_t)-n1, d) : (int64_t)sconv_div64((uint64_t)n1, d));
    *p2 = (int32_t)(n2 < 0 ? -(int64_t)sconv_div64((uint64_t)-n2, d) : (int64_t)sconv_div64((uint64_t)n2, d));
}

static void triangulo2(const gpu3d_estado2_t *e, sv2_t *v0, sv2_t *v1, sv2_t *v2)
{
    int64_t area = (int64_t)(v1->x - v0->x) * (v2->y - v0->y) - (int64_t)(v1->y - v0->y) * (v2->x - v0->x);
    if (area == 0) return;
    if (area < 0) { sv2_t *t = v1; v1 = v2; v2 = t; area = -area; }
    if (area >= 0x7FFFFFFF) return;
    sv_t a = { v0->x, v0->y, 0, 0, 0, 0, { 0 }, { 0 } }, b = { v1->x, v1->y, 0, 0, 0, 0, { 0 }, { 0 } }, c = { v2->x, v2->y, 0, 0, 0, 0, { 0 }, { 0 } };
    a.q = v0->q; b.q = v1->q; c.q = v2->q;
    int persp = v0->q != v1->q || v0->q != v2->q;
    int32_t minx = v0->x, maxx = v0->x, miny = v0->y, maxy = v0->y;
    const sv2_t *vs[3] = { v0, v1, v2 };
    for (int i = 1; i < 3; i++) {
        if (vs[i]->x < minx) minx = vs[i]->x;
        if (vs[i]->x > maxx) maxx = vs[i]->x;
        if (vs[i]->y < miny) miny = vs[i]->y;
        if (vs[i]->y > maxy) maxy = vs[i]->y;
    }
    int x0 = minx >> 4, x1 = (maxx + 15) >> 4, y0 = miny >> 4, y1 = (maxy + 15) >> 4;
    int tx0 = 0, ty0 = 0, tx1 = L, ty1 = A;
    if (e->flags & G3D2_TIJERA) { tx0 = e->tijera_x; ty0 = e->tijera_y; tx1 = tx0 + e->tijera_l; ty1 = ty0 + e->tijera_a; }
    if (x0 < tx0) x0 = tx0;
    if (y0 < ty0) y0 = ty0;
    if (x1 > tx1) x1 = tx1;
    if (y1 > ty1) y1 = ty1;
    if (x1 > L) x1 = L;
    if (y1 > A) y1 = A;
    int b0 = cima_esquerda(&b, &c) ? 0 : -1, b1 = cima_esquerda(&c, &a) ? 0 : -1, b2 = cima_esquerda(&a, &b) ? 0 : -1;
    bool est = (e->flags & G3D2_ESTENCIL) != 0;
    uint32_t mc = 0;
    if (e->mascara_cor & G3D_MC_R) mc |= 0xFF0000u;
    if (e->mascara_cor & G3D_MC_G) mc |= 0xFF00u;
    if (e->mascara_cor & G3D_MC_B) mc |= 0xFFu;
    if ((e->mascara_cor & G3D_MC_A) && dest_v >= 0) mc |= 0xFF000000u;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int32_t px = x * 16 + 8, py = y * 16 + 8;
            int64_t w0 = aresta(&b, &c, px, py), w1 = aresta(&c, &a, px, py), w2 = aresta(&a, &b, px, py);
            if (w0 + b0 < 0 || w1 + b1 < 0 || w2 + b2 < 0) continue;
            int32_t l1 = (int32_t)sconv_div64((uint64_t)w1 << 16, (uint32_t)area);
            int32_t l2 = (int32_t)sconv_div64((uint64_t)w2 << 16, (uint32_t)area);
            int i = y * L + x;
            uint32_t z = (uint32_t)mistura3(v0->z, v1->z, v2->z, l1, l2), zs = zb[i], zv = zs & 0xFFFFFFu, sv = zs >> 24;
            if (persp) perspectiva(l1, l2, &a, &b, &c, &l1, &l2);
            int c0[4], es[4], out[4];
            int32_t tc[4][2];
            for (int k = 0; k < 4; k++) {
                c0[k] = sat255((mistura3((int32_t)v0->c[k] << 8, (int32_t)v1->c[k] << 8, (int32_t)v2->c[k] << 8, l1, l2) + 128) >> 8);
                es[k] = sat255((mistura3((int32_t)v0->e[k] << 8, (int32_t)v1->e[k] << 8, (int32_t)v2->e[k] << 8, l1, l2) + 128) >> 8);
                tc[k][0] = mistura3(v0->tc[k][0], v1->tc[k][0], v2->tc[k][0], l1, l2);
                tc[k][1] = mistura3(v0->tc[k][1], v1->tc[k][1], v2->tc[k][1], l1, l2);
            }
            /* Nivel de detalle por unidad (diferencias con el píxel de la derecha y el de abajo). */
            int lod[4] = { -256, -256, -256, -256 };
            for (int u = 0; u < 4 && u < e->unidades; u++) {
                const gpu3d_unidade_t *x = &e->u[u];
                if (!x->ativa || !tex[x->vaga].px) continue;
                if (!((x->filtro & (G3D_FIL_MIP_VECINO | G3D_FIL_MIP_LINEAR)) && tex[x->vaga].niveis > 1) &&
                    ((x->filtro & G3D_FIL_LINEAR_MIN) != 0) == ((x->filtro & G3D_FIL_LINEAR_MAG) != 0)) continue;
                int co = x->coord > 3 ? u : x->coord;
                int64_t d2[2] = { 0, 0 };
                for (int dir = 0; dir < 2; dir++) {
                    int32_t qx = px + (dir ? 0 : 16), qy = py + (dir ? 16 : 0);
                    /* Pesos con signo (el píxel vecino puede caer fuera del triángulo) y con perspectiva. */
                    int32_t m1 = peso_s(aresta(&c, &a, qx, qy), area), m2 = peso_s(aresta(&a, &b, qx, qy), area);
                    if (persp) perspectiva_s(m1, m2, &a, &b, &c, &m1, &m2);
                    int64_t ds = (int64_t)(mistura3(v0->tc[co][0], v1->tc[co][0], v2->tc[co][0], m1, m2) - tc[co][0]) * tex[x->vaga].larg;
                    int64_t dt = (int64_t)(mistura3(v0->tc[co][1], v1->tc[co][1], v2->tc[co][1], m1, m2) - tc[co][1]) * tex[x->vaga].alt;
                    ds >>= 8; dt >>= 8;                         /* texels en 8 bits de fracción */
                    d2[dir] = ds * ds + dt * dt;                /* 16 bits de fracción */
                }
                lod[u] = lod256((uint64_t)(d2[0] > d2[1] ? d2[0] : d2[1]));
            }
            fragmento(e, c0, es, tc, lod, out);
            if ((e->flags & G3D2_ALFA_TESTE) && !cmp8(e->func_alfa, out[3], e->ref_alfa)) continue;
            uint32_t ref = e->est_ref, msk = e->est_mascara, wm = e->est_escrita;
            if (est && !cmp8(e->est_func, (int)(ref & msk), (int)(sv & msk))) {
                sv = (sv & ~wm) | (est_op(e->est_falha, sv, ref) & wm);
                zb[i] = zv | (sv << 24);
                continue;
            }
            if ((e->flags & G3D2_Z_TESTE) && !cmp8(e->func_z, (int)z, (int)zv)) {
                if (est) { sv = (sv & ~wm) | (est_op(e->est_zfalha, sv, ref) & wm); zb[i] = zv | (sv << 24); }
                continue;
            }
            if (est) sv = (sv & ~wm) | (est_op(e->est_zpassa, sv, ref) & wm);
            if ((e->flags & G3D2_Z_TESTE) && (e->flags & G3D2_Z_ESCRITA)) zv = z;
            zb[i] = zv | (sv << 24);
            uint32_t dv = cor[i], r;
            int d[4] = { (int)((dv >> 16) & 0xFF), (int)((dv >> 8) & 0xFF), (int)(dv & 0xFF), dest_v >= 0 ? (int)(dv >> 24) : 255 };
            uint32_t s32 = ((uint32_t)out[3] << 24) | ((uint32_t)out[0] << 16) | ((uint32_t)out[1] << 8) | (uint32_t)out[2];
            if (e->flags & G3D2_LOGICA) r = logica(e->op_logica, s32, dv);
            else if (e->flags & G3D2_MEZCLA) {
                int res[4];
                for (int k = 0; k < 4; k++) {
                    int fo = k == 3 ? e->mez_orig_alfa : e->mez_orig_rgb, fd = k == 3 ? e->mez_dest_alfa : e->mez_dest_rgb;
                    int eq = k == 3 ? e->mez_eq_alfa : e->mez_eq_rgb;
                    int so = out[k] * fator2(fo, out, d, k), de = d[k] * fator2(fd, out, d, k);
                    res[k] = sat255((eq == G3D_EQ_SUBTRAI ? so - de : eq == G3D_EQ_SUBTRAI_INV ? de - so : so + de) / 255);
                }
                r = ((uint32_t)res[3] << 24) | ((uint32_t)res[0] << 16) | ((uint32_t)res[1] << 8) | (uint32_t)res[2];
            } else r = s32;
            r ^= R_XOR & 0x00FFFFFFu;
            cor[i] = (r & mc) | (dv & ~mc);
        }
}

static int s_desenhar2(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n, g3d_vala_t *va)
{
    if (!pronto || !cor) return G3D_FALHA;
    int nu = e->unidades > 4 ? 4 : e->unidades;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        if (x->ativa && (x->vaga >= GPU3D_TEXTURAS || !tex[x->vaga].px || (int)x->vaga == dest_v || x->tipo != G3D_TEX_2D)) return G3D_NAO;
    }
    ajustes(G3D_AJ_DIBUJO);
    if (!n_v2++) klog("[GPU3D] simulado: primer dibujo v2 (%d unidades)\n", e->unidades);
    for (int i = 0; i + 2 < n && !R_MUDO; i += 3) {
        sv2_t a, b, c;
        converter2(&v[i], &a); converter2(&v[i + 1], &b); converter2(&v[i + 2], &c);
        triangulo2(e, &a, &b, &c);
        gpu3d_respirar();   /* D436 */
    }
    vala++;
    if (va) *va = vala;
    return G3D_OK;
}

static int s_limpar2(const gpu3d_limpar2_t *l)
{
    if (!pronto || !cor) return G3D_FALHA;
    ajustes(G3D_AJ_DIBUJO);
    if (R_MUDO) return G3D_OK;
    uint32_t mc = 0;
    if (l->mascara_cor & G3D_MC_R) mc |= 0xFF0000u;
    if (l->mascara_cor & G3D_MC_G) mc |= 0xFF00u;
    if (l->mascara_cor & G3D_MC_B) mc |= 0xFFu;
    if ((l->mascara_cor & G3D_MC_A) && dest_v >= 0) mc |= 0xFF000000u;
    if (dest_v < 0) mc &= 0x00FFFFFFu;
    uint32_t cv = dest_v >= 0 ? l->cor : (l->cor & 0x00FFFFFFu);
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) {
            if (l->tijera && (x < l->tx || y < l->ty || x >= l->tx + l->tl || y >= l->ty + l->ta)) continue;
            int i = y * L + x;
            if (l->mascara & G3D_LIMPA_COR) cor[i] = (cv & mc) | (cor[i] & ~mc);
            if (l->mascara & G3D_LIMPA_Z) zb[i] = (zb[i] & 0xFF000000u) | (l->z & 0xFFFFFFu);
            if (l->mascara & G3D_LIMPA_EST) {
                uint32_t s = zb[i] >> 24, m = l->mascara_est;
                zb[i] = (zb[i] & 0xFFFFFFu) | (((s & ~m) | (l->estencil & m)) << 24);
            }
        }
    return G3D_OK;
}

/* DXT1/3/5 -> ARGB, bloque a bloque. */
static uint32_t de565(uint32_t c)
{
    uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}

static void bloco_dxt(const uint8_t *p, uint32_t fmt, uint32_t sai[16])
{
    const uint8_t *cb = fmt == G3D_FMT_DXT1 ? p : p + 8;
    uint32_t c0 = cb[0] | (cb[1] << 8), c1 = cb[2] | (cb[3] << 8), idx = cb[4] | (cb[5] << 8) | (cb[6] << 16) | ((uint32_t)cb[7] << 24);
    uint32_t pal[4];
    pal[0] = de565(c0) | 0xFF000000u; pal[1] = de565(c1) | 0xFF000000u;
    bool quatro = c0 > c1 || fmt != G3D_FMT_DXT1;
    for (int k = 0; k < 24; k += 8) {
        uint32_t a = (pal[0] >> k) & 0xFF, b = (pal[1] >> k) & 0xFF;
        if (quatro) { pal[2] = (pal[2] & ~(0xFFu << k)) | (((2 * a + b) / 3) << k); pal[3] = (pal[3] & ~(0xFFu << k)) | (((a + 2 * b) / 3) << k); }
        else { pal[2] = (pal[2] & ~(0xFFu << k)) | (((a + b) / 2) << k); pal[3] &= ~(0xFFu << k); }
    }
    pal[2] |= 0xFF000000u;
    pal[3] = quatro ? pal[3] | 0xFF000000u : 0;
    for (int i = 0; i < 16; i++) sai[i] = pal[(idx >> (2 * i)) & 3];
    if (fmt == G3D_FMT_DXT3)
        for (int i = 0; i < 16; i++) { uint32_t a = (p[i / 2] >> (4 * (i & 1))) & 15; sai[i] = (sai[i] & 0xFFFFFFu) | ((a * 17) << 24); }
    if (fmt == G3D_FMT_DXT5) {
        uint32_t a0 = p[0], a1 = p[1], ap[8];
        ap[0] = a0; ap[1] = a1;
        for (int k = 1; k < 7; k++) ap[1 + k] = a0 > a1 ? ((7 - k) * a0 + k * a1) / 7 : 0;
        if (a0 <= a1) { for (int k = 1; k < 5; k++) ap[1 + k] = ((5 - k) * a0 + k * a1) / 5; ap[6] = 0; ap[7] = 255; }
        uint64_t bits = 0;
        for (int k = 0; k < 6; k++) bits |= (uint64_t)p[2 + k] << (8 * k);
        for (int i = 0; i < 16; i++) sai[i] = (sai[i] & 0xFFFFFFu) | (ap[(bits >> (3 * i)) & 7] << 24);
    }
}

static int s_textura2(const gpu3d_textura2_t *t)
{
    if (!pronto) return G3D_FALHA;
    if (t->vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    if (!t->dados) return s_textura((int)t->vaga, 0, 0, NULL);
    if (t->tipo != G3D_TEX_2D || t->face || (int)t->vaga == dest_v || t->nivel > 11) return G3D_NAO;
    if (t->nivel) {   /* un mipmap del hueco ya creado (ARGB) */
        int v = (int)t->vaga, l = tex[v].larg >> t->nivel, a = tex[v].alt >> t->nivel;
        if (l < 1) l = 1;
        if (a < 1) a = 1;
        if (!tex[v].px || t->formato != G3D_FMT_ARGB8888 || (int)t->nivel >= tex[v].niveis || (int)t->larg != l || (int)t->alt != a)
            return G3D_NAO;
        if (!tex[v].nv[t->nivel]) tex[v].nv[t->nivel] = kmalloc((uint32_t)(l * a) * 4);
        if (!tex[v].nv[t->nivel]) return G3D_NAO;
        memcpy(tex[v].nv[t->nivel], (const void *)(uintptr_t)t->dados, (uint32_t)(l * a) * 4);
        return G3D_OK;
    }
    if (t->formato == G3D_FMT_ARGB8888) {
        int r = s_textura((int)t->vaga, (int)t->larg, (int)t->alt, (const uint32_t *)(uintptr_t)t->dados);
        if (r == G3D_OK) tex[t->vaga].niveis = t->niveis < 1 ? 1 : t->niveis > 12 ? 12 : (int)t->niveis;
        return r;
    }
    int l = (int)t->larg, a = (int)t->alt;
    if (t->formato > G3D_FMT_DXT5 || l < 1 || a < 1 || l > GPU3D_MAX_LADO || a > GPU3D_MAX_LADO) return G3D_NAO;
    uint32_t *px = kmalloc((uint32_t)(l * a) * 4);
    if (!px) return G3D_NAO;
    const uint8_t *o = (const uint8_t *)(uintptr_t)t->dados;
    uint32_t bs = t->formato == G3D_FMT_DXT1 ? 8u : 16u, nbx = (uint32_t)(l + 3) / 4;
    for (int by = 0; by < (a + 3) / 4; by++)
        for (int bx = 0; bx < (int)nbx; bx++) {
            uint32_t b[16];
            bloco_dxt(o + ((uint32_t)by * nbx + (uint32_t)bx) * bs, t->formato, b);
            for (int i = 0; i < 16; i++) {
                int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                if (x < l && y < a) px[y * l + x] = b[i];
            }
        }
    int r = s_textura((int)t->vaga, l, a, px);
    kfree(px);
    return r;
}

static int s_destino(int vaga, int zbits, int estencil)
{
    (void)zbits; (void)estencil;
    if (!pronto || !cor) return G3D_FALHA;
    voltar_principal();
    if (vaga < 0) return G3D_OK;
    if (vaga >= GPU3D_TEXTURAS || !tex[vaga].px || tex[vaga].larg < 8 || tex[vaga].alt < 8) return G3D_NAO;
    dest_zb = kmalloc((uint32_t)(tex[vaga].larg * tex[vaga].alt) * 4);
    if (!dest_zb) return G3D_NAO;
    princ_cor = cor; princ_zb = zb; princ_L = L; princ_A = A;
    cor = tex[vaga].px; zb = dest_zb; L = tex[vaga].larg; A = tex[vaga].alt;
    dest_v = vaga;
    return G3D_OK;
}

static void s_capacidades(gpu3d_capas_t *c)
{
    if (!pronto) return;
    uint32_t cand = G3D_CAP_V2 | G3D_CAP_COMBINAR | G3D_CAP_DOT3 | G3D_CAP_ALFA_TESTE | G3D_CAP_ESTENCIL | G3D_CAP_TIJERA |
                    G3D_CAP_MASCARAS | G3D_CAP_Z_FUNCS | G3D_CAP_MEZCLA_EXT | G3D_CAP_MEZCLA_SEP | G3D_CAP_MEZCLA_EQ |
                    G3D_CAP_LOGICA | G3D_CAP_NEBLINA | G3D_CAP_TEX_ESPELHO | G3D_CAP_TEX_NPOT | G3D_CAP_TEX_MIPMAP | G3D_CAP_LIMPAR2 | G3D_CAP_DESTINO_TEX;
    uint32_t un = 4, fmts = 0xFu;
    static const g3d_v2_ops_t ops = { s_alvo, s_limpar, s_textura, s_textura2, s_desenhar2, s_limpar2, s_ler, s_destino };
    uint32_t ok = g3d_v2_provar(&ops, "simulado", cand, &un, &fmts);
    c->bits = ok; c->unidades = ok ? un : 1; c->formatos = fmts; c->max_lado = GPU3D_MAX_LADO; c->max_aniso = 1; c->zbits = 3;
    /* D436: hasta GPU3D_MAX_LADO (antes 512): AoEN y la SDL suben el lienzo entero y atlas de 1024 */
    c->estencil_bits = (ok & G3D_CAP_ESTENCIL) ? 8 : 0;
    /* D457: v3 por la CPU (gpu3d_v3.c): lo que esa función sabe hacer; la capa lo pone a prueba */
    c->bits3 = regs[7] /* R_SEM_V3 */ ? 0 : G3D_V3_CPU_BITS3;
}

/* D432: presentar. La «GPU» escribe en el framebuffer de la tarjeta (no en el búfer de atrás del
 * compositor), que es lo que hace una de verdad: en QEMU una captura de pantalla lo ve. Vecino más
 * próximo siempre (el bilineal se acepta y se ignora). 0x1008 distinto de 0: «no puedo» (pruebas). */
#define R_SEM_APRES regs[2]
static int s_apresentar(const g3d_apres_t *p)
{
    if (!pronto) return G3D_FALHA;
    if (!p) return video_ativo() && video_formato_direto() && video_bpp() == 32 && !R_SEM_APRES ? G3D_OK : G3D_NAO;
    if (!video_ativo() || !video_formato_direto() || video_bpp() != 32 || R_SEM_APRES) return G3D_NAO;
    s_lento();
    const uint32_t *f; int fl;
    if (p->vaga < 0) { if (!princ_cor && !cor) return G3D_FALHA; f = dest_v >= 0 ? princ_cor : cor; fl = dest_v >= 0 ? princ_L : L; }
    else { if (p->vaga >= GPU3D_TEXTURAS || !tex[p->vaga].px) return G3D_NAO; f = tex[p->vaga].px; fl = tex[p->vaga].larg; }
    uint8_t *fb = video_placa();
    uint32_t pitch = video_pitch();
    int tl = (int)video_largura(), ta = (int)video_altura();
    if (p->flags & G3D_APRES_BANDAS) {
        int r[4][4], n = g3d_apres_bandas(p, tl, ta, r);
        for (int i = 0; i < n; i++)
            for (int y = r[i][1]; y < r[i][1] + r[i][3]; y++)
                memset(fb + (uint32_t)y * pitch + (uint32_t)r[i][0] * 4, 0, (uint32_t)r[i][2] * 4);
    }
    for (int y = 0; y < p->da; y++) {
        const uint32_t *o = f + (uint32_t)(p->oy + (int)(((uint32_t)y * 2 + 1) * (uint32_t)p->oa / (2u * (uint32_t)p->da))) * (uint32_t)fl + (uint32_t)p->ox;
        uint32_t *d = (uint32_t *)(fb + (uint32_t)(p->dy + y) * pitch) + p->dx;
        if (p->dl == p->ol) { for (int x = 0; x < p->dl; x++) d[x] = o[x] & 0x00FFFFFFu; continue; }
        for (int x = 0; x < p->dl; x++) d[x] = o[(uint32_t)(((uint32_t)x * 2 + 1) * (uint32_t)p->ol / (2u * (uint32_t)p->dl))] & 0x00FFFFFFu;
    }
    vala++;
    return G3D_OK;
}

/* ============================================================ gpu3d v3 (D457)
 *
 * Los búferes «residentes» son memoria del kernel y la transformación la hace g3d_v3_cpu (gpu3d_v3.c) con
 * enteros; lo que sale se pinta con el v2 de arriba. Así se prueba en QEMU el camino entero de v3: SaguiGL ->
 * VBUF/TRANSF/DESENHAR3 -> capa (contabilidad, expulsión, autoprueba) -> driver. «Registros» para las pruebas:
 *   0x1014 = N: la «memoria de la GPU» para búferes es de N KB (por defecto 8192): para provocar expulsiones;
 *   0x1018 distinto de 0: la «GPU» transforma mal (la x a la mitad): la autoprueba v3 debe notarlo y dejar v2;
 *   0x101C distinto de 0: el driver no anuncia v3 (para comparar en QEMU el mismo programa con y sin v3). */
#define R_VB_KB    regs[5]
#define R_V3_MAL   regs[6]
#define R_SEM_V3   regs[7]
static struct { uint8_t *p; uint32_t bytes; } vbs[GPU3D_VBUFS];
static uint32_t vbs_bytes, n_v3;
static gpu3d_vert2_t *lote3;
#define LOTE3 1536

static void s_vbuf_soltar_todos(void)
{
    for (int i = 0; i < GPU3D_VBUFS; i++) if (vbs[i].p) { kfree(vbs[i].p); vbs[i].p = NULL; vbs[i].bytes = 0; }
    vbs_bytes = 0;
    if (lote3) { kfree(lote3); lote3 = NULL; }
}

static int s_vbuf(int vaga, uint32_t bytes, uint32_t desloc, const void *dados, uint32_t n)
{
    if (!pronto) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_VBUFS) return G3D_NAO;
    if (dados) {
        if (!vbs[vaga].p || desloc > vbs[vaga].bytes || n > vbs[vaga].bytes - desloc) return G3D_NAO;
        memcpy(vbs[vaga].p + desloc, dados, n);
        return G3D_OK;
    }
    if (vbs[vaga].p) { kfree(vbs[vaga].p); vbs_bytes -= vbs[vaga].bytes; vbs[vaga].p = NULL; vbs[vaga].bytes = 0; }
    if (!bytes) return G3D_OK;
    uint32_t max = (R_VB_KB ? R_VB_KB : 8192u) << 10;
    if (bytes > max || vbs_bytes > max - bytes) return G3D_NAO;
    vbs[vaga].p = kmalloc(bytes);
    if (!vbs[vaga].p) return G3D_NAO;
    memset(vbs[vaga].p, 0, bytes);
    vbs[vaga].bytes = bytes; vbs_bytes += bytes;
    return G3D_OK;
}

static uint32_t n_v3_detalle(void) { return n_v3; }

static g3d_v3_mem_t s_v3_mem(void *ctx, int vaga)
{
    (void)ctx;
    if (vaga < 0 || vaga >= GPU3D_VBUFS) return (g3d_v3_mem_t){ NULL, 0 };
    return (g3d_v3_mem_t){ vbs[vaga].p, vbs[vaga].bytes };
}
static int s_v3_sair(void *ctx, const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n) { (void)ctx; return s_desenhar2(e, v, n, NULL); }

static int s_desenhar3(const g3d_desenho3_t *d, g3d_vala_t *va)
{
    if (!pronto || !cor) return G3D_FALHA;
    if (!lote3 && !(lote3 = kmalloc(sizeof(gpu3d_vert2_t) * LOTE3))) return G3D_NAO;
    if (!n_v3++) klog("[GPU3D] simulado: primer dibujo v3 (desde buferes; %u vertices, %s)\n", d->d.n,
                      d->d.indices == GPU3D_SEM_INDICES ? "sin indices" : "con indices");
    static gpu3d_transf_t mal;
    g3d_desenho3_t g = *d;
    if (R_V3_MAL) { mal = *d->t; mal.mvp[0] = g3d_fl_mul(mal.mvp[0], 0x3F000000u); g.t = &mal; }
    g3d_v3_cpu_t c = { s_v3_mem, s_v3_sair, NULL, lote3, LOTE3 };
    int r = g3d_v3_cpu(&g, &c);
    if (va) *va = vala;
    return r;
}

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
    s_capacidades, s_desenhar2, s_textura2, s_limpar2, s_destino,
    s_apresentar,
    s_vbuf, s_desenhar3,
};
