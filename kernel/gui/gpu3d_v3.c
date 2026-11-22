// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_v3.c - gpu3d v3 por la CPU: la transformación de los vértices con enteros (D457).
 *
 * En gpu3d v3 la GPU guarda los vértices y los transforma (matrices, niebla, matriz de
 * textura, recorte, vista, descarte de caras). Esto es lo mismo hecho por la CPU, con coma
 * flotante IEEE de 32 bits por software (el kernel no usa la de la máquina), y sirve para:
 *   - el driver simulado de QEMU (gpu3d_sim.c): su v3 es esta función + su v2;
 *   - la autoprueba de la capa (gpu3d.c): dibuja la misma escena por el desenhar3 del
 *     driver y por aquí + su desenhar2, y compara los píxeles. Así la referencia de la
 *     transformación de un driver de verdad es la CPU, sin constantes escritas a mano.
 * No toca hardware ni usa nada del kernel: se compila también en el PC para la prueba de
 * unidad (tests/g3d_v3_unidad.c), que la compara con una referencia de ES 1.1 en coma
 * flotante de verdad.
 *
 * Lo que NO hace (devuelve G3D_NAO): luces (G3D3_LUZ) y niebla exponencial. */
#include "gpu3d.h"

typedef uint32_t fl_t;
#define FL_UM    0x3F800000u
#define FL_ZERO  0u
#define FL_255   0x437F0000u
#define FL_SINAL 0x80000000u

static inline int  fl_e(fl_t a) { return (int)((a >> 23) & 0xFF); }
static inline bool fl_nulo(fl_t a) { return fl_e(a) == 0; }          /* 0 o desnormalizado */
static inline bool fl_mau(fl_t a) { return fl_e(a) == 0xFF; }        /* infinito o NaN */
static inline uint32_t fl_m(fl_t a) { return (a & 0x7FFFFFu) | 0x800000u; }

/* El valor m x 2^e con el signo s, redondeado al más próximo (empates, al par). */
static fl_t fl_montar(uint32_t s, int e, uint64_t m)
{
    if (!m) return s;
    int k = 63;
    while (!(m >> k)) k--;
    int E = e + k;
    uint32_t r;
    if (k > 23) {
        int d = k - 23;
        uint64_t resto = m & ((1ull << d) - 1), meio = 1ull << (d - 1);
        r = (uint32_t)(m >> d);
        if (resto > meio || (resto == meio && (r & 1))) { r++; if (r == 0x1000000u) { r >>= 1; E++; } }
    } else r = (uint32_t)(m << (23 - k));
    if (E + 127 >= 255) return s | 0x7F800000u;
    if (E + 127 <= 0) return s;
    return s | ((uint32_t)(E + 127) << 23) | (r & 0x7FFFFFu);
}

uint32_t g3d_fl_mul(uint32_t a, uint32_t b)
{
    uint32_t s = (a ^ b) & FL_SINAL;
    if (fl_mau(a) || fl_mau(b)) return s | 0x7F800000u;
    if (fl_nulo(a) || fl_nulo(b)) return s;
    return fl_montar(s, fl_e(a) + fl_e(b) - 2 * (127 + 23), (uint64_t)fl_m(a) * fl_m(b));
}

uint32_t g3d_fl_soma(uint32_t a, uint32_t b)
{
    if (fl_mau(a) || fl_mau(b)) return 0x7F800000u;
    if (fl_nulo(a)) return fl_nulo(b) ? 0 : b;
    if (fl_nulo(b)) return a;
    if (fl_e(a) < fl_e(b)) { uint32_t t = a; a = b; b = t; }
    int d = fl_e(a) - fl_e(b);
    if (d > 30) return a;
    uint64_t A = (uint64_t)fl_m(a) << d, B = fl_m(b);
    uint32_t sa = a & FL_SINAL, sb = b & FL_SINAL;
    int e = fl_e(b) - 127 - 23;
    if (sa == sb) return fl_montar(sa, e, A + B);
    if (A > B) return fl_montar(sa, e, A - B);
    if (B > A) return fl_montar(sb, e, B - A);
    return 0;
}

uint32_t g3d_fl_div(uint32_t a, uint32_t b)
{
    uint32_t s = (a ^ b) & FL_SINAL;
    if (fl_mau(a) || fl_mau(b) || fl_nulo(b)) return s | 0x7F800000u;
    if (fl_nulo(a)) return s;
    uint64_t n = (uint64_t)fl_m(a) << 40, q = n / fl_m(b);
    uint64_t resto = n - q * fl_m(b);
    return fl_montar(s, fl_e(a) - fl_e(b) - 41, (q << 1) | (resto ? 1u : 0u));
}

uint32_t g3d_fl_raiz(uint32_t a)
{
    if (fl_mau(a)) return 0x7F800000u;
    if (fl_nulo(a) || (a & FL_SINAL)) return 0;
    int e = fl_e(a) - 127 - 23;
    uint64_t m = fl_m(a);
    if (e & 1) { m <<= 1; e--; }
    m <<= 38;
    /* raíz entera de 64 bits, bit a bit */
    uint64_t r = 0, bit = 1ull << 62, x = m;
    while (bit) {
        if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return fl_montar(0, (e - 38) / 2 - 1, (r << 1) | (x ? 1u : 0u));
}

#define MUL g3d_fl_mul
#define SOM g3d_fl_soma
#define SUB g3d_fl_sub

/* Para comparar: un entero con el mismo orden que el número. */
static inline int32_t fl_ordem(fl_t a)
{
    if (fl_nulo(a)) return 0;
    int32_t m = (int32_t)(a & 0x7FFFFFFFu);
    return a & FL_SINAL ? -m : m;
}
static inline bool fl_menor(fl_t a, fl_t b) { return fl_ordem(a) < fl_ordem(b); }
static inline bool fl_negativo(fl_t a) { return (a & FL_SINAL) && !fl_nulo(a); }
static inline fl_t fl_limitar(fl_t a, fl_t lo, fl_t hi) { return fl_menor(a, lo) ? lo : fl_menor(hi, a) ? hi : a; }
static inline fl_t fl_de_int(int32_t v) { return g3d_float_de_fixo(v, 0); }
/* Un número de 0 a 255 al byte más próximo. */
static inline uint32_t fl_byte(fl_t a)
{
    if (fl_mau(a)) return a & FL_SINAL ? 0u : 255u;
    int32_t q = g3d_fixo_de_float(a, 8);
    return q <= 0 ? 0u : q >= 255 * 256 ? 255u : (uint32_t)(q + 128) >> 8;
}

/* ------------------------------------------------------------ la tubería */

typedef struct {
    fl_t c[4];            /* recorte */
    fl_t cor[4];          /* r, g, b, a de 0 a 255 */
    fl_t esp[3];          /* especular, de 0 a 255 */
    fl_t f;               /* factor de niebla, de 0 a 1 */
    fl_t tc[4][4];
    bool mau;             /* no se pudo leer o salió infinito: su triángulo no se dibuja */
    bool completo;        /* color, niebla y coordenadas ya calculados (solo se hace si su triángulo no cae fuera entero) */
    fl_t p[4];            /* la posición del objeto, para completarlo */
    uint32_t i;           /* su número de vértice */
} pv_t;

typedef struct {
    const g3d_desenho3_t *d;
    const g3d_v3_cpu_t   *c;
    const gpu3d_transf_t *t;
    g3d_v3_mem_t m_pos, m_cor, m_tc[4], m_idx;
    int   unidades;
    fl_t  x_min, x_max, y_min, y_max;   /* la caja de la vista */
    fl_t  neb_cor[3];
    int   n_lote, r;
} v3_t;

#define CACHE 32
static pv_t cache[CACHE];
static uint32_t cache_i[CACHE];
static bool cache_ok[CACHE];

static inline fl_t palavra(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

/* El atributo del vértice i como (x, y, z, w). false: cae fuera del búfer. */
static bool atributo(const g3d_v3_mem_t *m, const gpu3d_atrib_t *a, uint32_t i, fl_t o[4])
{
    uint32_t tam = a->formato == G3D_AT_UBYTE4N ? 4u : (uint32_t)a->formato * 4u;
    uint64_t off = (uint64_t)a->desloc + (uint64_t)i * a->passo;
    if (!m->p || off + tam > m->bytes) return false;
    const uint8_t *p = m->p + off;
    if (a->formato == G3D_AT_UBYTE4N) { for (int k = 0; k < 4; k++) o[k] = fl_de_int(p[k]); return true; }
    o[2] = FL_ZERO; o[3] = FL_UM;
    for (int k = 0; k < a->formato; k++) o[k] = palavra(p + k * 4);
    return true;
}

/* fila l de la matriz m (por columnas) por el vector v */
static fl_t fila(const uint32_t *m, int l, const fl_t v[4])
{
    fl_t r = SOM(MUL(m[l], v[0]), MUL(m[4 + l], v[1]));
    if (!fl_nulo(v[2])) r = SOM(r, MUL(m[8 + l], v[2]));
    return SOM(r, v[3] == FL_UM ? m[12 + l] : MUL(m[12 + l], v[3]));
}

static void vertice(v3_t *s, uint32_t i, pv_t *o)
{
    const gpu3d_transf_t *t = s->t;
    fl_t p[4];
    o->mau = !atributo(&s->m_pos, &s->d->d.pos, i, p);
    if (o->mau) return;
    for (int l = 0; l < 4; l++) o->c[l] = fila(t->mvp, l, p);
    for (int k = 0; k < 4; k++) { o->p[k] = p[k]; if (fl_mau(o->c[k])) o->mau = true; }
    o->i = i; o->completo = false;
}

/* El resto del vértice: color, niebla y coordenadas de textura. */
static void completar(v3_t *s, pv_t *o)
{
    const gpu3d_transf_t *t = s->t;
    const fl_t *p = o->p;
    uint32_t i = o->i;
    if (o->completo || o->mau) return;
    o->completo = true;
    if (s->d->d.cor_a.formato == G3D_AT_NADA) {
        o->cor[0] = fl_de_int((int32_t)((t->cor >> 16) & 0xFF)); o->cor[1] = fl_de_int((int32_t)((t->cor >> 8) & 0xFF));
        o->cor[2] = fl_de_int((int32_t)(t->cor & 0xFF)); o->cor[3] = fl_de_int((int32_t)(t->cor >> 24));
    } else {
        if (!atributo(&s->m_cor, &s->d->d.cor_a, i, o->cor)) { o->mau = true; return; }
        if (s->d->d.cor_a.formato != G3D_AT_UBYTE4N)
            for (int k = 0; k < 4; k++) o->cor[k] = fl_limitar(MUL(o->cor[k], FL_255), FL_ZERO, FL_255);
    }
    o->esp[0] = o->esp[1] = o->esp[2] = FL_ZERO;
    o->f = FL_UM;
    if (t->flags & G3D3_NEBLINA) {
        fl_t e[3];
        for (int l = 0; l < 3; l++) e[l] = fila(t->mv, l, p);
        fl_t dist = g3d_fl_raiz(SOM(SOM(MUL(e[0], e[0]), MUL(e[1], e[1])), MUL(e[2], e[2])));
        o->f = fl_limitar(SUB(t->neb_a, MUL(dist, t->neb_b)), FL_ZERO, FL_UM);
        if (t->flags & G3D3_NEB_MEZCLADA) {
            fl_t g = SUB(FL_UM, o->f);
            for (int k = 0; k < 3; k++) { o->cor[k] = MUL(o->cor[k], o->f); o->esp[k] = MUL(s->neb_cor[k], g); }
            o->f = FL_UM;
        }
    }
    for (int u = 0; u < s->unidades; u++) {
        fl_t q[4];
        if (s->d->d.tc[u].formato == G3D_AT_NADA) for (int k = 0; k < 4; k++) q[k] = t->tc_const[u][k];
        else if (!atributo(&s->m_tc[u], &s->d->d.tc[u], i, q)) { o->mau = true; return; }
        if (t->flags & G3D3_TEX_MAT(u)) for (int l = 0; l < 4; l++) o->tc[u][l] = fila(t->tex[u], l, q);
        else for (int k = 0; k < 4; k++) o->tc[u][k] = q[k];
    }
}

static const pv_t *buscar(v3_t *s, uint32_t k)
{
    uint32_t i = s->d->d.primeiro + k;
    if (s->d->d.indices != GPU3D_SEM_INDICES) {
        uint64_t off = (uint64_t)i * 2;
        if (!s->m_idx.p || off + 2 > s->m_idx.bytes) return NULL;
        i = (uint32_t)s->m_idx.p[off] | ((uint32_t)s->m_idx.p[off + 1] << 8);
    }
    uint32_t h = i % CACHE;
    if (!cache_ok[h] || cache_i[h] != i) { vertice(s, i, &cache[h]); cache_i[h] = i; cache_ok[h] = true; }
    return cache[h].mau ? NULL : &cache[h];
}

/* Distancia firmada al plano p del volumen de vista (dentro: >= 0). */
static fl_t distancia(const pv_t *v, int p)
{
    fl_t x = v->c[p >> 1];
    return p & 1 ? SUB(v->c[3], x) : SOM(v->c[3], x);
}

static void interpolar(pv_t *r, const pv_t *a, const pv_t *b, fl_t t, int unidades)
{
    #define MIX(campo) r->campo = SOM(a->campo, MUL(SUB(b->campo, a->campo), t))
    for (int k = 0; k < 4; k++) { MIX(c[k]); MIX(cor[k]); }
    for (int k = 0; k < 3; k++) MIX(esp[k]);
    MIX(f);
    for (int u = 0; u < unidades; u++) for (int k = 0; k < 4; k++) MIX(tc[u][k]);
    #undef MIX
    r->mau = false;
}

static void despejar(v3_t *s)
{
    if (s->n_lote && s->r == G3D_OK) s->r = s->c->saida(s->c->ctx, s->d->e, s->c->lote, s->n_lote);
    s->n_lote = 0;
}

#define POL_MAX 10   /* un triángulo recortado por seis planos: como mucho 9 vértices */

static void triangulo(v3_t *s, const pv_t *a, const pv_t *b, const pv_t *c)
{
    static pv_t pol[2][POL_MAX];
    const pv_t *in[3] = { a, b, c };
    unsigned todos = ~0u, alguno = 0;
    for (int i = 0; i < 3; i++) {
        unsigned k = 0;
        for (int p = 0; p < 6; p++) if (fl_negativo(distancia(in[i], p))) k |= 1u << p;
        todos &= k; alguno |= k;
    }
    if (todos) return;
    int n = 3, cur = 0;
    for (int i = 0; i < 3; i++) { pol[0][i] = *in[i]; completar(s, &pol[0][i]); if (pol[0][i].mau) return; }
    for (int p = 0; p < 6 && n >= 3; p++) {
        if (!(alguno & (1u << p))) continue;
        pv_t *e = pol[cur], *o = pol[cur ^ 1];
        int m = 0;
        for (int i = 0; i < n && m < POL_MAX - 1; i++) {
            const pv_t *x = &e[i], *y = &e[(i + 1) % n];
            fl_t dx = distancia(x, p), dy = distancia(y, p);
            bool fx = fl_negativo(dx), fy = fl_negativo(dy);
            if (!fx) o[m++] = *x;
            if (fx != fy && m < POL_MAX) interpolar(&o[m++], x, y, g3d_fl_div(dx, SUB(dx, dy)), s->unidades);
        }
        n = m; cur ^= 1;
    }
    if (n < 3) return;
    /* a la pantalla */
    const gpu3d_transf_t *t = s->t;
    gpu3d_vert2_t g[POL_MAX];
    const pv_t *q = pol[cur];
    for (int i = 0; i < n; i++) {
        const pv_t *v = &q[i];
        if (fl_nulo(v->c[3]) || (v->c[3] & FL_SINAL)) return;
        fl_t iw = g3d_fl_div(FL_UM, v->c[3]);
        if (fl_mau(iw)) return;
        fl_t x = SOM(MUL(MUL(v->c[0], iw), t->vp_escala[0]), t->vp_desloc[0]);
        fl_t y = SOM(MUL(MUL(v->c[1], iw), t->vp_escala[1]), t->vp_desloc[1]);
        fl_t z = SOM(MUL(MUL(v->c[2], iw), t->vp_escala[2]), t->vp_desloc[2]);
        g[i].x = fl_limitar(x, s->x_min, s->x_max);
        g[i].y = fl_limitar(y, s->y_min, s->y_max);
        g[i].z = fl_limitar(z, FL_ZERO, FL_UM);
        g[i].rhw = fl_limitar(iw, 0x358637BDu /* 1e-6 */, 0x49742400u /* 1e6 */);
        g[i].cor = (fl_byte(v->cor[3]) << 24) | (fl_byte(v->cor[0]) << 16) | (fl_byte(v->cor[1]) << 8) | fl_byte(v->cor[2]);
        g[i].espec = (fl_byte(MUL(v->f, FL_255)) << 24) | (fl_byte(v->esp[0]) << 16) | (fl_byte(v->esp[1]) << 8) | fl_byte(v->esp[2]);
        for (int u = 0; u < 4; u++)
            for (int k = 0; k < 4; k++)
                g[i].tc[u][k] = u < s->unidades ? fl_limitar(fl_mau(v->tc[u][k]) ? FL_ZERO : v->tc[u][k], 0xC6EA6000u /* -30000 */, 0x46EA6000u)
                                                : k == 3 ? FL_UM : FL_ZERO;
    }
    /* descarte de caras por el signo del área en la pantalla (la suma de todo el polígono) */
    fl_t area = FL_ZERO;
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; area = SOM(area, SUB(MUL(g[i].x, g[j].y), MUL(g[j].x, g[i].y))); }
    if (fl_nulo(area)) return;
    if (t->descarte == G3D_DESC_POS && !(area & FL_SINAL)) return;
    if (t->descarte == G3D_DESC_NEG && (area & FL_SINAL)) return;
    if (s->n_lote + (n - 2) * 3 > s->c->max) despejar(s);
    gpu3d_vert2_t *o = s->c->lote + s->n_lote;
    for (int i = 1; i + 1 < n; i++) { *o++ = g[0]; *o++ = g[i]; *o++ = g[i + 1]; }
    s->n_lote += (n - 2) * 3;
}

static fl_t fl_abs(fl_t a) { return a & 0x7FFFFFFFu; }

int g3d_v3_cpu(const g3d_desenho3_t *d, const g3d_v3_cpu_t *c)
{
    const gpu3d_transf_t *t = d->t;
    if ((t->flags & G3D3_LUZ) || ((t->flags & G3D3_NEBLINA) && t->neb_modo != G3D_NEB_LINEAR)) return G3D_NAO;
    if (!c->lote || c->max < 24 || d->d.modo > G3D_PRIM_ABANICO) return G3D_NAO;
    if (d->d.pos.formato < G3D_AT_FLOAT2 || d->d.pos.formato > G3D_AT_FLOAT4 || d->d.cor_a.formato > G3D_AT_UBYTE4N) return G3D_NAO;
    for (int u = 0; u < 4; u++) if (d->d.tc[u].formato > G3D_AT_FLOAT4 || d->d.tc[u].formato == 1) return G3D_NAO;
    v3_t s;
    s.d = d; s.c = c; s.t = t; s.n_lote = 0; s.r = G3D_OK;
    s.unidades = d->e->unidades > 4 ? 4 : d->e->unidades;
    s.m_pos = c->mem(c->ctx, d->d.pos.vaga);
    s.m_cor = d->d.cor_a.formato != G3D_AT_NADA ? c->mem(c->ctx, d->d.cor_a.vaga) : (g3d_v3_mem_t){ 0, 0 };
    for (int u = 0; u < 4; u++)
        s.m_tc[u] = u < s.unidades && d->d.tc[u].formato != G3D_AT_NADA ? c->mem(c->ctx, d->d.tc[u].vaga) : (g3d_v3_mem_t){ 0, 0 };
    s.m_idx = d->d.indices != GPU3D_SEM_INDICES ? c->mem(c->ctx, (int)d->d.indices) : (g3d_v3_mem_t){ 0, 0 };
    if (d->d.indices != GPU3D_SEM_INDICES && s.m_idx.bytes > d->bytes_indices) s.m_idx.bytes = d->bytes_indices;
    s.x_min = SUB(t->vp_desloc[0], fl_abs(t->vp_escala[0])); s.x_max = SOM(t->vp_desloc[0], fl_abs(t->vp_escala[0]));
    s.y_min = SUB(t->vp_desloc[1], fl_abs(t->vp_escala[1])); s.y_max = SOM(t->vp_desloc[1], fl_abs(t->vp_escala[1]));
    if (fl_negativo(s.x_min)) s.x_min = FL_ZERO;
    if (fl_negativo(s.y_min)) s.y_min = FL_ZERO;
    for (int k = 0; k < 3; k++) s.neb_cor[k] = fl_de_int((int32_t)((t->neb_cor >> (16 - 8 * k)) & 0xFF));
    for (int i = 0; i < CACHE; i++) cache_ok[i] = false;
    uint32_t n = d->d.n;
    const pv_t *a, *b, *e;
    pv_t pa, pb, pe;
    /* Las copias: buscar() devuelve un sitio de la caché que el siguiente buscar() puede pisar. */
    #define TRI(i, j, k) do { \
        if ((a = buscar(&s, (i))) != NULL) { pa = *a; \
            if ((b = buscar(&s, (j))) != NULL) { pb = *b; \
                if ((e = buscar(&s, (k))) != NULL) { pe = *e; triangulo(&s, &pa, &pb, &pe); } } } \
    } while (0)
    switch (d->d.modo) {
    case G3D_PRIM_TRIANGULOS:
        for (uint32_t i = 0; i + 2 < n && s.r == G3D_OK; i += 3) TRI(i, i + 1, i + 2);
        break;
    case G3D_PRIM_TIRA:
        for (uint32_t i = 0; i + 2 < n && s.r == G3D_OK; i++) { if (i & 1) TRI(i + 1, i, i + 2); else TRI(i, i + 1, i + 2); }
        break;
    default:
        for (uint32_t i = 1; i + 1 < n && s.r == G3D_OK; i++) TRI(0, i, i + 1);
        break;
    }
    #undef TRI
    despejar(&s);
    return s.r;
}
