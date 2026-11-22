// SPDX-License-Identifier: GPL-2.0-or-later
/* evergreen_3d_unidad.c - prueba en el PC de evergreen_3d_pacotes.c (D427): paquetes PM4,
 * sombreadores y valores de inicio del motor 3D de las Radeon Evergreen/NI.
 *
 * Tres comprobaciones independientes del driver:
 *   1. los sombreadores, contra una segunda codificación campo a campo (bit de inicio y
 *      ancho de eg_sq.h de Mesa y de los macros de evergreen_shader.h de xf86-video-ati) y
 *      contra palabras sacadas a mano de los sombreadores de xf86-video-ati;
 *   2. un recorrido de los paquetes como lo haría el CP: cabeceras, rangos de registros,
 *      solo registros de contexto dentro del preámbulo, y un banco de registros simulado
 *      del que se leen los valores que importan (color, Z, SPI, recursos, 40 bits);
 *   3. un pequeño simulador del ISA (solo lo que usan estos sombreadores: VC, TC, ALU con
 *      ADD/MUL/INTERP y exportaciones) que ejecuta el de vértices sobre un vértice y los de
 *      píxeles sobre sus parámetros, y exige color = sat(textura x color + especular).
 *
 *   gcc -O2 -Wall -DR600_ANFITRION -Ikernel/inc tests/evergreen_3d_unidad.c \
 *       kernel/dispositivos/evergreen_3d_pacotes.c kernel/dispositivos/r600_3d_pacotes.c */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "evergreen_3d.h"

static int falhas;
#define VERIFICA(c, ...) do { if (!(c)) { falhas++; printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

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

/* ------------------------------------------------------------------ 1. segunda codificación (eg_sq.h) */

static void e_cf(uint32_t *p, unsigned addr, unsigned n, unsigned inst)
{
    p[0] = J({ addr, 0, 24 }, { 0, 24, 3 });
    p[1] = J({ 0, 0, 3 }, { 0, 3, 5 }, { 0, 8, 2 }, { n - 1, 10, 6 }, { 0, 20, 1 }, { 0, 21, 1 }, { inst, 22, 8 }, { 0, 30, 1 }, { 1, 31, 1 });
}
static void e_cf_alu(uint32_t *p, unsigned addr, unsigned n)
{
    p[0] = J({ addr, 0, 22 }, { 0, 22, 4 }, { 0, 26, 4 }, { 0, 30, 2 });
    p[1] = J({ 0, 0, 2 }, { 0, 2, 8 }, { 0, 10, 8 }, { n - 1, 18, 7 }, { 0, 25, 1 }, { 8, 26, 4 }, { 0, 30, 1 }, { 1, 31, 1 });
}
static void e_exp(uint32_t *p, unsigned base, unsigned tipo, unsigned gpr, unsigned inst, unsigned fim, unsigned barreira)
{
    p[0] = J({ base, 0, 13 }, { tipo, 13, 2 }, { gpr, 15, 7 }, { 0, 22, 1 }, { 0, 23, 7 }, { 0, 30, 2 });
    p[1] = J({ 0, 0, 3 }, { 1, 3, 3 }, { 2, 6, 3 }, { 3, 9, 3 }, { 0, 16, 4 }, { 0, 20, 1 }, { fim, 21, 1 }, { inst, 22, 8 },
             { 0, 30, 1 }, { barreira, 31, 1 });
}
static void e_vtx(uint32_t *p, unsigned gpr, unsigned fmt, unsigned num, unsigned comp, unsigned desloc, unsigned bytes,
                  unsigned mega, unsigned sx, unsigned sy, unsigned sz, unsigned sw)
{
    p[0] = J({ 0, 0, 5 }, { 0, 5, 2 }, { 0, 7, 1 }, { 0, 8, 8 }, { 0, 16, 7 }, { 0, 23, 1 }, { 0, 24, 2 }, { bytes - 1, 26, 6 });
    p[1] = J({ gpr, 0, 7 }, { 0, 7, 1 }, { sx, 9, 3 }, { sy, 12, 3 }, { sz, 15, 3 }, { sw, 18, 3 }, { 0, 21, 1 }, { fmt, 22, 6 },
             { num, 28, 2 }, { comp, 30, 1 }, { 0, 31, 1 });
    p[2] = J({ desloc, 0, 16 }, { 0, 16, 2 }, { 0, 18, 1 }, { mega, 19, 1 }, { 0, 20, 1 }, { 0, 21, 2 });
    p[3] = 0;
}
static void e_alu(uint32_t *p, unsigned inst, unsigned dst, unsigned s0, unsigned c0, unsigned s1, unsigned c1, unsigned comp,
                  unsigned ultima, unsigned escreve, unsigned banco, unsigned sat)
{
    p[0] = J({ s0, 0, 9 }, { 0, 9, 1 }, { c0, 10, 2 }, { 0, 12, 1 }, { s1, 13, 9 }, { 0, 22, 1 }, { c1, 23, 2 }, { 0, 25, 1 },
             { 0, 26, 3 }, { 0, 29, 2 }, { ultima, 31, 1 });
    p[1] = J({ 0, 0, 1 }, { 0, 1, 1 }, { 0, 2, 1 }, { 0, 3, 1 }, { escreve, 4, 1 }, { 0, 5, 2 }, { inst, 7, 11 }, { banco, 18, 3 },
             { dst, 21, 7 }, { 0, 28, 1 }, { comp, 29, 2 }, { sat, 31, 1 });
}
static int e_interp(uint32_t *p, unsigned param, unsigned dst, int xyzw)
{
    int n = 0;
    for (int g = xyzw ? 0 : 1; g < 2; g++)
        for (unsigned c = 0; c < 4; c++, n += 2)
            e_alu(p + n, g == 0 ? 0xD7 : 0xD6, dst, 0, (c & 1) ? 0 : 1, 448 + param, 0, c, c == 3,
                  g == 0 ? c >= 2 : c < 2, 5, 0);
    return n;
}

static void sombreadores(void)
{
    uint32_t a[EG3D_SH_MAX], b[EG3D_SH_MAX];
    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    int na = eg3d_vs(a);
    e_cf(b + 0, 6, 4, 0x02);
    e_exp(b + 2, 60, 1, 1, 0x54, 0, 1);
    e_exp(b + 4, 0, 2, 2, 0x53, 0, 0);
    e_exp(b + 6, 1, 2, 3, 0x53, 0, 0);
    e_exp(b + 8, 2, 2, 4, 0x54, 1, 0);
    b[10] = b[11] = 0;
    e_vtx(b + 12, 1, 35, 2, 1, 0, 32, 1, 0, 1, 2, 3);
    e_vtx(b + 16, 2, 26, 0, 0, 16, 4, 0, 2, 1, 0, 3);
    e_vtx(b + 20, 3, 26, 0, 0, 20, 4, 0, 2, 1, 0, 3);
    e_vtx(b + 24, 4, 30, 2, 1, 24, 8, 0, 0, 1, 4, 5);
    compara("sombreador de vertices", a, na, b, 28);

    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    na = eg3d_ps_cor(a);
    e_cf_alu(b + 0, 2, 20);
    e_exp(b + 2, 0, 0, 1, 0x54, 1, 1);
    int n = 4;
    n += e_interp(b + n, 0, 1, 1);
    n += e_interp(b + n, 1, 2, 1);
    for (unsigned c = 0; c < 4; c++, n += 2) e_alu(b + n, 0x00, 1, 1, c, 2, c, c, c == 3, 1, 0, 1);
    compara("sombreador de color", a, na, b, n);

    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    na = eg3d_ps_tex(a);
    e_cf_alu(b + 0, 4, 20);
    e_cf(b + 2, 24, 1, 0x01);
    e_cf_alu(b + 4, 26, 8);
    e_exp(b + 6, 0, 0, 1, 0x54, 1, 1);
    n = 8;
    n += e_interp(b + n, 0, 1, 1);
    n += e_interp(b + n, 1, 2, 1);
    n += e_interp(b + n, 2, 3, 0);
    VERIFICA(n == 48, "la muestra empieza en la palabra 48 (alineada a 16 bytes): %d", n);
    b[n++] = J({ 0x10, 0, 5 }, { 0, 5, 2 }, { 0, 7, 1 }, { 0, 8, 8 }, { 3, 16, 7 }, { 0, 23, 1 }, { 0, 24, 1 }, { 0, 25, 2 }, { 0, 27, 2 });
    b[n++] = J({ 3, 0, 7 }, { 0, 7, 1 }, { 0, 9, 3 }, { 1, 12, 3 }, { 2, 15, 3 }, { 3, 18, 3 }, { 0, 21, 7 }, { 1, 28, 1 }, { 1, 29, 1 },
               { 1, 30, 1 }, { 1, 31, 1 });
    b[n++] = J({ 0, 0, 5 }, { 0, 5, 5 }, { 0, 10, 5 }, { 0, 15, 5 }, { 0, 20, 3 }, { 1, 23, 3 }, { 4, 26, 3 }, { 5, 29, 3 });
    b[n++] = 0;
    for (unsigned c = 0; c < 4; c++, n += 2) e_alu(b + n, 0x01, 1, 1, c, 3, c, c, c == 3, 1, 0, 1);
    for (unsigned c = 0; c < 4; c++, n += 2) e_alu(b + n, 0x00, 1, 1, c, 2, c, c, c == 3, 1, 0, 1);
    compara("sombreador de textura", a, na, b, n);
    VERIFICA(na <= EG3D_SH_MAX && na * 4 <= EG3D_SH_PASSO, "el de textura cabe en su hueco (%d palabras)", na);

    /* Palabras sacadas a mano de xf86-video-ati (evergreen_copy_vs/evergreen_copy_ps, MIT), para que la
     * segunda codificación no comparta un error de lectura de los campos con el driver:
     *   CF VC, ADDR(4), I_COUNT(2), BARRIER(1):          00000004 80800400
     *   exportación POS0 desde R1, EXPORT_DONE, BARRIER:  0000a03c 95000688 (con SWIZ x y z w)
     *   INTERP_XY x: GPR0.y por PARAM0.x, a R0.x, VEC_210: 00380400 00146b10 */
    uint32_t x[2];
    e_cf(x, 4, 2, 0x02);
    VERIFICA(x[0] == 0x00000004u && x[1] == 0x80800400u, "CF VC como xf86-video-ati: %08x %08x", x[0], x[1]);
    e_exp(x, 60, 1, 1, 0x54, 0, 1);
    VERIFICA(x[0] == 0x0000A03Cu && x[1] == 0x95000688u, "exportación POS0 como xf86-video-ati: %08x %08x", x[0], x[1]);
    e_alu(x, 0xD6, 0, 0, 1, 448, 0, 0, 0, 1, 5, 0);
    VERIFICA(x[0] == 0x00380400u && x[1] == 0x00146B10u, "INTERP_XY como xf86-video-ati: %08x %08x", x[0], x[1]);
    /* Y el driver: la exportación POS0 del de vértices es esa misma palabra. */
    eg3d_vs(a);
    VERIFICA(a[2] == 0x0000A03Cu && a[3] == 0x95000688u, "POS0 del driver = xf86: %08x %08x", a[2], a[3]);
}

/* ------------------------------------------------------------------ 2. recorrido de los paquetes */

#define NREG 0x40000
static uint32_t banco[NREG];      /* registro / 4 -> valor (los de contexto y de configuración) */
static uint8_t escrito[NREG];
static int n_draw, n_sync, n_clear, n_pre_ini, n_pre_fim, n_valla;
static uint32_t ultimo_sync;

static void limpa_banco(void) { memset(banco, 0, sizeof banco); memset(escrito, 0, sizeof escrito); n_draw = n_sync = n_clear = n_pre_ini = n_pre_fim = n_valla = 0; }

static void percorre(const char *que, const uint32_t *p, int n)
{
    int i = 0;
    bool preambulo = false;
    while (i < n) {
        uint32_t h = p[i];
        if (h == 0x80000000u) { i++; continue; }   /* tipo 2 */
        VERIFICA((h >> 30) == 3, "%s: palabra %d no es una cabecera de tipo 3 (%08x)", que, i, h);
        if ((h >> 30) != 3) return;
        uint32_t op = (h >> 8) & 0xFF, cnt = ((h >> 16) & 0x3FFF) + 1;
        VERIFICA((int)(i + 1 + cnt) <= n, "%s: el paquete %02x de la palabra %d se sale del lote", que, op, i);
        const uint32_t *d = p + i + 1;
        uint32_t base = 0;
        switch (op) {
        case 0x68: base = 0x8000; break;
        case 0x69: base = 0x28000; break;
        case 0x6D: base = 0x30000; break;
        case 0x6E: base = 0x3C000; break;
        case 0x6F: base = 0x3CFF0; break;
        case 0x28: VERIFICA(cnt == 2, "%s: CONTEXT_CONTROL de 2 palabras", que); break;
        case 0x4A:
            if (d[0] == EG3D_PREAMBLE_INICIO) { VERIFICA(!preambulo, "%s: preámbulo dentro de otro", que); preambulo = true; n_pre_ini++; }
            else if (d[0] == EG3D_PREAMBLE_FIM) { VERIFICA(preambulo, "%s: fin de preámbulo sin inicio", que); preambulo = false; n_pre_fim++; }
            else VERIFICA(0, "%s: PREAMBLE_CNTL con %08x", que, d[0]);
            break;
        case 0x12: n_clear++; VERIFICA(cnt == 1, "%s: CLEAR_STATE de una palabra", que); break;
        case 0x43: n_sync++; ultimo_sync = d[0];
            VERIFICA(cnt == 4 && d[1] == 0xFFFFFFFFu && d[2] == 0 && d[3] == 10, "%s: SURFACE_SYNC de toda la memoria", que); break;
        case 0x46: VERIFICA(cnt == 1 && d[0] == 0x16, "%s: EVENT_WRITE CACHE_FLUSH_AND_INV", que); n_valla++; break;
        case 0x2A: case 0x2F: VERIFICA(cnt == 1, "%s: INDEX_TYPE/NUM_INSTANCES de una palabra", que); break;
        case 0x2D: n_draw++; VERIFICA(cnt == 2 && d[1] == 2, "%s: DRAW_INDEX_AUTO con índices automáticos", que); break;
        case 0x44: VERIFICA(cnt == 6, "%s: ME_INITIALIZE de 6 palabras", que); break;
        default: VERIFICA(0, "%s: orden PM4 %02x inesperada", que, op);
        }
        if (base) {
            VERIFICA(cnt >= 2, "%s: SET_* sin valores", que);
            VERIFICA(!preambulo || op == 0x69, "%s: dentro del preámbulo solo registros de contexto (orden %02x)", que, op);
            uint32_t reg = base + d[0] * 4;
            for (uint32_t k = 1; k < cnt; k++, reg += 4) {
                uint32_t lim = op == 0x68 ? 0xAC00 : op == 0x69 ? 0x29000 : op == 0x6D ? 0x38000 : op == 0x6E ? 0x3C600 : 0x3FF0C;
                VERIFICA(reg < lim, "%s: registro %x fuera del rango de la orden %02x", que, reg, op);
                if (reg / 4 < NREG) { banco[reg / 4] = d[k]; escrito[reg / 4] = 1; }
            }
        }
        i += 1 + (int)cnt;
    }
    VERIFICA(!preambulo, "%s: el preámbulo se queda abierto", que);
}
static uint32_t R(uint32_t reg) { return banco[reg / 4]; }
static bool E(uint32_t reg) { return escrito[reg / 4]; }

static void pacotes(void)
{
    static uint32_t buf[4096];
    r6_lote_t l;
    /* Estado base de una Turks. */
    limpa_banco();
    r63d_lote(&l, buf, 4096);
    eg3d_estado_base(&l, EG3D_TURKS, 0x00000000u, 0xF00010000ull);
    VERIFICA(!l.cheio && l.n < 2048, "el estado base cabe en el lote del driver (%d palabras)", l.n);
    percorre("estado base", buf, l.n);
    VERIFICA(n_pre_ini == 1 && n_pre_fim == 1 && n_clear == 1, "preámbulo y CLEAR_STATE como evergreen_cp_start");
    VERIFICA(R(0x288A4) == 0x0F000100u, "SQ_PGM_START_FS con los bits 39:8 (%x)", R(0x288A4));
    VERIFICA(R(0x286CC) == (3u | (1u << 28)) && R(0x286E0) == 1u, "SPI: 3 interpolados con perspectiva, baricéntricas del centro");
    VERIFICA(R(0x2861C) == (1u | (2u << 8) | (3u << 16)) && R(0x28620) == 0, "SPI_VS_OUT_ID: semánticas 1, 2 y 3");
    VERIFICA(R(0x28644) == 0x101u && R(0x28648) == 0x102u && R(0x2864C) == 0x103u, "SPI_PS_INPUT_CNTL_0..2");
    VERIFICA(R(0x286C4) == 4u, "SPI_VS_OUT_CONFIG: exporta 3");
    VERIFICA(R(0x28818) == 0x300u && R(0x28810) == 0x10000u, "vértices ya en pantalla, sin recorte");
    VERIFICA(R(0x28808) == 0x00CC0010u && R(0x2880C) == 0x210u, "CB_COLOR_CONTROL normal y DB_SHADER_CONTROL (como Linux)");
    VERIFICA(R(0x28C08) == (1u | (2u << 1) | (5u << 3)), "PA_SU_VTX_CNTL de las R7xx");
    VERIFICA(R(0x28C3C) == 0xFFFFFFFFu && R(0x28C04) == 0, "sin MSAA");
    VERIFICA(E(0x28140) && E(0x281FC) && R(0x281FC) == 0 && E(0x28FFC), "tamaños de las constantes a cero");
    VERIFICA(E(0x28CAC) && R(0x28CAC) == 0 && E(0x28EA4) && R(0x28EA4) == 0, "CB_COLOR1..11_INFO apagados");
    uint32_t sq[9];
    eg3d_valores_sq(EG3D_TURKS, 0, sq);
    VERIFICA(R(0x8C00) == sq[0] && R(0x8C04) == sq[1] && R(0x8C28) == sq[8] && R(0x8E2C) == 0x10001000u, "SQ por la cola");
    VERIFICA(sq[1] == (93u | (46u << 16) | (4u << 28)) && sq[2] == (31u | (31u << 16)) && sq[3] == (23u | (23u << 16)),
             "GPR de Turks como Linux: 93/46/4, 31/31, 23/23 (%x %x %x)", sq[1], sq[2], sq[3]);
    VERIFICA(sq[4] == (128u | (16u << 8) | (16u << 16) | (16u << 24)) && sq[5] == (16u | (16u << 8)), "hilos de Turks como Linux");
    VERIFICA(sq[6] == (42u | (42u << 16)), "pila de Turks como Linux");
    VERIFICA((sq[0] & 3u) == 3u && ((sq[0] >> 24) & 0xFFu) == ((1u << 2) | (2u << 4) | (3u << 6)), "SQ_CONFIG: VC, EXPORT_SRC_C y prioridades");
    eg3d_valores_sq(EG3D_CAICOS, 0, sq);
    VERIFICA(!(sq[0] & 1u), "Caicos no tiene caché de vértices");
    eg3d_valores_sq(EG3D_CEDAR, 0, sq);
    VERIFICA((sq[4] & 0xFFu) == 96u && ((sq[4] >> 8) & 0xFFu) == 16u && !(sq[0] & 1u), "Cedar: 96 hilos de PS, sin caché de vértices");

    /* Destino con la VRAM por encima de 4 GB (la HD 7600M del Toshiba: 0xF_0000_0000). */
    limpa_banco();
    r63d_lote(&l, buf, 4096);
    eg3d_alvo(&l, 0xF00123400ull, 0xF00800000ull, 128, 128, 100, 90, 24, 2);
    percorre("destino", buf, l.n);
    VERIFICA(R(0x28C60) == 0x0F001234u && R(0x28C7C) == 0x0F001234u && R(0x28C84) == 0x0F001234u, "CB_COLOR0_BASE/CMASK/FMASK: bits 39:8");
    VERIFICA(R(0x28C64) == 15u && R(0x28C68) == 128u * 128u / 64u - 1u, "CB_COLOR0_PITCH y SLICE");
    VERIFICA(R(0x28C70) == ((0x1Au << 2) | (1u << 8) | (1u << 15) | (1u << 19) | (1u << 24)),
             "CB_COLOR0_INFO: 8888, lineal alineada, SWAP_ALT, saturada, EXPORT_4C_16BPC");
    VERIFICA(R(0x28C74) == ((1u << 4) | (4u << 5) | (2u << 10)), "CB_COLOR0_ATTRIB: lineal, 8 bancos");
    VERIFICA(E(0x28C78) && R(0x28C78) == 0, "CB_COLOR0_DIM a 0 (como Mesa)");
    VERIFICA(R(0x28040) == (2u | (2u << 4) | (4u << 8) | (2u << 12)) && R(0x28044) == 0, "DB_Z_INFO: Z_24 en mosaico 1D, sin plantilla");
    VERIFICA(R(0x28048) == 0x0F008000u && R(0x28050) == 0x0F008000u, "DB_Z_READ/WRITE_BASE: bits 39:8");
    VERIFICA(R(0x28058) == (15u | (15u << 11)) && R(0x2805C) == 255u, "DB_DEPTH_SIZE y SLICE");
    VERIFICA(R(0x28034) == (100u | (90u << 16)) && R(0x28208) == (100u | (90u << 16)) && R(0x28244) == (100u | (90u << 16)), "recortes");
    VERIFICA(R(0x28204) == (1u << 31), "WINDOW_OFFSET_DISABLE");
    limpa_banco();
    r63d_lote(&l, buf, 4096);
    eg3d_alvo(&l, 0x1000000ull, 0x2000000ull, 64, 64, 64, 64, 16, 1);
    percorre("destino 16", buf, l.n);
    VERIFICA((R(0x28040) & 3u) == 1u, "Z_16");

    /* Sombreadores, textura, estado, dibujo y valla. */
    limpa_banco();
    r63d_lote(&l, buf, 4096);
    eg3d_programas(&l, 0xF00010000ull, 0xF00010400ull);
    eg3d_textura(&l, 0xF00200000ull, 100, 50, 128, true, false, true, 2);
    eg3d_estado(&l, true, true, 3, true, 4, 5, true, 0, 0, 100, 90);
    eg3d_desenhar(&l, 0xF00300020ull, 6, 4, true);
    eg3d_valla(&l, 0x8508, 0x3D000001u, true);
    percorre("dibujo", buf, l.n);
    VERIFICA(R(0x2885C) == 0x0F000100u && R(0x28840) == 0x0F000104u, "SQ_PGM_START_VS/PS con los bits 39:8");
    VERIFICA((R(0x28860) & 0xFFu) == EG3D_VS_GPRS && (R(0x28844) & 0xFFu) == EG3D_PS_GPRS && R(0x2884C) == 2u, "GPR y un color exportado");
    VERIFICA(R(0x30000) == (1u | (15u << 6) | (99u << 18)) && R(0x30004) == (49u | (1u << 28)), "textura: 2D, paso 128, 100x50, lineal alineada");
    VERIFICA(R(0x30008) == 0x0F002000u && R(0x3000C) == 0x0F002000u, "textura: base con los bits 39:8");
    VERIFICA(R(0x30010) == ((2u << 16) | (1u << 19) | (0u << 22) | (3u << 25)), "textura: ARGB -> RGBA");
    VERIFICA(R(0x3001C) == (0x1Au | (2u << 16) | (2u << 30)), "textura: 8_8_8_8 válida");
    VERIFICA(R(0x3C000) == (2u | (0u << 3) | (2u << 6) | (1u << 9) | (1u << 11)) && R(0x3C008) == ((1u << 29) | (1u << 31)),
             "muestreador: sujetar en s, repetir en t, bilineal");
    VERIFICA(R(0x28800) == (2u | 4u | (3u << 4)) && R(0x28238) == 0xFu, "Z <= con escritura y color");
    VERIFICA(R(0x28780) == (4u | (5u << 8) | (1u << 30)) && R(0x28808) == 0x00CC0010u, "mezcla en CB_BLEND0_CONTROL (bit 30)");
    uint32_t vs0 = EG3D_RES_VS0;
    VERIFICA(vs0 == 0x31600u, "recurso 0 del VS: el 176 (0x31600)");
    VERIFICA(R(vs0) == 0x00300020u && R(vs0 + 4) == 6u * 32u - 1u && R(vs0 + 8) == (0x0Fu | (32u << 8)), "vértices: 40 bits, paso 32");
    VERIFICA(R(vs0 + 12) == ((1u << 6) | (2u << 9) | (3u << 12)) && R(vs0 + 28) == 0xC0000000u, "vértices: XYZW y búfer válido");
    VERIFICA(R(0x8958) == 4u && n_draw == 1, "lista de triángulos, un dibujo");
    VERIFICA(R(0x8508) == 0x3D000001u && R(0x8040) == ((1u << 15) | (1u << 17)) && n_valla == 1, "valla: apunte tras esperar al 3D");
    VERIFICA(ultimo_sync == (EG3D_SYNC_CB0 | EG3D_SYNC_DB), "la valla vacía el color y el Z al final");
    /* Sin caché de vértices (Cedar, Caicos): los vértices se sincronizan por la de texturas. */
    limpa_banco();
    r63d_lote(&l, buf, 4096);
    eg3d_desenhar(&l, 0x100000ull, 3, 4, false);
    percorre("dibujo sin caché de vértices", buf, l.n);
    VERIFICA(buf[1] == EG3D_SYNC_TC, "sin caché de vértices: SURFACE_SYNC de texturas");
    /* Un registro que no va por la cola llena el lote (error de quien llama). */
    r63d_lote(&l, buf, 4096);
    eg3d_reg(&l, 0x6800, 1);
    VERIFICA(l.cheio, "un registro de pantalla no se puede mandar por la cola");
}

/* ------------------------------------------------------------------ inicio del motor */

static void inicio(void)
{
    r6_par_t p[EG3D_INICIO_MAX];
    eg3d_leituras_t l;
    memset(&l, 0, sizeof l);
    VERIFICA(eg3d_mapa_backends(2, 2, 8, 0) == 0x1100u, "GB_BACKEND_MAP de Turks (4 tuberías, 2 backends): %x", eg3d_mapa_backends(2, 2, 8, 0));
    VERIFICA(eg3d_mapa_backends(1, 1, 8, 0) == 0x00u, "Cedar/Caicos: todo al backend 0");
    VERIFICA(eg3d_mapa_backends(3, 8, 8, 0) == 0x76543210u, "Barts/Cypress (8 tuberías, 8 backends): %x", eg3d_mapa_backends(3, 8, 8, 0));
    VERIFICA(eg3d_mapa_backends(2, 2, 8, 0x1) == 0x1111u, "con el backend 0 apagado todo va al 1: %x", eg3d_mapa_backends(2, 2, 8, 1));
    uint32_t ctx = 0;
    int n = eg3d_inicio_motor(EG3D_TURKS, &l, p, EG3D_INICIO_MAX, &ctx);
    VERIFICA(n > 25 && ctx == 8, "Turks: %d registros, 8 contextos", n);
    for (int i = 0; i < n; i++) {
        uint32_t r = p[i].reg;
        VERIFICA(r < 0x10000 && !(r >= 0x6000 && r < 0x8000) && !(r >= 0x2C00 && r < 0x3000) && !(r >= 0x5400 && r < 0x5500) &&
                 !(r >= 0xD000 && r < 0xE000) && !(r >= 0x2000 && r < 0x2C00) && r != 0x0E60 && r != 0x8020 &&
                 (!(r >= 0x8600 && r < 0x8800) || r == 0x8760 || r == 0x8764 || r == 0x87FC)
                 && !(r >= 0xC000 && r < 0xD000),
                 "el registro %x del inicio no es de pantalla, HDP, MC, DMA, reinicio ni CP (salvo sus umbrales y CP_PERFMON_CNTL, como Linux)", r);
    }
    bool gb = false, map = false, sar = false;
    for (int i = 0; i < n; i++) {
        if (p[i].reg == 0x98F8) gb = p[i].valor == 0x02010002u;
        if (p[i].reg == 0x98FC) map = p[i].valor == 0x1100u;
        if (p[i].reg == 0xA008) sar = true;
    }
    VERIFICA(gb && map, "sin valores de la BIOS: GB_ADDR_CONFIG golden de Turks y el mapa calculado");
    VERIFICA(!sar, "SMX_SAR_CTL0 solo en Evergreen, no en Northern Islands");
    l.gb_addr_config = 0x12345678u; l.gb_backend_map = 0x2211u;
    n = eg3d_inicio_motor(EG3D_TURKS, &l, p, EG3D_INICIO_MAX, &ctx);
    gb = map = false;
    for (int i = 0; i < n; i++) {
        if (p[i].reg == 0x98F8) gb = p[i].valor == 0x12345678u;
        if (p[i].reg == 0x98FC) map = p[i].valor == 0x2211u;
    }
    VERIFICA(gb && map, "con valores de la BIOS: se dejan");
    n = eg3d_inicio_motor(EG3D_CEDAR, &l, p, EG3D_INICIO_MAX, &ctx);
    sar = false;
    for (int i = 0; i < n; i++) if (p[i].reg == 0xA008) sar = p[i].valor == 0x10000u;
    VERIFICA(sar && ctx == 4, "Cedar: SMX_SAR_CTL0 y 4 contextos");
    VERIFICA(eg3d_inicio_motor(99, &l, p, EG3D_INICIO_MAX, &ctx) == -1 && eg3d_inicio_motor(EG3D_TURKS, &l, p, 5, &ctx) == -1,
             "familia desconocida o sin sitio: -1");
    VERIFICA(eg3d_alinhamento_passo(0x02010002u) == 64 && eg3d_alinhamento_passo(0x02010012u) == 128, "paso alineado según el grupo");
    VERIFICA(eg3d_bancos(0) == 1 && eg3d_bancos(1) == 2 && eg3d_bancos(2) == 3, "NUM_BANKS");
    VERIFICA(eg3d_familia_por_nome("Turks (Mobility)") == EG3D_TURKS && eg3d_familia_por_nome("Cedar (Mobility HD 5400)") == EG3D_CEDAR &&
             eg3d_familia_por_nome("Hemlock") == EG3D_CYPRESS && eg3d_familia_por_nome("Cayman") == -1 && eg3d_familia_por_nome(NULL) == -1,
             "familias por el nombre de la tabla del 2D (Cayman no)");
    const eg3d_chip_t *c = eg3d_chip(EG3D_TURKS);
    VERIFICA(c && c->contextos == 8 && c->backends == 2 && !strcmp(c->firmware, "TURKS") && c->cache_vertices, "tabla de Turks");
}

/* ------------------------------------------------------------------ 3. simulador del ISA */

typedef struct { float v[128][4]; float pos[4], param[4][4], cor[4]; int npos, nparam, ncor; } maq_t;

static float sat(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

/* Ejecuta un programa. vb: el vértice (VS); par: los parámetros interpolados (PS); texel: lo que da la muestra. */
static void executa(const char *que, const uint32_t *p, int n, maq_t *m, const uint8_t *vb, const float par[4][4], const float texel[4])
{
    for (int pc = 0; pc < 32; pc++) {
        VERIFICA(pc * 2 + 1 < n, "%s: el CF se sale del programa", que);
        if (pc * 2 + 1 >= n) return;
        uint32_t w0 = p[pc * 2], w1 = p[pc * 2 + 1];
        uint32_t alu_inst = (w1 >> 26) & 0xF;
        if (alu_inst == 8 && ((w1 >> 22) & 0xFF) != 0x53 && ((w1 >> 22) & 0xFF) != 0x54) {   /* cláusula ALU */
            uint32_t addr = w0 & 0x3FFFFF, cnt = ((w1 >> 18) & 0x7F) + 1;
            float nv[128][4];
            memcpy(nv, m->v, sizeof nv);
            int grupo = 0;
            for (uint32_t k = 0; k < cnt; k++) {
                VERIFICA((int)(addr + k) * 2 + 1 < n, "%s: la ALU se sale", que);
                uint32_t a0 = p[(addr + k) * 2], a1 = p[(addr + k) * 2 + 1];
                uint32_t s0 = a0 & 0x1FF, c0 = (a0 >> 10) & 3, s1 = (a0 >> 13) & 0x1FF, c1 = (a0 >> 23) & 3, last = a0 >> 31;
                uint32_t inst = (a1 >> 7) & 0x7FF, dst = (a1 >> 21) & 0x7F, dc = (a1 >> 29) & 3, wm = (a1 >> 4) & 1, cl = a1 >> 31;
                float r = 0;
                if (inst == 0xD6 || inst == 0xD7) {
                    /* Con las baricéntricas a 0 el parámetro interpolado es el del vértice: se exige la forma de Mesa. */
                    VERIFICA(s0 == 0 && c0 == (dc & 1 ? 0u : 1u) && s1 >= 448 && s1 < 452 && c1 == 0 && ((a1 >> 18) & 7) == 5,
                             "%s: INTERP con la forma de evergreen_interp_alu", que);
                    VERIFICA(!wm || (inst == 0xD6 ? dc < 2 : dc >= 2), "%s: INTERP_XY escribe x, y; INTERP_ZW z, w", que);
                    r = par[s1 - 448][dc];
                } else {
                    float a = s0 < 128 ? m->v[s0][c0] : 0, b = s1 < 128 ? m->v[s1][c1] : 0;
                    if (inst == 0x00) r = a + b;
                    else if (inst == 0x01) r = a * b;
                    else VERIFICA(0, "%s: operación ALU %x", que, inst);
                }
                if (cl) r = sat(r);
                if (wm) nv[dst][dc] = r;
                grupo++;
                if (last) { memcpy(m->v, nv, sizeof nv); VERIFICA(grupo <= 5, "%s: grupo de más de 5", que); grupo = 0; }
            }
            VERIFICA(grupo == 0, "%s: la cláusula ALU no termina un grupo", que);
            continue;
        }
        uint32_t inst = (w1 >> 22) & 0xFF;
        if (inst == 0x02 || inst == 0x01) {   /* VC o TC */
            uint32_t addr = w0 & 0xFFFFFF, cnt = ((w1 >> 10) & 0x3F) + 1;
            VERIFICA(!(addr & 1), "%s: las lecturas van alineadas a 16 bytes", que);
            for (uint32_t k = 0; k < cnt; k++) {
                const uint32_t *f = p + (addr + 2 * k) * 2;
                VERIFICA((int)((addr + 2 * k) * 2 + 3) < n, "%s: la lectura se sale", que);
                if (inst == 0x02) {
                    uint32_t gpr = f[1] & 0x7F, fmt = (f[1] >> 22) & 0x3F, off = f[2] & 0xFFFF;
                    VERIFICA((f[0] & 0x1F) == 0 && ((f[0] >> 8) & 0xFF) == 0 && ((f[0] >> 16) & 0x7F) == 0, "%s: lee el búfer 0 con R0.x", que);
                    float in[6] = { 0, 0, 0, 0, 0, 1 };
                    if (fmt == 35) for (int q = 0; q < 4; q++) memcpy(&in[q], vb + off + 4 * q, 4);
                    else if (fmt == 30) for (int q = 0; q < 2; q++) memcpy(&in[q], vb + off + 4 * q, 4);
                    else if (fmt == 26) for (int q = 0; q < 4; q++) in[q] = vb[off + q] / 255.0f;
                    else VERIFICA(0, "%s: formato %u", que, fmt);
                    for (int q = 0; q < 4; q++) m->v[gpr][q] = in[(f[1] >> (9 + 3 * q)) & 7];
                } else {
                    uint32_t src = (f[0] >> 16) & 0x7F, gpr = f[1] & 0x7F;
                    VERIFICA((f[0] & 0x1F) == 0x10 && src == 3 && ((f[1] >> 28) & 3) == 3, "%s: SAMPLE con R3 normalizado", que);
                    float t[6] = { texel[0], texel[1], texel[2], texel[3], 0, 1 };
                    float o[4];
                    for (int q = 0; q < 4; q++) o[q] = t[(f[1] >> (9 + 3 * q)) & 7];
                    memcpy(m->v[gpr], o, sizeof o);
                }
            }
            continue;
        }
        if (inst == 0x53 || inst == 0x54) {
            uint32_t base = w0 & 0x1FFF, tipo = (w0 >> 13) & 3, gpr = (w0 >> 15) & 0x7F;
            float *d = tipo == 1 ? m->pos : tipo == 2 ? m->param[base & 3] : m->cor;
            for (int q = 0; q < 4; q++) d[q] = m->v[gpr][(w1 >> (3 * q)) & 7];
            if (tipo == 1) { m->npos++; VERIFICA(base == 60, "%s: POS0", que); }
            else if (tipo == 2) m->nparam++;
            else m->ncor++;
            if ((w1 >> 21) & 1) return;   /* fin del programa */
            continue;
        }
        VERIFICA(0, "%s: instrucción CF %x", que, inst);
        return;
    }
    VERIFICA(0, "%s: sin fin de programa", que);
}

static void simulador(void)
{
    uint32_t vs[EG3D_SH_MAX], pc[EG3D_SH_MAX], pt[EG3D_SH_MAX];
    int nvs = eg3d_vs(vs), npc = eg3d_ps_cor(pc), npt = eg3d_ps_tex(pt);
    /* Un vértice como gpu3d_vert_t: x, y, z, 1/w, color 0xAARRGGBB, especular 0x00RRGGBB, u, v. */
    uint8_t vb[32];
    float f[4] = { 10.5f, 20.5f, 0.25f, 0.5f }, uv[2] = { 0.75f, 0.125f };
    memcpy(vb, f, 16);
    uint32_t cor = 0x80FF4020u, esp = 0x00102030u;
    memcpy(vb + 16, &cor, 4); memcpy(vb + 20, &esp, 4); memcpy(vb + 24, uv, 8);
    static maq_t m;
    memset(&m, 0, sizeof m);
    float nada[4][4] = { { 0 } }, t0[4] = { 0 };
    executa("vértices", vs, nvs, &m, vb, nada, t0);
    VERIFICA(m.npos == 1 && m.nparam == 3, "el VS exporta la posición y 3 parámetros");
    VERIFICA(m.pos[0] == 10.5f && m.pos[1] == 20.5f && m.pos[2] == 0.25f && m.pos[3] == 0.5f, "posición x, y, z, 1/w tal cual");
    VERIFICA(fabsf(m.param[0][0] - 1.0f) < 1e-6f && fabsf(m.param[0][1] - 0x40 / 255.0f) < 1e-6f && fabsf(m.param[0][2] - 0x20 / 255.0f) < 1e-6f &&
             fabsf(m.param[0][3] - 0x80 / 255.0f) < 1e-6f, "color 0xAARRGGBB -> (R, G, B, A)");
    VERIFICA(fabsf(m.param[1][0] - 0x10 / 255.0f) < 1e-6f && fabsf(m.param[1][2] - 0x30 / 255.0f) < 1e-6f && m.param[1][3] == 0,
             "especular 0x00RRGGBB -> (R, G, B, 0)");
    VERIFICA(m.param[2][0] == 0.75f && m.param[2][1] == 0.125f && m.param[2][2] == 0 && m.param[2][3] == 1, "uv -> (u, v, 0, 1)");

    float par[4][4];
    memcpy(par, m.param, sizeof par);
    static maq_t a;
    memset(&a, 0, sizeof a);
    executa("píxeles de color", pc, npc, &a, vb, par, t0);
    VERIFICA(a.ncor == 1, "el PS de color exporta un color");
    for (int q = 0; q < 4; q++) VERIFICA(fabsf(a.cor[q] - sat(par[0][q] + par[1][q])) < 1e-6f, "color = sat(color + especular), canal %d", q);

    float texel[4] = { 0.5f, 1.0f, 0.25f, 0.5f };   /* lo que sale del recurso ya en RGBA */
    static maq_t b;
    memset(&b, 0, sizeof b);
    executa("píxeles de textura", pt, npt, &b, vb, par, texel);
    VERIFICA(b.ncor == 1, "el PS de textura exporta un color");
    for (int q = 0; q < 4; q++)
        VERIFICA(fabsf(b.cor[q] - sat(sat(texel[q] * par[0][q]) + par[1][q])) < 1e-6f, "color = sat(textura x color + especular), canal %d", q);
}

int main(void)
{
    sombreadores();
    pacotes();
    inicio();
    simulador();
    if (falhas) { printf("%d fallos\n", falhas); return 1; }
    printf("evergreen_3d_unidad: todo bien\n");
    return 0;
}
