// SPDX-License-Identifier: GPL-2.0-or-later
/* r600_v3_unidad.c - prueba en el PC del sombreador de vértices de gpu3d v3 de las R6xx/R7xx (D457, «v3 en
 * R6xx/R7xx»): r63d_vs3_gerar genera el programa; aquí un DECODIFICADOR propio (campos según r600_sq.h de Mesa, no
 * las funciones del generador) lo lee palabra a palabra y un SIMULADOR lo ejecuta en coma flotante sobre vértices al
 * azar, y el resultado se compara con la cuenta de ES 1.1 hecha directamente (la misma que hace gpu3d_v3.c).
 * Comprueba también las reglas de los puertos de lectura (otra vez, con su propio código), que cada instrucción
 * transcendente cae en la ranura t, las órdenes de r63d_desenhar3 y las constantes.
 *
 *   gcc -O2 -Wall -DR600_ANFITRION -Ikernel/inc tests/r600_v3_unidad.c kernel/dispositivos/r600_3d_pacotes.c -lm
 *
 * No puede saber si la HD 4500 entiende esas palabras igual: eso lo dice la autoprueba v3 de la capa en la máquina. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "r600_3d.h"

static int falhas, casos;
#define FALHA(...) do { if (falhas < 30) { printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); } falhas++; } while (0)

static uint32_t semente = 12345;
static uint32_t azar(void) { semente = semente * 1103515245u + 12345u; return semente >> 8; }
static float fazar(float a, float b) { return a + (b - a) * (float)(azar() & 0xFFFF) / 65535.0f; }
static uint32_t fb(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float bf(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

/* ---------------------------------------------------------------- simulador */
typedef struct { const uint8_t *p; uint32_t bytes, passo; } buf_t;
static float gpr[128][4];
static float cfile[256][4];
static float saida_pos[4], saida_par[6][4];
static int exportados;

static float satf(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

static void ler_vtx(const uint32_t *w, const buf_t *bufs, int nbufs, int r7)
{
    (void)r7;
    uint32_t inst = w[0] & 0x1F, tipo = (w[0] >> 5) & 3, buf = (w[0] >> 8) & 0xFF, src = (w[0] >> 16) & 0x7F, ssx = (w[0] >> 24) & 3;
    uint32_t dst = w[1] & 0x7F, sel[4] = { (w[1] >> 9) & 7, (w[1] >> 12) & 7, (w[1] >> 15) & 7, (w[1] >> 18) & 7 };
    uint32_t fmt = (w[1] >> 22) & 0x3F, nfmt = (w[1] >> 28) & 3, desl = w[2] & 0xFFFF, mega = (w[2] >> 19) & 1;
    if (inst != 0 || tipo != 0) FALHA("lectura: instruccion %u tipo %u", inst, tipo);
    if (!mega) FALHA("lectura sin MEGA_FETCH");
    if ((int)buf >= nbufs) { FALHA("lectura del recurso %u (solo hay %d)", buf, nbufs); return; }
    uint32_t idx = (uint32_t)gpr[src][ssx];
    const buf_t *b = &bufs[buf];
    uint64_t off = (uint64_t)idx * b->passo + desl;
    float v[4] = { 0, 0, 0, 0 };
    int nc;
    switch (fmt) {
    case 0x1E: nc = 2; break;
    case 0x30: nc = 3; break;
    case 0x23: nc = 4; break;
    case 0x1A: nc = -4; break;
    default: FALHA("formato de lectura %x", fmt); return;
    }
    if (nc < 0) {
        if (nfmt != 0) FALHA("bytes sin normalizar");
        for (int k = 0; k < 4; k++) v[k] = off + k < b->bytes ? b->p[off + k] / 255.0f : 0;
    } else {
        if (nfmt == 0 && 0) FALHA("float normalizado");
        for (int k = 0; k < nc; k++) { uint32_t u = 0; if (off + 4 * k + 4 <= b->bytes) memcpy(&u, b->p + off + 4 * k, 4); v[k] = bf(u); }
    }
    for (int k = 0; k < 4; k++) {
        float x;
        if (sel[k] < 4) x = v[sel[k]];
        else if (sel[k] == 4) x = 0;
        else if (sel[k] == 5) x = 1;
        else { FALHA("dst_sel %u", sel[k]); x = 0; }
        if (sel[k] != 7) gpr[dst][k] = x;
    }
}

/* Lee un origen. sel: 0-127 registro, 248 0.0, 249 1.0, 252 0.5, 256-511 constante. */
static float origem(uint32_t sel, uint32_t ch, uint32_t neg)
{
    float x;
    if (sel < 128) x = gpr[sel][ch];
    else if (sel == 248) x = 0;
    else if (sel == 249) x = 1;
    else if (sel == 252) x = 0.5f;
    else if (sel >= 256 && sel < 512) x = cfile[sel - 256][ch];
    else { FALHA("origen %u", sel); x = 0; }
    return neg ? -x : x;
}

/* Un grupo del ALU: decodifica hasta la marca LAST, reparte por ranuras como el hardware (la transcendente en t; si
 * no, la del canal si está libre; si no, t), comprueba los puertos, calcula (lee todo antes de escribir) y escribe. */
typedef struct { int usado, op3, ranura; uint32_t op, s[3][3], dst, ch, wm, clamp; } ins_t;
static int grupo(const uint32_t *p, int *pos, int max, int r7)
{
    ins_t in[5];
    int n = 0, ocup[5] = { 0, 0, 0, 0, 0 };
    for (;;) {
        if (*pos + 2 > max || n == 5) { FALHA("grupo sin LAST"); return -1; }
        uint32_t w0 = p[*pos], w1 = p[*pos + 1];
        *pos += 2;
        ins_t *x = &in[n++];
        memset(x, 0, sizeof *x);
        x->s[0][0] = w0 & 0x1FF; x->s[0][1] = (w0 >> 10) & 3; x->s[0][2] = (w0 >> 12) & 1;
        x->s[1][0] = (w0 >> 13) & 0x1FF; x->s[1][1] = (w0 >> 23) & 3; x->s[1][2] = (w0 >> 25) & 1;
        if ((w0 >> 9) & 1 || (w0 >> 22) & 1 || ((w0 >> 26) & 7) || ((w0 >> 29) & 3)) FALHA("REL, INDEX_MODE o PRED_SEL en un origen");
        uint32_t op3 = (w1 >> 13) & 0x1F;
        x->dst = (w1 >> 21) & 0x7F; x->ch = (w1 >> 29) & 3; x->clamp = w1 >> 31;
        if ((w1 >> 28) & 1) FALHA("DST_REL");
        if ((w1 >> 18) & 7) FALHA("BANK_SWIZZLE distinto de 0");
        if (op3 >= 8) {      /* las de tres orígenes (ALU_WORD1_OP3: el código >= 8) */
            x->op3 = 1; x->op = op3;
            x->s[2][0] = w1 & 0x1FF; x->s[2][1] = (w1 >> 10) & 3; x->s[2][2] = (w1 >> 12) & 1;
            x->wm = 1;
        } else {
            x->op = r7 ? (w1 >> 7) & 0x7FF : (w1 >> 8) & 0x3FF;
            x->wm = (w1 >> 4) & 1;
            uint32_t omod = r7 ? (w1 >> 5) & 3 : (w1 >> 6) & 3;
            if (omod) FALHA("OMOD");
        }
        int trans_so = !x->op3 && (x->op == 0x6A || x->op == 0x69 || x->op == 0x66 || x->op == 0x61);
        if (trans_so) x->ranura = 4;
        else if (!ocup[x->ch]) x->ranura = (int)x->ch;
        else x->ranura = 4;
        if (ocup[x->ranura]) { FALHA("dos instrucciones en la ranura %d", x->ranura); return -1; }
        ocup[x->ranura] = 1;
        if (w0 >> 31) break;
    }
    /* puertos */
    int hw[3][4], nk = 0, ka[4], kc[4];
    for (int c = 0; c < 3; c++) for (int k = 0; k < 4; k++) hw[c][k] = -1;
    for (int i = 0; i < n; i++) {
        ins_t *x = &in[i];
        int ns = x->op3 ? 3 : (x->op == 0x19 || x->op == 0x6A) ? 1 : 2, consts = 0;
        for (int s = 0; s < ns; s++) {
            uint32_t sel = x->s[s][0], ch = x->s[s][1];
            if (sel >= 256 && sel < 512) {
                uint32_t el = r7 ? ch / 2 : ch;
                int j = 0;
                while (j < nk && !(ka[j] == (int)sel && kc[j] == (int)el)) j++;
                if (j == nk) { if (nk == (r7 ? 2 : 4)) FALHA("demasiadas constantes en un grupo"); else { ka[nk] = (int)sel; kc[nk] = (int)el; nk++; } }
            }
            if (sel >= 248) consts++;
        }
        if (x->ranura == 4 && consts > 2) FALHA("la ranura t lee mas de 2 constantes");
        for (int s = 0; s < ns; s++) {
            uint32_t sel = x->s[s][0], ch = x->s[s][1];
            if (sel >= 128) continue;
            int ciclo = x->ranura == 4 ? 2 - s : s;
            if (x->ranura == 4 && ciclo < consts) FALHA("la ranura t lee un registro antes de su constante");
            if (x->ranura < 4 && s == 1 && sel == x->s[0][0] && ch == x->s[0][1]) continue;
            if (hw[ciclo][ch] == -1) hw[ciclo][ch] = (int)sel;
            else if (hw[ciclo][ch] != (int)sel) FALHA("dos registros por el canal %u en el ciclo %d", ch, ciclo);
        }
    }
    /* calcular */
    float res[5];
    float dot = 0;
    int ndot = 0;
    for (int i = 0; i < n; i++) if (!in[i].op3 && in[i].op == 0x50) { ndot++; dot += origem(in[i].s[0][0], in[i].s[0][1], in[i].s[0][2]) * origem(in[i].s[1][0], in[i].s[1][1], in[i].s[1][2]); }
    if (ndot && ndot != 4) FALHA("DOT4 en %d ranuras", ndot);
    for (int i = 0; i < n; i++) {
        ins_t *x = &in[i];
        float a = origem(x->s[0][0], x->s[0][1], x->s[0][2]);
        float b = x->op3 || x->op != 0x19 ? origem(x->s[1][0], x->s[1][1], x->s[1][2]) : 0;
        float r;
        if (x->op3) {
            if (x->op != 0x10) { FALHA("op3 %x", x->op); r = 0; }
            else r = a * b + origem(x->s[2][0], x->s[2][1], x->s[2][2]);
        } else switch (x->op) {
            case 0x01: r = a * b; break;
            case 0x19: r = a; break;
            case 0x50: if (x->ranura == 4) FALHA("DOT4 en la ranura t"); r = dot; break;
            case 0x6A: if (x->ranura != 4) FALHA("SQRT fuera de la ranura t"); r = sqrtf(a); break;
            default: FALHA("operacion %x", x->op); r = 0;
        }
        res[i] = x->clamp ? satf(r) : r;
    }
    for (int i = 0; i < n; i++) if (in[i].wm) gpr[in[i].dst][in[i].ch] = res[i];
    return n;
}

static int exec(const uint32_t *p, int np, const buf_t *bufs, int nbufs, int r7, uint32_t idx)
{
    memset(gpr, 0x7F, sizeof gpr);          /* basura: un registro sin escribir sale enorme */
    gpr[0][0] = (float)idx; gpr[0][1] = gpr[0][2] = gpr[0][3] = 0;
    exportados = 0;
    for (int cf = 0; cf < 64; cf++) {
        if (2 * cf + 1 >= np) { FALHA("el CF se sale del programa"); return -1; }
        uint32_t w0 = p[2 * cf], w1 = p[2 * cf + 1];
        uint32_t alu_inst = (w1 >> 26) & 0xF;
        if (alu_inst == 0x8) {          /* CF_ALU */
            uint32_t ender = w0 & 0x3FFFFF, cnt = ((w1 >> 18) & 0x7F) + 1;
            if ((w0 >> 22) || (w1 & 0x3FFFF)) FALHA("KCACHE en una clausula del ALU");
            if (!(w1 >> 31)) FALHA("clausula del ALU sin barrera");
            int pos = (int)(2 * ender), fim = pos + 2 * (int)cnt;
            if (fim > np) { FALHA("clausula del ALU fuera"); return -1; }
            while (pos < fim) if (grupo(p, &pos, fim, r7) < 0) return -1;
            if (pos != fim) FALHA("la clausula no acaba en un grupo");
            continue;
        }
        uint32_t inst = (w1 >> 23) & 0x7F;
        if (inst == 0x02) {             /* VTX */
            uint32_t ender = w0, cnt = (((w1 >> 10) & 7) | (((w1 >> 19) & 1) << 3)) + 1;
            if (ender & 1) FALHA("lecturas sin alinear a 16 bytes");
            if (!(w1 >> 31)) FALHA("lecturas sin barrera");
            for (uint32_t k = 0; k < cnt; k++) {
                if ((int)(2 * ender + 4 * k + 4) > np) { FALHA("lectura fuera"); return -1; }
                ler_vtx(p + 2 * ender + 4 * k, bufs, nbufs, r7);
            }
            continue;
        }
        if (inst == 0x27 || inst == 0x28) {   /* EXPORT, EXPORT_DONE */
            uint32_t base = w0 & 0x1FFF, tipo = (w0 >> 13) & 3, g = (w0 >> 15) & 0x7F, fim = (w1 >> 21) & 1;
            uint32_t sw[4] = { w1 & 7, (w1 >> 3) & 7, (w1 >> 6) & 7, (w1 >> 9) & 7 };
            float v[4];
            for (int k = 0; k < 4; k++) v[k] = sw[k] < 4 ? gpr[g][sw[k]] : sw[k] == 4 ? 0.0f : sw[k] == 5 ? 1.0f : NAN;
            if (tipo == 1 && base == 60) memcpy(saida_pos, v, sizeof v);
            else if (tipo == 2 && base < 6) memcpy(saida_par[base], v, sizeof v);
            else FALHA("exportacion tipo %u base %u", tipo, base);
            exportados++;
            if (fim) return 0;
            continue;
        }
        FALHA("instruccion de CF %x / %x", inst, alu_inst);
        return -1;
    }
    FALHA("el programa no acaba");
    return -1;
}

/* ---------------------------------------------------------------- la referencia (ES 1.1 en float) */
static float fila(const uint32_t *m, int l, const float v[4])
{
    return bf(m[l]) * v[0] + bf(m[4 + l]) * v[1] + bf(m[8 + l]) * v[2] + bf(m[12 + l]) * v[3];
}

static int perto(float a, float b) { float d = fabsf(a - b), m = fabsf(a) > fabsf(b) ? fabsf(a) : fabsf(b); return d <= 1e-4f + 2e-5f * m; }

static void caso(int r7)
{
    r63d_vs3_desc_t d;
    memset(&d, 0, sizeof d);
    d.pos_fmt = (uint8_t)(2 + azar() % 3);
    static const uint8_t cf[3] = { G3D_AT_NADA, G3D_AT_UBYTE4N, G3D_AT_FLOAT4 };
    d.cor_fmt = cf[azar() % 3];
    d.unidades = (uint8_t)(azar() % 5);
    for (int u = 0; u < d.unidades; u++) { static const uint8_t tf[4] = { G3D_AT_NADA, 2, 3, 4 }; d.tc_fmt[u] = tf[azar() % 4]; }
    d.neblina = (uint8_t)(azar() % 3);
    d.tex_mat = (uint8_t)(azar() & ((1u << d.unidades) - 1u));
    static r63d_vs3_t vs;
    r63d_vs3_gerar(&vs, &d, r7 != 0);
    casos++;
    if (vs.erro) { FALHA("no se genero (pos %d cor %d unidades %d neblina %d mat %x)", d.pos_fmt, d.cor_fmt, d.unidades, d.neblina, d.tex_mat); return; }
    /* transformación al azar */
    gpu3d_transf_t t;
    memset(&t, 0, sizeof t);
    t.versao = 3;
    for (int i = 0; i < 16; i++) { t.mvp[i] = fb(fazar(-2, 2)); t.mv[i] = fb(fazar(-2, 2)); for (int u = 0; u < 4; u++) t.tex[u][i] = fb(fazar(-2, 2)); }
    t.cor = azar();
    t.neb_cor = azar() & 0xFFFFFF;
    t.neb_a = fb(fazar(0.5f, 2)); t.neb_b = fb(fazar(0.05f, 0.5f));
    for (int u = 0; u < 4; u++) for (int k = 0; k < 4; k++) t.tc_const[u][k] = fb(fazar(-3, 3));
    uint32_t c[R63D_VS3_NCONST * 4];
    r63d_vs3_constantes(c, &t);
    for (int i = 0; i < R63D_VS3_NCONST; i++) for (int k = 0; k < 4; k++) cfile[i][k] = bf(c[i * 4 + k]);
    for (int i = R63D_VS3_NCONST; i < 256; i++) for (int k = 0; k < 4; k++) cfile[i][k] = NAN;
    /* un búfer intercalado con todo: pos (16), color (16: bytes o floats), 4 juegos (16 cada uno) */
    enum { PASSO = 96, NV = 40 };
    static uint8_t mem[PASSO * NV];
    for (int i = 0; i < PASSO * NV; i += 4) { uint32_t v = fb(fazar(-4, 4)); memcpy(mem + i, &v, 4); }
    for (int i = 0; i < NV; i++) {          /* el color en bytes en los 4 primeros del hueco del color, si toca */
        if (d.cor_fmt == G3D_AT_UBYTE4N) for (int k = 0; k < 4; k++) mem[i * PASSO + 16 + k] = (uint8_t)azar();
        if (d.cor_fmt == G3D_AT_FLOAT4) for (int k = 0; k < 4; k++) { uint32_t v = fb(fazar(-0.3f, 1.3f)); memcpy(mem + i * PASSO + 16 + 4 * k, &v, 4); }
    }
    buf_t bufs[R63D_VS3_BUFS];
    for (int k = 0; k < vs.lecturas; k++) {
        uint32_t desl = vs.atr[k] == 0 ? 0 : vs.atr[k] == 1 ? 16 : 32 + 16 * (vs.atr[k] - 2);
        bufs[k] = (buf_t){ mem + desl, PASSO * NV - desl, PASSO };
    }
    for (uint32_t v = 0; v < NV; v++) {
        if (exec(vs.p, vs.n, bufs, vs.lecturas, r7, v) < 0) return;
        if (exportados != 7) FALHA("%d exportaciones", exportados);
        const uint8_t *m = mem + v * PASSO;
        float p[4] = { 0, 0, 0, 1 };
        for (int k = 0; k < d.pos_fmt; k++) { uint32_t u; memcpy(&u, m + 4 * k, 4); p[k] = bf(u); }
        float clip[4];
        for (int l = 0; l < 4; l++) clip[l] = fila(t.mvp, l, p);
        float cor[4];
        if (d.cor_fmt == G3D_AT_NADA) { cor[0] = ((t.cor >> 16) & 255) / 255.0f; cor[1] = ((t.cor >> 8) & 255) / 255.0f; cor[2] = (t.cor & 255) / 255.0f; cor[3] = (t.cor >> 24) / 255.0f; }
        else if (d.cor_fmt == G3D_AT_UBYTE4N) for (int k = 0; k < 4; k++) cor[k] = m[16 + k] / 255.0f;
        else for (int k = 0; k < 4; k++) { uint32_t u; memcpy(&u, m + 16 + 4 * k, 4); cor[k] = satf(bf(u)); }
        float esp[4] = { 0, 0, 0, 1 };
        if (d.neblina) {
            float e[3];
            for (int l = 0; l < 3; l++) e[l] = fila(t.mv, l, p);
            float dist = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
            float f = satf(bf(t.neb_a) - dist * bf(t.neb_b));
            if (d.neblina == R63D_V3_NEB_MEZCLADA) {
                for (int k = 0; k < 3; k++) { cor[k] *= f; esp[k] = ((t.neb_cor >> (16 - 8 * k)) & 255) / 255.0f * (1 - f); }
            } else esp[3] = f;
        }
        for (int l = 0; l < 4; l++) if (!perto(saida_pos[l], clip[l])) FALHA("r%d pos[%d] %g en vez de %g (pos %d)", 7 - !r7, l, saida_pos[l], clip[l], d.pos_fmt);
        for (int k = 0; k < 4; k++) if (fabsf(saida_par[0][k] - cor[k]) > 2e-3f) FALHA("r%d color[%d] %g en vez de %g (formato %d, niebla %d)", 7 - !r7, k, saida_par[0][k], cor[k], d.cor_fmt, d.neblina);
        for (int k = 0; k < 4; k++) if (fabsf(saida_par[1][k] - esp[k]) > 2e-3f) FALHA("r%d especular[%d] %g en vez de %g (niebla %d)", 7 - !r7, k, saida_par[1][k], esp[k], d.neblina);
        for (int u = 0; u < 4; u++) {
            float q[4] = { 0, 0, 0, 1 }, o[4];
            if (u < d.unidades) {
                if (d.tc_fmt[u] == G3D_AT_NADA) for (int k = 0; k < 4; k++) q[k] = bf(t.tc_const[u][k]);
                else for (int k = 0; k < d.tc_fmt[u]; k++) { uint32_t w; memcpy(&w, m + 32 + 16 * u + 4 * k, 4); q[k] = bf(w); }
                if (d.tex_mat & (1u << u)) for (int l = 0; l < 4; l++) o[l] = fila(t.tex[u], l, q);
                else memcpy(o, q, sizeof o);
            } else { o[0] = o[1] = o[2] = 0; o[3] = 1; }
            for (int k = 0; k < 4; k++) if (!perto(saida_par[2 + u][k], o[k])) FALHA("r%d juego %d[%d] %g en vez de %g (formato %d, matriz %d)", 7 - !r7, u, k, saida_par[2 + u][k], o[k], u < d.unidades ? d.tc_fmt[u] : -1, (d.tex_mat >> u) & 1);
        }
    }
}

/* ---------------------------------------------------------------- órdenes */
static void ordens(void)
{
    uint32_t b[256];
    r6_lote_t l;
    r63d_vbuf_t vb[2] = { { 0x10000100u, 4096, 24 }, { 0x10000110u, 4080, 24 } };
    r63d_lote(&l, b, 256);
    r63d_desenhar3(&l, vb, 2, G3D_PRIM_TIRA, 30, false, 0);
    /* SET_RESOURCE de 7 palabras en (160 + k) * 7 */
    if (l.cheio || b[0] != (0xC0000000u | (7u << 16) | (0x6Du << 8)) || b[1] != 160u * 7u || b[2] != 0x10000100u || b[3] != 4095u ||
        b[4] != (24u << 8) || b[8] != 0xC0000000u) FALHA("recurso de vertices 0: %08x %08x %08x %08x %08x", b[0], b[1], b[2], b[3], b[4]);
    if (b[10] != 161u * 7u || b[11] != 0x10000110u) FALHA("recurso de vertices 1");
    int i = 18;
    if (b[i] != (0xC0000000u | (1u << 16) | (0x68u << 8)) || b[i + 1] != (0x8958u - 0x8000u) / 4 || b[i + 2] != 6u) FALHA("VGT_PRIMITIVE_TYPE de una tira");
    i += 3;
    if (b[i] != (0xC0000000u | (0x2Au << 8)) || b[i + 1] != 0) FALHA("INDEX_TYPE");
    i += 4;   /* NUM_INSTANCES */
    if (b[i] != (0xC0000000u | (1u << 16) | (0x2Du << 8)) || b[i + 1] != 30u || b[i + 2] != 2u) FALHA("DRAW_INDEX_AUTO");
    r63d_lote(&l, b, 256);
    r63d_desenhar3(&l, vb, 1, G3D_PRIM_ABANICO, 12, true, 0x10002002u);
    i = 9;
    if (b[i + 2] != 5u) FALHA("abanico");
    i += 3 + 2 + 2;
    if (b[i] != (0xC0000000u | (3u << 16) | (0x2Bu << 8)) || b[i + 1] != 0x10002002u || b[i + 2] != 0 || b[i + 3] != 12u || b[i + 4] != 0)
        FALHA("DRAW_INDEX: %08x %08x %08x %08x %08x", b[i], b[i + 1], b[i + 2], b[i + 3], b[i + 4]);
    /* rasterizador */
    r63d_lote(&l, b, 256);
    r63d_v3_entrar(&l);
    if (b[1] != (0x28810u - 0x28000u) / 4 || b[2] != (1u << 24) || b[3] != (1u << 2) || b[4] != 0x43Fu) FALHA("r63d_v3_entrar");
    r63d_lote(&l, b, 256);
    r63d_v3_sair(&l);
    if (b[2] != (1u << 16) || b[3] != (1u << 2) || b[4] != 0x300u) FALHA("r63d_v3_sair");
    gpu3d_transf_t t;
    memset(&t, 0, sizeof t);
    for (int k = 0; k < 3; k++) { t.vp_escala[k] = 100u + (uint32_t)k; t.vp_desloc[k] = 200u + (uint32_t)k; }
    r63d_lote(&l, b, 256);
    r63d_v3_vista(&l, &t);
    if (b[1] != (0x2843Cu - 0x28000u) / 4 || b[2] != 100 || b[3] != 200 || b[4] != 101 || b[5] != 201 || b[6] != 102 || b[7] != 202) FALHA("la vista");
    if (r63d_v3_descarte(G3D_DESC_NADA, false) != 4 || r63d_v3_descarte(G3D_DESC_POS, false) != 5 || r63d_v3_descarte(G3D_DESC_NEG, false) != 6 ||
        r63d_v3_descarte(G3D_DESC_POS, true) != 6) FALHA("descarte");
    /* constantes al 256 del constant file */
    uint32_t c[R63D_VS3_NCONST * 4];
    for (int k = 0; k < R63D_VS3_NCONST * 4; k++) c[k] = (uint32_t)k;
    static uint32_t bb[512];
    r63d_lote(&l, bb, 512);
    r63d_vs3_consts_emitir(&l, c);
    if (l.cheio || bb[0] != (0xC0000000u | ((uint32_t)R63D_VS3_NCONST * 4u << 16) | (0x6Au << 8)) || bb[1] != 1024u || bb[2] != 0 || bb[121] != 119u)
        FALHA("SET_ALU_CONST: %08x %08x", bb[0], bb[1]);
}

int main(void)
{
    ordens();
    for (int i = 0; i < 600; i++) caso(i & 1);
    printf("r600 v3: %d programas generados y simulados (40 vertices cada uno), %d fallos%s\n", casos, falhas, falhas ? "" : ": todo bien");
    return falhas ? 1 : 0;
}
