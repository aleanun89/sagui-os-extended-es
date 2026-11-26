// SPDX-License-Identifier: GPL-3.0-only
/* sgl_ri_cpu.c - el intérprete de la representación intermedia por la CPU (SaguiGL 2.0, D475).
 *
 * Ejecuta 4 vías a la vez: un cuadrado de 2x2 píxeles (vía 0 abajo a la izquierda, 1 a su derecha, 2 encima
 * de la 0, 3 encima de la 1; las derivadas son diferencias entre vías, como en una GPU) o 4 vértices. Los
 * registros están en SoA: cada componente de cada registro es un vector de 4 float (SSE2 con las
 * extensiones de vector de GCC). Las instrucciones se predecodifican una vez (registros absolutos,
 * swizzles separados). El control de flujo divergente va por máscaras de vías. Las funciones (seno, 2^x,
 * log2) son polinomios sobre las 4 vías a la vez (sin libm ni x87) y la raíz es la de SSE: los mismos bits en
 * el anfitrión y en el SaguiOS. */
#include "sgl_ri.h"
#include "sgl2_base.h"

typedef float v4 __attribute__((vector_size(16)));
typedef int   v4i __attribute__((vector_size(16)));

typedef struct {
    uint8_t op, masc, sat, tex, nf;
    uint8_t mod[3], rel[3], rel_d;
    uint8_t sw[3][4];
    int     d, f[3];      /* registro absoluto en el banco */
    v4     *pd;           /* &banco[d * 4] */
    const v4 *pf[3][4];   /* cada componente de cada fuente, ya con el swizzle (sin índice dinámico) */
    int     lim_d, lim_f[3];   /* para los índices dinámicos: [base, lim) del fichero */
    int     base_d, base_f[3];
    int     alvo, amost;
} xins_t;

struct ri_exec {
    const ri_prog_t *p;
    v4      *banco;        /* [registro * 4 + componente] */
    void    *banco_mem;
    int      nreg, o_ent, o_sai, o_uni, o_imed;
    int      a0[4];
    xins_t  *x;
    int      nx;
};

static const v4i MASC_V[16] = {
#define M_(m) { (m) & 1 ? -1 : 0, (m) & 2 ? -1 : 0, (m) & 4 ? -1 : 0, (m) & 8 ? -1 : 0 }
    M_(0), M_(1), M_(2), M_(3), M_(4), M_(5), M_(6), M_(7), M_(8), M_(9), M_(10), M_(11), M_(12), M_(13), M_(14), M_(15)
#undef M_
};

/* ---------------------------------------------------------------- funciones */
static inline float f_sqrt(float x) { return __builtin_sqrtf(x); }
/* seno de las 4 vías: reducción a [-pi, pi] (2*pi en dos partes) y a [-pi/2, pi/2], polinomio de grado 11
 * (error < 1e-7 en la reducción; |x| > 1e7 da 0) */
static v4 v_seno(v4 x)
{
    v4 r;
    for (int v = 0; v < 4; v++) {
        float a = x[v];
        if (!(a > -1e7f && a < 1e7f)) { r[v] = a == a ? 0.0f : a; continue; }
        float k = a * 0.15915494309189535f;
        k = (float)(int)(k < 0 ? k - 0.5f : k + 0.5f);
        a = (a - k * 6.28125f) - k * 0.0019353071795864769f;
        if (a > 1.5707963267948966f) a = 3.1415926535897932f - a;
        else if (a < -1.5707963267948966f) a = -3.1415926535897932f - a;
        r[v] = a;
    }
    v4 s2 = r * r;
    v4 p = s2 * (-2.5052108385441720e-8f) + 2.7557319223985893e-6f;
    p = p * s2 - 1.9841269841269841e-4f;
    p = p * s2 + 8.3333333333333333e-3f;
    p = p * s2 - 1.6666666666666667e-1f;
    return r + r * s2 * p;
}
/* 2^x: parte entera en el exponente, la fracción por un polinomio de grado 6 (error relativo < 2e-7) */
static v4 v_ex2(v4 x)
{
    v4 r;
    for (int v = 0; v < 4; v++) {
        float a = x[v];
        if (!(a == a)) { r[v] = a; continue; }
        if (a < -126.0f) { r[v] = 0; continue; }
        if (a >= 128.0f) { r[v] = __builtin_inff(); continue; }
        int i = (int)a; if ((float)i > a) i--;
        float f = a - (float)i;
        float p = 1.0f + f * (0.69314718f + f * (0.24022652f + f * (0.055504108f + f * (0.0096181291f + f * (0.0013333558f + f * 0.00015403530f)))));
        union { float f; uint32_t u; } e; e.u = (uint32_t)(i + 127) << 23;
        r[v] = p * e.f;
    }
    return r;
}
/* log2(x): exponente + log2 de la mantisa en [0.707, 1.414) por la serie de atanh (error < 3e-8) */
static v4 v_lg2(v4 x)
{
    v4 r;
    for (int v = 0; v < 4; v++) {
        float a = x[v];
        if (!(a == a) || a < 0) { r[v] = __builtin_nanf(""); continue; }
        if (a == 0) { r[v] = -__builtin_inff(); continue; }
        if (a == __builtin_inff()) { r[v] = a; continue; }
        union { float f; uint32_t u; } m = { a };
        int e = (int)((m.u >> 23) & 255);
        if (e == 0) { m.f = a * 8388608.0f; e = (int)((m.u >> 23) & 255) - 23; }   /* subnormal */
        e -= 127;
        m.u = (m.u & 0x007FFFFFu) | 0x3F800000u;
        float y = m.f;
        if (y > 1.41421356f) { y *= 0.5f; e++; }
        float t = (y - 1.0f) / (y + 1.0f), t2 = t * t;
        float sr = t * (2.0f + t2 * (0.66666667f + t2 * (0.4f + t2 * (0.28571429f + t2 * 0.22222222f))));
        r[v] = (float)e + sr * 1.4426950408889634f;
    }
    return r;
}
static float f_piso(float x)
{
    if (!(x > -8388608.0f && x < 8388608.0f)) return x;   /* ya es entero (o NaN/inf) */
    float t = (float)(int)x;
    return t > x ? t - 1.0f : t;
}
static float f_trunc(float x)
{
    if (!(x > -8388608.0f && x < 8388608.0f)) return x;
    return (float)(int)x;
}

/* ---------------------------------------------------------------- preparación */
ri_exec_t *ri_exec_criar(const ri_prog_t *p)
{
    ri_exec_t *e = aloca_zerado(sizeof *e);
    if (!e) return NULL;
    e->p = p;
    e->o_ent = p->ntemp; e->o_sai = e->o_ent + p->nent; e->o_uni = e->o_sai + p->nsai; e->o_imed = e->o_uni + p->nuni;
    e->nreg = e->o_imed + p->nimed + 1;
    e->banco_mem = aloca_zerado((size_t)e->nreg * 4 * sizeof(v4) + 16);
    e->x = aloca_zerado((size_t)(p->nins ? p->nins : 1) * sizeof(xins_t));
    if (!e->banco_mem || !e->x) { ri_exec_liberar(e); return NULL; }
    e->banco = (v4 *)(((unsigned long)e->banco_mem + 15) & ~(unsigned long)15);
    for (int r = 0; r < p->nimed; r++)
        for (int c = 0; c < 4; c++) { float v = p->imed[r][c]; v4 b = { v, v, v, v }; e->banco[(e->o_imed + r) * 4 + c] = b; }
    for (int i = 0; i < p->nins; i++) {
        const ri_ins_t *I = &p->ins[i];
        xins_t *X = &e->x[i];
        X->op = I->op; X->masc = I->d.masc; X->sat = I->d.sat; X->tex = I->tex; X->alvo = I->alvo; X->rel_d = I->d.rel;
        int base = 0, lim = 0;
        switch (I->d.arq) { case RI_TEMP: base = 0; lim = p->ntemp; break; case RI_SAI: base = e->o_sai; lim = p->nsai; break; default: base = 0; lim = 1; }
        X->d = base + I->d.ind; X->base_d = base; X->lim_d = base + lim;
        for (int k = 0; k < 3; k++) {
            const ri_fonte_t *f = &I->f[k];
            switch (f->arq) {
            case RI_TEMP: base = 0; lim = p->ntemp; break;
            case RI_ENT: base = e->o_ent; lim = p->nent; break;
            case RI_UNI: base = e->o_uni; lim = p->nuni; break;
            case RI_IMED: base = e->o_imed; lim = p->nimed; break;
            default: base = 0; lim = 1;
            }
            X->f[k] = base + f->ind; X->base_f[k] = base; X->lim_f[k] = base + lim;
            X->mod[k] = f->mod; X->rel[k] = f->rel;
            for (int c = 0; c < 4; c++) X->sw[k][c] = (uint8_t)RI_SW_C(f->sw, c);
            if (f->arq == RI_AMOST) X->amost = f->ind;
        }
    }
    e->nx = p->nins;
    for (int i = 0; i < p->nins; i++) {
        xins_t *X = &e->x[i];
        X->pd = &e->banco[X->d * 4];
        for (int k = 0; k < 3; k++) for (int c = 0; c < 4; c++) X->pf[k][c] = &e->banco[X->f[k] * 4 + X->sw[k][c]];
    }
    return e;
}
void ri_exec_liberar(ri_exec_t *e)
{
    if (!e) return;
    if (e->banco_mem) libera(e->banco_mem);
    if (e->x) libera(e->x);
    libera(e);
}
void ri_exec_uniformes(ri_exec_t *e, const float (*uni)[4])
{
    for (int r = 0; r < e->p->nuni; r++)
        for (int c = 0; c < 4; c++) { float v = uni[r][c]; v4 b = { v, v, v, v }; e->banco[(e->o_uni + r) * 4 + c] = b; }
}
float (*ri_exec_ent(ri_exec_t *e))[4][4] { return (float (*)[4][4])(void *)&e->banco[e->o_ent * 4]; }
float (*ri_exec_sai(ri_exec_t *e))[4][4] { return (float (*)[4][4])(void *)&e->banco[e->o_sai * 4]; }

/* ---------------------------------------------------------------- ejecución */
static inline v4 modificar(v4 a, int mod)
{
    if (mod & RI_ABS) a = (v4)((v4i)a & (v4i){ 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff });
    if (mod & RI_NEG) a = -a;
    return a;
}
static v4 ler_rel(const ri_exec_t *e, const xins_t *X, int k, int c)
{
    v4 r;
    for (int v = 0; v < 4; v++) {
        int reg = X->f[k] + e->a0[v];
        if (reg < X->base_f[k]) reg = X->base_f[k];
        if (reg >= X->lim_f[k]) reg = X->lim_f[k] - 1;
        r[v] = e->banco[reg * 4 + X->sw[k][c]][v];
    }
    return r;
}
static inline v4 ler_f(const ri_exec_t *e, const xins_t *X, int k, int c)
{
    v4 a = X->rel[k] ? ler_rel(e, X, k, c) : *X->pf[k][c];
    return X->mod[k] ? modificar(a, X->mod[k]) : a;
}
static inline v4 saturar(v4 a)
{
    v4 z = { 0, 0, 0, 0 }, u = { 1, 1, 1, 1 };
    a = (v4)(((v4i)(a > z) & (v4i)a));            /* max(a, 0) (NaN -> 0) */
    return (v4)(((v4i)(a < u) & (v4i)a) | (~(v4i)(a < u) & (v4i)u));
}
static void gravar(ri_exec_t *e, const xins_t *X, const v4 *r, int em)
{
    for (int c = 0; c < 4; c++) {
        if (!(X->masc & (1 << c))) continue;
        v4 val = X->sat ? saturar(r[c]) : r[c];
        if (X->rel_d) {
            for (int v = 0; v < 4; v++) {
                if (!(em & (1 << v))) continue;
                int reg = X->d + e->a0[v];
                if (reg < X->base_d || reg >= X->lim_d) continue;
                e->banco[reg * 4 + c][v] = val[v];
            }
            continue;
        }
        v4 *d = X->pd + c;
        if (em == 15) *d = val;
        else { v4i m = MASC_V[em]; *d = (v4)(((v4i)val & m) | ((v4i)*d & ~m)); }
    }
}

#define POR_VIA(r, a, EXPR) for (int v_ = 0; v_ < 4; v_++) { float x = (a)[v_]; (r)[v_] = (EXPR); }

int ri_exec_rodar(ri_exec_t *e, int vivo, ri_amostrar_fn amostrar, void *ctx)
{
    const xins_t *x = e->x;
    int em = vivo & 15, morto = 0, inicial = em;
    struct { int entrada, brk, cont; } laco[32]; int nl = 0;
    struct { int salvo, cond; } se[64]; int ns = 0;
    long voltas = 0;
    for (int pc = 0; pc < e->nx; pc++) {
        const xins_t *X = &x[pc];
        int op = X->op;
        if (op >= RI_IF) {
            switch (op) {
            case RI_IF: {
                int m = 0;
                if (em) { v4 a = ler_f(e, X, 0, 0); for (int v = 0; v < 4; v++) if (a[v] != 0.0f) m |= 1 << v; }
                if (ns < 64) { se[ns].salvo = em; se[ns].cond = m; ns++; }
                em &= m;
                if (!em) pc = X->alvo - 1;   /* va al ELSE o al ENDIF, que se ejecutan */
                break;
            }
            case RI_ELSE:
                if (ns) em = se[ns - 1].salvo & ~se[ns - 1].cond & ~morto;
                if (nl) em &= ~(laco[nl - 1].brk | laco[nl - 1].cont);
                if (!em) pc = X->alvo - 1;
                break;
            case RI_ENDIF:
                if (ns) { ns--; em = se[ns].salvo & ~morto; if (nl) em &= ~(laco[nl - 1].brk | laco[nl - 1].cont); }
                break;
            case RI_BGNLOOP:
                if (!em) { pc = X->alvo; break; }   /* nadie entra: salta detrás del ENDLOOP */
                if (nl < 32) { laco[nl].entrada = em; laco[nl].brk = 0; laco[nl].cont = 0; nl++; }
                break;
            case RI_ENDLOOP:
                if (!nl) break;
                em = (em | laco[nl - 1].cont) & ~(laco[nl - 1].brk | morto);
                laco[nl - 1].cont = 0;
                if (em && ++voltas < 1000000) pc = X->alvo;   /* otra vuelta (detrás del BGNLOOP) */
                else { nl--; em = laco[nl].entrada & ~morto; }
                break;
            case RI_BRK: if (nl) { laco[nl - 1].brk |= em; } em = 0; break;
            case RI_CONT: if (nl) { laco[nl - 1].cont |= em; } em = 0; break;
            case RI_KILL: morto |= em; em = 0; if (morto == inicial) return 0; break;
            case RI_FIM: return inicial & ~morto;
            }
            continue;
        }
        if (!em || op == RI_NOP) continue;
        v4 r[4];
        switch (op) {
        case RI_MOV: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) r[c] = ler_f(e, X, 0, c); break;
        case RI_ADD: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) r[c] = ler_f(e, X, 0, c) + ler_f(e, X, 1, c); break;
        case RI_MUL: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) r[c] = ler_f(e, X, 0, c) * ler_f(e, X, 1, c); break;
        case RI_MAD: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) r[c] = ler_f(e, X, 0, c) * ler_f(e, X, 1, c) + ler_f(e, X, 2, c); break;
        case RI_MIN: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) { v4 a = ler_f(e, X, 0, c), b = ler_f(e, X, 1, c); v4i m = (v4i)(a < b); r[c] = (v4)(((v4i)a & m) | ((v4i)b & ~m)); } break;
        case RI_MAX: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) { v4 a = ler_f(e, X, 0, c), b = ler_f(e, X, 1, c); v4i m = (v4i)(a > b); r[c] = (v4)(((v4i)a & m) | ((v4i)b & ~m)); } break;
        case RI_FLR: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) { v4 a = ler_f(e, X, 0, c); POR_VIA(r[c], a, f_piso(x)); } break;
        case RI_FRC: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) { v4 a = ler_f(e, X, 0, c); POR_VIA(r[c], a, x - f_piso(x)); } break;
        case RI_TRUNC: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) { v4 a = ler_f(e, X, 0, c); POR_VIA(r[c], a, f_trunc(x)); } break;
        case RI_SLT: case RI_SGE: case RI_SEQ: case RI_SNE: {
            v4 u = { 1, 1, 1, 1 };
            for (int c = 0; c < 4; c++) {
                if (!(X->masc & (1 << c))) continue;
                v4 a = ler_f(e, X, 0, c), b = ler_f(e, X, 1, c);
                v4i m = op == RI_SLT ? (v4i)(a < b) : op == RI_SGE ? (v4i)(a >= b) : op == RI_SEQ ? (v4i)(a == b) : (v4i)(a != b);
                r[c] = (v4)(m & (v4i)u);
            }
            break;
        }
        case RI_CMP: for (int c = 0; c < 4; c++) if (X->masc & (1 << c)) { v4 a = ler_f(e, X, 0, c), b = ler_f(e, X, 1, c), d = ler_f(e, X, 2, c); v4 z = { 0, 0, 0, 0 }; v4i m = (v4i)(a < z); r[c] = (v4)(((v4i)b & m) | ((v4i)d & ~m)); } break;
        case RI_DP2: case RI_DP3: case RI_DP4: {
            int n = op == RI_DP2 ? 2 : op == RI_DP3 ? 3 : 4;
            v4 s = ler_f(e, X, 0, 0) * ler_f(e, X, 1, 0);
            for (int c = 1; c < n; c++) s = s + ler_f(e, X, 0, c) * ler_f(e, X, 1, c);
            r[0] = r[1] = r[2] = r[3] = s;
            break;
        }
        case RI_RCP: { v4 a = ler_f(e, X, 0, 0); v4 u = { 1, 1, 1, 1 }; r[0] = u / a; r[1] = r[2] = r[3] = r[0]; break; }
        case RI_RSQ: { v4 a = ler_f(e, X, 0, 0); POR_VIA(r[0], a, 1.0f / f_sqrt(x < 0 ? -x : x)); r[1] = r[2] = r[3] = r[0]; break; }
        case RI_EX2: r[0] = v_ex2(ler_f(e, X, 0, 0)); r[1] = r[2] = r[3] = r[0]; break;
        case RI_LG2: r[0] = v_lg2(ler_f(e, X, 0, 0)); r[1] = r[2] = r[3] = r[0]; break;
        case RI_SIN: r[0] = v_seno(ler_f(e, X, 0, 0)); r[1] = r[2] = r[3] = r[0]; break;
        case RI_COS: { v4 a = ler_f(e, X, 0, 0) + 1.5707963267948966f; r[0] = v_seno(a); r[1] = r[2] = r[3] = r[0]; break; }
        case RI_POW: r[0] = v_ex2(ler_f(e, X, 1, 0) * v_lg2(ler_f(e, X, 0, 0))); r[1] = r[2] = r[3] = r[0]; break;
        case RI_DDX: case RI_DDY:
            for (int c = 0; c < 4; c++) {
                if (!(X->masc & (1 << c))) continue;
                v4 a = ler_f(e, X, 0, c);
                if (op == RI_DDX) { float d0 = a[1] - a[0], d1 = a[3] - a[2]; r[c] = (v4){ d0, d0, d1, d1 }; }
                else { float d0 = a[2] - a[0], d1 = a[3] - a[1]; r[c] = (v4){ d0, d1, d0, d1 }; }
            }
            break;
        case RI_ARL: {
            v4 a = ler_f(e, X, 0, 0);
            for (int v = 0; v < 4; v++) if (em & (1 << v)) { float f = f_piso(a[v]); e->a0[v] = f > 30000.0f ? 30000 : f < -30000.0f ? -30000 : (int)f; }
            continue;
        }
        case RI_TEX: case RI_TXB: case RI_TXL: {
            float coord[4][4], out[4][4];
            for (int c = 0; c < 4; c++) { v4 a = ler_f(e, X, 0, c); for (int v = 0; v < 4; v++) coord[c][v] = a[v]; }
            if (amostrar) amostrar(ctx, X->amost, X->tex, op, coord, em, e->p->tipo == 1, out);
            else memset(out, 0, sizeof out);
            for (int c = 0; c < 4; c++) r[c] = (v4){ out[c][0], out[c][1], out[c][2], out[c][3] };
            break;
        }
        default: continue;
        }
        gravar(e, X, r, em);
    }
    return inicial & ~morto;
}
