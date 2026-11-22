// SPDX-License-Identifier: GPL-2.0-or-later
/* intel9_3d_pacotes.c - el 3D de Intel Gen9-12 (D438): ensamblador mínimo de las EU, los dos
 * sombreadores de píxeles, las órdenes de una tanda de dibujo y los workarounds. Lógica pura
 * (sin registros ni memoria de la GPU): se prueba también en el PC (tests/intel9_3d_unidad.c).
 * Referencias y forma del cauce: intel9_3d.h. */
#include "intel9_3d.h"

/* ------------------------------------------------------------------ utilidades */

/* Pone los bits [hi:lo] de una instrucción de 128 bits (4 palabras). */
static void bits(uint32_t *w, unsigned hi, unsigned lo, uint32_t v)
{
    for (unsigned b = lo; b <= hi; b++) {
        uint32_t m = 1u << (b & 31u);
        if ((v >> (b - lo)) & 1u) w[b >> 5] |= m;
        else w[b >> 5] &= ~m;
    }
}

/* Los bits IEEE-754 de un entero pequeño (sin coma flotante: el kernel no la tiene). */
static uint32_t fbits(int v)
{
    if (v == 0) return 0;
    uint32_t s = v < 0 ? 0x80000000u : 0, m = (uint32_t)(v < 0 ? -v : v);
    int e = 31;
    while (!(m & 0x80000000u)) { m <<= 1; e--; }
    return s | (uint32_t)(e + 127) << 23 | ((m >> 8) & 0x007FFFFFu);
}

/* ------------------------------------------------------------------ ensamblador de las EU */

void i9eu_iniciar(i9eu_t *e, uint32_t *p, int max_inst, int gen)
{
    e->p = p; e->n = 0; e->max = max_inst; e->gen = gen; e->transbordou = false;
}

/* Opcodes de la ISA (opcode_descs de brw_eu.c): en Gen12 cambian MOV y NOP. */
uint32_t i9eu_opcode(int op, int gen)
{
    switch (op) {
    case I9_OP_MOV:   return gen >= 12 ? 97u : 1u;
    case I9_OP_ADD:   return 64u;
    case I9_OP_MUL:   return 65u;
    case I9_OP_SEND:  return 49u;
    case I9_OP_SENDC: return 50u;
    case I9_OP_NOP:   return gen >= 12 ? 96u : 126u;
    case I9_OP_MATH:  return 56u;
    default:          return 0u;
    }
}

/* Tipos de la ISA (gfx8_hw_type, gfx11_hw_type y gfx12 de brw_reg_type.c): iguales para
 * registros e inmediatos en lo que se usa aquí. */
static uint32_t tipo_hw(int tipo, int gen)
{
    if (gen >= 12) return tipo == I9_F ? 0xAu : tipo == I9_D ? 0x6u : tipo == I9_UW ? 0x1u : 0x2u;
    if (gen >= 11) return tipo == I9_F ? 9u : tipo == I9_D ? 1u : tipo == I9_UW ? 2u : 0u;
    return tipo == I9_F ? 7u : tipo == I9_D ? 1u : tipo == I9_UW ? 2u : 0u;
}

/* Códigos de la región (BRW_VERTICAL_STRIDE_*, BRW_WIDTH_*, BRW_HORIZONTAL_STRIDE_*). */
static uint32_t cod_vs(int v) { return v == 0 ? 0u : v == 1 ? 1u : v == 2 ? 2u : v == 4 ? 3u : v == 8 ? 4u : 5u; }
static uint32_t cod_w(int v)  { return v == 1 ? 0u : v == 2 ? 1u : v == 4 ? 2u : v == 8 ? 3u : 4u; }
static uint32_t cod_hs(int v) { return v == 0 ? 0u : v == 1 ? 1u : v == 2 ? 2u : 3u; }

static uint32_t *nova(i9eu_t *e)
{
    if (e->n >= e->max) { e->transbordou = true; return NULL; }
    uint32_t *w = e->p + 4 * e->n++;
    w[0] = w[1] = w[2] = w[3] = 0;
    return w;
}

/* Formato de Gen8-11 (brw_inst.h, columnas «4+»/«8+»). */
static void alu_gen8(i9eu_t *e, uint32_t *w, int op, bool sat, i9_op_t d, i9_op_t a, i9_op_t b)
{
    int g = e->gen;
    bits(w, 6, 0, i9eu_opcode(op, g));
    bits(w, 23, 21, 3);                         /* 8 canales */
    if (sat) bits(w, 31, 31, 1);
    if (op == I9_OP_NOP) return;
    bits(w, 36, 35, d.arq); bits(w, 40, 37, tipo_hw(d.tipo, g));
    bits(w, 52, 48, d.sub); bits(w, 60, 53, d.nr); bits(w, 62, 61, 1);
    bits(w, 42, 41, a.arq); bits(w, 46, 43, tipo_hw(a.tipo, g));
    if (a.arq == I9_IMM) {
        w[3] = a.imm;
        bits(w, 90, 89, I9_ARF); bits(w, 94, 91, tipo_hw(a.tipo, g));   /* como brw_set_src0 */
        return;
    }
    bits(w, 68, 64, a.sub); bits(w, 76, 69, a.nr);
    if (a.neg) bits(w, 78, 78, 1);              /* src0_negate (brw_inst.h, Gen8-11) */
    bits(w, 81, 80, cod_hs(a.hs)); bits(w, 84, 82, cod_w(a.w)); bits(w, 88, 85, cod_vs(a.vs));
    if (op == I9_OP_MOV) return;
    bits(w, 90, 89, b.arq); bits(w, 94, 91, tipo_hw(b.tipo, g));
    if (b.arq == I9_IMM) { w[3] = b.imm; return; }
    bits(w, 100, 96, b.sub); bits(w, 108, 101, b.nr);
    if (b.neg) bits(w, 110, 110, 1);            /* src1_negate */
    bits(w, 113, 112, cod_hs(b.hs)); bits(w, 116, 114, cod_w(b.w)); bits(w, 120, 117, cod_vs(b.vs));
}

/* Formato de Gen12 (columna «12+» de brw_inst.h): otros sitios, un bit de fichero y SWSB. */
static void alu_gen12(i9eu_t *e, uint32_t *w, int op, bool sat, i9_op_t d, i9_op_t a, i9_op_t b, uint8_t swsb)
{
    int g = e->gen;
    bits(w, 6, 0, i9eu_opcode(op, g));
    bits(w, 15, 8, swsb);
    bits(w, 18, 16, 3);
    if (sat) bits(w, 34, 34, 1);
    if (op == I9_OP_NOP) return;
    bits(w, 39, 36, tipo_hw(d.tipo, g)); bits(w, 50, 50, d.arq == I9_GRF);
    bits(w, 49, 48, 1); bits(w, 55, 51, d.sub); bits(w, 63, 56, d.nr);
    bits(w, 43, 40, tipo_hw(a.tipo, g));
    if (a.arq == I9_IMM) { bits(w, 46, 46, 1); w[3] = a.imm; return; }
    bits(w, 66, 66, a.arq == I9_GRF); bits(w, 71, 67, a.sub); bits(w, 79, 72, a.nr);
    bits(w, 65, 64, cod_hs(a.hs)); bits(w, 83, 81, cod_w(a.w)); bits(w, 87, 84, cod_vs(a.vs));
    if (op == I9_OP_MOV) return;
    bits(w, 91, 88, tipo_hw(b.tipo, g));
    if (b.arq == I9_IMM) { bits(w, 47, 47, 1); w[3] = b.imm; return; }
    bits(w, 98, 98, b.arq == I9_GRF); bits(w, 103, 99, b.sub); bits(w, 111, 104, b.nr);
    bits(w, 97, 96, cod_hs(b.hs)); bits(w, 115, 113, cod_w(b.w)); bits(w, 119, 116, cod_vs(b.vs));
}

void i9eu_alu(i9eu_t *e, int op, bool sat, i9_op_t dst, i9_op_t s0, i9_op_t s1, uint8_t swsb)
{
    uint32_t *w = nova(e);
    if (!w) return;
    if (e->gen >= 12) alu_gen12(e, w, op, sat, dst, s0, s1, swsb);
    else alu_gen8(e, w, op, sat, dst, s0, s1);
}

/* D457: MATH de una fuente en Gen8-11 (brw_inst.h: la función en los bits 27:24, donde va cond_modifier; src1 nulo,
 * como gfx6_math en brw_fs_generator con brw_null_reg()). */
void i9eu_math(i9eu_t *e, uint32_t funcao, i9_op_t dst, i9_op_t src)
{
    if (e->gen >= 12) { e->transbordou = true; return; }
    uint32_t *w = nova(e);
    if (!w) return;
    alu_gen8(e, w, I9_OP_MATH, false, dst, src, i9_nulo(I9_F));
    bits(w, 27, 24, funcao);
}

/* D457: Mask Control = NoMask en la última instrucción (bit 34 en Gen6-11; FF(mask_control) de brw_inst.h). */
void i9eu_nomask(i9eu_t *e)
{
    if (e->n < 1 || e->gen >= 12) { if (e->gen >= 12) e->transbordou = true; return; }
    bits(e->p + 4 * (e->n - 1), 34, 34, 1);
}

/* SEND/SENDC: Gen9-11 con el descriptor inmediato en src1 (brw_set_desc_ex: src1 IMM:UD y el
 * descriptor extendido a 0, que deja la región de src0 a <0;1,0>, como el compilador de Mesa);
 * Gen12 con el descriptor repartido (brw_inst_set_send_desc) y src1 nulo (sin segunda parte). */
void i9eu_send(i9eu_t *e, int op, int sfid, i9_op_t dst, uint8_t src0_nr, uint32_t desc, bool eot, uint8_t swsb)
{
    uint32_t *w = nova(e);
    if (!w) return;
    int g = e->gen;
    bits(w, 6, 0, i9eu_opcode(op, g));
    if (g >= 12) {
        bits(w, 15, 8, swsb);
        bits(w, 18, 16, 3);
        if (eot) bits(w, 34, 34, 1);
        bits(w, 50, 50, dst.arq == I9_GRF); bits(w, 63, 56, dst.nr);
        bits(w, 66, 66, 1); bits(w, 79, 72, src0_nr);
        bits(w, 98, 98, 0); bits(w, 111, 104, 0);                 /* src1 = null: sin segunda parte (ex_mlen 0) */
        bits(w, 95, 92, (uint32_t)sfid);
        bits(w, 123, 122, desc >> 30); bits(w, 71, 67, desc >> 25); bits(w, 55, 51, desc >> 20);
        bits(w, 121, 113, desc >> 11); bits(w, 91, 81, desc);
        return;
    }
    bits(w, 23, 21, 3);
    bits(w, 27, 24, (uint32_t)sfid);
    bits(w, 36, 35, dst.arq); bits(w, 40, 37, tipo_hw(I9_UW, g));
    bits(w, 60, 53, dst.nr); bits(w, 62, 61, 1);
    bits(w, 42, 41, I9_GRF); bits(w, 46, 43, tipo_hw(sfid == I9_SFID_RENDER ? I9_F : I9_UD, g));
    bits(w, 76, 69, src0_nr);
    bits(w, 90, 89, I9_IMM); bits(w, 94, 91, tipo_hw(I9_UD, g));
    w[3] = (desc & 0x7FFFFFFFu) | (eot ? 0x80000000u : 0u);
}

/* ------------------------------------------------------------------ los sombreadores */

#define G_B1     2      /* baricéntrica b1 (8 floats) */
#define G_B2     3
#define G_TMP    20
#define G_COR    10     /* color interpolado: g10-g13 (R, G, B, A) */
#define G_ESP    14     /* especular: g14-g16 */
#define G_UV     30     /* u, v: g30-g31 (la carga del mensaje del muestreador) */
#define G_TEX    32     /* lo que devuelve el muestreador: g32-g35 */
#define G_SAIDA  120    /* el RT write con EOT sale de g112-g127 */

/* El valor de la componente «c» del atributo «k» en el píxel:
 *   v0 + b1 * (v1 - v0) + b2 * (v2 - v0)
 * (LINTERP de Mesa sin PLN: interp[3] + delta_x * interp[0] + delta_y * interp[1]). */
static void interp_t(i9eu_t *e, int k, int c, uint8_t dst, uint8_t tmp)
{
    uint8_t r = (uint8_t)(I9_PS_GRF_SETUP + 2 * k + c / 2), s = (uint8_t)((c & 1) * 16);
    uint8_t sw = I9_SWSB_REG(1);
    i9eu_alu(e, I9_OP_MUL, false, i9_vec(dst, I9_F), i9_vec(G_B1, I9_F), i9_esc(r, s, I9_F), sw);
    i9eu_alu(e, I9_OP_ADD, false, i9_vec(dst, I9_F), i9_vec(dst, I9_F), i9_esc(r, (uint8_t)(s + 12), I9_F), sw);
    i9eu_alu(e, I9_OP_MUL, false, i9_vec(tmp, I9_F), i9_vec(G_B2, I9_F), i9_esc(r, (uint8_t)(s + 4), I9_F), sw);
    i9eu_alu(e, I9_OP_ADD, false, i9_vec(dst, I9_F), i9_vec(dst, I9_F), i9_vec(tmp, I9_F), sw);
}
static void interp(i9eu_t *e, int k, int c, uint8_t dst) { interp_t(e, k, c, dst, G_TMP); }

int i9_ps_gerar(uint32_t *p, int gen, bool textura)
{
    if (gen != 9 && gen != 11 && gen != 12) return 0;
    i9eu_t e;
    i9eu_iniciar(&e, p, I9_PS_MAX_INST, gen);
    i9_op_t z = i9_nulo(I9_F);
    if (textura) {
        /* Primero u, v y el mensaje; el color y el especular se interpolan mientras llega. */
        interp(&e, 2, 0, G_UV);
        interp(&e, 2, 1, G_UV + 1);
        i9eu_send(&e, I9_OP_SEND, I9_SFID_SAMPLER, i9_vec(G_TEX, I9_UW), G_UV, i9_desc_sample_simd8(1, 0), false,
                  I9_SWSB_REG_SET(1, 0));
    }
    for (int c = 0; c < 4; c++) interp(&e, 0, c, (uint8_t)(G_COR + c));
    for (int c = 0; c < 3; c++) interp(&e, 1, c, (uint8_t)(G_ESP + c));
    if (textura)
        for (int c = 0; c < 4; c++)   /* la primera espera al testigo del muestreador ($0.dst) */
            i9eu_alu(&e, I9_OP_MUL, false, i9_vec((uint8_t)(G_COR + c), I9_F), i9_vec((uint8_t)(G_COR + c), I9_F),
                     i9_vec((uint8_t)(G_TEX + c), I9_F), c == 0 ? I9_SWSB_REG_SET(1, 0) : I9_SWSB_REG(1));
    for (int c = 0; c < 3; c++)
        i9eu_alu(&e, I9_OP_ADD, true, i9_vec((uint8_t)(G_SAIDA + c), I9_F), i9_vec((uint8_t)(G_COR + c), I9_F),
                 i9_vec((uint8_t)(G_ESP + c), I9_F), I9_SWSB_REG(1));
    i9eu_alu(&e, I9_OP_MOV, true, i9_vec(G_SAIDA + 3, I9_F), i9_vec(G_COR + 3, I9_F), z, I9_SWSB_REG(1));
    i9eu_send(&e, I9_OP_SENDC, I9_SFID_RENDER, i9_nulo(I9_UW), G_SAIDA, i9_desc_rt_simd8(0), true, I9_SWSB_REG(1));
    return e.transbordou ? 0 : e.n;
}

/* ------------------------------------------------------------------ el sombreador de v2 (D453) */

/* Los bits IEEE-754 de n / d (d > 0), redondeados al más próximo. */
static uint32_t fbits_q(int32_t n, int32_t d)
{
    if (!n || d <= 0) return 0;
    uint32_t sgn = n < 0 ? 0x80000000u : 0u;
    uint64_t num = (uint64_t)(n < 0 ? -(int64_t)n : n), den = (uint64_t)d;
    int ex = 0;
    while (num >= 2u * den) { den <<= 1; ex++; }
    while (num < den) { num <<= 1; ex--; }
    uint64_t m = (num << 23) / den, r = (num << 23) % den;
    if (2u * r >= den) m++;
    if (m >= (1u << 24)) { m >>= 1; ex++; }
    return sgn | (uint32_t)(ex + 127) << 23 | (uint32_t)(m & 0x7FFFFFu);
}

#define P2_P    16      /* color del vértice: g16-g19 */
#define P2_S    20      /* especular: g20-g22; factor de niebla g23 */
#define P2_F    23
#define P2_TC   24      /* (s, t) de la unidad u: g24 + 2u, g25 + 2u */
#define P2_T    32      /* lo que devuelve el muestreador u: g32 + 4u .. g35 + 4u */
#define P2_R0   48      /* resultados de las unidades, alternando: g48-g51 y g52-g55 */
#define P2_R1   52
#define P2_A    56      /* argumentos: g56 + 4k + componente */
#define P2_TMP  68      /* temporales: g68-g76 */
#define P2_ACC  75
#define P2_TI   76      /* temporal de la interpolación */

static i9_op_t v(uint8_t r) { return i9_vec(r, I9_F); }
static void op2(i9eu_t *e, int op, bool sat, uint8_t d, i9_op_t a, i9_op_t b) { i9eu_alu(e, op, sat, v(d), a, b, I9_SWSB_REG(1)); }

/* Byte «k» (0 R, 1 G, 2 B, 3 A) de un 0xAARRGGBB. */
static uint32_t byte_k(uint32_t c, int k) { return (c >> (k == 3 ? 24 : 16 - 8 * k)) & 0xFFu; }

/* Un argumento de la unidad u: la componente «comp» de la fuente f con el operando o. Si hace falta
 * (constante, o 1 - x), lo deja en «tmp». */
static i9_op_t arg2(i9eu_t *e, const i9_unid_t *un, int u, uint8_t pv, int f, int o, int comp, uint8_t tmp)
{
    int k = (o == 2 || o == 3) ? 3 : comp;                 /* G3D_OPD_ALFA, UM_MENOS_ALFA: el alfa de la fuente */
    bool inv = o == 1 || o == 3;                           /* G3D_OPD_UM_MENOS_* */
    if (f == 1) {                                          /* G3D_FT_CONSTANTE: inmediato */
        uint32_t b = byte_k(un->constante, k);
        op2(e, I9_OP_MOV, false, tmp, i9_imf(fbits_q((int32_t)(inv ? 255u - b : b), 255)), i9_nulo(I9_F));
        return v(tmp);
    }
    uint8_t r = (uint8_t)((f == 0 ? P2_T + 4 * u : f == 2 ? P2_P : pv) + k);   /* TEXTURA, PRIMARIA, ANTERIOR */
    if (!inv) return v(r);
    op2(e, I9_OP_ADD, false, tmp, i9_neg(v(r)), i9_imf(0x3F800000u));          /* 1 - x */
    return v(tmp);
}

static int n_args(int f) { return f == 0 ? 1 : f == 4 ? 3 : 2; }

/* Una componente de una unidad: func(args) en d, saturado y por la escala. */
static void canal(i9eu_t *e, const i9_unid_t *un, int u, uint8_t pv, uint8_t d, int comp, bool alfa)
{
    int f = alfa ? un->alfa_func : un->rgb_func;
    const uint8_t *fo = alfa ? un->alfa_fonte : un->rgb_fonte, *od = alfa ? un->alfa_opd : un->rgb_opd;
    i9_op_t a[3];
    for (int k = 0; k < n_args(f); k++) a[k] = arg2(e, un, u, pv, fo[k], od[k], comp, (uint8_t)(P2_A + 4 * k + comp));
    uint8_t t = (uint8_t)(P2_TMP + comp);
    switch (f) {
    case 0: op2(e, I9_OP_MOV, false, d, a[0], i9_nulo(I9_F)); break;                  /* SUBSTITUIR */
    case 1: op2(e, I9_OP_MUL, false, d, a[0], a[1]); break;                          /* MODULAR */
    case 2: op2(e, I9_OP_ADD, false, d, a[0], a[1]); break;                          /* SOMAR */
    case 3: op2(e, I9_OP_ADD, false, d, a[0], a[1]);                                 /* SOMAR_SINAL */
            op2(e, I9_OP_ADD, false, d, v(d), i9_imf(0xBF000000u)); break;
    case 4: op2(e, I9_OP_ADD, false, t, a[0], i9_neg(a[1]));                         /* INTERPOLAR: (a0 - a1) a2 + a1 */
            op2(e, I9_OP_MUL, false, t, v(t), a[2]);
            op2(e, I9_OP_ADD, false, d, v(t), a[1]); break;
    default: op2(e, I9_OP_ADD, false, d, a[0], i9_neg(a[1])); break;                /* SUBTRAIR */
    }
    int esc = alfa ? un->alfa_escala : un->rgb_escala;
    if (esc == 2 || esc == 4) op2(e, I9_OP_MUL, true, d, v(d), i9_imf(esc == 2 ? 0x40000000u : 0x40800000u));
    else op2(e, I9_OP_MOV, true, d, v(d), i9_nulo(I9_F));
}

/* DOT3: 4 x suma((a0 - 0,5)(a1 - 0,5)) en R, G y B (y A con DOT3_RGBA), por la escala. */
static void dot3(i9eu_t *e, const i9_unid_t *un, int u, uint8_t pv, uint8_t d, bool rgba)
{
    for (int c = 0; c < 3; c++) {
        i9_op_t a0 = arg2(e, un, u, pv, un->rgb_fonte[0], un->rgb_opd[0], c, (uint8_t)(P2_A + c));
        i9_op_t a1 = arg2(e, un, u, pv, un->rgb_fonte[1], un->rgb_opd[1], c, (uint8_t)(P2_A + 4 + c));
        op2(e, I9_OP_ADD, false, (uint8_t)(P2_TMP + c), a0, i9_imf(0xBF000000u));
        op2(e, I9_OP_ADD, false, (uint8_t)(P2_TMP + 4 + c), a1, i9_imf(0xBF000000u));
    }
    op2(e, I9_OP_MUL, false, P2_ACC, v(P2_TMP), v(P2_TMP + 4));
    for (int c = 1; c < 3; c++) {
        op2(e, I9_OP_MUL, false, P2_TMP + 3, v((uint8_t)(P2_TMP + c)), v((uint8_t)(P2_TMP + 4 + c)));
        op2(e, I9_OP_ADD, false, P2_ACC, v(P2_ACC), v(P2_TMP + 3));
    }
    int esc = un->rgb_escala == 2 || un->rgb_escala == 4 ? un->rgb_escala : 1;
    op2(e, I9_OP_MUL, true, d, v(P2_ACC), i9_imf(fbits_q(4 * esc, 1)));
    op2(e, I9_OP_MOV, false, (uint8_t)(d + 1), v(d), i9_nulo(I9_F));
    op2(e, I9_OP_MOV, false, (uint8_t)(d + 2), v(d), i9_nulo(I9_F));
    if (rgba) op2(e, I9_OP_MOV, false, (uint8_t)(d + 3), v(d), i9_nulo(I9_F));
}

int i9_ps2_gerar(uint32_t *p, int max_inst, int gen, const i9_ps2_t *c)
{
    if ((gen != 9 && gen != 11) || c->n > 4 || max_inst > I9_PS2_MAX_INST) return 0;
    i9eu_t e;
    i9eu_iniciar(&e, p, max_inst, gen);
    /* Primero las coordenadas y los mensajes al muestreador; lo demás se interpola mientras llegan. */
    for (int u = 0; u < c->n; u++) {
        if (!c->u[u].ativa) continue;
        interp_t(&e, 2 + u, 0, (uint8_t)(P2_TC + 2 * u), P2_TI);
        interp_t(&e, 2 + u, 1, (uint8_t)(P2_TC + 2 * u + 1), P2_TI);
        i9eu_send(&e, I9_OP_SEND, I9_SFID_SAMPLER, i9_vec((uint8_t)(P2_T + 4 * u), I9_UW), (uint8_t)(P2_TC + 2 * u),
                  i9_desc_sample_simd8((uint32_t)(1 + u), (uint32_t)u), false, 0);
    }
    for (int k = 0; k < 4; k++) interp_t(&e, 0, k, (uint8_t)(P2_P + k), P2_TI);
    for (int k = 0; k < 3; k++) interp_t(&e, 1, k, (uint8_t)(P2_S + k), P2_TI);
    if (c->neblina) interp_t(&e, 1, 3, P2_F, P2_TI);
    uint8_t pv = P2_P, d = P2_R0;
    for (int u = 0; u < c->n; u++) {
        const i9_unid_t *un = &c->u[u];
        if (!un->ativa) continue;
        if (un->rgb_func == 6 || un->rgb_func == 7) dot3(&e, un, u, pv, d, un->rgb_func == 7);
        else for (int k = 0; k < 3; k++) canal(&e, un, u, pv, (uint8_t)(d + k), k, false);
        if (un->rgb_func != 7) canal(&e, un, u, pv, (uint8_t)(d + 3), 3, true);
        pv = d;
        d = d == P2_R0 ? P2_R1 : P2_R0;
    }
    /* El especular (saturando), la niebla (f x color + (1 - f) x niebla) y el alfa. */
    for (int k = 0; k < 3; k++) op2(&e, I9_OP_ADD, true, (uint8_t)(G_SAIDA + k), v((uint8_t)(pv + k)), v((uint8_t)(P2_S + k)));
    if (c->neblina)
        for (int k = 0; k < 3; k++) {
            uint32_t fc = fbits_q((int32_t)byte_k(c->cor_neblina, k), 255);
            op2(&e, I9_OP_ADD, false, P2_TMP, v((uint8_t)(G_SAIDA + k)), i9_imf(fc ? fc ^ 0x80000000u : 0));
            op2(&e, I9_OP_MUL, false, P2_TMP, v(P2_TMP), v(P2_F));
            op2(&e, I9_OP_ADD, true, (uint8_t)(G_SAIDA + k), v(P2_TMP), i9_imf(fc));
        }
    op2(&e, I9_OP_MOV, true, G_SAIDA + 3, v((uint8_t)(pv + 3)), i9_nulo(I9_F));
    i9eu_send(&e, I9_OP_SENDC, I9_SFID_RENDER, i9_nulo(I9_UW), G_SAIDA, i9_desc_rt_simd8(0), true, 0);
    return e.transbordou ? 0 : e.n;
}

/* ------------------------------------------------------------------ el sombreador de vértices de v3 (D457) */

#define V3_C0     2       /* primera constante */
#define V3_T      64      /* temporales: g64-g79 */
#define V3_F      80      /* el factor de niebla */
#define V3_M0     82      /* mensajes que no son el último: g82, g92, g102 (cabecera + 8 de datos) */
#define V3_MFIM   112     /* el último (con EOT): g112-g120 (el SEND con EOT lee de g112-g127) */

int i9_vs_nc(const i9_vs_t *k)
{
    int u_max = -1;
    for (int u = 0; u < k->n_unid && u < 4; u++) if (k->tex_mat & (1u << u)) u_max = u;
    return u_max < 0 ? 8 : 8 + 2 * (u_max + 1);
}

int i9_vs_n_atr(const i9_vs_t *k)
{
    int n = 1 + (k->cor ? 1 : 0);
    for (int u = 0; u < k->n_unid && u < 4; u++) n += k->tc[u] ? 1 : 0;
    return n;
}

/* La constante «i» (float) como escalar. */
static i9_op_t cst(int i) { return i9_esc((uint8_t)(V3_C0 + i / 8), (uint8_t)((i % 8) * 4), I9_F); }

/* dst = fila «l» de la matriz (por columnas) que empieza en la constante «m» por el vector v[0..3] (registros). */
static void fila_vs(i9eu_t *e, uint8_t dst, int m, int l, const uint8_t vec[4], uint8_t tmp)
{
    op2(e, I9_OP_MUL, false, dst, v(vec[0]), cst(m + l));
    for (int k = 1; k < 4; k++) {
        op2(e, I9_OP_MUL, false, tmp, v(vec[k]), cst(m + 4 * k + l));
        op2(e, I9_OP_ADD, false, dst, v(dst), v(tmp));
    }
}

/* El registro de la componente «c» de la ranura «r» de la VUE en el mensaje que la lleva (de dos en dos). */
static uint8_t saida(int r, int c, int n_msg)
{
    int j = r / 2;
    uint8_t base = (uint8_t)(j == n_msg - 1 ? V3_MFIM : V3_M0 + 10 * j);
    return (uint8_t)(base + 1 + (r % 2) * 4 + c);
}

int i9_vs_gerar(uint32_t *p, int max_inst, int gen, const i9_vs_t *k)
{
    if ((gen != 9 && gen != 11) || k->n_unid > 4 || k->cor > 2 || k->neblina > 2 || max_inst > I9_VS_MAX_INST) return 0;
    i9eu_t e;
    i9eu_iniciar(&e, p, max_inst, gen);
    int nc = i9_vs_nc(k), a = V3_C0 + nc, atr = 0;
    uint8_t pos[4], cor[4] = { 0, 0, 0, 0 }, tc[4][4];
    for (int c = 0; c < 4; c++) pos[c] = (uint8_t)(a + 4 * atr + c);
    atr++;
    if (k->cor) { for (int c = 0; c < 4; c++) cor[c] = (uint8_t)(a + 4 * atr + c); atr++; }
    for (int u = 0; u < k->n_unid; u++)
        if (k->tc[u]) { for (int c = 0; c < 4; c++) tc[u][c] = (uint8_t)(a + 4 * atr + c); atr++; }
    int n_msg = 2 + (k->n_unid > 0) + (k->n_unid > 2);
    i9_op_t zero = i9_imf(0), um = i9_imf(0x3F800000u);
    /* Ranura 0: la cabecera de la VUE a cero (sin tamaño de punto ni índices de vista). */
    for (int c = 0; c < 4; c++) op2(&e, I9_OP_MOV, false, saida(0, c, n_msg), zero, i9_nulo(I9_F));
    /* Ranura 1: la posición de recorte. */
    for (int l = 0; l < 4; l++) fila_vs(&e, saida(1, l, n_msg), 0, l, pos, V3_T);
    /* La niebla: f = sat(neb_a - |mv x pos| x neb_b). */
    if (k->neblina) {
        for (int l = 0; l < 3; l++) fila_vs(&e, (uint8_t)(V3_T + 1 + l), 16, l, pos, V3_T);
        op2(&e, I9_OP_MUL, false, V3_T + 4, v(V3_T + 1), v(V3_T + 1));
        op2(&e, I9_OP_MUL, false, V3_T, v(V3_T + 2), v(V3_T + 2));
        op2(&e, I9_OP_ADD, false, V3_T + 4, v(V3_T + 4), v(V3_T));
        op2(&e, I9_OP_MUL, false, V3_T, v(V3_T + 3), v(V3_T + 3));
        op2(&e, I9_OP_ADD, false, V3_T + 4, v(V3_T + 4), v(V3_T));
        i9eu_math(&e, I9_MATH_SQRT, v(V3_T + 5), v(V3_T + 4));
        op2(&e, I9_OP_MUL, false, V3_T + 6, v(V3_T + 5), cst(37));
        op2(&e, I9_OP_ADD, true, V3_F, i9_neg(v(V3_T + 6)), cst(36));
    }
    /* Ranura 2: el color. */
    for (int c = 0; c < 4; c++) {
        uint8_t d = saida(2, c, n_msg);
        if (k->cor) op2(&e, I9_OP_MOV, k->cor == 2, d, v(cor[c]), i9_nulo(I9_F));
        else op2(&e, I9_OP_MOV, false, d, cst(32 + c), i9_nulo(I9_F));
        if (k->neblina == 2 && c < 3) op2(&e, I9_OP_MUL, false, d, v(d), v(V3_F));
    }
    /* Ranura 3: el especular (sin luces, 0) y el factor de niebla en el alfa; mezclada: neb_cor x (1 - f), alfa 1. */
    if (k->neblina == 2) {
        op2(&e, I9_OP_ADD, false, V3_T + 7, i9_neg(v(V3_F)), um);
        for (int c = 0; c < 3; c++) op2(&e, I9_OP_MUL, false, saida(3, c, n_msg), v(V3_T + 7), cst(40 + c));
        op2(&e, I9_OP_MOV, false, saida(3, 3, n_msg), um, i9_nulo(I9_F));
    } else {
        for (int c = 0; c < 3; c++) op2(&e, I9_OP_MOV, false, saida(3, c, n_msg), zero, i9_nulo(I9_F));
        op2(&e, I9_OP_MOV, false, saida(3, 3, n_msg), k->neblina ? v(V3_F) : um, i9_nulo(I9_F));
    }
    /* Ranuras 4 + u: las coordenadas (s, t, 0, 1), por la matriz de la unidad si se pide. Las del último mensaje
     * que no son de ninguna unidad, a (0, 0, 0, 1). */
    int n_ranuras = 2 * n_msg;
    for (int u = 0; u + 4 < n_ranuras; u++) {
        int r = 4 + u;
        if (u >= k->n_unid) {
            for (int c = 0; c < 4; c++) op2(&e, I9_OP_MOV, false, saida(r, c, n_msg), c == 3 ? um : zero, i9_nulo(I9_F));
            continue;
        }
        uint8_t vv[4] = { 0, 0, 0, 0 };
        if (k->tc[u]) for (int c = 0; c < 4; c++) vv[c] = tc[u][c];
        else if (k->tex_mat & (1u << u))   /* la constante, a registros para multiplicarla */
            for (int c = 0; c < 4; c++) { vv[c] = (uint8_t)(V3_T + 8 + c); op2(&e, I9_OP_MOV, false, vv[c], cst(48 + 4 * u + c), i9_nulo(I9_F)); }
        for (int c = 0; c < 2; c++) {
            uint8_t d = saida(r, c, n_msg);
            if (k->tex_mat & (1u << u)) fila_vs(&e, d, 64 + 16 * u, c, vv, V3_T);
            else if (k->tc[u]) op2(&e, I9_OP_MOV, false, d, v(vv[c]), i9_nulo(I9_F));
            else op2(&e, I9_OP_MOV, false, d, cst(48 + 4 * u + c), i9_nulo(I9_F));
        }
        op2(&e, I9_OP_MOV, false, saida(r, 2, n_msg), zero, i9_nulo(I9_F));
        op2(&e, I9_OP_MOV, false, saida(r, 3, n_msg), um, i9_nulo(I9_F));
    }
    /* Las cabeceras (los asideros de g1, en todos los canales) y los mensajes; el último termina el hilo. */
    for (int j = 0; j < n_msg; j++) {
        uint8_t m = (uint8_t)(j == n_msg - 1 ? V3_MFIM : V3_M0 + 10 * j);
        i9eu_alu(&e, I9_OP_MOV, false, i9_vec(m, I9_UD), i9_vec(1, I9_UD), i9_nulo(I9_UD), 0);
        i9eu_nomask(&e);
        i9eu_send(&e, I9_OP_SEND, I9_SFID_URB, i9_nulo(I9_UD), m, i9_desc_urb_simd8(9, (uint32_t)(2 * j)), j == n_msg - 1, 0);
    }
    return e.transbordou ? 0 : e.n;
}

void i9_vs_consts(uint32_t *c, const i9_vs_t *k, const i9_vs_dados_t *d)
{
    int nc = i9_vs_nc(k);
    for (int i = 0; i < nc * 8; i++) c[i] = 0;
    for (int i = 0; i < 16; i++) { c[i] = d->mvp[i]; c[16 + i] = d->mv[i]; }
    c[32] = fbits_q((int32_t)((d->cor >> 16) & 0xFFu), 255); c[33] = fbits_q((int32_t)((d->cor >> 8) & 0xFFu), 255);
    c[34] = fbits_q((int32_t)(d->cor & 0xFFu), 255);         c[35] = fbits_q((int32_t)(d->cor >> 24), 255);
    c[36] = d->neb_a; c[37] = d->neb_b;
    for (int j = 0; j < 3; j++) c[40 + j] = fbits_q((int32_t)((d->neb_cor >> (16 - 8 * j)) & 0xFFu), 255);
    for (int u = 0; u < 4; u++) for (int j = 0; j < 4; j++) c[48 + 4 * u + j] = d->tc_const[u][j];
    for (int u = 0; u < 4 && 64 + 16 * u + 16 <= nc * 8; u++) for (int j = 0; j < 16; j++) c[64 + 16 * u + j] = d->tex[u][j];
}

/* ------------------------------------------------------------------ workarounds (Gen9) */

/* i915 v6.6 (gt/intel_workarounds.c, MIT): solo las de Skylake, Kaby Lake y Coffee/Comet Lake
 * (no las de Broxton/Gemini Lake, que no están en la lista de D434). Las de Gen11 y Gen12 se
 * anotan en D438 para después. «mcr»: registros de varias copias (se leen con la dirección de
 * la BIOS; si dan 0 o ~0 no se tocan, ver gpu3d_intel9.c). */
static const i9_wa_t wa9_comuns[] = {
    /* contexto (gen9_ctx_workarounds_init), por LRI en cada tanda */
    { 0x7014u, 1u << 13, 0, 1, I9_WA_CTX, "WaCompressedResourceSamplerPbeMediaNewHashMode (COMMON_SLICE_CHICKEN2)" },
    { 0xE194u, 1u << 8, 0, 1, I9_WA_CTX, "WaCompressedResourceSamplerPbeMediaNewHashMode (HALF_SLICE_CHICKEN7)" },
    { 0xE4F0u, (1u << 15) | (1u << 8), 0, 1, I9_WA_CTX, "WaClearFlowControlGpgpuContextSave, WaDisablePartialInstShootdown" },
    { 0xE194u, (1u << 4) | (1u << 2), 0, 1, I9_WA_CTX, "WaEnableYV12BugFixInHalfSliceChicken7, WaEnableSamplerGPGPUPreemptionSupport" },
    { 0x7004u, (1u << 6) | (1u << 1), 0, 1, I9_WA_CTX, "Wa4x4STCOptimizationDisable, WaDisablePartialResolveInVc" },
    { 0xE188u, 0, 1u << 3, 1, I9_WA_CTX, "WaCcsTlbPrefetchDisable" },
    { 0x7300u, (1u << 5) | (1u << 15) | (1u << 4), 0, 1, I9_WA_CTX, "WaForceContextSaveRestoreNonCoherent, WaForceEnableNonCoherent" },
    { 0xE184u, 1u << 1, 0, 1, I9_WA_CTX, "WaDisableSamplerPowerBypassForSOPingPong" },
    { 0xE180u, 1u << 13, 0, 1, I9_WA_CTX, "WaDisableSTUnitPowerOptimization" },
    { 0x2580u, 0, 1u << 0, 1, I9_WA_CTX, "WaDisable3DMidCmdPreemption" },
    { 0x2580u, 1u << 2, 1u << 1, 1, I9_WA_CTX, "WaDisableGPGPUMidCmdPreemption" },
    /* motor (rcs_engine_wa_init), por MMIO al encender */
    { 0x20E0u, 1u << 14, 0, 1, I9_WA_MOTOR, "GEN9_FFSC_PERCTX_PREEMPT_CTRL" },
    { 0xB004u, 1u << 7, 0, 0, I9_WA_MOTOR, "WaEnableGapsTsvCreditFix" },
    { 0x20D4u, 1u << 2, 0, 1, I9_WA_MOTOR, "WaContextSwitchWithConcurrentTLBInvalidate" },
    { 0xB11Cu, 1u << 2, 1u << 8, 0, I9_WA_MOTOR, "WaEnableLbsSlaRetryTimerDecrement; sin EVICTION_PERF_FIX (mcr)" },
    { 0xB118u, 1u << 21, 1u << 22, 0, I9_WA_MOTOR, "WaOCLCoherentLineFlush; atomicas de L3 apagadas (mcr)" },
    { 0xB008u, 0, 1u << 0, 0, I9_WA_MOTOR, "atomicas no coherentes de LNCF apagadas" },
    /* GT (gen9_gt_workarounds_init) */
    { 0x4DDCu, (1u << 31) | (1u << 27), 0, 0, I9_WA_GT, "WaCompressedResourceSamplerPbeMediaNewHashMode (MMCD_MISC_CTRL)" },
    { 0x4090u, 1u << 25, 0, 0, I9_WA_GT, "WaDisableHDCInvalidation" },
    { 0x940Cu, 1u << 14, 0, 0, I9_WA_GT, "WaDisableGafsUnitClkGating" },
};
static const i9_wa_t wa9_skl_kbl[] = {
    { 0x4090u, 1u << 8, 0, 0, I9_WA_GT, "WaDisableKillLogic" },
};
static const i9_wa_t wa9_kbl_cfl[] = {
    { 0xE100u, 1u << 4, 0, 1, I9_WA_CTX, "WaDisableSbeCacheDispatchPortSharing" },
    { 0x4AB0u, 1u << 18, 0, 0, I9_WA_GT, "WaInPlaceDecompressionHang" },
};
static const i9_wa_t wa9_cfl[] = {
    { 0x7014u, 1u << 8, 0, 1, I9_WA_CTX, "WaToEnableHwFixForPushConstHWBug" },
};

#define N_WA_MAX 32
static i9_wa_t wa_tab[N_WA_MAX];

int i9_plataforma(uint16_t m)
{
    if ((m >> 8) == 0x19) return I9_PLAT_SKL;
    if ((m >> 8) == 0x59 || m == 0x87C0) return I9_PLAT_KBL;
    return I9_PLAT_CFL;
}

int i9_workarounds(int gen, int plat, const i9_wa_t **t)
{
    int n = 0;
    *t = wa_tab;
    if (gen != 9) return 0;
    for (unsigned i = 0; i < sizeof wa9_comuns / sizeof wa9_comuns[0] && n < N_WA_MAX; i++) wa_tab[n++] = wa9_comuns[i];
    if (plat != I9_PLAT_CFL) wa_tab[n++] = wa9_skl_kbl[0];
    if (plat != I9_PLAT_SKL)
        for (unsigned i = 0; i < sizeof wa9_kbl_cfl / sizeof wa9_kbl_cfl[0] && n < N_WA_MAX; i++) wa_tab[n++] = wa9_kbl_cfl[i];
    if (plat == I9_PLAT_CFL) wa_tab[n++] = wa9_cfl[0];
    return n;
}

/* ------------------------------------------------------------------ estado y tandas */

int i9_fator(int f)
{
    switch (f) {
    case 0:  return 17;   /* G3D_F_ZERO: BLENDFACTOR_ZERO */
    case 1:  return 1;    /* G3D_F_UM: ONE */
    case 2:  return 3;    /* G3D_F_ALFA: SRC_ALPHA */
    case 3:  return 19;   /* G3D_F_UM_MENOS_ALFA: INV_SRC_ALPHA */
    default: return 1;
    }
}

int i9_func_z(int z)
{
    switch (z) {
    case 0:  return 2;    /* G3D_Z_MENOR: COMPAREFUNCTION_LESS */
    case 1:  return 4;    /* G3D_Z_MENOR_IGUAL: LEQUAL */
    default: return 0;    /* G3D_Z_SEMPRE: ALWAYS */
    }
}

/* D453: gpu3d v2 (el orden de GL) en los códigos de la GPU (genxml gen9: 3D_Compare_Function,
 * 3D_Stencil_Operation, 3D_Color_Buffer_Blend_Factor/Function, 3D_Logic_Op_Function). */
int i9_cmp(int c)
{
    static const int8_t t[8] = { 1, 2, 3, 4, 5, 6, 7, 0 };   /* NEVER LESS EQUAL LEQUAL GREATER NOTEQUAL GEQUAL ALWAYS */
    return c >= 0 && c < 8 ? t[c] : -1;
}

int i9_op_est(int e)
{
    /* MANTER ZERO SUBSTITUIR INCR(sat) DECR(sat) INVERTER INCR_CICLO DECR_CICLO -> KEEP ZERO REPLACE INCRSAT
     * DECRSAT INVERT INCR DECR */
    static const int8_t t[8] = { 0, 1, 2, 3, 4, 7, 5, 6 };
    return e >= 0 && e < 8 ? t[e] : -1;
}

int i9_fator2(int f)
{
    /* ZERO ONE SRC_ALPHA INV_SRC_ALPHA SRC_COLOR INV_SRC_COLOR DST_COLOR INV_DST_COLOR DST_ALPHA INV_DST_ALPHA
     * SRC_ALPHA_SATURATE */
    static const int8_t t[11] = { 0x11, 0x01, 0x03, 0x13, 0x02, 0x12, 0x05, 0x15, 0x04, 0x14, 0x06 };
    return f >= 0 && f < 11 ? t[f] : -1;
}

int i9_eq(int e)
{
    return e == 0 ? 0 : e == 1 ? 1 : e == 2 ? 2 : -1;   /* ADD, SUBTRACT (orig - dest), REVERSE_SUBTRACT */
}

int i9_logica(int o)
{
    /* GL: CLEAR AND AND_REVERSE COPY AND_INVERTED NOOP XOR OR NOR EQUIV INVERT OR_REVERSE COPY_INVERTED
     * OR_INVERTED NAND SET -> LOGICOP_* de la GPU (el orden de los ROP2) */
    static const int8_t t[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };
    return o >= 0 && o < 16 ? t[o] : -1;
}

/* ---- lotes (D448) */

static void put(i9_lote_t *l, uint32_t v) { if (l->n < l->max) l->cmd[l->n++] = v; else l->erro = true; }
static void zeros(i9_lote_t *l, int n) { while (n-- > 0) put(l, 0); }

/* PIPE_CONTROL de 6 palabras: «fl» con los bits de la palabra 1; con I9_PC_WRITE_IMM, «dir» y
 * el valor (64 bits). */
static void pc(i9_lote_t *l, uint32_t fl, uint32_t dir, uint32_t lo, uint32_t hi)
{
    put(l, I9_PIPE_CONTROL); put(l, fl);
    put(l, (fl & I9_PC_WRITE_IMM) ? dir & ~7u : 0); put(l, 0);
    put(l, lo); put(l, hi);
}

/* Fin de cauce (iris_emit_end_of_pipe_sync): los bits pedidos, CS stall y una escritura. */
static void pc_fim(i9_lote_t *l, uint32_t fl)
{
    pc(l, fl | I9_PC_CS_STALL | I9_PC_WRITE_IMM, l->c.scratch, 0, 0);
}

static void lri(i9_lote_t *l, uint32_t reg, uint32_t val)
{
    put(l, I9_MI_LOAD_REGISTER_IMM(1)); put(l, reg); put(l, val);
}

static uint32_t masc(uint32_t set, uint32_t clr) { return (set | clr) << 16 | set; }

/* D448: marca de tiempo de la GPU «k» (cuando todo lo de antes acabó: CS stall), si se pidió. */
static void tempo(i9_lote_t *l, uint32_t k)
{
    if (l->c.perfil_ggtt) pc(l, I9_PC_CS_STALL | I9_PC_WRITE_TS | I9_PC_GGTT, l->c.perfil_ggtt + 8u * k, 0, 0);
}

/* Un trozo del hueco de estado (alineado, a cero). Devuelve su desplazamiento desde la base y deja
 * en *p dónde escribirlo; sin sitio, el lote queda con error y *p apunta a un sitio de descarte. */
static uint32_t descarte[I9_DESENHO_EST / 4];
static uint32_t est_aloca(i9_lote_t *l, uint32_t bytes, uint32_t al, uint32_t **p)
{
    uint32_t o = (l->est_n + al - 1u) & ~(al - 1u);
    if (bytes > sizeof descarte || o + bytes > l->est_max) { l->erro = true; *p = descarte; return 0; }
    *p = l->est + o / 4u;
    for (uint32_t i = 0; i < bytes / 4u; i++) (*p)[i] = 0;
    l->est_n = o + bytes;
    return l->est_off + o;
}

static int lado_nivel(int w, int l) { w >>= l; return w < 1 ? 1 : w; }
static int al4(int v) { return (v + 3) & ~3; }

void i9_mip_pos(int w0, int h0, int l, int *x, int *y)
{
    *x = 0; *y = 0;
    if (l <= 0) return;
    *y = al4(h0);
    if (l == 1) return;
    *x = al4(lado_nivel(w0, 1));
    for (int k = 2; k < l; k++) *y += al4(lado_nivel(h0, k));
}

void i9_mip_tamanho(int w0, int h0, int niveis, int *w, int *h)
{
    *w = al4(w0); *h = al4(h0);
    if (niveis <= 1) return;
    int abaixo = al4(lado_nivel(h0, 1)), dir = 0;
    for (int k = 2; k < niveis; k++) dir += al4(lado_nivel(h0, k));
    *h += abaixo > dir ? abaixo : dir;
    if (niveis > 2 && al4(lado_nivel(w0, 1)) + al4(lado_nivel(w0, 2)) > *w) *w = al4(lado_nivel(w0, 1)) + al4(lado_nivel(w0, 2));
}

/* RENDER_SURFACE_STATE 2D lineal B8G8R8A8_UNORM (genxml gen9). «mocs»: entrada de la tabla. */
static void surf_2d(uint32_t *s, uint32_t dir, int larg, int alt, uint32_t passo, uint8_t mocs)
{
    for (int i = 0; i < 16; i++) s[i] = 0;
    s[0] = 1u << 29 | 0x0C0u << 18 | 1u << 16 | 1u << 14;   /* 2D, B8G8R8A8_UNORM, VALIGN_4, HALIGN_4, lineal */
    s[1] = (uint32_t)(mocs & 0x3Fu) << 25;                 /* MOCS: índice en los bits 6:1 del campo 30:24 */
    s[2] = (uint32_t)(alt - 1) << 16 | (uint32_t)(larg - 1);
    s[3] = passo - 1;
    s[7] = 4u << 25 | 5u << 22 | 6u << 19 | 7u << 16;     /* canales R, G, B, A tal cual */
    s[5] = 15u << 8;                                       /* Mip Tail Start LOD 15: sin cola de mipmaps (isl_surface_state.c) */
    s[8] = dir;
}

/* Elemento de vértice: búfer 0, válido, formato, desplazamiento y las cuatro componentes. */
static void elem(i9_lote_t *l, uint32_t fmt, uint32_t off, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3)
{
    put(l, 1u << 25 | fmt << 16 | off);
    put(l, c0 << 28 | c1 << 24 | c2 << 20 | c3 << 16);
}
#define VFC_SRC 1u
#define VFC_0   2u
#define VFC_1F  3u
#define FMT_RGBA32F 0x000u
#define FMT_BGRA8   0x0C0u
#define FMT_RG32F   0x085u

int i9_lote_abrir(i9_lote_t *l, const i9_lote_cfg_t *c, uint32_t *cmd, int max, uint32_t *est, uint32_t est_off,
                  uint32_t est_max)
{
    for (unsigned i = 0; i < sizeof *l; i++) ((uint8_t *)l)[i] = 0;
    if ((c->gen != 9 && c->gen != 11 && c->gen != 12) || c->larg < 1 || c->alt < 1 || c->larg > 16384 || c->alt > 16384 ||
        (c->cor_passo & 63u) || (c->z_passo & 127u) || (est_off & 63u) || (c->k_cor & 63u) || (c->k_tex & 63u) ||
        est_off + est_max > 0x10000u || est_max < I9_PROLOGO_EST || (c->marca_ggtt & 7u) || (c->scratch & 7u) ||
        (c->zbits != 16 && c->zbits != 24) || (c->est && (c->est_passo & 127u)) || !c->vb_bytes || (c->vb_bytes % I9_VERT_BYTES) || c->n_wa < 0 || c->n_aj < 0 ||
        max < I9_PROLOGO_PAL + I9_FIM_PAL)
        return -1;
    l->c = *c;
    l->cmd = cmd; l->max = max;
    l->est = est; l->est_off = est_off; l->est_max = est_max;

    /* ---- estado del destino (dinámico y de superficies, en el hueco del lote) */
    uint32_t *p;
    uint32_t o_ccvp = est_aloca(l, 32, 32, &p);
    p[0] = 0; p[1] = 0x3F800000u;                         /* Z de 0 a 1 */
    uint32_t o_sfvp = l->o_sfvp = est_aloca(l, 64, 64, &p);
    p[0] = p[1] = p[2] = 0x3F800000u;                     /* no se usa: transformación apagada */
    p[8] = fbits(-16384); p[9] = fbits(16384); p[10] = fbits(-16384); p[11] = fbits(16384);
    p[12] = 0; p[13] = fbits(c->larg - 1); p[14] = 0; p[15] = fbits(c->alt - 1);
    l->o_borda = est_aloca(l, 64, 64, &p);                /* color de borde: 64 bytes a cero */
    l->o_rt = est_aloca(l, 64, 64, &p);
    surf_2d(p, c->cor, c->larg, c->alt, c->cor_passo, c->cor_mocs);

    /* ---- registros: vaciar antes (el cambio de L3 lo pide con el cauce parado), el TLB si la
     *      PPGTT cambió, los workarounds de contexto, el L3, CS_DEBUG_MODE2, CACHE_MODE_1 y los ajustes */
    tempo(l, 0);
    pc_fim(l, I9_PC_DC_FLUSH);
    if (c->tlb) pc_fim(l, I9_PC_TLB_INVAL);
    for (int i = 0; i < c->n_wa; i++)
        if (c->wa[i].tipo == I9_WA_CTX) lri(l, c->wa[i].reg, c->wa[i].mascarado ? masc(c->wa[i].set, c->wa[i].clr) : c->wa[i].set);
    if (c->gen < 12 && !c->sem_l3) lri(l, 0x7034u, 48u << 1 | 48u << 25);   /* L3CNTLREG: URB 48, ALL 48 (chv/Gen9 de Mesa) */
    lri(l, 0x20D8u, masc(1u << 4, 0));                          /* CS_DEBUG_MODE2: CONSTANT_BUFFER sin desplazamiento */
    if (c->gen == 9) lri(l, 0x7004u, masc((1u << 4) | (1u << 9) | (1u << 1), 0));
    for (int i = 0; i < c->n_aj; i++) lri(l, c->aj_reg[i], c->aj_val[i]);
    tempo(l, 1);

    /* ---- PIPELINE_SELECT 3D (flujos de antes, iris emit_pipeline_select) */
    pc(l, I9_PC_RT_FLUSH | I9_PC_DEPTH_FLUSH | I9_PC_DC_FLUSH | I9_PC_CS_STALL, 0, 0, 0);
    pc(l, I9_PC_TEX_INVAL | I9_PC_CONST_INVAL | I9_PC_STATE_INVAL | I9_PC_INST_INVAL, 0, 0, 0);
    put(l, I9_PIPELINE_SELECT | (c->gen >= 12 ? 0x13u : 3u) << 8 | 0u);

    /* ---- STATE_BASE_ADDRESS: todo desde la zona, tamaños al máximo */
    pc_fim(l, I9_PC_RT_FLUSH | I9_PC_DEPTH_FLUSH | I9_PC_DC_FLUSH);
    put(l, c->gen >= 11 ? I9_STATE_BASE_ADDRESS_11 : I9_STATE_BASE_ADDRESS_9);
    put(l, 1); put(l, 0);                     /* general: 0 */
    put(l, 0);                                /* MOCS de los accesos sin estado */
    put(l, c->base | 1u); put(l, 0);          /* superficies */
    put(l, c->base | 1u); put(l, 0);          /* dinámico */
    put(l, 1); put(l, 0);                     /* objetos indirectos: 0 */
    put(l, c->base | 1u); put(l, 0);          /* instrucciones */
    put(l, 0xFFFFFu << 12 | 1u); put(l, 0xFFFFFu << 12 | 1u); put(l, 0xFFFFFu << 12 | 1u); put(l, 0xFFFFFu << 12 | 1u);
    put(l, 1); put(l, 0); put(l, 0);          /* sin superficies «bindless» */
    if (c->gen >= 11) { put(l, 1); put(l, 0); put(l, 0); }   /* sin muestreadores «bindless» */
    pc_fim(l, I9_PC_TEX_INVAL | I9_PC_CONST_INVAL | I9_PC_STATE_INVAL);

    /* ---- invalidar lo que se vuelve a escribir en el mismo sitio en cada lote (vértices,
     *      texturas, estado): en Gen9 el de VF va tras un PIPE_CONTROL vacío y con escritura */
    if (c->gen == 9) pc(l, 0, 0, 0, 0);
    pc(l, I9_PC_VF_INVAL | I9_PC_TEX_INVAL | I9_PC_CONST_INVAL | I9_PC_STATE_INVAL | I9_PC_WRITE_IMM, c->scratch, 0, 0);

    /* ---- estado fijo del cauce */
    put(l, I9_3DSTATE_DRAWING_RECTANGLE); put(l, 0);
    put(l, (uint32_t)(c->alt - 1) << 16 | (uint32_t)(c->larg - 1)); put(l, 0);
    put(l, I9_3DSTATE_SAMPLE_PATTERN); zeros(l, 7); put(l, 0x00880000u);   /* 1x: en el centro del píxel */
    put(l, I9_3DSTATE_AA_LINE_PARAMETERS); zeros(l, 2);
    put(l, I9_3DSTATE_WM_CHROMAKEY); put(l, 0);
    put(l, I9_3DSTATE_WM_HZ_OP); zeros(l, 4);
    put(l, I9_3DSTATE_POLY_STIPPLE_OFFSET); put(l, 0);
    /* Constantes: 6 KB por etapa y 8 para píxeles (iris_alloc_push_constants); ninguna se usa. */
    for (uint32_t i = 0; i < 5; i++) {
        put(l, I9_3DSTATE_PUSH_CONSTANT_ALLOC_VS + (i << 16));
        put(l, (i == 4 ? 8u : 6u) | (6u * i) << 16);
    }
    /* URB: 32 KB de constantes delante (4 trozos de 8 KB); el VS (que solo escribe el VF) con
     * 512 entradas de 128 bytes (cabecera, posición, color, especular, u v); HS/DS/GS a 0. */
    put(l, I9_3DSTATE_URB_VS); put(l, 4u << 25 | 1u << 16 | 512u);
    for (uint32_t i = 1; i < 4; i++) { put(l, I9_3DSTATE_URB_VS + (i << 16)); put(l, 4u << 25); }
    static const uint32_t consts[5] = { I9_3DSTATE_CONSTANT_VS, I9_3DSTATE_CONSTANT_HS, I9_3DSTATE_CONSTANT_DS,
                                        I9_3DSTATE_CONSTANT_GS, I9_3DSTATE_CONSTANT_PS };
    for (int i = 0; i < 5; i++) { put(l, consts[i]); zeros(l, 10); }
    for (uint32_t i = 0; i < 4; i++) { put(l, I9_3DSTATE_BINDING_TABLE_POINTERS_VS + (i << 16)); put(l, 0); }
    put(l, I9_3DSTATE_VIEWPORT_STATE_POINTERS_CC); put(l, o_ccvp);
    put(l, I9_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP); put(l, o_sfvp);
    put(l, I9_3DSTATE_MULTISAMPLE); put(l, 0);
    put(l, I9_3DSTATE_SAMPLE_MASK); put(l, 1);

    /* Geometría: VS, HS, TE, DS, GS y SOL apagados; el clipper sin división ni recorte; el SF sin
     * transformación (x, y ya son píxeles); sin descarte de caras. */
    put(l, I9_3DSTATE_VS); zeros(l, 8);
    put(l, I9_3DSTATE_HS); zeros(l, 8);
    put(l, I9_3DSTATE_TE); zeros(l, 3);
    put(l, I9_3DSTATE_DS); zeros(l, 10);
    put(l, I9_3DSTATE_GS); zeros(l, 9);
    put(l, I9_3DSTATE_STREAMOUT); zeros(l, 4);
    put(l, I9_3DSTATE_CLIP); put(l, 0); put(l, 1u << 9); put(l, 0);      /* PerspectiveDivideDisable */
    put(l, I9_3DSTATE_SF); zeros(l, 3);
    put(l, I9_3DSTATE_RASTER); put(l, 1u << 16 | 1u << 1); zeros(l, 3);     /* CULLMODE_NONE; tijera (D453: cada dibujo la suya) */
    put(l, I9_3DSTATE_SBE_SWIZ); zeros(l, 10);
    put(l, I9_3DSTATE_WM); put(l, 1u << 11);                                 /* baricéntricas de perspectiva */

    /* Z (mosaico Y, implícito en Gen9), sin esténcil ni HiZ. Antes, el cauce del Z parado. */
    pc_fim(l, I9_PC_DEPTH_STALL | I9_PC_DEPTH_FLUSH);
    put(l, I9_3DSTATE_DEPTH_BUFFER);
    put(l, 1u << 29 | 1u << 28 | (c->est ? 1u << 27 : 0u) | (c->zbits == 16 ? 5u : 3u) << 18 | (c->z_passo - 1u));
    put(l, c->z); put(l, 0);
    put(l, (uint32_t)(c->alt - 1) << 18 | (uint32_t)(c->larg - 1) << 4);
    zeros(l, 3);
    if (c->gen >= 12) { put(l, 0x78060006u); zeros(l, 7); }
    else if (c->est) {                       /* D453: esténcil de 8 bits en mosaico W, aparte del Z */
        put(l, I9_3DSTATE_STENCIL_BUFFER_9); put(l, 1u << 31 | (c->est_passo - 1u)); put(l, c->est); put(l, 0); put(l, 0);
    } else { put(l, I9_3DSTATE_STENCIL_BUFFER_9); zeros(l, 4); }
    put(l, I9_3DSTATE_HIER_DEPTH_BUFFER); zeros(l, 4);
    put(l, I9_3DSTATE_CLEAR_PARAMS); zeros(l, 2);

    /* Los vértices (búfer, elementos y SBE) los pone el primer dibujo de cada formato. */
    for (uint32_t i = 0; i < 8; i++) { put(l, I9_3DSTATE_VF_INSTANCING); put(l, i); put(l, 0); }
    put(l, I9_3DSTATE_VF_SGVS); put(l, 0);
    put(l, I9_3DSTATE_VF_TOPOLOGY); put(l, 4);                     /* TRILIST */
    l->topo = 4;
    put(l, I9_3DSTATE_VF); put(l, 0);
    put(l, I9_3DSTATE_VF_STATISTICS);
    tempo(l, 2);
    return l->erro ? -1 : 0;
}

bool i9_lote_cabe(const i9_lote_t *l)
{
    return !l->erro && l->n + I9_DESENHO_PAL + I9_FIM_PAL <= l->max && l->est_n + I9_DESENHO_EST <= l->est_max;
}

static bool mesmo_estado(const i9_desenho_t *a, const i9_desenho_t *b)
{
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    for (unsigned i = 0; i < sizeof *a; i++) if (x[i] != y[i]) return false;
    return true;
}

/* El búfer de vértices desde «base», sus elementos y el SBE del formato (cabecera de la VUE a 0,
 * posición, color, especular y las coordenadas: una pareja (u, v) en v1, (s, t) de cuatro unidades en v2). */
static void vertices(i9_lote_t *l, uint8_t fmt, uint32_t base)
{
    uint32_t passo = fmt == I9_FMT_V2 ? I9_VERT2_BYTES : I9_VERT_BYTES, nt = fmt == I9_FMT_V2 ? 4u : 1u;
    put(l, I9_3DSTATE_VERTEX_BUFFERS | 3u);
    put(l, 1u << 14 | passo); put(l, l->c.vb + base); put(l, 0); put(l, l->c.vb_bytes - base);
    put(l, I9_3DSTATE_VERTEX_ELEMENTS | (2u * (4u + nt) - 1u));
    elem(l, FMT_RGBA32F, 0, VFC_0, VFC_0, VFC_0, VFC_0);            /* cabecera de la VUE */
    elem(l, FMT_RGBA32F, 0, VFC_SRC, VFC_SRC, VFC_SRC, VFC_SRC);    /* x, y, z, rhw */
    elem(l, FMT_BGRA8, 16, VFC_SRC, VFC_SRC, VFC_SRC, VFC_SRC);     /* color 0xAARRGGBB */
    elem(l, FMT_BGRA8, 20, VFC_SRC, VFC_SRC, VFC_SRC, VFC_SRC);     /* especular (y la niebla en el alfa) */
    for (uint32_t u = 0; u < nt; u++)                               /* u v (v1); s t de cada unidad (v2) */
        elem(l, FMT_RG32F, 24u + 16u * u, VFC_SRC, VFC_SRC, VFC_0, VFC_1F);
    /* SBE: 2 + nt atributos desde la VUE (saltando cabecera y posición: desplazamiento 1 en unidades de
     * dos atributos), longitud (2 + nt) / 2 redondeada, con todas las componentes. */
    uint32_t na = 2u + nt;
    put(l, I9_3DSTATE_SBE);
    put(l, 1u << 29 | 1u << 28 | na << 22 | ((na + 1u) / 2u) << 11 | 1u << 5);
    put(l, 0); put(l, 0); put(l, 0xFFFFFFFFu); put(l, 0xFFFFFFFFu);
    l->fmt = fmt;
    l->vb_base = base;
}

/* D457: el cauce de geometría de v1/v2 (sin sombreador de vértices, sin recorte, sin vista: los vértices ya vienen en
 * píxeles) o el de v3 (VS encendido; el clipper recorta a -w <= x, y, z <= w y divide por w; el SF aplica la vista).
 * Con una espera del cauce delante (fin de cauce con CS stall): el VS, el recorte y el SF cambian para lo que sigue. */
static void geometria_fixa(i9_lote_t *l)
{
    pc_fim(l, 0);
    put(l, I9_3DSTATE_VS); zeros(l, 8);
    put(l, I9_3DSTATE_CLIP); put(l, 0); put(l, 1u << 9); put(l, 0);      /* PerspectiveDivideDisable */
    put(l, I9_3DSTATE_SF); zeros(l, 3);
    put(l, I9_3DSTATE_RASTER); put(l, 1u << 16 | 1u << 1); zeros(l, 3);
    put(l, I9_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP); put(l, l->o_sfvp);
    if (l->topo != 4) { put(l, I9_3DSTATE_VF_TOPOLOGY); put(l, 4); l->topo = 4; }
    l->v3 = false;
    l->fmt = 0;                                                         /* el siguiente dibujo vuelve a poner sus vértices */
}

/* El estado de píxeles de un dibujo (v1, v2 o v3), si cambió. false: no cupo en el hueco de estado. */
static bool estado_pixel(i9_lote_t *l, const i9_desenho_t *d)
{
    if (l->tem_ult && mesmo_estado(d, &l->ult)) return true;
    {
        uint32_t *p;
        bool sep = d->mezcla && d->alfa_sep;
        uint32_t fo = d->mezcla ? d->f_orig : 1u, fd = d->mezcla ? d->f_dest : 0x11u;
        uint32_t fao = sep ? d->f_orig_a : fo, fad = sep ? d->f_dest_a : fd, eq = d->mezcla ? d->eq : 0u, eqa = sep ? d->eq_a : eq;
        /* BLEND_STATE: la cabecera (prueba alfa, alfa aparte) y un destino (mezcla, máscara de canales, lógica). */
        uint32_t o_bl = est_aloca(l, 64, 64, &p);
        p[0] = (sep ? 1u << 30 : 0u) | (d->alfa ? 1u << 27 | (uint32_t)(d->alfa_func & 7u) << 24 : 0u);
        p[1] = (d->mezcla && !d->logica ? 1u << 31 : 0u) | fo << 26 | fd << 21 | (eq & 7u) << 18 | fao << 13 | fad << 8 |
               (eqa & 7u) << 5 | (d->mascara & 8u ? 0u : 1u << 3) | (d->mascara & 1u ? 0u : 1u << 2) |
               (d->mascara & 2u ? 0u : 1u << 1) | (d->mascara & 4u ? 0u : 1u << 0);
        p[2] = (d->logica ? 1u << 31 | (uint32_t)(d->op_logica & 15u) << 27 : 0u) | 1u << 1 | 1u << 0 | 2u << 2;   /* clamp antes y después */
        /* COLOR_CALC_STATE: la referencia de la prueba alfa (UNORM8). */
        uint32_t o_cc = est_aloca(l, 64, 64, &p);
        p[1] = d->alfa_ref;
        /* Tijera (inclusive), dentro del destino. */
        uint32_t o_sc = est_aloca(l, 32, 32, &p);
        int x0 = 0, y0 = 0, x1 = l->c.larg - 1, y1 = l->c.alt - 1;
        if (d->tijera) {
            x0 = d->tx; y0 = d->ty; x1 = (int)d->tx + (int)d->tl - 1; y1 = (int)d->ty + (int)d->ta - 1;
            if (x1 > l->c.larg - 1) x1 = l->c.larg - 1;
            if (y1 > l->c.alt - 1) y1 = l->c.alt - 1;
            if (x0 > x1 || y0 > y1) { x0 = 1; y0 = 1; x1 = 0; y1 = 0; }   /* vacía: mínimo mayor que el máximo */
        }
        p[0] = (uint32_t)y0 << 16 | (uint32_t)x0; p[1] = (uint32_t)y1 << 16 | (uint32_t)x1;
        /* Las texturas (o algo válido que no se muestrea) y sus muestreadores (16 bytes cada uno, seguidos). */
        uint32_t o_tx[4];
        int nt = d->n_tex ? d->n_tex : 1;
        for (int u = 0; u < nt; u++) {
            o_tx[u] = est_aloca(l, 64, 64, &p);
            if (d->n_tex) {
                surf_2d(p, d->t[u].tex, d->t[u].larg, d->t[u].alt, d->t[u].passo, 0);
                if (d->t[u].niveis > 1) p[5] |= (uint32_t)(d->t[u].niveis - 1) & 15u;  /* MIP Count / LOD; Min LOD 0 */
                if (d->t[u].mosaico_y) p[0] |= 3u << 12;                               /* Tile Mode YMAJOR */
            } else surf_2d(p, l->c.cor, 1, 1, l->c.cor_passo, l->c.cor_mocs);
        }
        uint32_t o_sm = est_aloca(l, 16u * (uint32_t)nt, 32, &p);
        for (int u = 0; u < nt; u++) {
            const i9_tex_t *t = &d->t[u];
            static const uint8_t modo[3] = { 0, 2, 1 };   /* G3D_ENV_*: WRAP, CLAMP, MIRROR */
            uint32_t ms = d->n_tex ? modo[t->env_s < 3 ? t->env_s : 1] : 2u, mt = d->n_tex ? modo[t->env_t < 3 ? t->env_t : 1] : 2u;
            bool mag = d->n_tex && t->mag_linear, min = d->n_tex && t->min_linear;
            uint32_t *q = p + 4 * u;
            bool mip = d->n_tex && t->niveis > 1 && t->mip;
            /* LOD PreClamp OGL; MipModeFilter (1 NEAREST, 3 LINEAR); filtros de ampliar y reducir */
            q[0] = 2u << 27 | (mip ? (t->mip == 2 ? 3u : 1u) << 20 : 0u) | (mag ? 1u << 17 : 0u) | (min ? 1u << 14 : 0u) |
                   (d->n_tex && t->lod_bias ? ((uint32_t)(int32_t)(t->lod_bias * 256) & 0x1FFFu) << 1 : 0u);   /* sesgo S4.8 */
            q[1] = mip ? (uint32_t)(t->niveis - 1) << 16 : 0;   /* Min LOD 0, Max LOD (U4.8 en 19:8): el último nivel */
            q[2] = l->o_borda;
            /* Redondeo de direcciones (bits 18:13: U, V, R para ampliar en los pares y para reducir en los impares). */
            q[3] = ms << 6 | mt << 3 | 2u << 0 | (mag ? 0x54000u : 0u) | (min ? 0x2A000u : 0u);
        }
        uint32_t o_bt = est_aloca(l, 32, 32, &p);
        p[0] = l->o_rt;
        for (int u = 0; u < nt; u++) p[1 + u] = o_tx[u];
        if (l->erro) return false;
        put(l, I9_3DSTATE_BINDING_TABLE_POINTERS_VS + (4u << 16)); put(l, o_bt);
        put(l, I9_3DSTATE_SAMPLER_STATE_POINTERS_PS); put(l, o_sm);
        put(l, I9_3DSTATE_BLEND_STATE_POINTERS); put(l, o_bl | 1u);
        put(l, I9_3DSTATE_CC_STATE_POINTERS); put(l, o_cc | 1u);
        put(l, I9_3DSTATE_SCISSOR_STATE_POINTERS); put(l, o_sc);
        /* PS SIMD8 (núcleo 0), 1 + texturas superficies, un grupo de muestreadores si hay texturas, 64 hilos por PSD. */
        put(l, I9_3DSTATE_PS);
        put(l, d->k_ps); put(l, 0);
        /* Vector Mask Enable: los canales de los píxeles «ayudantes» de cada 2x2 también ejecutan (Mesa lo pone siempre
         * antes de XeHP, brw_fs.cpp «uses_vmask»). Sin él calculan solo los cubiertos y el muestreador saca las derivadas de
         * basura en los 2x2 a medio cubrir: el LOD sale mal y con mipmaps se lee otro nivel (D453, el Dell UHD 620). La
         * escritura del destino sigue con la máscara de píxeles cubiertos. */
        put(l, 1u << 30 | (uint32_t)(1 + nt) << 18 | (d->n_tex ? 1u << 27 : 0u));
        put(l, 0); put(l, 0);
        put(l, 63u << 23 | 1u << 0);
        put(l, (uint32_t)I9_PS_GRF_SETUP << 16);
        zeros(l, 4);
        put(l, I9_3DSTATE_PS_EXTRA); put(l, 1u << 31 | (d->alfa ? 1u << 28 : 0u) | 1u << 8);   /* válido, mata píxeles (alfa), atributos */
        put(l, I9_3DSTATE_PS_BLEND);
        put(l, 1u << 30 | (d->mezcla && !d->logica ? 1u << 29 : 0u) | fao << 24 | fad << 19 | fo << 14 | fd << 9 |
                (d->alfa ? 1u << 8 : 0u) | (sep ? 1u << 7 : 0u));
        put(l, I9_3DSTATE_WM_DEPTH_STENCIL);
        uint32_t w1 = (d->z_escrita ? 1u : 0u) | (d->z_teste ? 1u << 1 : 0u) | (uint32_t)(d->func_z & 7u) << 5, w2 = 0, w3 = 0;
        if (d->est) {
            w1 |= (uint32_t)(d->est_falha & 7u) << 29 | (uint32_t)(d->est_zfalha & 7u) << 26 | (uint32_t)(d->est_zpassa & 7u) << 23 |
                  (uint32_t)(d->est_func & 7u) << 8 | 1u << 3 | (d->est_escrita ? 1u << 2 : 0u);
            w2 = (uint32_t)d->est_mascara << 24 | (uint32_t)d->est_escrita << 16;
            w3 = (uint32_t)d->est_ref << 8;
        }
        put(l, w1); put(l, w2); put(l, w3);
        l->ult = *d;
        l->tem_ult = true;
        l->mudancas++;
    }
    return !l->erro;
}

int i9_lote_desenho(i9_lote_t *l, const i9_desenho_t *d, uint32_t off, int n)
{
    if (l->erro) return -1;
    uint32_t passo = d->formato == I9_FMT_V2 ? I9_VERT2_BYTES : I9_VERT_BYTES;
    if (n <= 0 || n % 3 || (d->formato != I9_FMT_V1 && d->formato != I9_FMT_V2) || d->n_tex > 4 || (d->k_ps & 63u) ||
        (uint64_t)off + (uint64_t)(uint32_t)n * passo > l->c.vb_bytes || (d->est && !l->c.est) || !i9_lote_cabe(l)) {
        l->erro = true;
        return -1;
    }
    for (int u = 0; u < d->n_tex; u++)
        if (d->t[u].larg < 1 || d->t[u].alt < 1 || (d->t[u].passo & 3u)) { l->erro = true; return -1; }
    if (l->v3) geometria_fixa(l);                                       /* D457: venía de un dibujo v3 */
    /* Los vértices: otro búfer si cambia el formato o si no empiezan en un vértice entero del de ahora. */
    if (l->fmt != d->formato || off < l->vb_base || (off - l->vb_base) % passo) vertices(l, d->formato, off);
    uint32_t v0 = (off - l->vb_base) / passo;
    if (!estado_pixel(l, d)) return -1;
    put(l, I9_3DPRIMITIVE); put(l, 4); put(l, (uint32_t)n); put(l, v0); put(l, 1); put(l, 0); put(l, 0);
    l->desenhos++;
    return l->erro ? -1 : 0;
}

/* ---- D457: un dibujo v3 */

static bool iguais(const uint32_t *a, const uint32_t *b, int n)
{
    for (int i = 0; i < n; i++) if (a[i] != b[i]) return false;
    return true;
}

int i9_lote_desenho3(i9_lote_t *l, const i9_desenho_t *d, const i9_geo3_t *g)
{
    if (l->erro) return -1;
    if (d->formato != I9_FMT_V2 || d->n_tex > 4 || (d->k_ps & 63u) || (g->k_vs & 63u) || (d->est && !l->c.est) || !i9_lote_cabe(l) ||
        g->n_atr < 1 || g->n_atr > I9_VS_ATR_MAX || g->nc < 8 || g->nc > 16 || !g->cst || g->n > 0xFFFFFFu ||
        (g->topologia != 4 && g->topologia != 5 && g->topologia != 6) || g->cull < 1 || g->cull > 3 || g->frente > 1 ||
        (g->ib && (g->ib & 1u))) {
        l->erro = true;
        return -1;
    }
    for (int u = 0; u < d->n_tex; u++)
        if (d->t[u].larg < 1 || d->t[u].alt < 1 || (d->t[u].passo & 3u)) { l->erro = true; return -1; }
    for (int k = 0; k < g->n_atr; k++) {
        const i9_vb3_t *b = &g->vb[k];
        if (!b->dir || (b->dir & 3u) || !b->bytes || !b->passo || b->passo > 2048u || (b->passo & 3u) || b->comps < 2 || b->comps > 4 ||
            (b->fmt != I9_VF_RGBA32F && b->fmt != I9_VF_RGB32F && b->fmt != I9_VF_RG32F && b->fmt != I9_VF_RGBA8)) {
            l->erro = true;
            return -1;
        }
    }
    uint32_t *p;
    bool novo = !l->v3;
    if (novo) {
        /* Del cauce sin VS al de v3: el recorte (sin la banda de guarda: lo que se sale de la vista se recorta), la
         * división por w y la vista; el Z se recorta al cercano y al lejano (3DSTATE_RASTER, Gen9). */
        pc_fim(l, 0);
        put(l, I9_3DSTATE_CLIP); put(l, 1u << 18);                       /* EarlyCullEnable (como iris) */
        put(l, 1u << 31 | 1u << 28);                                     /* ClipEnable, ViewportXYClipTestEnable, modo NORMAL, OGL */
        put(l, 0);
        put(l, I9_3DSTATE_SF); put(l, 1u << 1); put(l, 0); put(l, 0);    /* ViewportTransformEnable */
        l->v3 = true;
    }
    if (novo || !l->v3_tem || g->cull != l->v3_cull || g->frente != l->v3_frente) {
        put(l, I9_3DSTATE_RASTER);
        put(l, 1u << 26 | (uint32_t)g->frente << 21 | (uint32_t)g->cull << 16 | 1u << 1 | 1u << 0);   /* Z lejano y cercano, tijera */
        zeros(l, 3);
        l->v3_cull = g->cull; l->v3_frente = g->frente;
    }
    if (novo || !l->v3_tem || !iguais(g->vp, l->v3_vp, 6)) {
        uint32_t o = est_aloca(l, 64, 64, &p);
        for (int i = 0; i < 6; i++) p[i] = g->vp[i];
        p[8] = 0xBF800000u; p[9] = 0x3F800000u; p[10] = 0xBF800000u; p[11] = 0x3F800000u;   /* banda de guarda = la vista (apagada) */
        p[12] = 0; p[13] = fbits(l->c.larg - 1); p[14] = 0; p[15] = fbits(l->c.alt - 1);
        put(l, I9_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP); put(l, o);
        for (int i = 0; i < 6; i++) l->v3_vp[i] = g->vp[i];
    }
    if (novo || !l->v3_tem || g->k_vs != l->v3_kvs || g->nc != l->v3_nc || g->n_atr != l->v3_natr) {
        /* VS SIMD8: la parte de la URB (las constantes y detrás los atributos: «URB portion (Constant + Vertices)»)
         * desde g2 (Mesa: dispatch_grf_start_reg = payload.num_regs = 2), (n_atr + 1) / 2 pares de atributos, 64 hilos;
         * la salida que leen los de detrás desde la ranura 2 (desplazamiento 1 en pares), 3 pares (color, especular y
         * las cuatro coordenadas). Dell UHD 620 (2026-11-22): con 2 + nc no salía nada (las constantes caían en g2 + nc). */
        put(l, I9_3DSTATE_VS);
        put(l, g->k_vs); put(l, 0);
        put(l, 0); put(l, 0); put(l, 0);
        put(l, 2u << 20 | (uint32_t)((g->n_atr + 1u) / 2u) << 11);
        put(l, 63u << 23 | 1u << 2 | 1u << 0);
        put(l, 1u << 21 | 3u << 16);
        l->v3_kvs = g->k_vs; l->v3_natr = g->n_atr;
        l->v3_estados++;
    }
    if (novo || !l->v3_tem || g->nc != l->v3_nc || !iguais(g->cst, l->v3_cst, 8 * g->nc)) {
        /* Las constantes por el búfer 3 (el 0 solo se usa si el 3 también: la restricción de Skylake que cita iris), con
         * la dirección gráfica entera (CS_DEBUG_MODE2 sin desplazamiento). En Gen9 no valen hasta el
         * 3DSTATE_BINDING_TABLE_POINTERS_VS. */
        uint32_t bytes = 32u * g->nc, o = est_aloca(l, bytes, 32, &p);
        for (uint32_t i = 0; i < 8u * g->nc; i++) { p[i] = g->cst[i]; l->v3_cst[i] = g->cst[i]; }
        put(l, I9_3DSTATE_CONSTANT_VS); put(l, 0); put(l, (uint32_t)g->nc << 16);
        zeros(l, 6);
        put(l, l->c.base + o); put(l, 0);
        put(l, I9_3DSTATE_BINDING_TABLE_POINTERS_VS); put(l, 0);
        l->v3_nc = g->nc;
    }
    /* Los búferes (uno por atributo) y, si cambian, sus elementos y el SBE de siempre (6 atributos desde la ranura 2). */
    put(l, I9_3DSTATE_VERTEX_BUFFERS | (4u * g->n_atr - 1u));
    for (uint32_t k = 0; k < g->n_atr; k++) {
        put(l, k << 26 | (uint32_t)(g->mocs & 0x3Fu) << 17 | 1u << 14 | g->vb[k].passo);
        put(l, g->vb[k].dir); put(l, 0); put(l, g->vb[k].bytes);
    }
    uint32_t ve[2 * I9_VS_ATR_MAX];
    for (uint32_t k = 0; k < g->n_atr; k++) {
        uint32_t c = g->vb[k].comps;
        ve[2 * k] = k << 26 | 1u << 25 | (uint32_t)g->vb[k].fmt << 16;
        ve[2 * k + 1] = VFC_SRC << 28 | VFC_SRC << 24 | (c >= 3 ? VFC_SRC : VFC_0) << 20 | (c >= 4 ? VFC_SRC : VFC_1F) << 16;
    }
    if (l->fmt != I9_FMT_V3 || g->n_atr != l->v3_nve || !iguais(ve, l->v3_ve, 2 * g->n_atr)) {
        put(l, I9_3DSTATE_VERTEX_ELEMENTS | (2u * g->n_atr - 1u));
        for (uint32_t i = 0; i < 2u * g->n_atr; i++) { put(l, ve[i]); l->v3_ve[i] = ve[i]; }
        l->v3_nve = g->n_atr;
        if (l->fmt != I9_FMT_V3) {
            put(l, I9_3DSTATE_SBE);
            put(l, 1u << 29 | 1u << 28 | 6u << 22 | 3u << 11 | 1u << 5);
            put(l, 0); put(l, 0); put(l, 0xFFFFFFFFu); put(l, 0xFFFFFFFFu);
        }
        l->fmt = I9_FMT_V3;
    }
    if (g->ib) { put(l, I9_3DSTATE_INDEX_BUFFER); put(l, 1u << 8 | (uint32_t)(g->mocs & 0x3Fu) << 1); put(l, g->ib); put(l, 0); put(l, g->ib_bytes); }
    if (g->topologia != l->topo) { put(l, I9_3DSTATE_VF_TOPOLOGY); put(l, g->topologia); l->topo = g->topologia; }
    l->v3_tem = true;
    if (!estado_pixel(l, d)) return -1;
    put(l, I9_3DPRIMITIVE); put(l, (g->ib ? 1u << 8 : 0u) | g->topologia); put(l, g->n); put(l, g->primeiro); put(l, 1); put(l, 0); put(l, 0);
    l->desenhos++;
    l->v3_desenhos++;
    return l->erro ? -1 : 0;
}

int i9_lote_fechar(i9_lote_t *l)
{
    /* Fin: vaciar color, Z y datos con CS stall y, después, la marca (gen8_emit_fini_breadcrumb_rcs). */
    tempo(l, 3);
    pc(l, I9_PC_RT_FLUSH | I9_PC_DEPTH_FLUSH | I9_PC_DC_FLUSH | I9_PC_CS_STALL | I9_PC_WRITE_IMM | I9_PC_GGTT,
       l->c.marca_ggtt, l->c.marca, 0);
    tempo(l, 4);
    if (l->n & 1) put(l, I9_MI_NOOP);
    return l->erro ? -1 : l->n;
}
