// SPDX-License-Identifier: GPL-2.0-or-later
/* r600_3d_pacotes.c - paquetes PM4, sombreadores y valores de inicio del motor 3D
 * de las Radeon R6xx/R7xx (D284). Lógica pura: se prueba también en el PC
 * (tests/r600_3d_unidad.c). Ver r600_3d.h (referencias y formato). */
#include "r600_3d.h"

/* ------------------------------------------------------------------ lotes */

void r63d_lote(r6_lote_t *l, uint32_t *p, int max) { l->p = p; l->n = 0; l->max = max; l->cheio = false; }

void r63d_p(r6_lote_t *l, uint32_t v)
{
    if (l->cheio) return;
    if (l->n >= l->max) { l->cheio = true; return; }
    l->p[l->n++] = v;
}

/* Cabecera de «escribir n registros desde reg», según el intervalo del registro. */
static void cabeca_regs(r6_lote_t *l, uint32_t reg, int n)
{
    uint32_t op, base;
    if (reg >= 0x8000 && reg < 0xAC00)        { op = R63D_IT_SET_CONFIG_REG;  base = 0x8000; }
    else if (reg >= 0x28000 && reg < 0x29000) { op = R63D_IT_SET_CONTEXT_REG; base = 0x28000; }
    else if (reg >= 0x30000 && reg < 0x32000) { op = R63D_IT_SET_ALU_CONST;   base = 0x30000; }
    else if (reg >= 0x38000 && reg < 0x3C000) { op = R63D_IT_SET_RESOURCE;    base = 0x38000; }
    else if (reg >= 0x3C000 && reg < 0x3CFF0) { op = R63D_IT_SET_SAMPLER;     base = 0x3C000; }
    else if (reg >= 0x3CFF0 && reg < 0x3E200) { op = R63D_IT_SET_CTL_CONST;   base = 0x3CFF0; }
    else { l->cheio = true; return; }   /* un registro que no va por la cola: error de quien llama */
    r63d_p(l, R63D_PKT3(op, n + 1));
    r63d_p(l, (reg - base) >> 2);
}

void r63d_reg(r6_lote_t *l, uint32_t reg, uint32_t valor)
{
    cabeca_regs(l, reg, 1);
    r63d_p(l, valor);
}

void r63d_regs(r6_lote_t *l, uint32_t reg, const uint32_t *v, int n)
{
    cabeca_regs(l, reg, n);
    for (int i = 0; i < n; i++) r63d_p(l, v ? v[i] : 0);
}

uint32_t r63d_float(int v)
{
    if (v == 0) return 0;
    uint32_t s = 0, m = (uint32_t)v;
    if (v < 0) { s = 0x80000000u; m = (uint32_t)(-(int64_t)v); }
    int e = 31;
    while (!(m & 0x80000000u)) { m <<= 1; e--; }
    /* m tiene el 1 en el bit 31: la mantisa son los 23 siguientes, con redondeo al más próximo (par). */
    uint32_t frac = (m >> 8) & 0x7FFFFFu, resto = m & 0xFFu;
    uint32_t r = ((uint32_t)(e + 127) << 23) | frac;
    if (resto > 0x80u || (resto == 0x80u && (frac & 1u))) r++;
    return s | r;
}

/* SURFACE_SYNC sobre [mc, mc + bytes). CP_COHER_BASE va en unidades de 256 bytes: si «mc» no está
 * alineada, la base se redondea hacia abajo y el tamaño tiene que llegar hasta el final del rango
 * (D402: en el dv7 los vértices que caían detrás de los primeros 256 bytes se leían viejos de la
 * caché de texturas, porque el tamaño se contaba desde «mc» y no desde la base redondeada). */
static void sync(r6_lote_t *l, uint32_t tipo, uint32_t bytes, uint32_t mc)
{
    uint32_t base = mc & ~0xFFu;
    r63d_p(l, R63D_PKT3(R63D_IT_SURFACE_SYNC, 4));
    r63d_p(l, tipo);
    r63d_p(l, (mc - base + bytes + 255) >> 8);
    r63d_p(l, base >> 8);
    r63d_p(l, 10);   /* intervalo de sondeo */
}

#define SYNC_CB0   ((1u << 25) | (1u << 6))
#define SYNC_DB    ((1u << 26) | (1u << 14))
#define SYNC_TC    (1u << 23)
#define SYNC_VC    (1u << 24)
#define SYNC_SH    (1u << 27)

/* ------------------------------------------------------------------ inicio del motor (R7xx) */

typedef struct {
    int pipes, tile_pipes, simds, backends, gprs, threads, pilha, contextos, gs_threads;
    int exp, exp_pos, exp_smx, cf_insts, sx_sets, prim_fifo, hiz_fifo, earlyz_fifo, gs_prim;
} r7_chip_t;

static const r7_chip_t chips[4] = {
    /* RV770 */ { 4, 8, 10, 4, 256, 248, 512, 8, 32, 128, 16, 112, 2, 7, 0xF9, 0x30, 0x130, 384 },
    /* RV730 */ { 2, 4, 8, 2, 128, 248, 256, 8, 32, 256, 16, 240, 2, 7, 0xF9, 0x30, 0x130, 384 },
    /* RV740 */ { 4, 4, 8, 4, 256, 248, 512, 8, 32, 256, 16, 240, 2, 7, 0x100, 0x30, 0x130, 384 },
    /* RV710 */ { 2, 2, 2, 1, 256, 192, 256, 4, 16, 128, 16, 112, 1, 7, 0x40, 0x30, 0x130, 128 },
};

static int bits(uint32_t v) { int n = 0; while (v) { n += (int)(v & 1u); v >>= 1; } return n; }

/* A qué «backend» va cada tubería de mosaico (dos bits por tubería en R6xx/R7xx). */
static uint32_t mapa_backends(uint32_t n_tub_log2, uint32_t max_rb, uint32_t total_rb, uint32_t desativados)
{
    uint32_t dados = 0, mascara = 1u << (max_rb - 1);
    uint32_t tmp = desativados | ((0xFFu << max_rb) & 0xFFu);
    if ((tmp & 0xFFu) != 0xFFu) desativados = tmp;
    uint32_t tub = 1u << n_tub_log2, pedidos = total_rb - (uint32_t)bits(desativados & 0xFFu);
    if (!pedidos || tub < pedidos) return 0;
    uint32_t razao = tub / pedidos, resto = tub - razao * pedidos;
    for (uint32_t i = 0; i < max_rb; i++) {
        if (!(mascara & desativados)) {
            for (uint32_t j = 0; j < razao; j++) { dados <<= 2; dados |= max_rb - i - 1; }
            if (resto) { dados <<= 2; dados |= max_rb - i - 1; resto--; }
        }
        mascara >>= 1;
    }
    return dados;
}

int r7_inicio_motor(int f, const r7_leituras_t *l, r6_par_t *p, int max, uint32_t *max_contextos)
{
    if (f < 0 || f > 3) return -1;
    const r7_chip_t *c = &chips[f];
    int n = 0;
#define W(r, v) do { if (n >= max) return -1; p[n].reg = (r); p[n].valor = (v); n++; } while (0)
    if (max_contextos) *max_contextos = (uint32_t)c->contextos;
    W(0x8000, 0xFF);                                            /* GRBM_CNTL: plazo de lectura */
    uint32_t inativas = (l->cc_gc_shader_pipe_config >> 8) & 0xFFu;
    W(0x9100, bits(~inativas & 0xFFu) == 1 ? (1u << 5) : 0);    /* SPI_CONFIG_CNTL */

    uint32_t gb, pt = c->tile_pipes == 8 ? 3u : c->tile_pipes == 4 ? 2u : c->tile_pipes == 2 ? 1u : 0u;
    gb = pt << 1;
    uint32_t rb_des = (l->cc_rb_backend_disable >> 16) & 0xFFu, todos = (1u << c->backends) - 1u;
    if ((rb_des & todos) == todos) rb_des &= ~todos;            /* todos apagados: se corrige */
    gb |= mapa_backends(pt, (uint32_t)c->backends, 8, rb_des) << 16;
    if (f == R63D_RV770 || (l->mc_arb_ramcfg & 0x3u)) gb |= 1u << 4;      /* BANK_TILING */
    gb |= ((l->mc_arb_ramcfg >> 9) & 1u) << 6;                             /* GROUP_SIZE */
    uint32_t filas = (l->mc_arb_ramcfg >> 3) & 0x7u;
    if (filas > 3) filas = 3;
    gb |= filas << 8;                                                      /* ROW_TILING */
    gb |= filas << 14;                                                     /* SAMPLE_SPLIT */
    gb |= 1u << 11;                                                        /* BANK_SWAPS */
    W(0x98F0, gb);                                              /* GB_TILING_CONFIG */
    W(0x3F90, 0); W(0x9148, 0); W(0x3F94, 0); W(0x914C, 0);     /* CGTS_*_TCC_DISABLE */

    W(0x8760, 0x16u | (0x2Bu << 8));                            /* CP_QUEUE_THRESHOLDS */
    W(0x8764, 0x30);                                            /* CP_MEQ_THRESHOLDS */
    W(0x9508, l->ta_cntl_aux | (1u << 1));                      /* TA_CNTL_AUX: sin anisotropía en cubos */
    W(0x9058, l->sx_debug_1 | (1u << 16));                      /* SX_DEBUG_1: dirección SMX nueva */
    W(0xA020, (l->smx_dc_ctl0 & ~(0x1FFu << 1)) | ((uint32_t)(c->sx_sets * 64 - 1) << 1));   /* SMX_DC_CTL0 */
    if (f != R63D_RV740) W(0xA02C, 4u | (4u << 3) | (3u << 6) | (1u << 8));   /* SMX_EVENT_CTL */
    if (f != R63D_RV770) W(0xA008, 0x3F3F);                     /* SMX_SAR_CTL0 */
    W(0x98B0, (l->db_debug3 & ~(0x1Fu << 11)) | ((f == R63D_RV770 || f == R63D_RV740 ? 0x1Fu : 2u) << 11));
    if (f != R63D_RV770) W(0x9B8C, l->db_debug4 | (1u << 6));   /* DB_DEBUG4 */
    W(0x900C, (uint32_t)(c->exp / 4 - 1) | ((uint32_t)(c->exp_pos / 4 - 1) << 8) | ((uint32_t)(c->exp_smx / 4 - 1) << 16));
    W(0x8BCC, (uint32_t)c->prim_fifo | ((uint32_t)c->hiz_fifo << 12) | ((uint32_t)c->earlyz_fifo << 20));
    W(0x8B20, 0);                                               /* PA_SC_MULTI_CHIP_CNTL */
    W(0x8974, 1);                                               /* VGT_NUM_INSTANCES */
    W(0x913C, 4);                                               /* SPI_CONFIG_CNTL_1 */
    W(0x87FC, 0);                                               /* CP_PERFMON_CNTL */
    W(0x8CF0, (uint32_t)(16 * c->cf_insts) | ((f == R63D_RV740 ? 4u : 1u) << 8) | (0xE0u << 16) | (0x8u << 24));
    uint32_t sq = (l->sq_config & 0x00FFFFFFu) | (1u << 2) | (1u << 0) | (1u << 1) | (1u << 26) | (2u << 28) | (3u << 30);
    if (f == R63D_RV710) sq &= ~1u;                             /* sin caché de vértices */
    W(0x8C00, sq);                                              /* SQ_CONFIG */
    uint32_t g = (uint32_t)c->gprs;
    W(0x8C04, (g * 24 / 64) | ((g * 24 / 64) << 16) | ((g * 24 / 64 / 2) << 28));
    W(0x8C08, (g * 7 / 64) | ((g * 7 / 64) << 16));
    uint32_t t = (uint32_t)c->threads, gs = (uint32_t)c->gs_threads;
    W(0x8C0C, (t * 4 / 8) | ((t * 2 / 8) << 8) | ((t / 8) << 24) | ((t / 8 > gs ? gs : gs / 8) << 16));
    uint32_t st = (uint32_t)c->pilha / 4;
    W(0x8C10, st | (st << 16));
    W(0x8C14, st | (st << 16));
    uint32_t dg = g * 38 / 64;
    for (uint32_t i = 0; i < 8; i++) W(0x8DB0 + i * 4, dg | (dg << 8) | (dg << 16) | (dg << 24));
    W(0x8B24, 4095u | (255u << 16));                            /* PA_SC_FORCE_EOV_MAX_CNTS */
    W(0x88C4, (f == R63D_RV710 ? 1u : 2u) | (3u << 6));         /* VGT_CACHE_INVALIDATION */
    uint32_t gpe = (uint32_t)(c->gs_prim + c->pipes * 16);
    W(0x88CC, 128);                                             /* VGT_ES_PER_GS */
    W(0x88C8, gpe > 256 ? 256 : gpe);                           /* VGT_GS_PER_ES */
    W(0x88E8, 2);                                               /* VGT_GS_PER_VS */
    W(0x88D4, 16);                                              /* VGT_GS_VERTEX_REUSE */
    W(0x8B10, 0);                                               /* PA_SC_LINE_STIPPLE_STATE */
    W(0x9610, 0);                                               /* TCP_CNTL */
    W(0x8A14, 1u | (3u << 1));                                  /* PA_CL_ENHANCE */
    W(0x9714, 0);                                               /* VC_ENHANCE */
#undef W
    return n;
}

/* ------------------------------------------------------------------ inicio del motor (R6xx, D357) */

/* De r600_gpu_init (Linux 6.6, r600.c, cabecera MIT): tuberías, backends y reparto del secuenciador
 * por familia. Índice: familia - R63D_R600. */
typedef struct {
    int pipes, tile_pipes, backends, contextos;
    uint32_t gpr1, gpr2, threads, pilha1, pilha2;   /* SQ_GPR_RESOURCE_MGMT_1/2, THREAD, STACK_1/2 */
} r6_chip_t;

#define R6_GPRS(ps, vs, tmp)       ((uint32_t)(ps) | ((uint32_t)(vs) << 16) | ((uint32_t)(tmp) << 28))
#define R6_GPRS2(gs, es)           ((uint32_t)(gs) | ((uint32_t)(es) << 16))
#define R6_HILOS(ps, vs, gs, es)   ((uint32_t)(ps) | ((uint32_t)(vs) << 8) | ((uint32_t)(gs) << 16) | ((uint32_t)(es) << 24))
#define R6_PILHA(a, b)             ((uint32_t)(a) | ((uint32_t)(b) << 16))

static const r6_chip_t chips6[5] = {
    /* R600  */ { 4, 8, 4, 8, R6_GPRS(124, 124, 4), R6_GPRS2(0, 0), R6_HILOS(136, 48, 4, 4), R6_PILHA(128, 128), R6_PILHA(0, 0) },
    /* RV610 */ { 1, 1, 1, 4, R6_GPRS(44, 44, 2), R6_GPRS2(17, 17), R6_HILOS(79, 78, 4, 31), R6_PILHA(40, 40), R6_PILHA(32, 16) },
    /* RV630 */ { 2, 2, 1, 8, R6_GPRS(44, 44, 2), R6_GPRS2(18, 18), R6_HILOS(79, 78, 4, 31), R6_PILHA(40, 40), R6_PILHA(32, 16) },
    /* RV670 */ { 4, 4, 4, 8, R6_GPRS(44, 44, 2), R6_GPRS2(17, 17), R6_HILOS(79, 78, 4, 31), R6_PILHA(64, 64), R6_PILHA(64, 64) },
    /* RV635 */ { 2, 2, 1, 8, R6_GPRS(44, 44, 2), R6_GPRS2(18, 18), R6_HILOS(79, 78, 4, 31), R6_PILHA(40, 40), R6_PILHA(32, 16) },
};

int r6_inicio_motor(int f, const r7_leituras_t *l, r6_par_t *p, int max, uint32_t *max_contextos)
{
    if (!R63D_E_R6XX(f)) return -1;
    const r6_chip_t *c = &chips6[f - R63D_R600];
    int n = 0;
#define W(r, v) do { if (n >= max) return -1; p[n].reg = (r); p[n].valor = (v); n++; } while (0)
    if (max_contextos) *max_contextos = (uint32_t)c->contextos;
    /* No se hace: el borrado de las 32 superficies del HDP (0x2C14...), DCP/HDP/DMA_TILING_CONFIG,
     * HDP_HOST_PATH_CNTL ni el árbitro (ARB_POP, ARB_GDEC_RD_CNTL): pantalla, apertura de la CPU, 2D y
     * memoria siguen como los dejó el VBE (D284 punto 7). Lo de contexto (0x28xxx) va por la cola. */
    W(0x8000, 0xFF);                                            /* GRBM_CNTL: plazo de lectura */

    /* GB_TILING_CONFIG con los campos de RAMCFG de las R6xx (otra posición que en R7xx). */
    uint32_t pt = c->tile_pipes == 8 ? 3u : c->tile_pipes == 4 ? 2u : c->tile_pipes == 2 ? 1u : 0u;
    uint32_t gb = pt << 1;
    gb |= (l->mc_arb_ramcfg & 0x1u) << 4;                       /* BANK_TILING(NOOFBANK) */
    gb |= ((l->mc_arb_ramcfg >> 8) & 1u) << 6;                  /* GROUP_SIZE(BURSTLENGTH) */
    uint32_t filas = (l->mc_arb_ramcfg >> 2) & 0x7u;            /* NOOFROWS */
    if (filas > 3) filas = 3;
    gb |= filas << 8;                                           /* ROW_TILING */
    gb |= filas << 14;                                          /* SAMPLE_SPLIT */
    gb |= 1u << 11;                                             /* BANK_SWAPS */
    uint32_t rb_des = (l->cc_rb_backend_disable >> 16) & 0xFFu, todos = (1u << c->backends) - 1u;
    if ((rb_des & todos) == todos) rb_des &= ~todos;            /* todos apagados: se corrige */
    gb |= mapa_backends(pt, (uint32_t)c->backends, 8, rb_des) << 16;
    W(0x98F0, gb);                                              /* GB_TILING_CONFIG */

    W(0x8760, 0x16u | (0x2Bu << 8));                            /* CP_QUEUE_THRESHOLDS */
    W(0x8764, (0x40u << 16) | (0x40u << 24));                   /* CP_MEQ_THRESHOLDS: MEQ_END, ROQ_END */
    W(0x9508, (1u << 1) | (1u << 24) | (1u << 25) | (1u << 26)); /* TA_CNTL_AUX: sin anisotropía en cubos, SYNC_* */
    W(R6_SX_DEBUG_1, l->sx_debug_1 | 1u | (f != R63D_R600 ? 1u << 16 : 0u));   /* SMX_EVENT_RELEASE, dirección SMX nueva */
    W(0x9830, f == R63D_RV670 || f == R63D_RV635 ? 0u : 1u << 31);   /* DB_DEBUG: PREZ_MUST_WAIT_FOR_POSTZ_DONE */
    W(0x9838, 4u | (16u << 5) | (4u << 15) | (16u << 20));      /* DB_WATERMARKS */
    W(0x8B20, 0);                                               /* PA_SC_MULTI_CHIP_CNTL */
    W(0x8974, 0);                                               /* VGT_NUM_INSTANCES */
    W(0x9100, 0);                                               /* SPI_CONFIG_CNTL */
    W(0x913C, 0);                                               /* SPI_CONFIG_CNTL_1 */
    uint32_t fifo = l->sq_ms_fifo_sizes;
    if (f == R63D_RV610) fifo = 0xAu | (0xAu << 8) | (0xE0u << 16) | (0x8u << 24);
    else if (f == R63D_R600 || f == R63D_RV630) fifo = (fifo & ~(0xFFu << 16)) | (0x4u << 16);
    W(R6_SQ_MS_FIFO_SIZES, fifo);                               /* SQ_MS_FIFO_SIZES */
    uint32_t sq = (l->sq_config & ~((3u << 24) | (3u << 26) | (3u << 28) | (3u << 30))) | (1u << 2) | 1u |
                  (0u << 24) | (1u << 26) | (2u << 28) | (3u << 30);
    if (R63D_SEM_CACHE_VTX(f)) sq &= ~1u;                       /* sin caché de vértices */
    W(0x8C00, sq);                                              /* SQ_CONFIG */
    W(0x8C04, c->gpr1);
    W(0x8C08, c->gpr2);
    W(0x8C0C, c->threads);
    W(0x8C10, c->pilha1);
    W(0x8C14, c->pilha2);
    W(0x88C4, R63D_SEM_CACHE_VTX(f) ? 1u : 2u);                 /* VGT_CACHE_INVALIDATION: TC o VC y TC */
    W(0x8B40, 0xCu | (0x4u << 4) | (0x4u << 8) | (0xCu << 12)); /* PA_SC_AA_SAMPLE_LOCS_2S */
    W(0x8B44, 0xEu | (0xEu << 4) | (0x2u << 8) | (0x2u << 12) | (0xAu << 16) | (0x6u << 20) | (0x6u << 24) | (0xAu << 28));
    W(0x8B48, 0xEu | (0xBu << 4) | (0x4u << 8) | (0xCu << 12) | (0x1u << 16) | (0x6u << 20) | (0xAu << 24) | (0xEu << 28));
    W(0x8B4C, 0x6u | (0x1u << 4) | (0x0u << 8) | (0x0u << 12) | (0xBu << 16) | (0x4u << 20) | (0x7u << 24) | (0x8u << 28));
    uint32_t gpe = (uint32_t)c->pipes * 16u + (f == R63D_RV610 ? 32u : f == R63D_RV670 ? 128u : 0u);
    W(0x88CC, 128);                                             /* VGT_ES_PER_GS */
    W(0x88C8, gpe > 256 ? 256 : gpe);                           /* VGT_GS_PER_ES */
    W(0x88E8, 2);                                               /* VGT_GS_PER_VS */
    W(0x88D4, 16);                                              /* VGT_GS_VERTEX_REUSE */
    W(0x8B10, 0);                                               /* PA_SC_LINE_STIPPLE_STATE */
    W(0x9608, f == R63D_RV610 ? 8u << 5 : f == R63D_RV630 || f == R63D_RV635 ? 4u << 5 : f == R63D_R600 ? 1u << 9 : 0u);   /* TC_CNTL */
    W(0x8A14, 1u | (3u << 1));                                  /* PA_CL_ENHANCE */
    W(0x8BF0, 4095);                                            /* PA_SC_ENHANCE: FORCE_EOV_MAX_CLK_CNT */
    W(0x9714, 0);                                               /* VC_ENHANCE */
#undef W
    return n;
}

int r63d_inicio_motor(int f, const r7_leituras_t *l, r6_par_t *p, int max, uint32_t *max_contextos)
{
    return R63D_E_R6XX(f) ? r6_inicio_motor(f, l, p, max, max_contextos) : r7_inicio_motor(f, l, p, max, max_contextos);
}

/* ------------------------------------------------------------------ paquetes */

void r63d_me_initialize(r6_lote_t *l, bool r7xx, uint32_t max_contextos)
{
    r63d_p(l, R63D_PKT3(R63D_IT_ME_INITIALIZE, 6));
    r63d_p(l, 1);
    r63d_p(l, r7xx ? 0 : 3);
    r63d_p(l, max_contextos - 1);
    r63d_p(l, 1u << 16);   /* identificador de dispositivo 1 */
    r63d_p(l, 0);
    r63d_p(l, 0);
}

void r63d_teste_cola(r6_lote_t *l, uint32_t scratch, uint32_t valor) { r63d_reg(l, scratch, valor); }

void r63d_estado_base(r6_lote_t *l, int f, bool r7xx, uint32_t qd_pipes, uint32_t mc_fs)
{
    static const uint32_t zeros[17] = { 0 };
    if (!r7xx) { r63d_p(l, R63D_PKT3(R63D_IT_START_3D, 1)); r63d_p(l, 0); }
    r63d_p(l, R63D_PKT3(R63D_IT_CONTEXT_CONTROL, 2));
    r63d_p(l, 0x80000000u);
    r63d_p(l, 0x80000000u);

    /* Recursos del secuenciador: registros, hilos y pila por familia (r6xx_accel.c de xf86-video-ati;
     * en las R6xx, además, 4 hilos de GS y de ES y pila para ellos en las RV6xx). */
    static const struct { uint32_t ps, vs, hps, hvs, hgs, pilha, pgs, pes; } sq[R63D_N_FAMILIAS] = {
        /* RV770 */ { 192, 56, 188, 60, 0, 256, 0, 0 },
        /* RV730 */ { 84, 36, 188, 60, 0, 128, 0, 0 },
        /* RV740 */ { 84, 36, 188, 60, 0, 128, 0, 0 },
        /* RV710 */ { 192, 56, 144, 48, 0, 128, 0, 0 },
        /* R600  */ { 192, 56, 136, 48, 4, 128, 0, 0 },
        /* RV610 */ { 84, 36, 136, 48, 4, 40, 32, 16 },
        /* RV630 */ { 84, 36, 144, 40, 4, 40, 32, 16 },
        /* RV670 */ { 144, 40, 136, 48, 4, 40, 32, 16 },
        /* RV635 */ { 84, 36, 144, 40, 4, 40, 32, 16 },
    };
    int k = f >= 0 && f < R63D_N_FAMILIAS ? f : (r7xx ? R63D_RV730 : R63D_RV610);
    if (R63D_E_R7XX(k) != r7xx) k = r7xx ? R63D_RV730 : R63D_RV610;   /* familia y generación tienen que cuadrar */
    uint32_t v[9];
    v[0] = (R63D_SEM_CACHE_VTX(k) ? 0u : 1u) | (1u << 2) | (1u << 3) | (1u << 26) | (2u << 28) | (3u << 30);
    v[1] = sq[k].ps | (sq[k].vs << 16) | (4u << 28);
    v[2] = 0;
    v[3] = sq[k].hps | (sq[k].hvs << 8) | (sq[k].hgs << 16) | (sq[k].hgs << 24);
    v[4] = sq[k].pilha | (sq[k].pilha << 16);
    v[5] = sq[k].pgs | (sq[k].pes << 16);
    r63d_regs(l, 0x8C00, v, 6);                                 /* SQ_CONFIG ... SQ_STACK_RESOURCE_MGMT_2 */
    r63d_reg(l, 0x8D8C, r7xx ? 1u << 14 : 0u);                  /* SQ_DYN_GPR_CNTL_PS_FLUSH_REQ: límite del PC de vértices (R7xx) */
    r63d_reg(l, 0x9830, r7xx ? 0u : 0x82000000u);               /* DB_DEBUG (el valor de las R6xx es el de xf86-video-ati) */
    r63d_reg(l, 0x9838, 4u | (16u << 5) | (4u << 15) | ((r7xx ? 4u : 16u) << 20));   /* DB_WATERMARKS */
    r63d_regs(l, 0x3CFF0, zeros, 2);                            /* SQ_VTX_BASE_VTX_LOC, SQ_VTX_START_INST_LOC */
    r63d_regs(l, 0x288A8, zeros, 9);                            /* tamaños de los anillos de geometría: sin usar */

    /* Profundidad y plantilla. */
    r63d_reg(l, 0x28010, 0);                                    /* DB_DEPTH_INFO: sin Z hasta r63d_alvo */
    r63d_reg(l, 0x28800, 0);                                    /* DB_DEPTH_CONTROL */
    v[0] = (1u << 5) | (1u << 6);                               /* DB_RENDER_CONTROL: sin compresión de Z ni plantilla */
    v[1] = 2u | (2u << 2) | (2u << 4) | (r7xx ? 0u : 1u << 6);  /* DB_RENDER_OVERRIDE: sin Z ni plantilla jerárquicas;
                                                                   R6xx: FORCE_SHADER_Z_ORDER */
    r63d_regs(l, 0x28D0C, v, 2);
    r63d_reg(l, 0x28D30, 0);                                    /* DB_PRELOAD_CONTROL */
    r63d_reg(l, 0x28D44, 0x0000AA00u);                          /* DB_ALPHA_TO_MASK */
    r63d_reg(l, 0x2880C, (1u << 4) | (1u << 9));                /* DB_SHADER_CONTROL: Z antes y exportación doble */
    r63d_regs(l, 0x28028, zeros, 2);                            /* DB_STENCIL_CLEAR, DB_DEPTH_CLEAR */
    r63d_regs(l, 0x28430, zeros, 3);                            /* DB_STENCILREFMASK(_BF), SX_ALPHA_REF */

    /* Color. */
    v[0] = 1u << 24; v[1] = v[2] = v[3] = 0;
    r63d_regs(l, 0x28C30, v, 4);                                /* CB_CLRCMP_* */
    r63d_reg(l, 0x2823C, 0xF);                                  /* CB_SHADER_MASK */
    /* CB_SHADER_CONTROL: la superficie 0. En todas (D357, revisión): xf86-video-ati (r6xx_accel.c,
     * r600_set_default_state) y Mesa (r600_state.c, r600_emit_framebuffer_state) lo escriben también
     * en las R6xx; sin él, la superficie 0 podría quedar sin escribir en una R6xx. */
    r63d_reg(l, 0x287A0, 1);
    r63d_regs(l, 0x28410, zeros, 5);                            /* SX_ALPHA_TEST_CONTROL, CB_BLEND_RED..ALPHA */

    /* Recortes: todo abierto; el de la superficie lo pone r63d_alvo y el del programa r63d_estado. */
    r63d_reg(l, 0x28200, 0);                                    /* PA_SC_WINDOW_OFFSET */
    r63d_reg(l, 0x28230, r7xx ? 0xAAAAAAAAu : 0);               /* PA_SC_EDGERULE */
    r63d_reg(l, 0x2820C, 0xFFFF);                               /* PA_SC_CLIPRECT_RULE */
    v[0] = 0; v[1] = 8192u | (8192u << 16);
    for (uint32_t i = 0; i < 4; i++) r63d_regs(l, 0x28210 + i * 8, v, 2);
    v[0] = 1u << 31;
    for (uint32_t i = 0; i < 16; i++) r63d_regs(l, 0x28250 + i * 8, v, 2);
    v[0] = R63D_F_0; v[1] = R63D_F_1;
    r63d_regs(l, 0x282D0, v, 2);                                /* PA_SC_VPORT_ZMIN_0, ZMAX_0 */

    v[0] = 0;
    /* R7xx: FORCE_EOV_CNTDWN | FORCE_EOV_REZ | 0x00500000; R6xx: FORCE_EOV_CNTDWN | WALK_ORDER_ENABLE (bit 4;
     * D357: aquí ponía el bit 0, que es MSAA_ENABLE). */
    v[1] = r7xx ? ((1u << 14) | (1u << 16) | 0x00500000u) : ((1u << 14) | (1u << 4));
    r63d_regs(l, 0x28A48, v, 2);                                /* PA_SC_MPASS_PS_CNTL, PA_SC_MODE_CNTL */
    v[0] = 0; v[1] = 0;
    v[2] = (2u << 1) | 1u | (5u << 3);                          /* PA_SU_VTX_CNTL: centro en .5, redondeo al par, 1/256 */
    v[3] = v[4] = v[5] = v[6] = R63D_F_1;                       /* bandas de guarda */
    v[7] = v[8] = 0;
    r63d_regs(l, 0x28C00, v, 9);
    r63d_reg(l, 0x28C48, 0xFFFFFFFFu);                          /* PA_SC_AA_MASK */
    v[0] = 1u << 16;                                            /* PA_CL_CLIP_CNTL: sin recorte (SaguiGL ya recortó) */
    v[1] = 1u << 2;                                             /* PA_SU_SC_MODE_CNTL: sin descarte de caras */
    v[2] = (1u << 8) | (1u << 9);                               /* PA_CL_VTE_CNTL: x, y, z ya en pantalla; w llega como 1/w */
    v[3] = v[4] = 0;
    r63d_regs(l, 0x28810, v, 5);
    r63d_regs(l, 0x28DF8, zeros, 6);                            /* desplazamiento de polígonos: no */

    /* Interpolación: tres parámetros (0 = color, 1 = especular, 2 = textura), con perspectiva. */
    r63d_reg(l, 0x286C8, r7xx ? 1u : 0u);                       /* SPI_THREAD_GROUPING: PS_GROUPING solo en R7xx */
    r63d_reg(l, 0x28614, 0u | (1u << 8) | (2u << 16));          /* SPI_VS_OUT_ID_0: semánticas 0, 1 y 2 */
    v[0] = 0u | (1u << 8);
    v[1] = 1u | (1u << 8);
    v[2] = 2u | (1u << 8);
    r63d_regs(l, 0x28644, v, 3);                                /* SPI_PS_INPUT_CNTL_0, _1, _2 */
    r63d_reg(l, 0x286C4, (3u - 1u) << 1);                       /* SPI_VS_OUT_CONFIG: exporta 3 */
    v[0] = 3u | (1u << 28); v[1] = 0; v[2] = 0;
    r63d_regs(l, 0x286CC, v, 3);                                /* SPI_PS_IN_CONTROL_0, _1, SPI_INTERP_CONTROL_0 */
    r63d_regs(l, 0x286D8, zeros, 4);                            /* SPI_INPUT_Z y niebla */

    /* Sin sombreador de lectura de vértices aparte (lo hace el de vértices). */
    r63d_reg(l, 0x28894, mc_fs >> 8);                           /* SQ_PGM_START_FS */
    r63d_reg(l, 0x288A4, 0);
    r63d_reg(l, 0x288DC, 0);

    /* Geometría. */
    v[0] = 0xFFFFFFu; v[1] = v[2] = v[3] = 0;
    r63d_regs(l, 0x28400, v, 4);                                /* VGT_MAX_VTX_INDX, MIN, INDX_OFFSET, RESET_INDX */
    r63d_reg(l, 0x28A84, 0);
    r63d_reg(l, 0x28A94, 0);
    r63d_regs(l, 0x28AA0, zeros, 2);
    uint32_t pt[17] = { 0 };
    pt[2] = 8;                                                  /* PA_SU_LINE_CNTL: líneas de 1 píxel */
    r63d_regs(l, 0x28A00, pt, 17);
    r63d_regs(l, 0x28AB0, zeros, 3);
    r63d_reg(l, 0x28B20, 0);
    r63d_reg(l, 0x28350, 0);                                    /* SX_MISC */
    v[0] = (qd_pipes * 4 - 2) & 0xFFu;                          /* VGT_VERTEX_REUSE_BLOCK_CNTL */
    v[1] = (qd_pipes * 4) & 0x7Fu;                              /* VGT_OUT_DEALLOC_CNTL */
    r63d_regs(l, 0x28C58, v, 2);
}

void r63d_alvo(r6_lote_t *l, uint32_t mc_cor, uint32_t mc_z, int passo, int alt_buf, int larg, int alt, int zbits, bool sbu)
{
    r63d_alvo_modo(l, mc_cor, mc_z, passo, alt_buf, larg, alt, zbits, sbu, 0);
}

/* D432: lo mismo con el modo de la superficie de color dado (ARRAY_MODE de CB_COLOR0_INFO: 0 lineal
 * general, como los destinos propios; 1 lineal alineada). Para dibujar en la pantalla del CRTC, que
 * lleva el modo que le puso la BIOS (D1GRPH_CONTROL), sin Z (zbits 0: DB_DEPTH_INFO sin formato). */
void r63d_alvo_modo(r6_lote_t *l, uint32_t mc_cor, uint32_t mc_z, int passo, int alt_buf, int larg, int alt, int zbits, bool sbu,
                    uint32_t modo)
{
    uint32_t tam = (uint32_t)(passo / 8 - 1) | ((uint32_t)(passo * alt_buf / 64 - 1) << 10);
    r63d_reg(l, 0x28040, mc_cor >> 8);                          /* CB_COLOR0_BASE */
    r63d_reg(l, 0x280C0, mc_cor >> 8);                          /* CB_COLOR0_TILE (sin usar: apunta al color) */
    r63d_reg(l, 0x280E0, mc_cor >> 8);                          /* CB_COLOR0_FRAG */
    r63d_reg(l, 0x28060, tam);                                  /* CB_COLOR0_SIZE */
    r63d_reg(l, 0x28080, 0);                                    /* CB_COLOR0_VIEW */
    r63d_reg(l, 0x28100, 0);                                    /* CB_COLOR0_MASK */
    /* 8888 sin signo normalizado, lineal, bytes en el orden ARGB (0xAARRGGBB), mezcla saturada. */
    r63d_reg(l, 0x280A0, (0x1Au << 2) | ((modo & 0xFu) << 8) | (0u << 12) | (1u << 16) | (1u << 20) | (1u << 27));
    if (sbu) { r63d_p(l, R63D_PKT3(R63D_IT_SURFACE_BASE_UPDATE, 1)); r63d_p(l, 1u << 1); }   /* RV6xx: COLOR0 */
    if (zbits) {
        r63d_reg(l, 0x28000, tam);                              /* DB_DEPTH_SIZE */
        r63d_reg(l, 0x28004, 0);                                /* DB_DEPTH_VIEW */
        r63d_reg(l, 0x2800C, mc_z >> 8);                        /* DB_DEPTH_BASE */
        r63d_reg(l, 0x28014, mc_z >> 8);                        /* DB_HTILE_DATA_BASE (sin usar) */
        /* DB_DEPTH_INFO: mosaico 1D; el de 24 bits con su esténcil de 8 (DEPTH_8_24, D426: antes X8_24) */
        r63d_reg(l, 0x28010, (zbits == 16 ? 1u : 3u) | (2u << 15));
        r63d_reg(l, 0x28D34, (uint32_t)(alt_buf / 8 - 1));      /* DB_PREFETCH_LIMIT: filas de bloques de 8 - 1 (como Mesa) */
        if (sbu) { r63d_p(l, R63D_PKT3(R63D_IT_SURFACE_BASE_UPDATE, 1)); r63d_p(l, 1u << 0); }   /* RV6xx: DEPTH */
    } else {
        r63d_reg(l, 0x28010, 0);
    }
    uint32_t v[2];
    v[0] = 0; v[1] = (uint32_t)larg | ((uint32_t)alt << 16);
    r63d_regs(l, 0x28030, v, 2);                                /* PA_SC_SCREEN_SCISSOR */
    v[0] = 1u << 31;
    r63d_regs(l, 0x28204, v, 2);                                /* PA_SC_WINDOW_SCISSOR */
    r63d_regs(l, 0x28240, v, 2);                                /* PA_SC_GENERIC_SCISSOR */
}

void r63d_programas(r6_lote_t *l, uint32_t mc_vs, uint32_t mc_ps, bool com_textura)
{
    sync(l, SYNC_SH, R63D_SH_MAX * 4, mc_vs);
    r63d_reg(l, 0x28858, mc_vs >> 8);                           /* SQ_PGM_START_VS */
    r63d_reg(l, 0x28868, 5);                                    /* SQ_PGM_RESOURCES_VS: 5 registros, sin pila */
    r63d_reg(l, 0x288D0, 0);                                    /* SQ_PGM_CF_OFFSET_VS */
    sync(l, SYNC_SH, R63D_SH_MAX * 4, mc_ps);
    r63d_reg(l, 0x28840, mc_ps >> 8);                           /* SQ_PGM_START_PS */
    (void)com_textura;
    r63d_reg(l, 0x28850, 3u | (1u << 28));                      /* SQ_PGM_RESOURCES_PS: R0 a R2 (entran los tres parámetros siempre) */
    r63d_reg(l, 0x28854, 2);                                    /* SQ_PGM_EXPORTS_PS: un color */
    r63d_reg(l, 0x288CC, 0);                                    /* SQ_PGM_CF_OFFSET_PS */
}

void r63d_textura(r6_lote_t *l, uint32_t mc, int larg, int alt, int passo, bool borde_s, bool borde_t, bool linear)
{
    uint32_t v[7];
    sync(l, SYNC_TC, (uint32_t)passo * (uint32_t)alt * 4u, mc);
    v[0] = 1u | (1u << 3) | ((uint32_t)(passo / 8 - 1) << 8) | ((uint32_t)(larg - 1) << 19);   /* 2D, lineal alineada */
    v[1] = (uint32_t)(alt - 1) | (26u << 26);                   /* 8_8_8_8 */
    v[2] = mc >> 8;
    v[3] = mc >> 8;                                             /* sin niveles de detalle */
    v[4] = (1u << 14) | (2u << 16) | (1u << 19) | (0u << 22) | (3u << 25);   /* 0xAARRGGBB -> R, G, B, A */
    v[5] = 0;
    v[6] = 2u << 30;                                            /* textura válida */
    r63d_regs(l, 0x38000, v, 7);                                /* recurso 0 del sombreador de píxeles */
    uint32_t cs = borde_s ? 2u : 0u, ct = borde_t ? 2u : 0u, fl = linear ? 1u : 0u;
    v[0] = cs | (ct << 3) | (fl << 9) | (fl << 12);
    v[1] = 0;
    v[2] = 0;
    r63d_regs(l, 0x3C000, v, 3);                                /* muestreador 0 */
}

void r63d_estado(r6_lote_t *l, bool z_teste, bool z_escrita, uint32_t z_func, bool mz, uint32_t src, uint32_t dst, bool cor_escrita,
                 int rx, int ry, int rl, int ra, bool r600_puro)
{
    uint32_t z = 0;
    if (z_teste) z = (1u << 1) | (z_escrita ? 1u << 2 : 0u) | ((z_func & 7u) << 4);
    r63d_reg(l, 0x28800, z);                                    /* DB_DEPTH_CONTROL */
    r63d_reg(l, 0x28238, cor_escrita ? 0xFu : 0u);              /* CB_TARGET_MASK */
    r63d_reg(l, 0x28808, (0xCCu << 16) | (mz ? (1u << 8) | (r600_puro ? 0u : 1u << 7) : 0u));   /* CB_COLOR_CONTROL */
    uint32_t b = (src & 0x1Fu) | ((dst & 0x1Fu) << 8);
    r63d_reg(l, 0x28804, b);                                    /* CB_BLEND_CONTROL */
    if (!r600_puro) r63d_reg(l, 0x28780, b);                    /* CB_BLEND0_CONTROL */
    uint32_t v[2];
    v[0] = (uint32_t)rx | ((uint32_t)ry << 16) | (1u << 31);
    v[1] = (uint32_t)(rx + rl) | ((uint32_t)(ry + ra) << 16);
    r63d_regs(l, 0x28240, v, 2);                                /* PA_SC_GENERIC_SCISSOR */
}

void r63d_desenhar(r6_lote_t *l, uint32_t mc_vb, int n, int prim, bool sem_cache_vertices)
{
    uint32_t bytes = (uint32_t)n * R63D_VERT_BYTES, v[7];
    sync(l, sem_cache_vertices ? SYNC_TC : SYNC_VC, bytes, mc_vb);
    v[0] = mc_vb;
    v[1] = bytes - 1;
    v[2] = (uint32_t)R63D_VERT_BYTES << 8;                      /* paso; formato en la instrucción de lectura */
    v[3] = 1;
    v[4] = v[5] = 0;
    v[6] = 3u << 30;                                            /* búfer válido */
    r63d_regs(l, 0x38000 + 160 * 28, v, 7);                     /* recurso 0 del sombreador de vértices */
    r63d_reg(l, 0x8958, prim == 4 ? 4u : 2u);                   /* VGT_PRIMITIVE_TYPE: triángulos o líneas sueltos */
    r63d_p(l, R63D_PKT3(R63D_IT_INDEX_TYPE, 1));
    r63d_p(l, 0);
    r63d_p(l, R63D_PKT3(R63D_IT_NUM_INSTANCES, 1));
    r63d_p(l, 1);
    r63d_p(l, R63D_PKT3(R63D_IT_DRAW_INDEX_AUTO, 2));
    r63d_p(l, (uint32_t)n);
    r63d_p(l, 2);                                               /* índices automáticos */
}

void r63d_valla(r6_lote_t *l, uint32_t mc_cor, uint32_t bytes_cor, uint32_t mc_z, uint32_t bytes_z, uint32_t scratch, uint32_t valor)
{
    r63d_p(l, R63D_PKT3(R63D_IT_EVENT_WRITE, 1));
    r63d_p(l, 0x16);                                            /* vaciar e invalidar cachés */
    r63d_reg(l, R6_WAIT_UNTIL, (1u << 15) | (1u << 17));        /* motor 3D parado y limpio */
    if (bytes_cor) sync(l, SYNC_CB0, bytes_cor, mc_cor);
    if (bytes_z) sync(l, SYNC_DB, bytes_z, mc_z);
    r63d_reg(l, scratch, valor);
}

bool r63d_cp_dma(r6_lote_t *l, uint32_t orig, uint32_t dest, uint32_t bytes)
{
    if (!bytes) return false;
    r63d_p(l, R63D_PKT3(R63D_IT_EVENT_WRITE, 1));
    r63d_p(l, 0x16);                                            /* vaciar e invalidar cachés (el color a la VRAM) */
    r63d_reg(l, R6_WAIT_UNTIL, (1u << 15) | (1u << 17));        /* 3D parado y limpio (como la valla) */
    sync(l, SYNC_CB0, bytes, orig);
    while (bytes) {
        uint32_t t = bytes > R63D_CP_DMA_MAX ? R63D_CP_DMA_MAX : bytes;
        bytes -= t;
        r63d_p(l, R63D_PKT3(R63D_IT_CP_DMA, 5));
        r63d_p(l, orig);
        r63d_p(l, bytes ? 0u : 1u << 31);                       /* bits altos de la dirección a 0; CP_SYNC en el último */
        r63d_p(l, dest);
        r63d_p(l, 0);
        r63d_p(l, t);                                           /* memoria a memoria, sin cambiar el orden de los bytes */
        orig += t; dest += t;
    }
    r63d_reg(l, R6_WAIT_UNTIL, 1u << 8);                        /* WAIT_CP_DMA_IDLE */
    return true;
}

/* ------------------------------------------------------------------ sombreadores */

#define SEL_X 0u
#define SEL_Y 1u
#define SEL_Z 2u
#define SEL_W 3u
#define SEL_0 4u
#define SEL_1 5u
#define CF_TEX   0x01u
#define CF_VTX   0x02u
#define CF_ALU   0x08u
#define CF_EXP   0x27u
#define CF_EXP_F 0x28u   /* exportar y terminar ese tipo */
#define EXP_PIXEL 0u
#define EXP_POS   1u
#define EXP_PARAM 2u

/* Instrucción de flujo que lanza n lecturas (de vértices o de textura) en «ender» (palabras de 64 bits). */
static void cf(uint32_t *p, uint32_t ender, uint32_t n, uint32_t inst)
{
    uint32_t c = n - 1;
    p[0] = ender;
    p[1] = ((c & 7u) << 10) | ((c >> 3) << 19) | (inst << 23) | (1u << 31);
}

static void cf_alu(uint32_t *p, uint32_t ender, uint32_t n)
{
    p[0] = ender;
    p[1] = ((n - 1) << 18) | (CF_ALU << 26) | (1u << 31);
}

static void cf_exp(uint32_t *p, uint32_t base, uint32_t tipo, uint32_t gpr, uint32_t inst, bool fim, bool barreira)
{
    p[0] = base | (tipo << 13) | (gpr << 15);
    p[1] = SEL_X | (SEL_Y << 3) | (SEL_Z << 6) | (SEL_W << 9) | ((fim ? 1u : 0u) << 21) | (inst << 23) |
           ((barreira ? 1u : 0u) << 31);
}

/* Lectura del búfer de vértices 0: formato, desplazamiento y cómo llegan los 4 componentes. */
static void vtx(uint32_t *p, uint32_t gpr, uint32_t fmt, bool norm, uint32_t desloc, uint32_t bytes, bool mega,
                uint32_t sx, uint32_t sy, uint32_t sz, uint32_t sw)
{
    p[0] = ((bytes - 1) << 26);                                   /* lee el vértice cuyo índice está en R0.x */
    p[1] = gpr | (sx << 9) | (sy << 12) | (sz << 15) | (sw << 18) | (fmt << 22) |
           ((norm ? 0u : 2u) << 28) | ((norm ? 0u : 1u) << 30);  /* normalizado sin signo, o a escala con signo */
    p[2] = desloc | ((mega ? 1u : 0u) << 19);
    p[3] = 0;
}

/* dst.comp = src0.comp OP src1.comp, saturado a 0..1. */
static void alu(uint32_t *p, uint32_t inst, uint32_t dst, uint32_t s0, uint32_t s1, uint32_t comp, bool ultima, bool r7xx)
{
    p[0] = s0 | (comp << 10) | (s1 << 13) | (comp << 23) | ((ultima ? 1u : 0u) << 31);
    p[1] = (1u << 4) | (inst << (r7xx ? 7 : 8)) | (dst << 21) | (comp << 29) | (1u << 31);
}

/* dst.comp = a.comp OP b.comp; «sat»: saturado a 0..1. */
int r63d_vs(uint32_t *p, bool r7xx)
{
    (void)r7xx;
    cf(p + 0, 6, 4, CF_VTX);                                       /* 0: cuatro lecturas en la palabra 6 */
    cf_exp(p + 2, 60, EXP_POS, 1, CF_EXP_F, false, true);          /* 1: POS0 = R1 */
    cf_exp(p + 4, 0, EXP_PARAM, 2, CF_EXP, false, false);          /* 2: PARAM0 = R2 (color) */
    cf_exp(p + 6, 1, EXP_PARAM, 3, CF_EXP, false, false);          /* 3: PARAM1 = R3 (especular) */
    cf_exp(p + 8, 2, EXP_PARAM, 4, CF_EXP_F, true, false);         /* 4: PARAM2 = R4 (textura); fin */
    p[10] = p[11] = 0;                                             /* 5: relleno (las lecturas van alineadas a 16 bytes) */
    vtx(p + 12, 1, 35, false, 0, R63D_VERT_BYTES, true, SEL_X, SEL_Y, SEL_Z, SEL_W);   /* 4 flotantes */
    vtx(p + 16, 2, 26, true, 16, 4, false, SEL_Z, SEL_Y, SEL_X, SEL_W);               /* 0xAARRGGBB -> RGBA */
    vtx(p + 20, 3, 26, true, 20, 4, false, SEL_Z, SEL_Y, SEL_X, SEL_W);               /* especular 0x00RRGGBB */
    vtx(p + 24, 4, 30, false, 24, 8, false, SEL_X, SEL_Y, SEL_0, SEL_1);              /* 2 flotantes */
    return 28;
}

int r63d_ps_cor(uint32_t *p, bool r7xx)
{
    cf_alu(p + 0, 2, 4);                                           /* 0: cuatro sumas en la palabra 2 */
    cf_exp(p + 2, 0, EXP_PIXEL, 0, CF_EXP_F, true, true);          /* 1: color 0 = R0; fin */
    for (uint32_t c = 0; c < 4; c++) alu(p + 4 + c * 2, 0x00, 0, 0, 1, c, c == 3, r7xx);   /* R0 = sat(R0 + R1) */
    return 12;
}

int r63d_ps_tex(uint32_t *p, bool r7xx)
{
    cf(p + 0, 12, 1, CF_TEX);                                      /* 0: una muestra en la palabra 12 */
    cf_alu(p + 2, 4, 8);                                           /* 1: ocho operaciones en la palabra 4 */
    cf_exp(p + 4, 0, EXP_PIXEL, 0, CF_EXP_F, true, true);          /* 2: color 0 = R0; fin */
    p[6] = p[7] = 0;                                               /* 3: relleno */
    for (uint32_t c = 0; c < 4; c++) alu(p + 8 + c * 2, 0x01, 0, 0, 2, c, c == 3, r7xx);    /* R0 = sat(R0 * R2) */
    for (uint32_t c = 0; c < 4; c++) alu(p + 16 + c * 2, 0x00, 0, 0, 1, c, c == 3, r7xx);   /* R0 = sat(R0 + R1) */
    p[24] = 0x10u | (0u << 8) | (2u << 16);                        /* muestra de la textura 0 con R2 */
    p[25] = 2u | (SEL_X << 9) | (SEL_Y << 12) | (SEL_Z << 15) | (SEL_W << 18) | (0xFu << 28);   /* a R2; coordenadas 0..1 */
    p[26] = (0u << 15) | (SEL_X << 20) | (SEL_Y << 23) | (SEL_0 << 26) | (SEL_1 << 29);         /* muestreador 0; (u, v, 0, 1) */
    p[27] = 0;
    return 28;
}

/* ================================================================== gpu3d v2 (D426) */

uint32_t r63d_float_byte(uint32_t b)
{
    if (!b) return 0;
    if (b >= 255) return R63D_F_1;
    return r63d_float((int)(b * 0x10101u)) - (24u << 23);   /* b * 0x10101 / 2^24 = b / 255 (casi exacto) */
}

int r63d_vs2(uint32_t *p)
{
    cf(p + 0, 8, 7, CF_VTX);                                       /* 0: siete lecturas en la palabra 8 */
    cf_exp(p + 2, 60, EXP_POS, 1, CF_EXP_F, false, true);          /* 1: POS0 = R1 */
    for (uint32_t i = 0; i < 6; i++)                               /* 2-7: PARAM0-5 = R2-R7; el último, fin */
        cf_exp(p + 4 + 2 * i, i, EXP_PARAM, 2 + i, i == 5 ? CF_EXP_F : CF_EXP, i == 5, false);
    vtx(p + 16, 1, 35, false, 0, R63D_VERT2_BYTES > 64 ? 32 : R63D_VERT2_BYTES, true, SEL_X, SEL_Y, SEL_Z, SEL_W);
    vtx(p + 20, 2, 26, true, 16, 4, false, SEL_Z, SEL_Y, SEL_X, SEL_W);      /* color 0xAARRGGBB -> RGBA */
    vtx(p + 24, 3, 26, true, 20, 4, false, SEL_Z, SEL_Y, SEL_X, SEL_W);      /* especular (alfa: niebla) */
    for (uint32_t i = 0; i < 4; i++)                                         /* (s, t, r, q) de cada juego */
        vtx(p + 28 + 4 * i, 4 + i, 35, false, 24 + 16 * i, 16, false, SEL_X, SEL_Y, SEL_Z, SEL_W);
    return 44;
}

void r63d_spi(r6_lote_t *l, int np)
{
    uint32_t v[6];
    if (np < 3) np = 3;
    if (np > 6) np = 6;
    v[0] = 0u | (1u << 8) | (2u << 16) | (np > 3 ? 3u << 24 : 0u);
    v[1] = np > 4 ? 4u | (5u << 8) : 0u;
    r63d_regs(l, 0x28614, v, 2);                                /* SPI_VS_OUT_ID_0, _1 */
    for (int i = 0; i < np; i++) v[i] = (uint32_t)i | (1u << 8);
    r63d_regs(l, 0x28644, v, np);                               /* SPI_PS_INPUT_CNTL_0.. */
    r63d_reg(l, 0x286C4, (uint32_t)(np - 1) << 1);              /* SPI_VS_OUT_CONFIG */
    r63d_reg(l, 0x286CC, (uint32_t)np | (1u << 28));            /* SPI_PS_IN_CONTROL_0 */
}

void r63d_programas2(r6_lote_t *l, uint32_t mc_vs, uint32_t mc_ps, int gprs_ps, uint32_t bytes_ps)
{
    sync(l, SYNC_SH, R63D_VS2_MAX * 4, mc_vs);
    r63d_reg(l, 0x28858, mc_vs >> 8);                           /* SQ_PGM_START_VS */
    r63d_reg(l, 0x28868, 8);                                    /* SQ_PGM_RESOURCES_VS: R0 a R7 */
    r63d_reg(l, 0x288D0, 0);
    sync(l, SYNC_SH, bytes_ps, mc_ps);
    r63d_reg(l, 0x28840, mc_ps >> 8);                           /* SQ_PGM_START_PS */
    r63d_reg(l, 0x28850, (uint32_t)gprs_ps | (1u << 28));       /* SQ_PGM_RESOURCES_PS */
    r63d_reg(l, 0x28854, 2);                                    /* SQ_PGM_EXPORTS_PS: un color */
    r63d_reg(l, 0x288CC, 0);
}

void r63d_textura2(r6_lote_t *l, int u, uint32_t mc, int larg, int alt, int passo, uint32_t fmt, const gpu3d_unidade_t *un,
                   uint32_t bytes)
{
    static const uint32_t dfmt[4] = { 26, 0x31, 0x32, 0x33 };  /* 8_8_8_8, BC1, BC2, BC3 */
    static const uint32_t env[3] = { 0, 2, 1 };                 /* repetir (WRAP), sujetar (CLAMP_LAST_TEXEL), espejo (MIRROR) */
    uint32_t v[7];
    sync(l, SYNC_TC, bytes, mc);
    /* ARGB lineal alineada (1); DXT en 1D en mosaico (2, D426, DXT: en lineal la tarjeta no las lee bien). */
    uint32_t modo = fmt == G3D_FMT_ARGB8888 ? 1u : 2u;
    v[0] = 1u | (modo << 3) | ((uint32_t)(passo / 8 - 1) << 8) | ((uint32_t)(larg - 1) << 19);
    v[1] = (uint32_t)(alt - 1) | (dfmt[fmt & 3] << 26);
    v[2] = mc >> 8;
    v[3] = mc >> 8;
    /* ARGB8888 está en memoria como B, G, R, A: X = B; los DXT dan R, G, B, A en X, Y, Z, W. */
    v[4] = (1u << 14) | (fmt == G3D_FMT_ARGB8888 ? (2u << 16) | (1u << 19) | (0u << 22) : (0u << 16) | (1u << 19) | (2u << 22)) | (3u << 25);
    v[5] = 0;
    v[6] = 2u << 30;
    r63d_regs(l, 0x38000 + (uint32_t)u * 28, v, 7);
    uint32_t mag = un->filtro & G3D_FIL_LINEAR_MAG ? 1u : 0u, min = un->filtro & G3D_FIL_LINEAR_MIN ? 1u : 0u, an = 0;
    if (un->aniso > 1) {
        mag |= 4u; min |= 4u;                                   /* SQ_TEX_XY_FILTER_ANISO_* */
        an = un->aniso >= 16 ? 4u : un->aniso >= 8 ? 3u : un->aniso >= 4 ? 2u : 1u;
    }
    v[0] = env[un->env_s > 2 ? 1 : un->env_s] | (env[un->env_t > 2 ? 1 : un->env_t] << 3) | (mag << 9) | (min << 12) | (an << 19);
    v[1] = 0;
    v[2] = 0;
    r63d_regs(l, 0x3C000 + (uint32_t)u * 12, v, 3);
}

/* G3D_F_* -> V_028804_BLEND_*; G3D_EQ_* -> COMB_*. */
static uint32_t fator2(uint8_t f) { static const uint8_t m[11] = { 0, 1, 4, 5, 2, 3, 8, 9, 6, 7, 10 }; return f <= 10 ? m[f] : 1u; }
static uint32_t eq2(uint8_t q) { return q == G3D_EQ_SUBTRAI ? 1u : q == G3D_EQ_SUBTRAI_INV ? 4u : 0u; }

void r63d_estado2(r6_lote_t *l, const gpu3d_estado2_t *e, int larg, int alt, bool estencil, bool r600_puro)
{
    static const uint8_t rop[16] = { 0x00, 0x88, 0x44, 0xCC, 0x22, 0xAA, 0x66, 0xEE, 0x11, 0x99, 0x55, 0xDD, 0x33, 0xBB, 0x77, 0xFF };
    uint32_t fl = e->flags, z = 0;
    if (fl & G3D2_Z_TESTE) z |= (1u << 1) | (fl & G3D2_Z_ESCRITA ? 1u << 2 : 0u) | ((uint32_t)(e->func_z & 7) << 4);
    if ((fl & G3D2_ESTENCIL) && estencil) {
        uint32_t s = ((uint32_t)(e->est_func & 7)) | ((uint32_t)(e->est_falha & 7) << 3) | ((uint32_t)(e->est_zpassa & 7) << 6) |
                     ((uint32_t)(e->est_zfalha & 7) << 9);
        z |= 1u | (s << 8) | (s << 20);                         /* delante y detrás, lo mismo */
    }
    r63d_reg(l, 0x28800, z);                                    /* DB_DEPTH_CONTROL */
    uint32_t rm[2];
    rm[0] = rm[1] = (uint32_t)e->est_ref | ((uint32_t)e->est_mascara << 8) | ((uint32_t)e->est_escrita << 16);
    r63d_regs(l, 0x28430, rm, 2);                               /* DB_STENCILREFMASK, _BF */
    bool alfa = (fl & G3D2_ALFA_TESTE) != 0;
    r63d_reg(l, 0x28410, alfa ? (uint32_t)(e->func_alfa & 7) | (1u << 3) : 0u);   /* SX_ALPHA_TEST_CONTROL */
    r63d_reg(l, 0x28438, r63d_float_byte(e->ref_alfa));         /* SX_ALPHA_REF */
    /* Con la prueba alfa, el Z después del sombreador (LATE_Z), como r600_update_db_shader_control de Mesa. */
    r63d_reg(l, 0x2880C, (alfa ? 0u : 1u << 4) | (1u << 9));    /* DB_SHADER_CONTROL */
    r63d_reg(l, 0x28238, (uint32_t)(e->mascara_cor & 15));      /* CB_TARGET_MASK: R, G, B, A */
    bool mz = (fl & G3D2_MEZCLA) && !(fl & G3D2_LOGICA);
    uint32_t r3 = (fl & G3D2_LOGICA) ? rop[e->op_logica & 15] : 0xCCu;
    r63d_reg(l, 0x28808, (r3 << 16) | (mz ? (1u << 8) | (r600_puro ? 0u : 1u << 7) : 0u));   /* CB_COLOR_CONTROL */
    uint32_t b = fator2(e->mez_orig_rgb) | (eq2(e->mez_eq_rgb) << 5) | (fator2(e->mez_dest_rgb) << 8);
    if (e->mez_orig_rgb != e->mez_orig_alfa || e->mez_dest_rgb != e->mez_dest_alfa || e->mez_eq_rgb != e->mez_eq_alfa)
        b |= (fator2(e->mez_orig_alfa) << 16) | (eq2(e->mez_eq_alfa) << 21) | (fator2(e->mez_dest_alfa) << 24) | (1u << 29);
    r63d_reg(l, 0x28804, b);                                    /* CB_BLEND_CONTROL */
    if (!r600_puro) r63d_reg(l, 0x28780, b);                    /* CB_BLEND0_CONTROL */
    int x0 = 0, y0 = 0, x1 = larg, y1 = alt;
    if (fl & G3D2_TIJERA) { x0 = e->tijera_x; y0 = e->tijera_y; x1 = x0 + e->tijera_l; y1 = y0 + e->tijera_a; }
    if (x1 > larg) x1 = larg;
    if (y1 > alt) y1 = alt;
    uint32_t v[2];
    v[0] = (uint32_t)x0 | ((uint32_t)y0 << 16) | (1u << 31);
    v[1] = (uint32_t)x1 | ((uint32_t)y1 << 16);
    r63d_regs(l, 0x28240, v, 2);                                /* PA_SC_GENERIC_SCISSOR */
}

void r63d_estado_v1(r6_lote_t *l)
{
    r63d_reg(l, 0x28410, 0);                                    /* sin prueba alfa */
    r63d_reg(l, 0x2880C, (1u << 4) | (1u << 9));                /* DB_SHADER_CONTROL del estado base */
}

void r63d_desenhar2(r6_lote_t *l, uint32_t mc_vb, int n, uint32_t passo, bool sem_cache_vertices)
{
    uint32_t bytes = (uint32_t)n * passo, v[7];
    sync(l, sem_cache_vertices ? SYNC_TC : SYNC_VC, bytes, mc_vb);
    v[0] = mc_vb;
    v[1] = bytes - 1;
    v[2] = passo << 8;
    v[3] = 1;
    v[4] = v[5] = 0;
    v[6] = 3u << 30;
    r63d_regs(l, 0x38000 + 160 * 28, v, 7);
    r63d_reg(l, 0x8958, 4u);
    r63d_p(l, R63D_PKT3(R63D_IT_INDEX_TYPE, 1));
    r63d_p(l, 0);
    r63d_p(l, R63D_PKT3(R63D_IT_NUM_INSTANCES, 1));
    r63d_p(l, 1);
    r63d_p(l, R63D_PKT3(R63D_IT_DRAW_INDEX_AUTO, 2));
    r63d_p(l, (uint32_t)n);
    r63d_p(l, 2);
}

/* ------------------------------------------------------------------ el sombreador de píxeles v2 */

#define R7_T_COR      0u
#define R7_T_ESPEC    1u
#define R7_T_COORD(i) (2u + (i))
#define R7_T_TEX(u)   (6u + (u))
#define R7_T_ANT(k)   (10u + (k))
#define R7_T_AUX(k)   (12u + (k))
#define R7_T_SAIDA    17u
#define R7_T_CONST    18u           /* GL_TEXTURE_ENV_COLOR de la unidad que se está combinando */
#define R7_SRC_0      248u          /* constantes del ALU: 0.0, 1.0, 0.5 y literal */
#define R7_SRC_1      249u
#define R7_SRC_05     252u
#define R7_SRC_LIT    253u
#define R7_OP_ADD     0x00u
#define R7_OP_MUL     0x01u
#define R7_OP_MOV     0x19u
#define R7_OP_DOT4    0x50u
#define R7_OP3_MULADD 0x10u         /* _M2 0x11, _M4 0x12: el resultado por 2 o por 4 */

/* Un argumento: un registro (sus canales r, g, b o su alfa repetido), un literal (los cuatro valores) o una
 * constante del ALU; con signo. */
typedef struct { uint8_t tipo; uint8_t reg; bool alfa; uint8_t neg; uint32_t lit[4]; } r7a_t;
#define R7A_GPR 0
#define R7A_LIT 1
#define R7A_K   2
/* Una media instrucción (la del color, en los canales x, y, z; o la del alfa, en w). */
typedef struct { bool usada; uint8_t op; uint8_t ns; r7a_t a[3]; uint8_t dst; uint8_t omod; bool sat; bool escreve_w; } r7m_t;

static r7a_t a_gpr(uint8_t reg, bool alfa) { r7a_t a = { R7A_GPR, reg, alfa, 0, { 0, 0, 0, 0 } }; return a; }
static r7a_t a_k(uint8_t k) { r7a_t a = { R7A_K, k, false, 0, { 0, 0, 0, 0 } }; return a; }
static r7a_t a_neg(r7a_t a) { a.neg ^= 1u; return a; }

/* Un operando del ALU de un canal: sel, canal, signo y (literal) su valor. */
typedef struct { uint32_t sel, chan, neg, lit; } r7s_t;
static r7s_t operando(const r7a_t *a, uint32_t canal)
{
    r7s_t s = { 0, 0, a->neg, 0 };
    if (a->tipo == R7A_GPR) { s.sel = a->reg; s.chan = a->alfa ? 3u : canal; }
    else if (a->tipo == R7A_LIT) { s.sel = R7_SRC_LIT; s.lit = a->lit[a->alfa ? 3u : canal]; }
    else s.sel = a->reg;
    return s;
}

/* Un grupo de instrucciones del ALU (hasta las cuatro ranuras x, y, z, w) y sus literales. */
typedef struct { bool usada[4]; uint8_t op[4], ns[4], dst[4], omod[4]; bool sat[4], escreve[4]; r7s_t s[4][3]; } r7g_t;

/* Comprueba los puertos de lectura de los registros (VEC_012: el origen i en el ciclo i; en cada ciclo, un
 * registro por canal) y que no haya más de 4 literales distintos, como check_vector/reserve_gpr de Mesa
 * (r600_asm.c). nlit/lit: los literales del grupo, con el canal que le toca a cada uno. */
static bool grupo_ok(r7g_t *g, uint32_t lit[4], int *nlit)
{
    int hw[3][4];
    for (int c = 0; c < 3; c++) for (int k = 0; k < 4; k++) hw[c][k] = -1;
    *nlit = 0;
    for (int r = 0; r < 4; r++) {
        if (!g->usada[r]) continue;
        for (int i = 0; i < g->ns[r]; i++) {
            r7s_t *s = &g->s[r][i];
            if (s->sel < 128) {
                if (i == 1 && s->sel == g->s[r][0].sel && s->chan == g->s[r][0].chan) continue;
                int *h = &hw[i][s->chan & 3];
                if (*h == -1) *h = (int)s->sel;
                else if (*h != (int)s->sel) return false;
            } else if (s->sel == R7_SRC_LIT) {
                int j = 0;
                while (j < *nlit && lit[j] != s->lit) j++;
                if (j == *nlit) { if (*nlit == 4) return false; lit[(*nlit)++] = s->lit; }
                s->chan = (uint32_t)j;
            }
        }
    }
    return true;
}

typedef struct { uint32_t *alu; int n, max; int grupos; bool erro; bool r7; int gini[160]; } r7_ps_t;

static void grupo_emitir(r7_ps_t *ps, r7g_t *g)
{
    uint32_t lit[4];
    int nlit;
    if (!grupo_ok(g, lit, &nlit)) { ps->erro = true; return; }
    int ultima = -1;
    for (int r = 0; r < 4; r++) if (g->usada[r]) ultima = r;
    if (ultima < 0) return;
    if (ps->n + 8 + 4 > ps->max || ps->grupos >= 159) { ps->erro = true; return; }
    ps->gini[ps->grupos] = ps->n;
    for (int r = 0; r < 4; r++) {
        if (!g->usada[r]) continue;
        const r7s_t *s = g->s[r];
        uint32_t w0 = s[0].sel | (s[0].chan << 10) | (s[0].neg << 12) | (r == ultima ? 1u << 31 : 0u);
        if (g->ns[r] > 1) w0 |= (s[1].sel << 13) | (s[1].chan << 23) | (s[1].neg << 25);
        uint32_t w1;
        if (g->op[r] == R7_OP3_MULADD) {
            uint32_t op = R7_OP3_MULADD + (g->omod[r] == 1 ? 1u : g->omod[r] == 2 ? 2u : 0u);
            w1 = s[2].sel | (s[2].chan << 10) | (s[2].neg << 12) | (op << 13);
        } else {
            w1 = (g->escreve[r] ? 1u << 4 : 0u) | ((uint32_t)g->omod[r] << (ps->r7 ? 5 : 6)) | ((uint32_t)g->op[r] << (ps->r7 ? 7 : 8));
        }
        w1 |= ((uint32_t)g->dst[r] << 21) | ((uint32_t)r << 29) | (g->sat[r] ? 1u << 31 : 0u);
        ps->alu[ps->n++] = w0;
        ps->alu[ps->n++] = w1;
    }
    for (int i = 0; i < nlit; i++) ps->alu[ps->n++] = lit[i];
    if (nlit & 1) ps->alu[ps->n++] = 0;
    ps->grupos++;
    ps->gini[ps->grupos] = ps->n;
}

/* Pone una media instrucción en las ranuras de un grupo (color: x, y, z; alfa: w). */
static void meia_por(r7g_t *g, const r7m_t *m, bool alfa)
{
    int r0 = alfa ? 3 : 0, r1 = alfa ? 4 : 3;
    for (int r = r0; r < r1; r++) {
        g->usada[r] = true;
        g->op[r] = m->op; g->ns[r] = m->ns; g->dst[r] = m->dst; g->omod[r] = m->omod; g->sat[r] = m->sat; g->escreve[r] = true;
        for (int i = 0; i < m->ns; i++) g->s[r][i] = operando(&m->a[i], alfa ? 3u : (uint32_t)r);
    }
}

/* El producto escalar (DOT4) de a·b en x, y, z y 0·0 en w: el resultado en el color y, si «com_alfa», en el alfa. */
static void dot_por(r7g_t *g, const r7m_t *m, bool com_alfa)
{
    for (int r = 0; r < 4; r++) {
        g->usada[r] = true;
        g->op[r] = R7_OP_DOT4; g->ns[r] = 2; g->dst[r] = m->dst; g->omod[r] = m->omod; g->sat[r] = m->sat;
        g->escreve[r] = r < 3 || com_alfa;
        if (r < 3) { g->s[r][0] = operando(&m->a[0], (uint32_t)r); g->s[r][1] = operando(&m->a[1], (uint32_t)r); }
        else { g->s[r][0] = (r7s_t){ R7_SRC_0, 0, 0, 0 }; g->s[r][1] = (r7s_t){ R7_SRC_0, 0, 0, 0 }; }
    }
}

static const r7g_t r7g_vazio;
typedef struct { r7m_t m[8]; int n; } r7l_t;
static r7m_t *l_nova(r7l_t *l, uint8_t op, uint8_t ns)
{
    static r7m_t lixo;
    if (l->n >= 8) return &lixo;
    r7m_t *m = &l->m[l->n++];
    *m = (r7m_t){ true, op, ns, { a_k(R7_SRC_0), a_k(R7_SRC_0), a_k(R7_SRC_0) }, 0, 0, false, false };
    return m;
}

/* Los tres argumentos de un canal de la unidad u: «1 - x» con una media instrucción antes (a un auxiliar).
 * La constante de la unidad ya está en R7_T_CONST (r63d_ps2_gerar la carga con literales: así ninguna
 * media instrucción necesita más de los 4 literales que caben en un grupo). */
static void args7(r7l_t *l, const uint8_t ft[3], const uint8_t opd[3], uint32_t u, uint8_t ant, bool alfa, r7a_t a[3])
{
    for (int k = 0; k < 3; k++) {
        bool de_alfa = alfa || opd[k] == G3D_OPD_ALFA || opd[k] == G3D_OPD_UM_MENOS_ALFA;
        bool inv = opd[k] == G3D_OPD_UM_MENOS_COR || opd[k] == G3D_OPD_UM_MENOS_ALFA;
        uint8_t reg = ft[k] == G3D_FT_TEXTURA ? (uint8_t)R7_T_TEX(u) : ft[k] == G3D_FT_PRIMARIA ? (uint8_t)R7_T_COR :
                      ft[k] == G3D_FT_CONSTANTE ? (uint8_t)R7_T_CONST : ant;
        r7a_t x = a_gpr(reg, de_alfa);
        if (inv) {
            r7m_t *m = l_nova(l, R7_OP_ADD, 2);              /* aux = 1 - x */
            m->a[0] = a_k(R7_SRC_1); m->a[1] = a_neg(x); m->dst = (uint8_t)R7_T_AUX(k);
            x = a_gpr((uint8_t)R7_T_AUX(k), alfa);
        }
        a[k] = x;
    }
}

/* Un canal de la unidad: func(arg0, arg1, arg2) * escala, saturado, a «dest». DOT3: devuelve true (va aparte). */
static bool canal7(r7l_t *l, uint8_t func, const r7a_t a[3], uint8_t escala, uint8_t dest, bool alfa)
{
    uint8_t omod = escala == 4 ? 2 : escala == 2 ? 1 : 0;
    r7a_t t3 = a_gpr((uint8_t)R7_T_AUX(3), alfa), t4 = a_gpr((uint8_t)R7_T_AUX(4), alfa);
    r7m_t *m;
    bool dot = false;
    switch (func) {
    case G3D_CB_MODULAR: m = l_nova(l, R7_OP_MUL, 2); m->a[0] = a[0]; m->a[1] = a[1]; break;
    case G3D_CB_SOMAR:   m = l_nova(l, R7_OP_ADD, 2); m->a[0] = a[0]; m->a[1] = a[1]; break;
    case G3D_CB_SOMAR_SINAL:
        m = l_nova(l, R7_OP_ADD, 2); m->a[0] = a[0]; m->a[1] = a[1]; m->dst = (uint8_t)R7_T_AUX(3);
        m = l_nova(l, R7_OP_ADD, 2); m->a[0] = t3; m->a[1] = a_neg(a_k(R7_SRC_05)); break;
    case G3D_CB_INTERPOLAR:      /* aux3 = a0 - a1; res = aux3 * a2 + a1 */
        m = l_nova(l, R7_OP_ADD, 2); m->a[0] = a[0]; m->a[1] = a_neg(a[1]); m->dst = (uint8_t)R7_T_AUX(3);
        m = l_nova(l, R7_OP3_MULADD, 3); m->a[0] = t3; m->a[1] = a[2]; m->a[2] = a[1]; break;
    case G3D_CB_SUBTRAIR: m = l_nova(l, R7_OP_ADD, 2); m->a[0] = a[0]; m->a[1] = a_neg(a[1]); break;
    case G3D_CB_DOT3_RGB:
    case G3D_CB_DOT3_RGBA:       /* aux3 = 2 (a0 - 0,5); aux4 = 2 (a1 - 0,5); res = aux3 · aux4 */
        m = l_nova(l, R7_OP3_MULADD, 3); m->a[0] = a[0]; m->a[1] = a_k(R7_SRC_1); m->a[2] = a_neg(a_k(R7_SRC_05)); m->omod = 1;
        m->dst = (uint8_t)R7_T_AUX(3);
        m = l_nova(l, R7_OP3_MULADD, 3); m->a[0] = a[1]; m->a[1] = a_k(R7_SRC_1); m->a[2] = a_neg(a_k(R7_SRC_05)); m->omod = 1;
        m->dst = (uint8_t)R7_T_AUX(4);
        m = l_nova(l, R7_OP_DOT4, 2); m->a[0] = t3; m->a[1] = t4;
        dot = true;
        break;
    default: m = l_nova(l, R7_OP_MOV, 1); m->a[0] = a[0]; break;
    }
    m->dst = dest; m->omod = omod; m->sat = true;
    return dot;
}

/* Las dos listas, en grupos: la i-ésima del color con la i-ésima del alfa si los puertos lo permiten (si no,
 * cada una en su grupo). El DOT4 ocupa las cuatro ranuras: va solo (con el alfa si es DOT3_RGBA). */
static void juntar7(r7_ps_t *ps, const r7l_t *c, const r7l_t *a, bool dot3_rgba)
{
    int ia = 0;
    for (int ic = 0; ic < c->n; ic++) {
        r7g_t g = r7g_vazio;
        if (c->m[ic].op == R7_OP_DOT4) { dot_por(&g, &c->m[ic], dot3_rgba); grupo_emitir(ps, &g); continue; }
        meia_por(&g, &c->m[ic], false);
        if (ia < a->n) {
            r7g_t h = g;
            meia_por(&h, &a->m[ia], true);
            uint32_t lit[4];
            int nl;
            r7g_t prova = h;
            if (grupo_ok(&prova, lit, &nl)) { grupo_emitir(ps, &h); ia++; continue; }
        }
        grupo_emitir(ps, &g);
    }
    for (; ia < a->n; ia++) {
        r7g_t g = r7g_vazio;
        meia_por(&g, &a->m[ia], true);
        grupo_emitir(ps, &g);
    }
}

void r63d_ps2_gerar(r63d_ps2_t *out, const gpu3d_estado2_t *e, bool r7xx)
{
    static uint32_t alu[R63D_PS2_MAX];
    static r7_ps_t ps;
    ps.alu = alu; ps.n = 0; ps.max = R63D_PS2_MAX - 64; ps.grupos = 0; ps.erro = false; ps.r7 = r7xx; ps.gini[0] = 0;
    out->erro = false; out->n = 0; out->grupos = 0; out->clausulas = 0;
    int nu = e->unidades > 4 ? 4 : e->unidades, ntex = 0;
    uint32_t tex[4][4];
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        if (!x->ativa) continue;
        uint32_t co = x->coord > 3 ? (uint32_t)u : x->coord;
        tex[ntex][0] = 0x10u | ((uint32_t)u << 8) | (R7_T_COORD(co) << 16);                  /* SAMPLE, recurso u */
        tex[ntex][1] = R7_T_TEX(u) | (SEL_X << 9) | (SEL_Y << 12) | (SEL_Z << 15) | (SEL_W << 18) | (0xFu << 28);
        tex[ntex][2] = ((uint32_t)u << 15) | (SEL_X << 20) | (SEL_Y << 23) | (SEL_0 << 26) | (SEL_1 << 29);   /* muestreador u */
        tex[ntex][3] = 0;
        ntex++;
    }
    uint8_t ant = (uint8_t)R7_T_COR;
    int k = 0;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        if (!x->ativa) continue;
        uint8_t dest = (uint8_t)R7_T_ANT(k);
        r7l_t lc, la;
        lc.n = la.n = 0;
        r7a_t ac[3], aa[3];
        bool usa_c = false, d3a = x->rgb_func == G3D_CB_DOT3_RGBA;
        for (int j = 0; j < 3; j++) usa_c |= x->rgb_fonte[j] == G3D_FT_CONSTANTE || (!d3a && x->alfa_fonte[j] == G3D_FT_CONSTANTE);
        if (usa_c) {   /* R18 = la constante (un grupo con sus cuatro literales) */
            r7g_t g = r7g_vazio;
            uint32_t c = x->constante, by[4] = { (c >> 16) & 0xFFu, (c >> 8) & 0xFFu, c & 0xFFu, c >> 24 };
            for (int r = 0; r < 4; r++) {
                g.usada[r] = true; g.op[r] = R7_OP_MOV; g.ns[r] = 1; g.dst[r] = (uint8_t)R7_T_CONST; g.escreve[r] = true;
                g.s[r][0] = (r7s_t){ R7_SRC_LIT, 0, 0, r63d_float_byte(by[r]) };
            }
            grupo_emitir(&ps, &g);
        }
        args7(&lc, x->rgb_fonte, x->rgb_opd, (uint32_t)u, ant, false, ac);
        if (!d3a) args7(&la, x->alfa_fonte, x->alfa_opd, (uint32_t)u, ant, true, aa);
        canal7(&lc, x->rgb_func, ac, x->rgb_escala, dest, false);
        if (!d3a) canal7(&la, x->alfa_func > G3D_CB_SUBTRAIR ? G3D_CB_SUBSTITUIR : x->alfa_func, aa, x->alfa_escala, dest, true);
        juntar7(&ps, &lc, &la, d3a);
        ant = dest;
        k ^= 1;
    }
    /* Salida: sat(ant + especular) y el alfa de ant; con niebla, mezclada con su color por el alfa del especular. */
    r7l_t lc, la;
    lc.n = la.n = 0;
    r7m_t *m = l_nova(&lc, R7_OP_ADD, 2);
    m->a[0] = a_gpr(ant, false); m->a[1] = a_gpr((uint8_t)R7_T_ESPEC, false); m->dst = (uint8_t)R7_T_SAIDA; m->sat = true;
    m = l_nova(&la, R7_OP_MOV, 1);
    m->a[0] = a_gpr(ant, true); m->dst = (uint8_t)R7_T_SAIDA;
    if (e->flags & G3D2_NEBLINA) {
        uint32_t c = e->cor_neblina;
        r7a_t nb = { R7A_LIT, 0, false, 0, { r63d_float_byte((c >> 16) & 0xFFu), r63d_float_byte((c >> 8) & 0xFFu), r63d_float_byte(c & 0xFFu), 0 } };
        m = l_nova(&lc, R7_OP_ADD, 2);                 /* aux0 = saida - niebla */
        m->a[0] = a_gpr((uint8_t)R7_T_SAIDA, false); m->a[1] = a_neg(nb); m->dst = (uint8_t)R7_T_AUX(0);
        m = l_nova(&lc, R7_OP3_MULADD, 3);             /* saida = aux0 * f + niebla */
        m->a[0] = a_gpr((uint8_t)R7_T_AUX(0), false); m->a[1] = a_gpr((uint8_t)R7_T_ESPEC, true); m->a[2] = nb;
        m->dst = (uint8_t)R7_T_SAIDA; m->sat = true;
    }
    juntar7(&ps, &lc, &la, false);
    if (ps.erro) { out->erro = true; return; }

    /* Montaje: CF (lectura de texturas, cláusulas del ALU de 128 ranuras como mucho, exportación), las lecturas
     * (alineadas a 16 bytes) y el código del ALU. Las cláusulas se cortan entre grupos. */
    int cl_ini[16], cl_n[16], ncl = 0;
    for (int g = 0; g < ps.grupos;) {
        if (ncl == 16) { out->erro = true; return; }
        int g0 = g;
        while (g < ps.grupos && ps.gini[g + 1] - ps.gini[g0] <= 256) g++;
        if (g == g0) { out->erro = true; return; }
        cl_ini[ncl] = ps.gini[g0]; cl_n[ncl] = ps.gini[g] - ps.gini[g0]; ncl++;
    }
    int ncf = (ntex ? 1 : 0) + ncl + 1;
    int tex_end = (ncf + 1) & ~1, alu_end = tex_end + 2 * ntex;   /* en palabras de 64 bits */
    int total = 2 * alu_end + ps.n;
    if (total > R63D_PS2_MAX) { out->erro = true; return; }
    uint32_t *p = out->p;
    for (int i = 0; i < total; i++) p[i] = 0;
    int c = 0;
    if (ntex) { cf(p + 2 * c, (uint32_t)tex_end, (uint32_t)ntex, CF_TEX); c++; }
    for (int i = 0; i < ncl; i++) { cf_alu(p + 2 * c, (uint32_t)(alu_end + cl_ini[i] / 2), (uint32_t)(cl_n[i] / 2)); c++; }
    cf_exp(p + 2 * c, 0, EXP_PIXEL, R7_T_SAIDA, CF_EXP_F, true, true);
    for (int t = 0; t < ntex; t++) for (int w = 0; w < 4; w++) p[2 * tex_end + 4 * t + w] = tex[t][w];
    for (int i = 0; i < ps.n; i++) p[2 * alu_end + i] = alu[i];
    out->grupos = ps.grupos;
    out->clausulas = ncl;
    out->n = total;
}
