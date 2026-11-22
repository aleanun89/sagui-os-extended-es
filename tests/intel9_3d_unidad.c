// SPDX-License-Identifier: GPL-2.0-or-later
/* intel9_3d_unidad.c - prueba en el PC del 3D de Intel Gen9-12 (D438): ensamblador de las EU,
 * sombreadores, órdenes de una tanda, imagen del contexto del RCS y workarounds.
 *
 * Comprobaciones independientes del driver:
 *   1. el ensamblador contra instrucciones de los vectores de prueba del ensamblador de Mesa
 *      (src/intel/tools/tests/gen9, ficheros .asm/.expected, MIT), byte a byte;
 *   2. un decodificador propio de la ISA de Gen9 (otra tabla de campos) y un pequeño simulador
 *      (MUL, ADD, MOV con saturación, el SEND del muestreador y el RT write) que ejecuta los dos
 *      sombreadores sobre una entrada sintética y exige color = sat(textura x color + especular);
 *   3. un recorrido de un lote (D448: prólogo, varios dibujos con solo el estado que cambia y el
 *      cierre) como lo haría el CS: cada cabecera conocida con su longitud de genxml, los
 *      PIPE_CONTROL (CS stall siempre con vaciado o escritura; el de VF con el PIPE_CONTROL vacío
 *      delante en Gen9; el TLB si la PPGTT cambió; la marca al final con vaciado de color, Z y
 *      datos), los LRI solo a registros permitidos y los campos que importan (URB, PS, SBE, Z,
 *      vértices, MOCS), y que un lote lleno nunca se pase de su sitio;
 *   4. la imagen del contexto del RCS contra las posiciones de i915 (gen9/11/12_rcs_offsets);
 *   5. la tabla de workarounds;
 *   6. D457 (gpu3d v3): el sombreador de vértices generado, ejecutado por el simulador (MATH, la copia de la
 *      cabecera con NoMask y la escritura SIMD8 en la URB) contra la transformación fija de ES 1.1 en coma flotante,
 *      y un lote con dibujos v3 entre otros de v1 y v2 (VS, recorte, vista, constantes, búferes e índices).
 *
 *   gcc -O2 -Wall -Wextra -DINTEL9_ANFITRION -Ikernel/inc tests/intel9_3d_unidad.c \
 *       kernel/dispositivos/intel9_3d_pacotes.c -o build/intel9_3d_unidad */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "intel9_3d.h"
#include "../kernel/dispositivos/acel_intel_logica.h"

/* Constantes de gpu3d v2 (kernel/inc/abi.h: aquí no se incluye, es del kernel). */
#define G3D_CMP_NUNCA 0
#define G3D_CMP_IGUAL 2
#define G3D_CMP_MAIOR 4
#define G3D_CMP_MAIOR_IGUAL 6
#define G3D_CMP_SEMPRE 7
#define G3D_EST_SUBSTITUIR 2
#define G3D_EST_INVERTER 5
#define G3D_EST_INCR_CICLO 6
#define G3D_EST_DECR_CICLO 7
#define G3D_F_ZERO 0
#define G3D_F_UM 1
#define G3D_F_COR_DEST 6
#define G3D_F_ALFA_SATURADO 10
#define G3D_EQ_SOMA 0
#define G3D_EQ_SUBTRAI_INV 2
#define G3D_LOG_COPIA 3

static int falhas;
#define VERIFICA(c, ...) do { if (!(c)) { falhas++; printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------------ 1. vectores de Mesa */

static void de_bytes(const char *hex, uint32_t w[4])
{
    unsigned b[16];
    sscanf(hex, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7],
           &b[8], &b[9], &b[10], &b[11], &b[12], &b[13], &b[14], &b[15]);
    for (int i = 0; i < 4; i++) w[i] = b[4 * i] | b[4 * i + 1] << 8 | b[4 * i + 2] << 16 | (uint32_t)b[4 * i + 3] << 24;
}

static void compara(const char *nome, const uint32_t *g, const char *hex, uint32_t mascara2)
{
    uint32_t e[4];
    de_bytes(hex, e);
    bool ok = g[0] == e[0] && g[1] == e[1] && (g[2] & mascara2) == (e[2] & mascara2) && g[3] == e[3];
    VERIFICA(ok, "%s: %08x %08x %08x %08x, Mesa %08x %08x %08x %08x", nome, g[0], g[1], g[2], g[3], e[0], e[1], e[2], e[3]);
}

static void vectores(void)
{
    uint32_t p[4 * 8];
    i9eu_t e;
    i9eu_iniciar(&e, p, 8, 9);
    /* add(8) g4<1>F g5.6<0,1,0>F g7.2<0,1,0>F { align1 1Q } */
    i9eu_alu(&e, I9_OP_ADD, false, i9_vec(4, I9_F), i9_esc(5, 24, I9_F), i9_esc(7, 8, I9_F), 0);
    /* mul(8) g22<1>F g4<8,8,1>F g2<0,1,0>F */
    i9eu_alu(&e, I9_OP_MUL, false, i9_vec(22, I9_F), i9_vec(4, I9_F), i9_esc(2, 0, I9_F), 0);
    /* mov.sat(8) g124<1>F g8<8,8,1>F */
    i9eu_alu(&e, I9_OP_MOV, true, i9_vec(124, I9_F), i9_vec(8, I9_F), i9_nulo(I9_F), 0);
    /* mov(8) g124<1>F 0x40c00000F */
    i9eu_alu(&e, I9_OP_MOV, false, i9_vec(124, I9_F), i9_imf(0x40c00000u), i9_nulo(I9_F), 0);
    /* sendc(8) null<1>UW g124<0,1,0>F 0x88031400 render RT write SIMD8 LastRT mlen 4 { EOT } */
    i9eu_send(&e, I9_OP_SENDC, I9_SFID_RENDER, i9_nulo(I9_UW), 124, i9_desc_rt_simd8(0), true, 0);
    /* send(8) g5<1>UW g2<8,8,1>UD 0x04420001 sampler sample SIMD8 Surface 1 mlen 2 rlen 4 (la
     * región de src0 de Mesa es <8,8,1>; aquí <0;1,0>, como el compilador: se comparan sin ella) */
    i9eu_send(&e, I9_OP_SEND, I9_SFID_SAMPLER, i9_vec(5, I9_UW), 2, i9_desc_sample_simd8(1, 0), false, 0);
    VERIFICA(e.n == 6 && !e.transbordou, "seis instrucciones");
    compara("add escalares", p + 0, "40 00 60 00 e8 3a 80 20 b8 00 00 3a e8 00 00 00", ~0u);
    compara("mul", p + 4, "41 00 60 00 e8 3a c0 22 80 00 8d 3a 40 00 00 00", ~0u);
    compara("mov.sat", p + 8, "01 00 60 80 e8 3a 80 2f 00 01 8d 00 00 00 00 00", ~0u);
    compara("mov inmediato", p + 12, "01 00 60 00 e8 3e 80 2f 00 00 00 38 00 00 c0 40", ~0u);
    compara("sendc RT write EOT", p + 16, "32 00 60 05 40 3a 00 20 80 0f 00 06 00 14 03 88", ~0u);
    compara("send sample SIMD8", p + 20, "31 00 60 02 48 02 a0 20 40 00 8d 06 01 00 42 04", ~0x01FF0000u);
    VERIFICA(i9_desc_rt_simd8(0) == 0x08031400u && i9_desc_sample_simd8(1, 0) == 0x04420001u, "descriptores de los mensajes");
    VERIFICA(i9eu_opcode(I9_OP_MOV, 12) == 97 && i9eu_opcode(I9_OP_NOP, 12) == 96 && i9eu_opcode(I9_OP_ADD, 12) == 64 &&
             i9eu_opcode(I9_OP_MOV, 9) == 1 && i9eu_opcode(I9_OP_NOP, 11) == 126, "opcodes de Gen9/11 y de Gen12");
    /* Gen11: el float es el tipo 9. */
    i9eu_iniciar(&e, p, 8, 11);
    i9eu_alu(&e, I9_OP_ADD, false, i9_vec(4, I9_F), i9_vec(5, I9_F), i9_vec(6, I9_F), 0);
    VERIFICA(((p[1] >> 5) & 0xF) == 9 && ((p[1] >> 11) & 0xF) == 9 && ((p[2] >> 27) & 0xF) == 9, "Gen11: tipo F = 9 en dst, src0 y src1");
    /* Gen12: SWSB en los bits 15:8, tipo F = 0xA, un bit de fichero. */
    i9eu_iniciar(&e, p, 8, 12);
    i9eu_alu(&e, I9_OP_MUL, true, i9_vec(10, I9_F), i9_vec(2, I9_F), i9_esc(4, 12, I9_F), I9_SWSB_REG(1));
    VERIFICA((p[0] & 0x7F) == 65 && ((p[0] >> 8) & 0xFF) == 1 && ((p[0] >> 16) & 7) == 3, "Gen12: opcode, SWSB @1 y 8 canales");
    VERIFICA(((p[1] >> 2) & 1) == 1 && ((p[1] >> 4) & 0xF) == 0xA && ((p[1] >> 8) & 0xF) == 0xA && ((p[2] >> 24) & 0xF) == 0xA,
             "Gen12: saturar y tipos F");
    VERIFICA(((p[1] >> 18) & 1) == 1 && (p[1] >> 24) == 10 && ((p[2] >> 2) & 1) == 1 && ((p[2] >> 8) & 0xFF) == 2 &&
             ((p[3] >> 2) & 1) == 1 && ((p[3] >> 3) & 0x1F) == 12 && ((p[3] >> 8) & 0xFF) == 4, "Gen12: registros y subregistro");
    i9eu_send(&e, I9_OP_SEND, I9_SFID_SAMPLER, i9_vec(32, I9_UW), 30, i9_desc_sample_simd8(1, 0), false, I9_SWSB_REG_SET(1, 0));
    uint32_t *s = p + 4;
    uint32_t desc = (s[3] >> 26 & 3) << 30 | (s[2] >> 3 & 0x1F) << 25 | (s[1] >> 19 & 0x1F) << 20 | (s[3] >> 17 & 0x1FF) << 11 |
                    (s[2] >> 17 & 0x7FF);
    VERIFICA(desc == i9_desc_sample_simd8(1, 0) && ((s[2] >> 28) & 0xF) == I9_SFID_SAMPLER && ((s[0] >> 8) & 0xFF) == 0x90,
             "Gen12: el descriptor repartido se recompone (%08x), SFID y SWSB @1 $0", desc);
}

/* ------------------------------------------------------------------ 2. decodificador y simulador */

typedef struct { unsigned op, ex, sat, sfid, df, dt, ds, dn, s0f, s0t, s0s, s0n, s0hs, s0w, s0vs, s1f, s1t, s1s, s1n, s1hs, s1w, s1vs;
                 unsigned s0neg, s1neg; uint32_t imm, desc; unsigned eot; } dec_t;

static unsigned campo(const uint32_t *w, unsigned hi, unsigned lo)
{
    unsigned v = 0;
    for (unsigned b = hi + 1; b-- > lo;) v = v << 1 | ((w[b / 32] >> (b % 32)) & 1u);
    return v;
}

/* Tabla de campos de Gen8-11 escrita aparte (Gen9 PRM vol. 2a, «EU instruction formats»). */
static dec_t decodifica(const uint32_t *w)
{
    dec_t d;
    memset(&d, 0, sizeof d);
    d.op = campo(w, 6, 0); d.ex = campo(w, 23, 21); d.sat = campo(w, 31, 31); d.sfid = campo(w, 27, 24);
    d.df = campo(w, 36, 35); d.dt = campo(w, 40, 37); d.ds = campo(w, 52, 48); d.dn = campo(w, 60, 53);
    d.s0f = campo(w, 42, 41); d.s0t = campo(w, 46, 43); d.s0s = campo(w, 68, 64); d.s0n = campo(w, 76, 69);
    d.s0hs = campo(w, 81, 80); d.s0w = campo(w, 84, 82); d.s0vs = campo(w, 88, 85);
    d.s0neg = campo(w, 78, 78); d.s1neg = campo(w, 110, 110);   /* src0/src1_negate (brw_inst.h, Gen4-11) */
    d.s1f = campo(w, 90, 89); d.s1t = campo(w, 94, 91); d.s1s = campo(w, 100, 96); d.s1n = campo(w, 108, 101);
    d.s1hs = campo(w, 113, 112); d.s1w = campo(w, 116, 114); d.s1vs = campo(w, 120, 117);
    d.imm = w[3]; d.desc = w[3] & 0x7FFFFFFFu; d.eot = w[3] >> 31;
    return d;
}

static float grf[128][8];

static float le_op(unsigned f, unsigned nr, unsigned sub, unsigned vs, unsigned wd, unsigned hs, uint32_t imm, int canal)
{
    if (f == I9_IMM) { float x; memcpy(&x, &imm, 4); return x; }
    unsigned ev = vs == 0 ? 0 : 1u << (vs - 1), ew = 1u << wd, eh = hs == 0 ? 0 : 1u << (hs - 1);
    unsigned el = sub / 4 + (unsigned)(canal / (int)ew) * ev + (unsigned)(canal % (int)ew) * eh;
    return grf[nr + el / 8][el % 8];
}

static float satf(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

/* Texturas de la simulación (una por muestreador): 4x4, vecino más próximo, coordenadas normalizadas sujetas. */
static uint32_t tex_simu[4][16];
#define tex_sim tex_simu[0]
static uint32_t rt_sim[8][4];
static int rt_escrito;
/* D457: lo que el sombreador de vértices escribe en la URB (8 ranuras de 4 floats por vértice). */
static float urb_sim[8][8][4];
static int urb_escritas, urb_eot, urb_cab_mal;

static int simula(const uint32_t *p, int n)
{
    for (int i = 0; i < n; i++) {
        dec_t d = decodifica(p + 4 * i);
        if (d.ex != 3) return -1;                          /* todo es de 8 canales */
        if (d.op == 1 && d.dt == 0 && d.s0t == 0) {        /* D457: MOV de 32 bits sin tipo (la cabecera de la URB) */
            if (d.df != I9_GRF || d.s0f != I9_GRF || d.ds || d.s0s || d.s0vs != 4 || d.s0w != 3 || d.s0hs != 1) return -6;
            if (!campo(p + 4 * i, 34, 34)) return -7;     /* NoMask: los 8 asideros, también los de los canales apagados */
            memcpy(grf[d.dn], grf[d.s0n], sizeof grf[0]);
            continue;
        }
        if (d.op == 56) {                                  /* D457: MATH (SQRT, RSQ, INV) */
            if (d.dt != 7 || d.s0t != 7 || d.s1f != 0 || d.s1n != 0) return -8;
            float r[8];
            for (int c = 0; c < 8; c++) {
                float a = le_op(d.s0f, d.s0n, d.s0s, d.s0vs, d.s0w, d.s0hs, d.imm, c);
                r[c] = d.sfid == 4 ? sqrtf(a) : d.sfid == 5 ? 1.0f / sqrtf(a) : d.sfid == 1 ? 1.0f / a : NAN;
            }
            memcpy(grf[d.dn], r, sizeof r);
            continue;
        }
        if (d.op == 49 && d.sfid == I9_SFID_URB) {         /* D457: escritura SIMD8 en la URB */
            unsigned mlen = (d.desc >> 25) & 0xF, ranura = (d.desc >> 4) & 0x7FF;
            if (!(d.desc & (1u << 19)) || (d.desc & 0xF) != 7 || ((d.desc >> 20) & 0x1F) || (mlen - 1) % 4 || d.s1f != I9_IMM) return -9;
            if (d.eot && (d.s0n < 112 || d.s0n + mlen > 128)) return -10;
            if (memcmp(grf[d.s0n], grf[1], sizeof grf[0])) urb_cab_mal++;
            for (unsigned r = 0; r < (mlen - 1) / 4; r++)
                for (int c = 0; c < 8; c++)
                    for (int k = 0; k < 4; k++)
                        if (ranura + r < 8) urb_sim[c][ranura + r][k] = grf[d.s0n + 1 + 4 * r + k][c];
            urb_escritas += (int)((mlen - 1) / 4);
            if (d.eot) { urb_eot = 1; if (i != n - 1) return -11; }
            continue;
        }
        if (d.op == 64 || d.op == 65 || d.op == 1) {
            if (d.dt != 7 || d.df != I9_GRF || d.ds) return -2;
            float r[8];
            for (int c = 0; c < 8; c++) {
                float a = le_op(d.s0f, d.s0n, d.s0s, d.s0vs, d.s0w, d.s0hs, d.imm, c);
                float b = d.op == 1 ? 0 : le_op(d.s1f, d.s1n, d.s1s, d.s1vs, d.s1w, d.s1hs, d.imm, c);
                if (d.s0neg && d.s0f != I9_IMM) a = -a;
                if (d.s1neg && d.s1f != I9_IMM) b = -b;
                r[c] = d.op == 64 ? a + b : d.op == 65 ? a * b : a;
                if (d.sat) r[c] = satf(r[c]);
            }
            memcpy(grf[d.dn], r, sizeof r);
        } else if (d.op == 49 && d.sfid == I9_SFID_SAMPLER) {
            unsigned smp = (d.desc >> 8) & 0xF;
            if (smp > 3 || d.desc != i9_desc_sample_simd8(1 + smp, smp) || d.s1f != I9_IMM) return -3;
            for (int c = 0; c < 8; c++) {
                float u = grf[d.s0n][c], v = grf[d.s0n + 1][c];
                int x = (int)floorf(u * 4), y = (int)floorf(v * 4);
                x = x < 0 ? 0 : x > 3 ? 3 : x; y = y < 0 ? 0 : y > 3 ? 3 : y;
                uint32_t t = tex_simu[smp][y * 4 + x];
                grf[d.dn][c] = (float)((t >> 16) & 0xFF) / 255.0f;
                grf[d.dn + 1][c] = (float)((t >> 8) & 0xFF) / 255.0f;
                grf[d.dn + 2][c] = (float)(t & 0xFF) / 255.0f;
                grf[d.dn + 3][c] = (float)(t >> 24) / 255.0f;
            }
        } else if (d.op == 50 && d.sfid == I9_SFID_RENDER) {
            if (d.desc != i9_desc_rt_simd8(0) || !d.eot || d.s0n < 112 || d.s0n + 4 > 128 || i != n - 1) return -4;
            for (int c = 0; c < 8; c++)
                for (int k = 0; k < 4; k++) rt_sim[c][k] = (uint32_t)lrintf(satf(grf[d.s0n + k][c]) * 255.0f);
            rt_escrito = 1;
        } else return -5;
    }
    return 0;
}

/* Datos de un atributo (2 registros desde g4 + 2k): por componente [v1-v0, v2-v0, -, v0]. */
static void atributo(int k, const float v[3][4])
{
    for (int c = 0; c < 4; c++) {
        float *q = &grf[I9_PS_GRF_SETUP + 2 * k + c / 2][(c & 1) * 4];
        q[0] = v[1][c] - v[0][c]; q[1] = v[2][c] - v[0][c]; q[2] = 0; q[3] = v[0][c];
    }
}

static void sombreadores(void)
{
    static uint32_t ps[I9_PS_MAX_INST * 4];
    const float cor[3][4] = { { 1, 0, 0, 1 }, { 0, 1, 0, 0.5f }, { 0, 0, 1, 0.25f } };
    const float esp[3][4] = { { 0.1f, 0.2f, 0.3f, 0 }, { 0.1f, 0.2f, 0.3f, 0 }, { 0.1f, 0.2f, 0.3f, 0 } };
    const float uv[3][4] = { { 0, 0, 0, 1 }, { 1, 0, 0, 1 }, { 0, 1, 0, 1 } };
    for (int i = 0; i < 16; i++) tex_sim[i] = 0xFF000000u | (uint32_t)(i * 16) << 16 | (uint32_t)(255 - i * 16) << 8 | 0x40u;
    for (int textura = 0; textura < 2; textura++) {
        int n = i9_ps_gerar(ps, 9, textura);
        VERIFICA(n > 0 && n < I9_PS_MAX_INST && (uint32_t)n * 16u <= I9_KERNEL_BYTES - 256u,
                 "sombreador %d: %d instrucciones, cabe con su relleno", textura, n);
        memset(grf, 0, sizeof grf);
        float b1[8], b2[8];
        for (int c = 0; c < 8; c++) { b1[c] = 0.05f + 0.1f * (float)c; b2[c] = 0.4f - 0.04f * (float)c; }
        memcpy(grf[2], b1, sizeof b1); memcpy(grf[3], b2, sizeof b2);
        atributo(0, cor); atributo(1, esp); atributo(2, uv);
        rt_escrito = 0;
        int r = simula(ps, n);
        VERIFICA(r == 0 && rt_escrito, "sombreador %d: el simulador lo ejecuta (%d) y acaba en el RT write con EOT", textura, r);
        int malos = 0;
        for (int c = 0; c < 8; c++) {
            float w0 = 1 - b1[c] - b2[c], f[4], s[4], t[4] = { 1, 1, 1, 1 };
            for (int k = 0; k < 4; k++) {
                f[k] = w0 * cor[0][k] + b1[c] * cor[1][k] + b2[c] * cor[2][k];
                s[k] = w0 * esp[0][k] + b1[c] * esp[1][k] + b2[c] * esp[2][k];
            }
            if (textura) {
                float u = b1[c], v = b2[c];
                int x = (int)floorf(u * 4), y = (int)floorf(v * 4);
                uint32_t tx = tex_sim[(y > 3 ? 3 : y) * 4 + (x > 3 ? 3 : x)];
                t[0] = (float)((tx >> 16) & 0xFF) / 255.0f; t[1] = (float)((tx >> 8) & 0xFF) / 255.0f;
                t[2] = (float)(tx & 0xFF) / 255.0f; t[3] = (float)(tx >> 24) / 255.0f;
            }
            for (int k = 0; k < 4; k++) {
                float esperado = satf(t[k] * f[k] + (k < 3 ? s[k] : 0));
                int e = (int)lrintf(esperado * 255.0f), o = (int)rt_sim[c][k];
                if (e - o > 1 || o - e > 1) { if (!malos) printf("  canal %d comp %d: %d, se esperaba %d\n", c, k, o, e); malos++; }
            }
        }
        VERIFICA(malos == 0, "sombreador %d: color = sat(textura x color + especular) en los 8 píxeles", textura);
        /* Gen11 y Gen12 también se generan (sin probar en hardware). */
        VERIFICA(i9_ps_gerar(ps, 11, textura) == n && i9_ps_gerar(ps, 12, textura) == n, "sombreador %d: también en Gen11/12", textura);
    }
    VERIFICA(i9_ps_gerar(ps, 8, false) == 0 && i9_ps_gerar(ps, 10, true) == 0, "Gen8 y Gen10: no");
}

/* ------------------------------------------------------------------ 2b. sombreadores de v2 (D453) */

/* La referencia: GL_COMBINE de ES 1.1 (docs/gpu3d.md, «El estado v2») en coma flotante. */
static float byte_f(uint32_t c, int k) { return (float)((c >> (k == 3 ? 24 : 16 - 8 * k)) & 0xFF) / 255.0f; }
static float ref_arg(const i9_unid_t *u, const float *tx, const float *pr, const float *pv, int f, int o, int comp)
{
    int k = (o == 2 || o == 3) ? 3 : comp;
    float x = f == 0 ? tx[k] : f == 1 ? byte_f(u->constante, k) : f == 2 ? pr[k] : pv[k];
    return (o == 1 || o == 3) ? 1.0f - x : x;
}
static float ref_func(int f, float a0, float a1, float a2)
{
    switch (f) {
    case 0: return a0;
    case 1: return a0 * a1;
    case 2: return a0 + a1;
    case 3: return a0 + a1 - 0.5f;
    case 4: return a0 * a2 + a1 * (1.0f - a2);
    default: return a0 - a1;
    }
}
static void ref_v2(const i9_ps2_t *c, const float pr[4], const float es[4], const float tx[4][4], float out[4])
{
    float pv[4], r[4];
    memcpy(pv, pr, sizeof pv);
    for (int u = 0; u < c->n; u++) {
        const i9_unid_t *un = &c->u[u];
        if (!un->ativa) continue;
        if (un->rgb_func >= 6) {
            float s = 0;
            for (int k = 0; k < 3; k++)
                s += (ref_arg(un, tx[u], pr, pv, un->rgb_fonte[0], un->rgb_opd[0], k) - 0.5f) *
                     (ref_arg(un, tx[u], pr, pv, un->rgb_fonte[1], un->rgb_opd[1], k) - 0.5f);
            s = satf(4.0f * s * (float)un->rgb_escala);
            r[0] = r[1] = r[2] = s;
            if (un->rgb_func == 7) r[3] = s;
        } else
            for (int k = 0; k < 3; k++)
                r[k] = satf((float)un->rgb_escala * ref_func(un->rgb_func, ref_arg(un, tx[u], pr, pv, un->rgb_fonte[0], un->rgb_opd[0], k),
                            ref_arg(un, tx[u], pr, pv, un->rgb_fonte[1], un->rgb_opd[1], k), ref_arg(un, tx[u], pr, pv, un->rgb_fonte[2], un->rgb_opd[2], k)));
        if (un->rgb_func != 7)
            r[3] = satf((float)un->alfa_escala * ref_func(un->alfa_func, ref_arg(un, tx[u], pr, pv, un->alfa_fonte[0], un->alfa_opd[0], 3),
                        ref_arg(un, tx[u], pr, pv, un->alfa_fonte[1], un->alfa_opd[1], 3), ref_arg(un, tx[u], pr, pv, un->alfa_fonte[2], un->alfa_opd[2], 3)));
        memcpy(pv, r, sizeof pv);
    }
    for (int k = 0; k < 3; k++) {
        out[k] = satf(pv[k] + es[k]);
        if (c->neblina) out[k] = es[3] * out[k] + (1.0f - es[3]) * byte_f(c->cor_neblina, k);
    }
    out[3] = satf(pv[3]);
}

static uint32_t sem = 0x449u;
static uint32_t aleat(uint32_t n) { sem = sem * 1103515245u + 12345u; return (sem >> 8) % n; }

static void sombreadores_v2(void)
{
    static uint32_t ps[I9_PS2_MAX_INST * 4];
    int casos = 0, malos_casos = 0, max_inst = 0;
    for (int t = 0; t < 4; t++)
        for (int i = 0; i < 16; i++) tex_simu[t][i] = aleat(0xFFFFFFFFu) | (uint32_t)aleat(256) << 24;
    for (int caso = 0; caso < 400; caso++) {
        i9_ps2_t c;
        memset(&c, 0, sizeof c);
        c.n = (uint8_t)(1 + aleat(4));
        c.neblina = aleat(3) == 0;
        c.cor_neblina = aleat(0xFFFFFFu);
        for (int u = 0; u < c.n; u++) {
            i9_unid_t *un = &c.u[u];
            un->ativa = caso < 20 || aleat(5) != 0;
            un->rgb_func = (uint8_t)aleat(8); un->alfa_func = (uint8_t)aleat(6);
            static const uint8_t esc[3] = { 1, 2, 4 };
            un->rgb_escala = esc[aleat(3)]; un->alfa_escala = esc[aleat(3)];
            for (int k = 0; k < 3; k++) {
                un->rgb_fonte[k] = (uint8_t)aleat(4); un->rgb_opd[k] = (uint8_t)aleat(4);
                un->alfa_fonte[k] = (uint8_t)aleat(4); un->alfa_opd[k] = (uint8_t)(2 + aleat(2));
            }
            un->constante = aleat(0xFFFFFFFFu) ^ (uint32_t)aleat(256) << 24;
        }
        int n = i9_ps2_gerar(ps, I9_PS2_MAX_INST, 9, &c);
        if (n > max_inst) max_inst = n;
        VERIFICA(n > 0 && (uint32_t)n * 16u <= I9_KERNEL2_BYTES - 256u, "v2 caso %d: se genera (%d instrucciones)", caso, n);
        if (n <= 0) continue;
        memset(grf, 0, sizeof grf);
        float b1[8], b2[8];
        for (int k = 0; k < 8; k++) { b1[k] = 0.03f + 0.11f * (float)k; b2[k] = 0.5f - 0.06f * (float)k; }
        memcpy(grf[2], b1, sizeof b1); memcpy(grf[3], b2, sizeof b2);
        float cor[3][4], esp[3][4], tc[4][3][4];
        for (int vtx = 0; vtx < 3; vtx++)
            for (int k = 0; k < 4; k++) {
                cor[vtx][k] = (float)aleat(256) / 255.0f; esp[vtx][k] = (float)aleat(256) / 255.0f * (k < 3 ? 0.3f : 1.0f);
                for (int u = 0; u < 4; u++) tc[u][vtx][k] = k < 2 ? (float)aleat(1000) / 1000.0f : k == 3 ? 1.0f : 0.0f;
            }
        atributo(0, (const float (*)[4])cor); atributo(1, (const float (*)[4])esp);
        for (int u = 0; u < 4; u++) atributo(2 + u, (const float (*)[4])tc[u]);
        rt_escrito = 0;
        int r = simula(ps, n);
        VERIFICA(r == 0 && rt_escrito, "v2 caso %d: el simulador lo ejecuta (%d)", caso, r);
        int malos = 0;
        for (int px = 0; px < 8; px++) {
            float w0 = 1 - b1[px] - b2[px], pr[4], es[4], tx[4][4], out[4];
            for (int k = 0; k < 4; k++) {
                pr[k] = w0 * cor[0][k] + b1[px] * cor[1][k] + b2[px] * cor[2][k];
                es[k] = w0 * esp[0][k] + b1[px] * esp[1][k] + b2[px] * esp[2][k];
            }
            for (int u = 0; u < 4; u++) {
                float s = w0 * tc[u][0][0] + b1[px] * tc[u][1][0] + b2[px] * tc[u][2][0];
                float tt = w0 * tc[u][0][1] + b1[px] * tc[u][1][1] + b2[px] * tc[u][2][1];
                int x = (int)floorf(s * 4), y = (int)floorf(tt * 4);
                x = x < 0 ? 0 : x > 3 ? 3 : x; y = y < 0 ? 0 : y > 3 ? 3 : y;
                uint32_t tv = tex_simu[u][y * 4 + x];
                tx[u][0] = (float)((tv >> 16) & 0xFF) / 255.0f; tx[u][1] = (float)((tv >> 8) & 0xFF) / 255.0f;
                tx[u][2] = (float)(tv & 0xFF) / 255.0f; tx[u][3] = (float)(tv >> 24) / 255.0f;
            }
            ref_v2(&c, pr, es, (const float (*)[4])tx, out);
            for (int k = 0; k < 4; k++) {
                int e = (int)lrintf(out[k] * 255.0f), o = (int)rt_sim[px][k];
                if (e - o > 2 || o - e > 2) {
                    if (!malos && malos_casos < 3) printf("  v2 caso %d píxel %d comp %d: %d, se esperaba %d\n", caso, px, k, o, e);
                    malos++;
                }
            }
        }
        casos++;
        if (malos) malos_casos++;
    }
    VERIFICA(malos_casos == 0, "v2: %d combinaciones al azar (1 a 4 unidades, todas las funciones, fuentes, operandos, escalas, niebla): "
             "%d mal", casos, malos_casos);
    printf("    v2: %d combinaciones, la más larga %d instrucciones\n", casos, max_inst);
    i9_ps2_t c;
    memset(&c, 0, sizeof c);
    VERIFICA(i9_ps2_gerar(ps, I9_PS2_MAX_INST, 9, &c) > 0, "v2 sin unidades: color + especular");
    VERIFICA(i9_ps2_gerar(ps, I9_PS2_MAX_INST, 12, &c) == 0 && i9_ps2_gerar(ps, 4, 9, &c) == 0, "v2: Gen12 todavía no; sin sitio, 0");
}

/* ------------------------------------------------------------------ 2c. sombreador de vértices de v3 (D457) */

static uint32_t fb(float x) { uint32_t u; memcpy(&u, &x, 4); return u; }
static float bf(uint32_t u) { float x; memcpy(&x, &u, 4); return x; }
static float alea_f(void) { return ((float)aleat(2001) - 1000.0f) / 400.0f; }

/* fila l de la matriz m (por columnas) por v */
static float fila_ref(const uint32_t *m, int l, const float v[4])
{
    return bf(m[l]) * v[0] + bf(m[4 + l]) * v[1] + bf(m[8 + l]) * v[2] + bf(m[12 + l]) * v[3];
}

static void sombreador_v3(void)
{
    static uint32_t vs[I9_VS_MAX_INST * 4];
    int casos = 0, malos_casos = 0, max_inst = 0;
    for (int caso = 0; caso < 300; caso++) {
        i9_vs_t k;
        memset(&k, 0, sizeof k);
        k.n_unid = (uint8_t)aleat(5);
        k.cor = (uint8_t)aleat(3);
        for (int u = 0; u < k.n_unid; u++) { k.tc[u] = (uint8_t)aleat(2); if (aleat(2)) k.tex_mat |= (uint8_t)(1u << u); }
        k.neblina = (uint8_t)aleat(3);
        int n = i9_vs_gerar(vs, I9_VS_MAX_INST, 9, &k);
        VERIFICA(n > 0 && (uint32_t)n * 16u <= I9_KERNELV_BYTES - 256u, "v3: el sombreador de vértices cabe (%d instrucciones)", n);
        if (n <= 0) continue;
        if (n > max_inst) max_inst = n;
        /* las constantes */
        static uint32_t mvp[16], mv[16], tex[4][16], tcc[4][4], cst[16 * 8];
        for (int i = 0; i < 16; i++) { mvp[i] = fb(alea_f()); mv[i] = fb(alea_f()); for (int u = 0; u < 4; u++) tex[u][i] = fb(alea_f()); }
        for (int u = 0; u < 4; u++) for (int j = 0; j < 4; j++) tcc[u][j] = fb(alea_f());
        uint32_t cor = (aleat(256) << 24) | (aleat(256) << 16) | (aleat(256) << 8) | aleat(256), neb_cor = aleat(0x1000000);
        float neb_a = 1.5f + (float)aleat(100) / 50.0f, neb_b = 0.05f + (float)aleat(100) / 200.0f;
        i9_vs_dados_t dd = { mvp, mv, (const uint32_t (*)[16])tex, cor, (const uint32_t (*)[4])tcc, fb(neb_a), fb(neb_b), neb_cor };
        i9_vs_consts(cst, &k, &dd);
        int nc = i9_vs_nc(&k), na = i9_vs_n_atr(&k);
        VERIFICA(nc >= 8 && nc <= 16 && na >= 1 && na <= I9_VS_ATR_MAX, "v3: %d constantes y %d atributos", nc, na);
        memset(grf, 0, sizeof grf);
        for (int c = 0; c < 8; c++) { uint32_t h = 0x1000u + 0x40u * (uint32_t)c; memcpy(&grf[1][c], &h, 4); }
        for (int r = 0; r < nc; r++) for (int j = 0; j < 8; j++) grf[2 + r][j] = bf(cst[8 * r + j]);
        /* los atributos de los 8 vértices (como los deja el VF: lo que falta, 0 y 1) */
        float ent[8][I9_VS_ATR_MAX][4];
        for (int c = 0; c < 8; c++)
            for (int a = 0; a < na; a++)
                for (int j = 0; j < 4; j++) {
                    float x = alea_f();
                    if (a == 1 && k.cor == 1) x = (float)aleat(256) / 255.0f;
                    ent[c][a][j] = x;
                    grf[2 + nc + 4 * a + j][c] = x;
                }
        memset(urb_sim, 0, sizeof urb_sim);
        urb_escritas = urb_eot = urb_cab_mal = 0;
        int r = simula(vs, n);
        VERIFICA(r == 0 && urb_eot && !urb_cab_mal, "v3: el simulador lo ejecuta (%d), termina con EOT y las cabeceras son los asideros de g1", r);
        if (r) continue;
        casos++;
        int malos = 0;
        for (int c = 0; c < 8; c++) {
            float ref[8][4];
            memset(ref, 0, sizeof ref);
            const float *pos = ent[c][0];
            for (int l = 0; l < 4; l++) ref[1][l] = fila_ref(mvp, l, pos);
            int a = 1;
            float cv[4];
            if (k.cor) { for (int j = 0; j < 4; j++) cv[j] = ent[c][a][j]; a++; if (k.cor == 2) for (int j = 0; j < 4; j++) cv[j] = satf(cv[j]); }
            else for (int j = 0; j < 4; j++) cv[j] = byte_f(cor, j);
            float f = 1, esp[4] = { 0, 0, 0, 1 };
            if (k.neblina) {
                float e0 = fila_ref(mv, 0, pos), e1 = fila_ref(mv, 1, pos), e2 = fila_ref(mv, 2, pos);
                f = satf(neb_a - sqrtf(e0 * e0 + e1 * e1 + e2 * e2) * neb_b);
                if (k.neblina == 2) { for (int j = 0; j < 3; j++) { cv[j] *= f; esp[j] = byte_f(neb_cor, j) * (1 - f); } }
                else esp[3] = f;
            }
            for (int j = 0; j < 4; j++) { ref[2][j] = cv[j]; ref[3][j] = esp[j]; }
            for (int u = 0; u < k.n_unid; u++) {
                float tv[4];
                if (k.tc[u]) { for (int j = 0; j < 4; j++) tv[j] = ent[c][a][j]; a++; }
                else for (int j = 0; j < 4; j++) tv[j] = bf(tcc[u][j]);
                for (int j = 0; j < 2; j++) ref[4 + u][j] = (k.tex_mat & (1u << u)) ? fila_ref(tex[u], j, tv) : tv[j];
                ref[4 + u][2] = 0; ref[4 + u][3] = 1;
            }
            int ranuras = 4 + k.n_unid;
            for (int s = 0; s < ranuras; s++)
                for (int j = 0; j < 4; j++) {
                    float e = ref[s][j], o = urb_sim[c][s][j], tol = 1e-4f * (1 + fabsf(e));
                    if (fabsf(e - o) > tol) {
                        if (!malos) printf("  caso %d (unid %d cor %d tc %d%d%d%d mat %x neb %d): vertice %d ranura %d comp %d: %g, se esperaba %g\n",
                                           caso, k.n_unid, k.cor, k.tc[0], k.tc[1], k.tc[2], k.tc[3], k.tex_mat, k.neblina, c, s, j, o, e);
                        malos++;
                    }
                }
        }
        if (malos) malos_casos++;
    }
    VERIFICA(malos_casos == 0 && casos > 250, "v3: %d sombreadores de vértices al azar (color, coordenadas, matrices de textura, niebla en "
             "el alfa y mezclada) contra ES 1.1 en coma flotante: %d mal", casos, malos_casos);
    printf("    v3: %d sombreadores de vértices, el más largo %d instrucciones\n", casos, max_inst);
    i9_vs_t k;
    memset(&k, 0, sizeof k);
    VERIFICA(i9_vs_gerar(vs, I9_VS_MAX_INST, 12, &k) == 0 && i9_vs_gerar(vs, I9_VS_MAX_INST, 8, &k) == 0 && i9_vs_gerar(vs, 10, 9, &k) == 0,
             "v3: ni Gen8 ni Gen12 todavía; sin sitio, 0");
}

/* ------------------------------------------------------------------ 3. recorrido de la tanda */

typedef struct { uint32_t cab; int len; const char *nome; } cmd_t;
static const cmd_t cmds9[] = {
    { 0x7a000004u, 6, "PIPE_CONTROL" }, { 0x61010011u, 19, "STATE_BASE_ADDRESS" }, { 0x78100007u, 9, "3DSTATE_VS" },
    { 0x781b0007u, 9, "3DSTATE_HS" }, { 0x781c0002u, 4, "3DSTATE_TE" }, { 0x781d0009u, 11, "3DSTATE_DS" },
    { 0x78110008u, 10, "3DSTATE_GS" }, { 0x781e0003u, 5, "3DSTATE_STREAMOUT" }, { 0x78120002u, 4, "3DSTATE_CLIP" },
    { 0x78130002u, 4, "3DSTATE_SF" }, { 0x78500003u, 5, "3DSTATE_RASTER" }, { 0x781f0004u, 6, "3DSTATE_SBE" },
    { 0x78510009u, 11, "3DSTATE_SBE_SWIZ" }, { 0x78140000u, 2, "3DSTATE_WM" }, { 0x7820000au, 12, "3DSTATE_PS" },
    { 0x784f0000u, 2, "3DSTATE_PS_EXTRA" }, { 0x784d0000u, 2, "3DSTATE_PS_BLEND" }, { 0x784e0002u, 4, "3DSTATE_WM_DEPTH_STENCIL" },
    { 0x78050006u, 8, "3DSTATE_DEPTH_BUFFER" }, { 0x78060003u, 5, "3DSTATE_STENCIL_BUFFER" },
    { 0x78070003u, 5, "3DSTATE_HIER_DEPTH_BUFFER" }, { 0x78040001u, 3, "3DSTATE_CLEAR_PARAMS" },
    { 0x78080003u, 5, "3DSTATE_VERTEX_BUFFERS (1)" }, { 0x78090009u, 11, "3DSTATE_VERTEX_ELEMENTS (5)" },
    { 0x7809000fu, 17, "3DSTATE_VERTEX_ELEMENTS (8)" },
    { 0x78080007u, 9, "3DSTATE_VERTEX_BUFFERS (2)" }, { 0x7808000bu, 13, "3DSTATE_VERTEX_BUFFERS (3)" },
    { 0x78090003u, 5, "3DSTATE_VERTEX_ELEMENTS (2)" }, { 0x78090005u, 7, "3DSTATE_VERTEX_ELEMENTS (3)" },
    { 0x780a0003u, 5, "3DSTATE_INDEX_BUFFER" },
    { 0x780c0000u, 2, "3DSTATE_VF" }, { 0x784b0000u, 2, "3DSTATE_VF_TOPOLOGY" }, { 0x784a0000u, 2, "3DSTATE_VF_SGVS" },
    { 0x78490001u, 3, "3DSTATE_VF_INSTANCING" }, { 0x680b0000u, 1, "3DSTATE_VF_STATISTICS" }, { 0x7b000005u, 7, "3DPRIMITIVE" },
    { 0x78300000u, 2, "3DSTATE_URB_VS" }, { 0x78310000u, 2, "3DSTATE_URB_HS" }, { 0x78320000u, 2, "3DSTATE_URB_DS" },
    { 0x78330000u, 2, "3DSTATE_URB_GS" }, { 0x79120000u, 2, "PUSH_CONSTANT_ALLOC_VS" }, { 0x79130000u, 2, "PUSH_CONSTANT_ALLOC_HS" },
    { 0x79140000u, 2, "PUSH_CONSTANT_ALLOC_DS" }, { 0x79150000u, 2, "PUSH_CONSTANT_ALLOC_GS" },
    { 0x79160000u, 2, "PUSH_CONSTANT_ALLOC_PS" }, { 0x78150009u, 11, "3DSTATE_CONSTANT_VS" }, { 0x78190009u, 11, "3DSTATE_CONSTANT_HS" },
    { 0x781a0009u, 11, "3DSTATE_CONSTANT_DS" }, { 0x78160009u, 11, "3DSTATE_CONSTANT_GS" }, { 0x78170009u, 11, "3DSTATE_CONSTANT_PS" },
    { 0x78260000u, 2, "BINDING_TABLE_POINTERS_VS" }, { 0x78270000u, 2, "BINDING_TABLE_POINTERS_HS" },
    { 0x78280000u, 2, "BINDING_TABLE_POINTERS_DS" }, { 0x78290000u, 2, "BINDING_TABLE_POINTERS_GS" },
    { 0x782a0000u, 2, "BINDING_TABLE_POINTERS_PS" }, { 0x782f0000u, 2, "SAMPLER_STATE_POINTERS_PS" },
    { 0x78240000u, 2, "BLEND_STATE_POINTERS" }, { 0x780e0000u, 2, "CC_STATE_POINTERS" }, { 0x78230000u, 2, "VIEWPORT_POINTERS_CC" },
    { 0x78210000u, 2, "VIEWPORT_POINTERS_SF_CLIP" }, { 0x780f0000u, 2, "SCISSOR_STATE_POINTERS" },
    { 0x79000002u, 4, "3DSTATE_DRAWING_RECTANGLE" }, { 0x780d0000u, 2, "3DSTATE_MULTISAMPLE" }, { 0x78180000u, 2, "3DSTATE_SAMPLE_MASK" },
    { 0x78520003u, 5, "3DSTATE_WM_HZ_OP" }, { 0x79060000u, 2, "3DSTATE_POLY_STIPPLE_OFFSET" },
    { 0x790a0001u, 3, "3DSTATE_AA_LINE_PARAMETERS" }, { 0x784c0000u, 2, "3DSTATE_WM_CHROMAKEY" },
    { 0x791c0007u, 9, "3DSTATE_SAMPLE_PATTERN" }, { 0x11000001u, 3, "MI_LOAD_REGISTER_IMM (1)" }, { 0x00000000u, 1, "MI_NOOP" },
};

static int busca(uint32_t cab, uint32_t sel)
{
    for (unsigned i = 0; i < sizeof cmds9 / sizeof cmds9[0]; i++)
        if (cmds9[i].cab == (cab & sel)) return (int)i;
    return -1;
}

static const uint32_t lri_permitidos[] = {
    0x7014, 0xE194, 0xE4F0, 0x7004, 0xE188, 0x7300, 0xE184, 0xE180, 0x2580, 0xE100, 0x7034, 0x20D8, 0xB118,
};

/* Recorre «n» palabras de un lote como lo haría el CS: cada cabecera conocida con su longitud y las
 * reglas de los PIPE_CONTROL y los LRI. Devuelve las posiciones de lo que interesa en «pos». */
typedef struct { int prim[8], n_prim, ultimo_pc, vf_inval, tlb, depth, sbe, urb, ve, vb, ps[8], n_ps, bl[8], n_bl, bt[8], n_bt; } pos_t;

static int recorre(const uint32_t *c, int n, uint32_t scratch, pos_t *q, int *vistos)
{
    memset(q, 0, sizeof *q);
    q->ultimo_pc = q->vf_inval = q->tlb = q->depth = q->sbe = q->urb = q->ve = q->vb = -1;
    int i = 0, pc_vazio = -1;
    while (i < n) {
        uint32_t h = c[i];
        int k = busca(h, 0xFFFFFFFFu);
        if (k < 0 && (h & 0xFFFFFF00u) == 0x69040300u) { i++; continue; }   /* PIPELINE_SELECT 3D (Mask 3) */
        VERIFICA(k >= 0, "palabra %d: cabecera %08x conocida", i, h);
        if (k < 0) return -1;
        vistos[k]++;
        const char *nm = cmds9[k].nome;
        if (!strcmp(nm, "PIPE_CONTROL")) {
            uint32_t fl = c[i + 1];
            if (fl & I9_PC_CS_STALL)
                VERIFICA(fl & (I9_PC_RT_FLUSH | I9_PC_DEPTH_FLUSH | I9_PC_DC_FLUSH | I9_PC_WRITE_IMM | I9_PC_DEPTH_STALL | I9_PC_STALL_SCOREBOARD),
                         "palabra %d: PIPE_CONTROL con CS stall y algo que vaciar o escribir (%08x)", i, fl);
            if (fl & I9_PC_TLB_INVAL) {
                q->tlb = i;
                VERIFICA((fl & I9_PC_CS_STALL) && (fl & I9_PC_WRITE_IMM), "el TLB se invalida con CS stall y una escritura");
            }
            if (fl == 0) pc_vazio = i;
            if (fl & I9_PC_VF_INVAL) {
                q->vf_inval = i;
                VERIFICA(pc_vazio == i - 6, "Gen9: el PIPE_CONTROL vacío va justo antes del que invalida el VF");
                VERIFICA((fl & I9_PC_WRITE_IMM) && c[i + 2] == scratch, "Gen9: el del VF lleva una escritura (al scratch)");
            }
            if (fl & I9_PC_WRITE_IMM) VERIFICA(c[i + 2] && !(c[i + 2] & 7), "palabra %d: escritura con dirección alineada", i);
            q->ultimo_pc = i;
        }
        if (!strcmp(nm, "MI_LOAD_REGISTER_IMM (1)")) {
            int ok = 0;
            for (unsigned j = 0; j < sizeof lri_permitidos / 4; j++) if (c[i + 1] == lri_permitidos[j]) ok = 1;
            VERIFICA(ok, "LRI a %x: registro de la lista (workarounds, L3, CS_DEBUG_MODE2, CACHE_MODE_1, ajustes)", c[i + 1]);
            VERIFICA(q->n_prim == 0, "los LRI van antes del primer dibujo");
        }
        if (!strcmp(nm, "3DPRIMITIVE") && q->n_prim < 8) q->prim[q->n_prim++] = i;
        if (!strcmp(nm, "3DSTATE_PS") && q->n_ps < 8) q->ps[q->n_ps++] = i;
        if (!strcmp(nm, "BLEND_STATE_POINTERS") && q->n_bl < 8) q->bl[q->n_bl++] = i;
        if (!strcmp(nm, "BINDING_TABLE_POINTERS_PS") && q->n_bt < 8) q->bt[q->n_bt++] = i;
        if (!strcmp(nm, "3DSTATE_DEPTH_BUFFER")) q->depth = i;
        if (!strcmp(nm, "3DSTATE_SBE")) q->sbe = i;
        if (!strcmp(nm, "3DSTATE_URB_VS")) q->urb = i;
        if (!strcmp(nm, "3DSTATE_VERTEX_ELEMENTS (5)")) q->ve = i;
        if (!strcmp(nm, "3DSTATE_VERTEX_BUFFERS (1)")) q->vb = i;
        i += cmds9[k].len;
    }
    VERIFICA(i == n, "el recorrido acaba justo al final (%d de %d)", i, n);
    return 0;
}

/* Lo que el GPU vería en el hueco de estado en el desplazamiento «o» (desde la base). */
static const uint32_t *em(const uint32_t *est, uint32_t est_off, uint32_t o) { return est + (o - est_off) / 4; }

static void lote(void)
{
    static uint32_t c[4096], est[0x8000 / 4];
    const i9_wa_t *wa;
    int nwa = i9_workarounds(9, I9_PLAT_CFL, &wa);
    uint32_t aj_r[1] = { 0xB118u }, aj_v[1] = { 0x12345678u };
    const uint32_t est_off = 0x8000;
    i9_lote_cfg_t cf;
    memset(&cf, 0, sizeof cf);
    cf.gen = 9; cf.base = 0x00A20000u; cf.marca_ggtt = 0x00A30200u; cf.marca = 0x77; cf.scratch = 0x00A31000u;
    cf.k_cor = 0x42000; cf.k_tex = 0x42800;
    cf.cor = 0x38000000u; cf.cor_passo = i9_passo_cor(128); cf.larg = 128; cf.alt = 96; cf.cor_mocs = 1;
    cf.z = 0x00AC0000u; cf.z_passo = i9_passo_z(128, 24); cf.zbits = 24;
    cf.vb = 0x00A72000u; cf.vb_bytes = 0x18000; cf.tlb = true;
    cf.wa = wa; cf.n_wa = nwa; cf.aj_reg = aj_r; cf.aj_val = aj_v; cf.n_aj = 1;
    i9_lote_t l;
    VERIFICA(i9_lote_abrir(&l, &cf, c, 4096, est, est_off, 0x8000) == 0, "el lote se abre");
    int prologo = l.n;
    i9_desenho_t d1, d2;
    memset(&d1, 0, sizeof d1);
    d1.formato = I9_FMT_V1; d1.k_ps = cf.k_tex; d1.n_tex = 1; d1.mascara = 15;
    d1.t[0].tex = 0x00B00000u; d1.t[0].passo = 64; d1.t[0].larg = 4; d1.t[0].alt = 4; d1.t[0].env_s = 0; d1.t[0].env_t = 1;
    d1.z_teste = true; d1.z_escrita = true; d1.func_z = (uint8_t)i9_func_z(0); d1.mezcla = true;
    d1.f_orig = (uint8_t)i9_fator(2); d1.f_dest = (uint8_t)i9_fator(3);
    d2 = d1; d2.n_tex = 0; d2.k_ps = cf.k_cor; d2.mezcla = false; d2.f_orig = 1; d2.f_dest = 0x11;
    memset(&d2.t, 0, sizeof d2.t);
    VERIFICA(i9_lote_desenho(&l, &d1, 0, 6) == 0, "primer dibujo (con textura y mezcla)");
    int tras1 = l.n;
    VERIFICA(i9_lote_desenho(&l, &d1, 6 * 32, 3) == 0 && l.n - tras1 == 7, "el mismo estado: solo el 3DPRIMITIVE (%d palabras)", l.n - tras1);
    VERIFICA(i9_lote_desenho(&l, &d2, 9 * 32, 3) == 0 && l.mudancas == 2 && l.desenhos == 3, "otro estado: se vuelve a poner");
    int n = i9_lote_fechar(&l);
    VERIFICA(n > 0 && n % 2 == 0, "el lote se cierra con un número par de palabras (%d; prólogo %d)", n, prologo);
    VERIFICA(prologo + I9_FIM_PAL <= I9_PROLOGO_PAL && tras1 - prologo <= I9_DESENHO_PAL && l.est_n <= I9_PROLOGO_EST + 2 * I9_DESENHO_EST,
             "los máximos declarados alcanzan (prólogo %d, dibujo %d, estado %u)", prologo, tras1 - prologo, l.est_n);
    if (n <= 0) return;
    printf("    lote de prueba: %d palabras (prólogo %d, %d workarounds de contexto), %u bytes de estado\n", n, prologo, nwa, l.est_n);
    int vistos[sizeof cmds9 / sizeof cmds9[0]] = { 0 };
    pos_t q;
    if (recorre(c, n, cf.scratch, &q, vistos) < 0) return;
    VERIFICA(q.n_prim == 3 && c[q.prim[0] + 1] == 4 && c[q.prim[0] + 2] == 6 && c[q.prim[0] + 3] == 0 && c[q.prim[0] + 4] == 1 &&
             c[q.prim[1] + 2] == 3 && c[q.prim[1] + 3] == 6 && c[q.prim[2] + 3] == 9,
             "3DPRIMITIVE: lista de triángulos, cuántos vértices y desde cuál, 1 instancia");
    VERIFICA(q.ultimo_pc > q.prim[2] && c[q.ultimo_pc + 1] == (I9_PC_RT_FLUSH | I9_PC_DEPTH_FLUSH | I9_PC_DC_FLUSH | I9_PC_CS_STALL |
             I9_PC_WRITE_IMM | I9_PC_GGTT) && c[q.ultimo_pc + 2] == cf.marca_ggtt && c[q.ultimo_pc + 4] == cf.marca,
             "la marca al final: vaciado de color, Z y datos, CS stall y escritura en la GGTT (D426: vaciar antes de esperar)");
    VERIFICA(q.vf_inval > 0 && q.vf_inval < q.prim[0], "las cachés de VF, texturas y estado se invalidan antes de dibujar");
    VERIFICA(q.tlb > 0 && q.tlb < q.vf_inval, "con la PPGTT cambiada, el TLB se invalida al principio");
    VERIFICA(q.depth > 0 && c[q.depth + 1] == (1u << 29 | 1u << 28 | 3u << 18 | (cf.z_passo - 1)) && c[q.depth + 2] == cf.z &&
             c[q.depth + 4] == (95u << 18 | 127u << 4), "3DSTATE_DEPTH_BUFFER: 2D, D24_UNORM_X8, paso, dirección y tamaño");
    VERIFICA(q.n_ps == 2 && c[q.ps[0] + 1] == cf.k_tex && c[q.ps[0] + 3] == (1u << 30 | 2u << 18 | 1u << 27) && c[q.ps[0] + 6] == (63u << 23 | 1u) &&
             c[q.ps[0] + 7] == 4u << 16 && c[q.ps[1] + 1] == cf.k_cor && c[q.ps[1] + 3] == (1u << 30 | 2u << 18),
             "3DSTATE_PS dos veces: núcleo de textura (1 muestreador) y luego el de color; SIMD8, 64 hilos, datos desde g4, Vector Mask (D453)");
    VERIFICA(q.ps[0] < q.prim[0] && q.ps[1] > q.prim[1] && q.ps[1] < q.prim[2], "cada estado justo antes de su dibujo");
    VERIFICA(q.sbe > 0 && ((c[q.sbe + 1] >> 22) & 0x3F) == 3 && ((c[q.sbe + 1] >> 5) & 0x3F) == 1 && ((c[q.sbe + 1] >> 11) & 0x1F) == 2 &&
             c[q.sbe + 4] == 0xFFFFFFFFu, "3DSTATE_SBE: 3 atributos desde la VUE (desplazamiento 1, longitud 2), XYZW");
    VERIFICA(q.urb > 0 && c[q.urb + 1] == (4u << 25 | 1u << 16 | 512u), "3DSTATE_URB_VS: desde 32 KB, entradas de 128 bytes, 512");
    VERIFICA(q.ve > 0 && (c[q.ve + 1] >> 16 & 0x1FF) == 0 && (c[q.ve + 5] >> 16 & 0x1FF) == 0xC0 && (c[q.ve + 5] & 0xFFF) == 16 &&
             (c[q.ve + 9] >> 16 & 0x1FF) == 0x85 && (c[q.ve + 9] & 0xFFF) == 24, "elementos: cabecera, posición, color BGRA8, especular, u v");
    VERIFICA(q.vb > 0 && q.vb < q.prim[0] && c[q.vb + 1] == (1u << 14 | 32u) && c[q.vb + 2] == cf.vb && c[q.vb + 4] == cf.vb_bytes,
             "un búfer de vértices para todo el lote (paso 32, su tamaño)");
    for (unsigned k = 0; k < sizeof cmds9 / sizeof cmds9[0]; k++)
        if (strcmp(cmds9[k].nome, "MI_NOOP") && strcmp(cmds9[k].nome, "MI_LOAD_REGISTER_IMM (1)") &&
            strcmp(cmds9[k].nome, "3DSTATE_VERTEX_ELEMENTS (8)") && !strstr(cmds9[k].nome, "(2)") && !strstr(cmds9[k].nome, "(3)") &&
            strcmp(cmds9[k].nome, "3DSTATE_INDEX_BUFFER"))
            VERIFICA(vistos[k] >= 1, "el lote lleva %s", cmds9[k].nome);
    /* Estado dinámico y de superficies: todo dentro del hueco, alineado. */
    VERIFICA(q.n_bt == 2 && q.n_bl == 2, "tabla de enlaces y mezcla, una por cambio de estado");
    uint32_t o_bt = c[q.bt[0] + 1], o_bl = c[q.bl[0] + 1] & ~63u;
    VERIFICA(o_bt >= est_off && o_bt + 8 <= est_off + l.est_n && !(o_bt & 31) && o_bl >= est_off && !(c[q.bl[0] + 1] & 62),
             "punteros de estado dentro del hueco y alineados");
    const uint32_t *bt = em(est, est_off, o_bt);
    VERIFICA(bt[0] == l.o_rt && !(bt[1] & 63), "tabla de enlaces: destino y textura");
    const uint32_t *rt = em(est, est_off, bt[0]), *tx = em(est, est_off, bt[1]);
    VERIFICA(rt[0] == (1u << 29 | 0xC0u << 18 | 1u << 16 | 1u << 14) && rt[1] == 1u << 25 && rt[2] == (95u << 16 | 127u) &&
             rt[3] == cf.cor_passo - 1 && rt[8] == cf.cor && rt[7] == (4u << 25 | 5u << 22 | 6u << 19 | 7u << 16),
             "superficie del destino: 2D lineal BGRA8, 128x96, MOCS 1 (RAM con caché)");
    VERIFICA(tx[1] == 0 && tx[2] == (3u << 16 | 3u) && tx[3] == 63 && tx[8] == d1.t[0].tex, "superficie de la textura: 4x4, paso 64, MOCS 0");
    const uint32_t *b = em(est, est_off, o_bl);
    VERIFICA(b[1] == (1u << 31 | 3u << 26 | 19u << 21 | 3u << 13 | 19u << 8), "mezcla: SRC_ALPHA, INV_SRC_ALPHA, ADD");
    int sp = -1;
    for (int i = 0; i < n; i++) if (c[i] == 0x782f0000u) { sp = i; break; }
    const uint32_t *sm = em(est, est_off, c[sp + 1]);
    VERIFICA(sp > 0 && sm[0] == 2u << 27 && sm[2] == l.o_borda && sm[3] == (0u << 6 | 2u << 3 | 2u),
             "muestreador: vecino más próximo, repetir en S, sujetar en T, borde en el hueco");
    const uint32_t *b2 = em(est, est_off, c[q.bl[1] + 1] & ~63u);
    VERIFICA(b2[1] == (1u << 26 | 17u << 21 | 1u << 13 | 17u << 8), "segundo estado: sin mezcla (ONE, ZERO)");
    /* Lo que no cuadra se niega. */
    VERIFICA(i9_lote_desenho(&l, &d1, 0x18000 - 3 * 32, 6) < 0, "vértices fuera del búfer: se niega");
    i9_lote_cfg_t m = cf;
    m.cor_passo = 100;
    VERIFICA(i9_lote_abrir(&l, &m, c, 4096, est, est_off, 0x8000) < 0, "paso del color sin alinear: se niega");
    VERIFICA(i9_lote_abrir(&l, &cf, c, 4096, est, 0xC000, 0x8000) < 0, "estado por encima de 64 KB: se niega");
    VERIFICA(i9_lote_abrir(&l, &cf, c, 40, est, est_off, 0x8000) < 0, "sin sitio para el prólogo: se niega");
    m = cf; m.gen = 8;
    VERIFICA(i9_lote_abrir(&l, &m, c, 4096, est, est_off, 0x8000) < 0, "Gen8: se niega");
    VERIFICA(i9_lote_abrir(&l, &cf, c, 4096, est, est_off, 0x8000) == 0 && i9_lote_desenho(&l, &d1, 0, 5) < 0 && i9_lote_fechar(&l) < 0,
             "vértices que no son de 3 en 3: se niega y el lote queda con error");
    /* Llenar el lote: i9_lote_cabe dice cuándo mandarlo, y nunca se pasa. */
    m = cf; m.tlb = false;
    VERIFICA(i9_lote_abrir(&l, &m, c, 4096, est, est_off, 0x8000) == 0, "otro lote");
    int k = 0;
    while (i9_lote_cabe(&l) && k < 100000) {
        i9_desenho_t d = (k & 1) ? d1 : d2;
        if (i9_lote_desenho(&l, &d, 0, 3) != 0) break;
        k++;
    }
    n = i9_lote_fechar(&l);
    VERIFICA(n > 0 && k > 50 && n <= 4096 && l.est_n <= 0x8000, "%d dibujos con el estado cambiando cada vez caben en 16 KB y 32 KB (%d palabras, %u bytes)",
             k, n, l.est_n);
    if (n > 0) { memset(vistos, 0, sizeof vistos); recorre(c, n, m.scratch, &q, vistos); VERIFICA(q.tlb < 0, "sin TLB si la PPGTT no cambió"); }

    /* D453: un lote con esténcil y un dibujo de v2 (cuatro texturas, esténcil, prueba alfa, tijera, mezcla
     * aparte para el alfa, máscara de canales) detrás de uno de v1. */
    m = cf; m.est = 0x00AD0000u; m.est_passo = i9_passo_est(150); m.perfil_ggtt = 0x00A30400u;
    VERIFICA(i9_lote_abrir(&l, &m, c, 4096, est, est_off, 0x8000) == 0, "lote con esténcil");
    i9_desenho_t d3;
    memset(&d3, 0, sizeof d3);
    d3.formato = I9_FMT_V2; d3.k_ps = 0x43000; d3.n_tex = 4; d3.mascara = 1 | 4;   /* R y B */
    for (int u = 0; u < 4; u++) { d3.t[u] = d1.t[0]; d3.t[u].tex = 0x00B00000u + 0x1000u * (uint32_t)u; d3.t[u].env_s = 2; d3.t[u].mag_linear = 1; }
    d3.z_teste = true; d3.func_z = (uint8_t)i9_cmp(G3D_CMP_MAIOR);
    d3.est = true; d3.est_func = (uint8_t)i9_cmp(G3D_CMP_IGUAL); d3.est_ref = 0x5A; d3.est_mascara = 0xF0; d3.est_escrita = 0x0F;
    d3.est_falha = (uint8_t)i9_op_est(G3D_EST_INVERTER); d3.est_zfalha = (uint8_t)i9_op_est(G3D_EST_INCR_CICLO);
    d3.est_zpassa = (uint8_t)i9_op_est(G3D_EST_SUBSTITUIR);
    d3.alfa = true; d3.alfa_func = (uint8_t)i9_cmp(G3D_CMP_MAIOR_IGUAL); d3.alfa_ref = 0x80;
    d3.tijera = true; d3.tx = 10; d3.ty = 20; d3.tl = 500; d3.ta = 30;
    d3.mezcla = true; d3.alfa_sep = true; d3.f_orig = (uint8_t)i9_fator2(G3D_F_COR_DEST); d3.f_dest = (uint8_t)i9_fator2(G3D_F_ZERO);
    d3.eq = (uint8_t)i9_eq(G3D_EQ_SUBTRAI_INV); d3.f_orig_a = (uint8_t)i9_fator2(G3D_F_UM); d3.f_dest_a = (uint8_t)i9_fator2(G3D_F_UM);
    d3.eq_a = (uint8_t)i9_eq(G3D_EQ_SOMA);
    VERIFICA(i9_lote_desenho(&l, &d2, 0, 3) == 0 && i9_lote_desenho(&l, &d3, 3 * 32, 3) == 0 && i9_lote_desenho(&l, &d2, 3 * 32 + 3 * 88, 3) == 0,
             "v1, v2 y v1 en el mismo lote");
    n = i9_lote_fechar(&l);
    VERIFICA(n > 0, "se cierra (%d palabras)", n);
    if (n <= 0) return;
    memset(vistos, 0, sizeof vistos);
    recorre(c, n, m.scratch, &q, vistos);
    int ve8 = -1, sbe2 = -1, wds = -1, psx = -1, psb = -1, stb = -1, scp = -1, n_vb = 0, n_ts = 0;
    for (int i = 0, k; i < n; i += k >= 0 ? cmds9[k].len : 1) {
        k = busca(c[i], 0xFFFFFFFFu);
        if (k < 0) continue;
        const char *nm = cmds9[k].nome;
        if (!strcmp(nm, "3DSTATE_VERTEX_ELEMENTS (8)")) ve8 = i;
        if (!strcmp(nm, "3DSTATE_SBE") && ((c[i + 1] >> 22) & 0x3F) == 6) sbe2 = i;
        if (!strcmp(nm, "3DSTATE_VERTEX_BUFFERS (1)")) n_vb++;
        if (!strcmp(nm, "PIPE_CONTROL") && ((c[i + 1] >> 14) & 3) == 3) n_ts++;
        if (q.n_prim >= 2 && i > q.prim[0] && i < q.prim[1]) {
            if (!strcmp(nm, "3DSTATE_WM_DEPTH_STENCIL")) wds = i;
            if (!strcmp(nm, "3DSTATE_PS_EXTRA")) psx = i;
            if (!strcmp(nm, "3DSTATE_PS_BLEND")) psb = i;
            if (!strcmp(nm, "SCISSOR_STATE_POINTERS")) scp = i;
        }
        if (!strcmp(nm, "3DSTATE_STENCIL_BUFFER")) stb = i;
    }
    VERIFICA(q.n_prim == 3 && c[q.prim[1] + 3] == 0 && c[q.prim[2] + 3] == 0, "cada cambio de formato empieza su búfer (vértice 0)");
    VERIFICA(n_vb == 3, "tres búferes de vértices (v1, v2, v1): %d", n_vb);
    VERIFICA(n_ts == 5, "las cinco marcas de tiempo del perfil (%d)", n_ts);
    VERIFICA(ve8 > q.prim[0] && ve8 < q.prim[1] && (c[ve8 + 15] & 0xFFF) == 24 + 16 * 3 && sbe2 > 0 && ((c[sbe2 + 1] >> 11) & 0x1F) == 3,
             "v2: 8 elementos (s t de cuatro unidades) y 6 atributos en el SBE (longitud 3)");
    VERIFICA(stb > 0 && c[stb + 1] == (1u << 31 | 383u) && c[stb + 2] == m.est && q.depth > 0 && (c[q.depth + 1] & (1u << 27)),
             "3DSTATE_STENCIL_BUFFER con su paso y dirección, y el Z con la escritura de esténcil");
    VERIFICA(wds > 0 && c[wds + 1] == (7u << 29 | 5u << 26 | 2u << 23 | 3u << 8 | 5u << 5 | 1u << 3 | 1u << 2 | 1u << 1) &&
             c[wds + 2] == (0xF0u << 24 | 0x0Fu << 16) && c[wds + 3] == 0x5Au << 8,
             "WM_DEPTH_STENCIL: INVERT, INCR, REPLACE, EQUAL, GREATER, máscaras y referencia (%08x %08x %08x)", c[wds + 1], c[wds + 2], c[wds + 3]);
    VERIFICA(psx > 0 && (c[psx + 1] & (1u << 28)) && psb > 0 && (c[psb + 1] & (1u << 8)) && (c[psb + 1] & (1u << 7)) &&
             ((c[psb + 1] >> 14) & 0x1F) == 5 && ((c[psb + 1] >> 24) & 0x1F) == 1, "prueba alfa en PS_EXTRA y PS_BLEND, alfa aparte, DST_COLOR");
    VERIFICA(q.n_ps == 3 && c[q.ps[1] + 1] == 0x43000 && c[q.ps[1] + 3] == (1u << 30 | 5u << 18 | 1u << 27), "v2: su sombreador, 5 superficies, muestreadores");
    const uint32_t *b3 = em(est, est_off, c[q.bl[1] + 1] & ~63u);
    VERIFICA(b3[0] == (1u << 30 | 1u << 27 | 7u << 24) && b3[1] == (1u << 31 | 5u << 26 | 0x11u << 21 | 2u << 18 | 1u << 13 | 1u << 8 | 0u << 5 | 1u << 3 | 1u << 1),
             "BLEND_STATE: alfa aparte, prueba alfa GEQUAL, DST_COLOR/ZERO con REVERSE_SUBTRACT, ONE/ONE, solo R y B (%08x %08x)", b3[0], b3[1]);
    const uint32_t *sc = em(est, est_off, c[scp + 1]);
    VERIFICA(scp > 0 && sc[0] == (20u << 16 | 10u) && sc[1] == (49u << 16 | 127u), "tijera recortada al destino (%08x %08x)", sc[0], sc[1]);
    const uint32_t *bt3 = em(est, est_off, c[q.bt[1] + 1]), *sm3 = em(est, est_off, c[(q.bt[1] + 2)] == 0x782f0000u ? c[q.bt[1] + 3] : 0);
    VERIFICA(bt3[0] == l.o_rt && em(est, est_off, bt3[4])[8] == 0x00B03000u, "tabla de enlaces: destino y las cuatro texturas");
    /* Mip Tail Start LOD a 15 en todas las superficies (sin cola de mipmaps, como isl_surface_state.c) y el MIP Count
     * en los bits 3:0 sin pisarlo. */
    VERIFICA(((em(est, est_off, bt3[0])[5] >> 8) & 15u) == 15u && ((em(est, est_off, bt3[4])[5] >> 8) & 15u) == 15u,
             "Mip Tail Start LOD = 15 en el destino y en las texturas");
    VERIFICA(i9_passo_est(320) == 640 && i9_passo_est(64) == 128 && i9_passo_est(1) == 128 && i9_alt_est(240) == 256,
             "esténcil en mosaico W: paso = 2 x ancho alineado a 64 (320 -> 640)");
    {
        i9_lote_cfg_t mm = m;
        mm.est_passo = 192;   /* el ancho sin doblar: no cuadra con el mosaico W */
        static i9_lote_t l2;
        static uint32_t c2[4096], est2[0x8000 / 4];
        VERIFICA(i9_lote_abrir(&l2, &mm, c2, 4096, est2, est_off, 0x8000) != 0, "un paso de esténcil que no es múltiplo de 128 se rechaza");
    }
    VERIFICA(c[q.bt[1] + 2] == 0x782f0000u && ((sm3[12 + 3] >> 6) & 7) == 1 && (sm3[12] & (1u << 17)) && !(sm3[12] & (1u << 14)),
             "cuatro muestreadores: espejo en S, ampliar bilineal, reducir vecino");
    /* Niveles de mipmap: la disposición «2D» de Gen4-9 (como brw_miptree_layout_2d de i965). */
    int mx, my, mw, mh;
    i9_mip_pos(256, 256, 1, &mx, &my); VERIFICA(mx == 0 && my == 256, "mip 1 debajo del 0");
    i9_mip_pos(256, 256, 2, &mx, &my); VERIFICA(mx == 128 && my == 256, "mip 2 a la derecha del 1");
    i9_mip_pos(256, 256, 4, &mx, &my); VERIFICA(mx == 128 && my == 256 + 64 + 32, "mip 4 debajo del 2 y el 3");
    i9_mip_tamanho(256, 256, 9, &mw, &mh); VERIFICA(mw == 256 && mh == 256 + 132, "256x256 con 9 niveles: 256x388 (%dx%d)", mw, mh);
    i9_mip_tamanho(4, 4, 3, &mw, &mh); VERIFICA(mw == 8 && mh == 8, "4x4 con 3 niveles: 8x8 (alineados a 4)");
    i9_mip_tamanho(6, 3, 1, &mw, &mh); VERIFICA(mw == 8 && mh == 4, "un nivel: alineado a 4");
    VERIFICA(i9_ytile_off(0, 0, 256) == 0 && i9_ytile_off(4, 0, 256) == 4 && i9_ytile_off(16, 0, 256) == 512 &&
             i9_ytile_off(0, 1, 256) == 16 && i9_ytile_off(128, 0, 256) == 4096 && i9_ytile_off(0, 32, 256) == 8192 &&
             i9_ytile_off(127, 31, 256) == 7 * 512 + 31 * 16 + 15, "mosaico Y: columnas de 16 bytes y 32 filas en bloques de 4 KB");
    i9_lote_cfg_t mc = cf;
    i9_desenho_t dm = d1;
    dm.t[0].niveis = 3; dm.t[0].mip = 2; dm.t[0].min_linear = 1; dm.t[0].mosaico_y = 1; dm.t[0].passo = 128; dm.t[0].lod_bias = -1;
    VERIFICA(i9_lote_abrir(&l, &mc, c, 4096, est, est_off, 0x8000) == 0 && i9_lote_desenho(&l, &dm, 0, 3) == 0 && i9_lote_fechar(&l) > 0, "lote con mipmaps");
    int smp = -1, btp = -1;
    for (int i = 0; i < l.n; i++) { if (c[i] == 0x782f0000u) smp = i; if (c[i] == 0x782a0000u) btp = i; }
    const uint32_t *smm = em(est, est_off, c[smp + 1]), *txm = em(est, est_off, em(est, est_off, c[btp + 1])[1]);
    VERIFICA(smp > 0 && ((smm[0] >> 20) & 3) == 3 && smm[1] == 2u << 16 && btp > 0 && (txm[5] & 15) == 2 && ((txm[0] >> 12) & 3) == 3 &&
             ((smm[0] >> 1) & 0x1FFF) == 0x1F00, "mipmaps: MipModeFilter LINEAR, Max LOD 2, sesgo -1, MIP Count 2 y mosaico Y");
    VERIFICA(i9_cmp(G3D_CMP_NUNCA) == 1 && i9_cmp(G3D_CMP_SEMPRE) == 0 && i9_op_est(G3D_EST_DECR_CICLO) == 6 && i9_fator2(G3D_F_ALFA_SATURADO) == 6 &&
             i9_logica(G3D_LOG_COPIA) == 12 && i9_logica(1) == 8 && i9_eq(3) < 0 && i9_fator2(11) < 0, "tablas de v2 a la GPU");
}

/* ------------------------------------------------------------------ 3b. un lote con dibujos v3 (D457) */

static void lote_v3(void)
{
    static uint32_t c[4096], est[0x8000 / 4];
    const uint32_t est_off = 0x8000;
    i9_lote_cfg_t cf;
    memset(&cf, 0, sizeof cf);
    cf.gen = 9; cf.base = 0x00A20000u; cf.marca_ggtt = 0x00A30200u; cf.marca = 0x78; cf.scratch = 0x00A31000u;
    cf.k_cor = 0x42000; cf.k_tex = 0x42800;
    cf.cor = 0x38000000u; cf.cor_passo = i9_passo_cor(128); cf.larg = 128; cf.alt = 96; cf.cor_mocs = 1;
    cf.z = 0x00AC0000u; cf.z_passo = i9_passo_z(128, 24); cf.zbits = 24;
    cf.vb = 0x00A72000u; cf.vb_bytes = 0x18000;
    i9_lote_t l;
    VERIFICA(i9_lote_abrir(&l, &cf, c, 4096, est, est_off, 0x8000) == 0, "v3: el lote se abre");
    i9_desenho_t d1, d3;
    memset(&d1, 0, sizeof d1);
    d1.formato = I9_FMT_V1; d1.k_ps = cf.k_cor; d1.mascara = 15; d1.f_orig = 1; d1.f_dest = 0x11;
    d3 = d1; d3.formato = I9_FMT_V2; d3.k_ps = 0x43000; d3.n_tex = 1; d3.t[0].tex = 0x00B00000u; d3.t[0].passo = 64; d3.t[0].larg = 4;
    d3.t[0].alt = 4;
    static uint32_t cst[16 * 8];
    for (int i = 0; i < 16 * 8; i++) cst[i] = 0x3F000000u + (uint32_t)i;
    i9_geo3_t g;
    memset(&g, 0, sizeof g);
    g.k_vs = 0xE8000; g.nc = 8; g.n_atr = 3; g.cst = cst; g.topologia = 5; g.cull = 2; g.frente = 0; g.mocs = 1;
    g.vb[0] = (i9_vb3_t){ 0x3C000040u, 0x1000, 12, I9_VF_RGB32F, 3 };
    g.vb[1] = (i9_vb3_t){ 0x3C00004Cu, 0x1000 - 12, 24, I9_VF_RGBA8, 4 };
    g.vb[2] = (i9_vb3_t){ 0x3C002000u, 0x800, 8, I9_VF_RG32F, 2 };
    g.ib = 0x3C004000u; g.ib_bytes = 64;
    for (int i = 0; i < 6; i++) g.vp[i] = 0x40000000u + (uint32_t)i;
    g.primeiro = 6; g.n = 12;
    VERIFICA(i9_lote_desenho(&l, &d1, 0, 3) == 0, "v3: un dibujo de v1 delante");
    int a0 = l.n;
    VERIFICA(i9_lote_desenho3(&l, &d3, &g) == 0, "v3: el primer dibujo v3");
    int a1 = l.n;
    VERIFICA(i9_lote_desenho3(&l, &d3, &g) == 0, "v3: otro igual");
    int a2 = l.n;
    VERIFICA(a2 - a1 == 13 + 5 + 7 && a1 - a0 <= I9_DESENHO_PAL, "v3: el segundo igual solo pone búferes, índices y el 3DPRIMITIVE (%d palabras; "
             "el primero %d)", a2 - a1, a1 - a0);
    i9_geo3_t g2 = g;
    g2.cst = cst + 8; g2.ib = 0; g2.topologia = 4; g2.n_atr = 2; g2.cull = 1;
    VERIFICA(i9_lote_desenho3(&l, &d3, &g2) == 0, "v3: otro con otras constantes, sin índices, lista y dos atributos");
    VERIFICA(i9_lote_desenho(&l, &d1, 3 * 32, 3) == 0, "v3: y otra vez v1");
    i9_geo3_t gm = g;
    gm.vb[1].fmt = 0x123;
    VERIFICA(i9_lote_desenho3(&l, &d3, &gm) < 0, "v3: un formato que no es de la lista se niega");
    l.erro = false;
    int n = i9_lote_fechar(&l);
    VERIFICA(n > 0 && l.v3_desenhos == 3, "v3: el lote se cierra (%d palabras, %d dibujos v3)", n, l.v3_desenhos);
    if (n <= 0) return;
    int vistos[sizeof cmds9 / sizeof cmds9[0]] = { 0 };
    pos_t q;
    if (recorre(c, n, cf.scratch, &q, vistos) < 0) return;
    VERIFICA(q.n_prim == 5, "v3: cinco 3DPRIMITIVE (%d)", q.n_prim);
    if (q.n_prim != 5) return;
    VERIFICA(c[q.prim[1] + 1] == (1u << 8 | 5u) && c[q.prim[1] + 2] == 12 && c[q.prim[1] + 3] == 6 && c[q.prim[3] + 1] == 4 &&
             c[q.prim[4] + 1] == 4, "v3: 3DPRIMITIVE con índices (acceso aleatorio), tira, 12 desde el 6; luego lista sin índices; v1 lista");
    int vs_on = -1, vs_off = -1, clip_on = -1, clip_off = -1, sf_on = -1, ras_on = -1, cvs = -1, btvs = -1, ib = -1, vb3 = -1, ve3 = -1,
        sbe = -1, vp = -1, n_cvs = 0, n_vs = 0, topo5 = -1, topo4 = -1;
    for (int i = 0, k; i < n; i += k >= 0 ? cmds9[k].len : 1) {
        k = busca(c[i], 0xFFFFFFFFu);
        if (k < 0) continue;
        const char *nm = cmds9[k].nome;
        bool dentro = i > q.prim[0] && i < q.prim[1];
        if (!strcmp(nm, "3DSTATE_VS")) { n_vs++; if ((c[i + 7] & 1) && vs_on < 0) vs_on = i; else if (!(c[i + 7] & 1) && i > q.prim[3]) vs_off = i; }
        if (!strcmp(nm, "3DSTATE_CLIP")) { if (c[i + 2] & (1u << 31)) clip_on = i; else if (i > q.prim[3]) clip_off = i; }
        if (!strcmp(nm, "3DSTATE_SF") && (c[i + 1] & 2)) sf_on = i;
        if (!strcmp(nm, "3DSTATE_RASTER") && dentro) ras_on = i;
        if (!strcmp(nm, "3DSTATE_CONSTANT_VS") && c[i + 2]) { n_cvs++; if (dentro) cvs = i; }
        if (!strcmp(nm, "BINDING_TABLE_POINTERS_VS") && dentro) btvs = i;
        if (!strcmp(nm, "3DSTATE_INDEX_BUFFER") && dentro) ib = i;
        if (!strcmp(nm, "3DSTATE_VERTEX_BUFFERS (3)") && dentro) vb3 = i;
        if (!strcmp(nm, "3DSTATE_VERTEX_ELEMENTS (3)") && dentro) ve3 = i;
        if (!strcmp(nm, "3DSTATE_SBE") && dentro) sbe = i;
        if (!strcmp(nm, "VIEWPORT_POINTERS_SF_CLIP") && dentro) vp = i;
        if (!strcmp(nm, "3DSTATE_VF_TOPOLOGY") && dentro && c[i + 1] == 5) topo5 = i;
        if (!strcmp(nm, "3DSTATE_VF_TOPOLOGY") && i > q.prim[3] && c[i + 1] == 4) topo4 = i;
    }
    VERIFICA(vs_on > 0 && c[vs_on + 1] == g.k_vs && c[vs_on + 6] == (2u << 20 | 2u << 11) && c[vs_on + 7] == (63u << 23 | 1u << 2 | 1u) &&
             c[vs_on + 8] == (1u << 21 | 3u << 16), "v3: 3DSTATE_VS SIMD8 con su núcleo, la URB (constantes y atributos) desde g2, 2 pares de atributos, salida 1/3");
    VERIFICA(n_vs == 4 && vs_off > q.prim[3] && vs_off < q.prim[4] && clip_off > q.prim[3] && clip_off < q.prim[4] && topo4 < 0 && l.topo == 4,
             "v3: el VS en el prólogo, en el primer v3, al cambiar de atributos y al volver a v1 (apagado, con el recorte; la lista ya estaba; %d veces)", n_vs);
    VERIFICA(clip_on > q.prim[0] && clip_on < q.prim[1] && c[clip_on + 2] == (1u << 31 | 1u << 28) && sf_on > 0 &&
             ras_on > 0 && c[ras_on + 1] == (1u << 26 | 2u << 16 | 1u << 1 | 1u), "v3: recorte, vista en el SF, Z cercano/lejano y descarte de caras");
    VERIFICA(cvs > 0 && c[cvs + 2] == 8u << 16 && c[cvs + 9] >= cf.base + est_off && !(c[cvs + 9] & 31) && btvs > cvs && n_cvs == 2,
             "v3: constantes por el búfer 3 (8 registros, dirección entera alineada) y su BINDING_TABLE_POINTERS_VS; una vez por cambio (%d)", n_cvs);
    const uint32_t *k0 = em(est, est_off, c[cvs + 9] - cf.base);
    VERIFICA(k0[0] == cst[0] && k0[63] == cst[63], "v3: las constantes en el hueco de estado");
    VERIFICA(ib > 0 && c[ib + 1] == (1u << 8 | 2u) && c[ib + 2] == g.ib && c[ib + 4] == 64, "v3: 3DSTATE_INDEX_BUFFER de 16 bits, MOCS 1");
    VERIFICA(vb3 > 0 && c[vb3 + 1] == (0u << 26 | 1u << 17 | 1u << 14 | 12u) && c[vb3 + 5] == (1u << 26 | 1u << 17 | 1u << 14 | 24u) &&
             c[vb3 + 6] == g.vb[1].dir && c[vb3 + 8] == g.vb[1].bytes, "v3: un búfer por atributo, con su paso, dirección, tamaño y MOCS 1");
    VERIFICA(ve3 > 0 && c[ve3 + 1] == (1u << 25 | 0x040u << 16) && c[ve3 + 2] == (1u << 28 | 1u << 24 | 1u << 20 | 3u << 16) &&
             c[ve3 + 3] == (1u << 26 | 1u << 25 | 0x0C7u << 16) && c[ve3 + 6] == (1u << 28 | 1u << 24 | 2u << 20 | 3u << 16),
             "v3: elementos (posición RGB32F con w = 1, color RGBA8, s t con 0 y 1)");
    VERIFICA(sbe > 0 && ((c[sbe + 1] >> 22) & 0x3F) == 6 && ((c[sbe + 1] >> 5) & 0x3F) == 1, "v3: el SBE de v2 (6 atributos desde la ranura 2)");
    const uint32_t *v = em(est, est_off, c[vp + 1]);
    VERIFICA(vp > 0 && !(c[vp + 1] & 63) && v[0] == g.vp[0] && v[5] == g.vp[5] && v[8] == 0xBF800000u && v[9] == 0x3F800000u,
             "v3: la vista (m00 ... m32) y la banda de guarda en el SF_CLIP_VIEWPORT");
    VERIFICA(topo5 > 0, "v3: la topología de tira");
}

/* ------------------------------------------------------------------ 4. contexto del RCS */

static void contexto(void)
{
    static uint32_t pag[1024];
    VERIFICA(intel_lrc_paginas_rcs(9) == 22 && intel_lrc_paginas_rcs(12) == 14 && !intel_lrc_tabela_rcs(8),
             "tamaño de la imagen del RCS (i915) y sin Gen8");
    VERIFICA(intel_lrc_monta_tab(pag, 1024, intel_lrc_tabela_rcs(9), 9, 0x2000u, 0, 0x40, 0x00A23000u, 0x7001u, 0x12345000ull),
             "Gen9: imagen del RCS montada");
    VERIFICA(pag[1] == (0x11001000u | 27u) && pag[2] == 0x2244u && pag[4] == 0x2034u && pag[28] == 0x21C8u,
             "Gen9: primer LRI (14 registros, POSTED) con la base del RCS");
    VERIFICA(pag[EX_CTX_RING_TAIL] == 0x40 && pag[EX_CTX_RING_START] == 0x00A23000u && pag[EX_CTX_RING_CTL] == 0x7001u &&
             pag[EX_CTX_PDP0_LDW] == 0x12345000u && pag[EX_CTX_PDP0_LDW - 1] == 0x2270u, "Gen9: anillo y PML4 en sus sitios");
    VERIFICA(pag[65] == 0x11000001u && pag[66] == 0x20C8u && pag[67] == 0, "Gen9: R_PWR_CLK_STATE (CTX_R_PWR_CLK_STATE = 0x43) sin POSTED");
    VERIFICA(pag[81] == (0x11001000u | 87u) && pag[82] == 0x2028u && pag[168] == 0x2068u && pag[170] == 0x05000000u,
             "Gen9: bloque de 44 registros y MI_BATCH_BUFFER_END de cierre");
    VERIFICA(pag[intel_lrc_mi_mode(9) + 1] == EX_STOP_RING << 16, "Gen9: RING_MI_MODE sin STOP_RING");
    VERIFICA(pag[EX_CTX_CONTROL] == 0x000F0009u, "Gen9: CONTEXT_CONTROL con el inhibidor (como i915)");
    VERIFICA(intel_lrc_monta_tab(pag, 1024, intel_lrc_tabela_rcs(11), 11, 0x2000u, 0, 0, 0, 1, 0), "Gen11: imagen del RCS");
    VERIFICA((pag[1] & (1u << 19)) && (pag[1] & 0xFF) == 29 && pag[30] == 0x2180u, "Gen11: LRM_CS_MMIO, 15 registros, 0x180");
    VERIFICA(intel_lrc_monta_tab(pag, 1024, intel_lrc_tabela_rcs(12), 12, 0x2000u, 0, 0, 0, 1, 0), "Gen12: imagen del RCS");
    int bbe = -1;
    for (int i = 0; i < 1024; i++) if (pag[i] == 0x05000001u) { bbe = i; break; }
    VERIFICA(bbe > 150 && pag[bbe - 5] == 0x2068u && pag[bbe - 3] == 0x2084u, "Gen12: el bloque de 51 registros acaba en 0x68 y 0x84 (%d)", bbe);
    VERIFICA(intel_lrc_monta(pag, 1024, 9, 0x22000u, 0, 0, 0, 1, 0) && pag[2] == 0x22244u, "el BCS (D434) sigue igual");
}

/* ------------------------------------------------------------------ 5. workarounds */

static void workarounds(void)
{
    const i9_wa_t *w;
    int n = i9_workarounds(9, I9_PLAT_CFL, &w), ctx = 0, motor = 0, gt = 0, sbe = 0, kill = 0;
    for (int i = 0; i < n; i++) {
        ctx += w[i].tipo == I9_WA_CTX; motor += w[i].tipo == I9_WA_MOTOR; gt += w[i].tipo == I9_WA_GT;
        if (w[i].reg == 0x7014 && w[i].set == 1u << 8) sbe++;
        if (w[i].reg == 0x4090 && w[i].set == 1u << 8) kill++;
        if (w[i].tipo == I9_WA_CTX)
            VERIFICA(w[i].mascarado == 1 && ((w[i].reg >= 0x7000 && w[i].reg < 0x7400) || (w[i].reg >= 0xE000 && w[i].reg < 0xE900) ||
                     w[i].reg == 0x2580), "wa de contexto %x: con máscara, en los registros del contexto", w[i].reg);
        VERIFICA(!(w[i].set & w[i].clr), "wa %x: no pone y quita el mismo bit", w[i].reg);
    }
    VERIFICA(n >= 20 && ctx >= 12 && motor == 6 && gt >= 4, "CFL: %d workarounds (%d de contexto, %d del motor, %d del GT)", n, ctx, motor, gt);
    VERIFICA(sbe == 1 && kill == 0, "CFL: WaToEnableHwFixForPushConstHWBug sí, WaDisableKillLogic no (como i915)");
    n = i9_workarounds(9, I9_PLAT_SKL, &w);
    kill = 0;
    for (int i = 0; i < n; i++) kill += w[i].reg == 0x4090 && w[i].set == 1u << 8;
    VERIFICA(kill == 1, "SKL: WaDisableKillLogic");
    VERIFICA(i9_workarounds(11, I9_PLAT_CFL, &w) == 0 && i9_workarounds(12, I9_PLAT_CFL, &w) == 0, "Gen11/12: todavía ninguno (D438)");
    VERIFICA(i9_plataforma(0x3EA0) == I9_PLAT_CFL && i9_plataforma(0x1916) == I9_PLAT_SKL && i9_plataforma(0x5916) == I9_PLAT_KBL,
             "plataformas: UHD 620 de Whiskey Lake = CFL, 1916 SKL, 5916 KBL");
    VERIFICA(i9_fator(0) == 17 && i9_fator(1) == 1 && i9_fator(2) == 3 && i9_fator(3) == 19 && i9_func_z(0) == 2 &&
             i9_func_z(1) == 4 && i9_func_z(2) == 0, "factores de mezcla y funciones del Z de gpu3d v1");
}

int main(void)
{
    vectores();
    sombreadores();
    sombreadores_v2();
    sombreador_v3();
    lote();
    lote_v3();
    contexto();
    workarounds();
    if (falhas) { printf("%d FALLOS\n", falhas); return 1; }
    printf("intel9_3d_unidad: todo bien\n");
    return 0;
}
