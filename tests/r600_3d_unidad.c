// SPDX-License-Identifier: GPL-2.0-or-later
/* r600_3d_unidad.c - prueba en el PC de r600_3d_pacotes.c (D284): los paquetes PM4,
 * los sombreadores y los valores de inicio del motor de las R7xx.
 *
 * La comparación de bytes de los sombreadores no usa las funciones del driver: aquí
 * hay una segunda codificación, campo a campo (bit de inicio y ancho, como en la
 * documentación del ISA de la R600 y en r600_shader.h de xf86-video-ati), y la
 * prueba exige que las dos den las mismas palabras. Los valores de inicio se
 * comparan con los que da la secuencia de rv770_gpu_init para una RV710 con los
 * registros a cero y con valores de ejemplo.
 *
 *   gcc -O2 -Wall -DR600_ANFITRION -Ikernel/inc tests/r600_3d_unidad.c kernel/dispositivos/r600_3d_pacotes.c */
#include <stdio.h>
#include <string.h>
#include "r600_3d.h"

static int falhas;
#define VERIFICA(c, ...) do { if (!(c)) { falhas++; printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Un campo: valor, bit de inicio, ancho. Comprueba que el valor cabe. */
typedef struct { unsigned v, ini, bits; } campo_t;
static uint32_t junta(const campo_t *c, int n)
{
    uint32_t r = 0;
    for (int i = 0; i < n; i++) {
        if (c[i].bits < 32 && c[i].v >> c[i].bits) { falhas++; printf("FALLO: el campo %d no cabe\n", i); }
        r |= (uint32_t)c[i].v << c[i].ini;
    }
    return r;
}
#define J(...) junta((campo_t[]){ __VA_ARGS__ }, (int)(sizeof((campo_t[]){ __VA_ARGS__ }) / sizeof(campo_t)))

static void compara(const char *que, const uint32_t *a, int na, const uint32_t *b, int nb)
{
    VERIFICA(na == nb, "%s: %d palabras, esperadas %d", que, na, nb);
    for (int i = 0; i < na && i < nb; i++) VERIFICA(a[i] == b[i], "%s: palabra %d = %08x, esperada %08x", que, i, a[i], b[i]);
}

/* Segunda codificación de las instrucciones. */
static void e_cf(uint32_t *p, unsigned ender, unsigned n, unsigned inst)
{
    p[0] = J({ ender, 0, 22 });
    p[1] = J({ 0, 0, 3 }, { 0, 3, 5 }, { 0, 8, 2 }, { (n - 1) & 7, 10, 3 }, { 0, 13, 6 }, { (n - 1) >> 3, 19, 1 }, { 0, 21, 1 },
             { 0, 22, 1 }, { inst, 23, 7 }, { 0, 30, 1 }, { 1, 31, 1 });
}
static void e_cf_alu(uint32_t *p, unsigned ender, unsigned n)
{
    p[0] = J({ ender, 0, 22 }, { 0, 22, 4 }, { 0, 26, 4 }, { 0, 30, 2 });
    p[1] = J({ 0, 0, 2 }, { 0, 2, 8 }, { 0, 10, 8 }, { n - 1, 18, 7 }, { 0, 25, 1 }, { 0x8, 26, 4 }, { 0, 30, 1 }, { 1, 31, 1 });
}
static void e_exp(uint32_t *p, unsigned base, unsigned tipo, unsigned gpr, unsigned inst, unsigned fim, unsigned barreira)
{
    p[0] = J({ base, 0, 13 }, { tipo, 13, 2 }, { gpr, 15, 7 }, { 0, 22, 1 }, { 0, 23, 7 }, { 0, 30, 2 });
    p[1] = J({ 0, 0, 3 }, { 1, 3, 3 }, { 2, 6, 3 }, { 3, 9, 3 }, { 0, 17, 4 }, { fim, 21, 1 }, { 0, 22, 1 }, { inst, 23, 7 },
             { 0, 30, 1 }, { barreira, 31, 1 });
}
static void e_vtx(uint32_t *p, unsigned gpr, unsigned fmt, unsigned num, unsigned comp, unsigned desloc, unsigned bytes,
                  unsigned mega, unsigned sx, unsigned sy, unsigned sz, unsigned sw)
{
    p[0] = J({ 0, 0, 5 }, { 0, 5, 2 }, { 0, 7, 1 }, { 0, 8, 8 }, { 0, 16, 7 }, { 0, 23, 1 }, { 0, 24, 2 }, { bytes - 1, 26, 6 });
    p[1] = J({ gpr, 0, 7 }, { 0, 7, 1 }, { sx, 9, 3 }, { sy, 12, 3 }, { sz, 15, 3 }, { sw, 18, 3 }, { 0, 21, 1 }, { fmt, 22, 6 },
             { num, 28, 2 }, { comp, 30, 1 }, { 0, 31, 1 });
    p[2] = J({ desloc, 0, 16 }, { 0, 16, 2 }, { 0, 18, 1 }, { mega, 19, 1 });
    p[3] = 0;
}
static void e_alu(uint32_t *p, unsigned inst, unsigned dst, unsigned s0, unsigned s1, unsigned comp, unsigned ultima, int r7xx)
{
    p[0] = J({ s0, 0, 9 }, { 0, 9, 1 }, { comp, 10, 2 }, { 0, 12, 1 }, { s1, 13, 9 }, { 0, 22, 1 }, { comp, 23, 2 }, { 0, 25, 1 },
             { 0, 26, 3 }, { 0, 29, 2 }, { ultima, 31, 1 });
    if (r7xx)
        p[1] = J({ 0, 0, 1 }, { 0, 1, 1 }, { 0, 2, 1 }, { 0, 3, 1 }, { 1, 4, 1 }, { 0, 5, 2 }, { inst, 7, 11 }, { 0, 18, 3 },
                 { dst, 21, 7 }, { 0, 28, 1 }, { comp, 29, 2 }, { 1, 31, 1 });
    else
        p[1] = J({ 0, 0, 1 }, { 0, 1, 1 }, { 0, 2, 1 }, { 0, 3, 1 }, { 1, 4, 1 }, { 0, 5, 1 }, { 0, 6, 2 }, { inst, 8, 10 },
                 { 0, 18, 3 }, { dst, 21, 7 }, { 0, 28, 1 }, { comp, 29, 2 }, { 1, 31, 1 });
}

static void sombreadores(int r7xx)
{
    uint32_t a[R63D_SH_MAX], b[R63D_SH_MAX];
    memset(a, 0xAA, sizeof a);
    int n = r63d_vs(a, r7xx);
    e_cf(b + 0, 6, 4, 0x02);
    e_exp(b + 2, 60, 1, 1, 0x28, 0, 1);
    e_exp(b + 4, 0, 2, 2, 0x27, 0, 0);
    e_exp(b + 6, 1, 2, 3, 0x27, 0, 0);
    e_exp(b + 8, 2, 2, 4, 0x28, 1, 0);
    b[10] = b[11] = 0;
    e_vtx(b + 12, 1, 35, 2, 1, 0, 32, 1, 0, 1, 2, 3);
    e_vtx(b + 16, 2, 26, 0, 0, 16, 4, 0, 2, 1, 0, 3);
    e_vtx(b + 20, 3, 26, 0, 0, 20, 4, 0, 2, 1, 0, 3);
    e_vtx(b + 24, 4, 30, 2, 1, 24, 8, 0, 0, 1, 4, 5);
    compara(r7xx ? "sombreador de vértices (R7xx)" : "sombreador de vértices (R6xx)", a, n, b, 28);

    memset(a, 0xAA, sizeof a);
    n = r63d_ps_cor(a, r7xx);
    e_cf_alu(b + 0, 2, 4);
    e_exp(b + 2, 0, 0, 0, 0x28, 1, 1);
    for (unsigned c = 0; c < 4; c++) e_alu(b + 4 + c * 2, 0x00, 0, 0, 1, c, c == 3, r7xx);
    compara(r7xx ? "sombreador de color (R7xx)" : "sombreador de color (R6xx)", a, n, b, 12);

    memset(a, 0xAA, sizeof a);
    n = r63d_ps_tex(a, r7xx);
    e_cf(b + 0, 12, 1, 0x01);
    e_cf_alu(b + 2, 4, 8);
    e_exp(b + 4, 0, 0, 0, 0x28, 1, 1);
    b[6] = b[7] = 0;
    for (unsigned c = 0; c < 4; c++) e_alu(b + 8 + c * 2, 0x01, 0, 0, 2, c, c == 3, r7xx);
    for (unsigned c = 0; c < 4; c++) e_alu(b + 16 + c * 2, 0x00, 0, 0, 1, c, c == 3, r7xx);
    b[24] = J({ 0x10, 0, 5 }, { 0, 5, 2 }, { 0, 7, 1 }, { 0, 8, 8 }, { 2, 16, 7 }, { 0, 23, 1 }, { 0, 24, 1 });
    b[25] = J({ 2, 0, 7 }, { 0, 7, 1 }, { 0, 9, 3 }, { 1, 12, 3 }, { 2, 15, 3 }, { 3, 18, 3 }, { 0, 21, 7 }, { 1, 28, 1 },
              { 1, 29, 1 }, { 1, 30, 1 }, { 1, 31, 1 });
    b[26] = J({ 0, 0, 5 }, { 0, 5, 5 }, { 0, 10, 5 }, { 0, 15, 5 }, { 0, 20, 3 }, { 1, 23, 3 }, { 4, 26, 3 }, { 5, 29, 3 });
    b[27] = 0;
    compara(r7xx ? "sombreador de textura (R7xx)" : "sombreador de textura (R6xx)", a, n, b, 28);
}

/* Recorre un lote y comprueba que es una secuencia de paquetes bien formada; devuelve cuántos paquetes. */
static int recorre(const char *que, const r6_lote_t *l)
{
    int i = 0, np = 0;
    VERIFICA(!l->cheio, "%s: el lote no cupo", que);
    while (i < l->n) {
        uint32_t h = l->p[i];
        VERIFICA((h >> 30) == 3, "%s: palabra %d (%08x) no es una cabecera de tipo 3", que, i, h);
        if ((h >> 30) != 3) return np;
        int n = (int)((h >> 16) & 0x3FFF) + 1;
        unsigned op = (h >> 8) & 0xFF;
        VERIFICA(i + 1 + n <= l->n, "%s: el paquete %d (orden %02x) se sale del lote", que, np, op);
        if (op >= 0x68 && op <= 0x6F) {
            static const uint32_t fim[8] = { (0xAC00 - 0x8000) / 4, 0x1000 / 4, 0x2000 / 4, 0, 0, 0x4000 / 4, 0xFF0 / 4, 0x1210 / 4 };
            uint32_t d = l->p[i + 1];
            VERIFICA(d + (uint32_t)(n - 1) <= fim[op - 0x68], "%s: orden %02x escribe fuera de su intervalo (%x + %d)", que, op, d, n - 1);
        }
        i += 1 + n;
        np++;
    }
    return np;
}

/* ¿Escribe el lote el registro (de contexto o de configuración) con ese valor? */
static int tem_reg(const r6_lote_t *l, uint32_t reg, uint32_t valor)
{
    int i = 0;
    while (i < l->n) {
        uint32_t h = l->p[i];
        int n = (int)((h >> 16) & 0x3FFF) + 1;
        unsigned op = (h >> 8) & 0xFF;
        uint32_t base = op == 0x68 ? 0x8000 : op == 0x69 ? 0x28000 : op == 0x6D ? 0x38000 : op == 0x6E ? 0x3C000 : 0;
        if (base)
            for (int k = 0; k < n - 1; k++)
                if (base + (l->p[i + 1] + (uint32_t)k) * 4 == reg && l->p[i + 2 + k] == valor) return 1;
        i += 1 + n;
    }
    return 0;
}

static void paquetes(void)
{
    uint32_t b[1024];
    r6_lote_t l;

    VERIFICA(R63D_PKT3(0x44, 6) == 0xC0054400u, "cabecera de ME_INITIALIZE");
    VERIFICA(R63D_PKT3(0x68, 2) == 0xC0016800u, "cabecera de SET_CONFIG_REG de un registro");
    r63d_lote(&l, b, 16);
    r63d_me_initialize(&l, true, 4);
    static const uint32_t me7[7] = { 0xC0054400u, 1, 0, 3, 0x10000, 0, 0 };
    compara("ME_INITIALIZE (R7xx, 4 contextos)", l.p, l.n, me7, 7);
    r63d_lote(&l, b, 16);
    r63d_me_initialize(&l, false, 8);
    static const uint32_t me6[7] = { 0xC0054400u, 1, 3, 7, 0x10000, 0, 0 };
    compara("ME_INITIALIZE (R6xx, 8 contextos)", l.p, l.n, me6, 7);

    r63d_lote(&l, b, 16);
    r63d_teste_cola(&l, 0x8504, 0xDEADBEEFu);
    static const uint32_t tc[3] = { 0xC0016800u, (0x8504 - 0x8000) / 4, 0xDEADBEEFu };
    compara("prueba de la cola", l.p, l.n, tc, 3);

    r63d_lote(&l, b, 64);
    r63d_reg(&l, 0x28800, 0x36);
    r63d_reg(&l, 0x3C000, 7);
    r63d_reg(&l, 0x3CFF0, 9);
    static const uint32_t rg[9] = { 0xC0016900u, 0x800 / 4, 0x36, 0xC0016E00u, 0, 7, 0xC0016F00u, 0, 9 };
    compara("registros por intervalo", l.p, l.n, rg, 9);
    r63d_lote(&l, b, 64);
    r63d_reg(&l, 0x6100, 1);   /* un registro de pantalla no puede ir por la cola */
    VERIFICA(l.cheio && l.n == 0, "un registro fuera de los intervalos se rechaza");
    r63d_lote(&l, b, 2);
    r63d_reg(&l, 0x28800, 1);
    VERIFICA(l.cheio, "un lote que no cabe queda marcado");

    /* Dibujo: 6 vértices de 32 bytes en 0x10200000, RV710 (sin caché de vértices). */
    r63d_lote(&l, b, 64);
    r63d_desenhar(&l, 0x10200000u, 6, 4, true);
    static const uint32_t dr[24] = {
        0xC0034300u, 1u << 23, 1, 0x102000, 10,
        0xC0076D00u, 160 * 7, 0x10200000u, 191, 32u << 8, 1, 0, 0, 3u << 30,
        0xC0016800u, (0x8958 - 0x8000) / 4, 4,
        0xC0002A00u, 0, 0xC0002F00u, 1, 0xC0012D00u, 6, 2,
    };
    compara("dibujo de 6 vértices", l.p, l.n, dr, 24);
    r63d_lote(&l, b, 64);
    r63d_desenhar(&l, 0x10200000u, 4, 1, false);
    VERIFICA(l.p[1] == 1u << 24 && l.p[16] == 2, "líneas y caché de vértices en las demás familias");

    /* Valla. */
    r63d_lote(&l, b, 64);
    r63d_valla(&l, 0x10300000u, 64 * 64 * 4, 0x10310000u, 64 * 64 * 4, 0x8508, 0x3D000001u);
    static const uint32_t va[18] = {
        0xC0004600u, 0x16, 0xC0016800u, 0x40 / 4, (1u << 15) | (1u << 17),
        0xC0034300u, (1u << 25) | (1u << 6), 64, 0x103000, 10,
        0xC0034300u, (1u << 26) | (1u << 14), 64, 0x103100, 10,
        0xC0016800u, 0x508 / 4, 0x3D000001u,
    };
    compara("valla", l.p, l.n, va, 18);

    /* Estado base, superficie, programas, textura y estado: bien formados y con los valores clave. */
    for (int f = 0; f < 4; f++) {
        r63d_lote(&l, b, 1024);
        r63d_estado_base(&l, f, true, 2, 0x10110000u);
        int np = recorre("estado base", &l);
        VERIFICA(np > 40 && l.n < 700, "estado base de la familia %d: %d paquetes, %d palabras", f, np, l.n);
        VERIFICA(l.p[0] == 0xC0012800u && l.p[1] == 0x80000000u, "empieza por CONTEXT_CONTROL");
        VERIFICA(tem_reg(&l, 0x28818, 0x300), "PA_CL_VTE_CNTL: x, y, z en pantalla y w = 1/w");
        VERIFICA(tem_reg(&l, 0x28810, 1u << 16), "PA_CL_CLIP_CNTL: sin recorte");
        VERIFICA(tem_reg(&l, 0x286C4, 4) && tem_reg(&l, 0x286CC, 3u | (1u << 28)) && tem_reg(&l, 0x28614, 0x020100) && tem_reg(&l, 0x2864C, 0x102), "tres parámetros con perspectiva");
        VERIFICA(tem_reg(&l, 0x282D0, 0) && tem_reg(&l, 0x282D4, 0x3F800000u), "Z de 0 a 1");
        VERIFICA(tem_reg(&l, 0x28C5C, 8) && tem_reg(&l, 0x28C58, 6), "VGT con 2 tuberías");
        VERIFICA(tem_reg(&l, 0x8C00, (f == R63D_RV710 ? 0u : 1u) | 0xE400000Cu), "SQ_CONFIG (caché de vértices salvo en la RV710)");
        VERIFICA(tem_reg(&l, 0x28894, 0x101100), "el sombreador de lectura apunta a memoria nuestra");
    }
    r63d_lote(&l, b, 1024);
    r63d_estado_base(&l, 0, false, 1, 0);
    recorre("estado base R6xx", &l);
    VERIFICA(l.p[0] == 0xC0002400u, "en R6xx empieza por START_3D_CMDBUF");

    r63d_lote(&l, b, 128);
    r63d_alvo(&l, 0x10300000u, 0x10400000u, 640, 512, 640, 480, 32, false);
    recorre("superficie", &l);
    VERIFICA(tem_reg(&l, 0x28040, 0x103000) && tem_reg(&l, 0x28060, 79u | ((640u * 512 / 64 - 1) << 10)), "base y tamaño del color");
    VERIFICA(tem_reg(&l, 0x280A0, 0x08110068u), "CB_COLOR0_INFO: 8888 lineal, ARGB, mezcla saturada");
    VERIFICA(tem_reg(&l, 0x2800C, 0x104000) && tem_reg(&l, 0x28010, 2u | (2u << 15)), "Z de 24 bits en mosaico 1D");
    VERIFICA(tem_reg(&l, 0x28034, 640u | (480u << 16)), "recorte de la superficie");
    r63d_lote(&l, b, 128);
    r63d_alvo(&l, 0x10300000u, 0x10400000u, 64, 64, 64, 64, 16, false);
    VERIFICA(tem_reg(&l, 0x28010, 1u | (2u << 15)), "Z de 16 bits");

    r63d_lote(&l, b, 128);
    r63d_programas(&l, 0x10110000u, 0x10110200u, true);
    recorre("programas", &l);
    VERIFICA(tem_reg(&l, 0x28858, 0x101100) && tem_reg(&l, 0x28840, 0x101102) && tem_reg(&l, 0x28850, 3u | (1u << 28)) &&
             tem_reg(&l, 0x28868, 5) && tem_reg(&l, 0x28854, 2), "direcciones y recursos de los sombreadores");

    r63d_lote(&l, b, 128);
    r63d_textura(&l, 0x10500000u, 100, 50, 128, true, false, true);
    recorre("textura", &l);
    VERIFICA(tem_reg(&l, 0x38000, 1u | (1u << 3) | (15u << 8) | (99u << 19)) && tem_reg(&l, 0x38004, 49u | (26u << 26)) &&
             tem_reg(&l, 0x38008, 0x105000) && tem_reg(&l, 0x38018, 2u << 30), "recurso de textura");
    VERIFICA(tem_reg(&l, 0x38010, (1u << 14) | (2u << 16) | (1u << 19) | (3u << 25)), "0xAARRGGBB llega como R, G, B, A");
    VERIFICA(tem_reg(&l, 0x3C000, 2u | (1u << 9) | (1u << 12)), "muestreador: borde en S, repetir en T, bilineal");

    r63d_lote(&l, b, 128);
    r63d_estado(&l, true, true, 3, true, 4, 5, true, 10, 20, 100, 50, false);
    recorre("estado", &l);
    VERIFICA(tem_reg(&l, 0x28800, (1u << 1) | (1u << 2) | (3u << 4)), "Z: comparar (<=) y escribir");
    VERIFICA(tem_reg(&l, 0x28808, 0x00CC0180u) && tem_reg(&l, 0x28780, 4u | (5u << 8)) && tem_reg(&l, 0x28804, 4u | (5u << 8)),
             "mezcla alfa por superficie");
    VERIFICA(tem_reg(&l, 0x28238, 0xF), "se escribe el color");
    VERIFICA(tem_reg(&l, 0x28240, 10u | (20u << 16) | (1u << 31)) && tem_reg(&l, 0x28244, 110u | (70u << 16)), "recorte del programa");
    r63d_lote(&l, b, 128);
    r63d_estado(&l, true, true, 7, false, 1, 0, false, 0, 0, 64, 64, true);
    VERIFICA(tem_reg(&l, 0x28800, (1u << 1) | (1u << 2) | (7u << 4)) && tem_reg(&l, 0x28238, 0) && tem_reg(&l, 0x28808, 0x00CC0000u) &&
             !tem_reg(&l, 0x28780, 1), "borrar solo el Z: siempre pasa, sin color; la R600 original sin mezcla por superficie");
}

static void flotantes(void)
{
    static const int v[] = { 0, 1, -1, 2, 3, 8, 56, 64, 100, 640, 1023, 1024, 16777215, 16777217, 33554431, -480, 2147483647 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        float f = (float)v[i];
        uint32_t b;
        memcpy(&b, &f, 4);
        VERIFICA(r63d_float(v[i]) == b, "float de %d: %08x, esperado %08x", v[i], r63d_float(v[i]), b);
    }
}

static uint32_t par(const r6_par_t *p, int n, uint32_t reg, int *achou)
{
    for (int i = 0; i < n; i++) if (p[i].reg == reg) { *achou = 1; return p[i].valor; }
    *achou = 0;
    return 0;
}

static void inicio(void)
{
    r6_par_t p[R7_INICIO_MAX];
    r7_leituras_t le;
    uint32_t ctx = 0;
    int a;
    memset(&le, 0, sizeof le);
    /* RV710: 2 tuberías activas (las otras 6 inactivas), sin backends apagados, RAMCFG de ejemplo. */
    le.cc_gc_shader_pipe_config = 0xFC00;
    le.mc_arb_ramcfg = (1u << 0) | (2u << 3) | (1u << 9);
    le.sq_config = 0xFF000008u;
    le.smx_dc_ctl0 = 0x3FF;
    le.db_debug3 = 0x1Fu << 11;
    int n = r7_inicio_motor(R63D_RV710, &le, p, R7_INICIO_MAX, &ctx);
    VERIFICA(n > 40 && n <= R7_INICIO_MAX && ctx == 4, "RV710: %d registros, %u contextos", n, ctx);
    for (int i = 0; i < n; i++)
        VERIFICA(p[i].reg < 0x10000 && (p[i].reg < 0x6000 || p[i].reg >= 0x8000) && p[i].reg != 0x0E60 &&
                 !(p[i].reg >= 0x2C00 && p[i].reg < 0x3000) && !(p[i].reg >= 0xD000 && p[i].reg < 0xD100),
                 "el registro %x no es de pantalla, HDP ni DMA y cae en la ventana del MMIO", p[i].reg);
    /* GB_TILING_CONFIG: PIPE_TILING(1) | BANK_TILING(1) | GROUP_SIZE(1) | ROW_TILING(2) | BANK_SWAPS | SAMPLE_SPLIT(2) | mapa 0. */
    VERIFICA(par(p, n, 0x98F0, &a) == ((1u << 1) | (1u << 4) | (1u << 6) | (2u << 8) | (1u << 11) | (2u << 14)) && a, "GB_TILING_CONFIG de la RV710: %x", par(p, n, 0x98F0, &a));
    VERIFICA(par(p, n, 0x9100, &a) == 0 && a, "SPI_CONFIG_CNTL con dos tuberías");
    VERIFICA(par(p, n, 0x8C00, &a) == 0xE4000006u + 0x8u && a, "SQ_CONFIG: prioridades, constantes DX9, sin caché de vértices: %x", par(p, n, 0x8C00, &a));
    VERIFICA(par(p, n, 0x8C04, &a) == (96u | (96u << 16)) && a, "SQ_GPR_RESOURCE_MGMT_1");
    VERIFICA(par(p, n, 0x8C0C, &a) == (96u | (48u << 8) | (16u << 16) | (24u << 24)) && a, "SQ_THREAD_RESOURCE_MGMT: %x", par(p, n, 0x8C0C, &a));
    VERIFICA(par(p, n, 0x8C10, &a) == (64u | (64u << 16)) && a, "SQ_STACK_RESOURCE_MGMT_1");
    VERIFICA(par(p, n, 0x8DCC, &a) == 0x98989898u && a, "SQ_DYN_GPR_SIZE_SIMD_AB_7");
    VERIFICA(par(p, n, 0xA020, &a) == ((0x3FFu & ~(0x1FFu << 1)) | (447u << 1)) && a, "SMX_DC_CTL0");
    VERIFICA(par(p, n, 0x98B0, &a) == (2u << 11) && a, "DB_DEBUG3");
    VERIFICA(par(p, n, 0x900C, &a) == (31u | (3u << 8) | (27u << 16)) && a, "SX_EXPORT_BUFFER_SIZES");
    VERIFICA(par(p, n, 0x8BCC, &a) == (0x40u | (0x30u << 12) | (0x130u << 20)) && a, "PA_SC_FIFO_SIZE");
    VERIFICA(par(p, n, 0x8CF0, &a) == (16u | (1u << 8) | (0xE0u << 16) | (8u << 24)) && a, "SQ_MS_FIFO_SIZES");
    VERIFICA(par(p, n, 0x88C4, &a) == (1u | (3u << 6)) && a, "VGT_CACHE_INVALIDATION (solo la de texturas)");
    VERIFICA(par(p, n, 0x88C8, &a) == 160 && a, "VGT_GS_PER_ES");
    (void)par(p, n, 0xA008, &a); VERIFICA(a, "SMX_SAR_CTL0 en la RV710");
    /* RV770: 8 tuberías de mosaico, 4 backends. */
    memset(&le, 0, sizeof le);
    n = r7_inicio_motor(R63D_RV770, &le, p, R7_INICIO_MAX, &ctx);
    VERIFICA(n > 40 && ctx == 8, "RV770: %d registros", n);
    /* Mapa: 8 tuberías entre 4 backends, de dos en dos: 3,3,2,2,1,1,0,0 (dos bits cada una). */
    VERIFICA((par(p, n, 0x98F0, &a) >> 16) == 0xFA50u, "mapa de backends de la RV770: %x", par(p, n, 0x98F0, &a) >> 16);
    (void)par(p, n, 0xA008, &a); VERIFICA(!a, "sin SMX_SAR_CTL0 en la RV770");
    VERIFICA(par(p, n, 0x8C00, &a) & 1u, "la RV770 sí tiene caché de vértices");
    VERIFICA(r7_inicio_motor(7, &le, p, R7_INICIO_MAX, &ctx) < 0 && r7_inicio_motor(R63D_RV710, &le, p, 10, &ctx) < 0,
             "familia desconocida o sin sitio: error");
}

int main(void)
{
    sombreadores(1);
    sombreadores(0);
    paquetes();
    flotantes();
    inicio();
    if (falhas) { printf("%d fallos\n", falhas); return 1; }
    printf("sombreadores (3, en R6xx y R7xx), paquetes PM4, flotantes e inicio del motor: todo bien\n");
    return 0;
}
