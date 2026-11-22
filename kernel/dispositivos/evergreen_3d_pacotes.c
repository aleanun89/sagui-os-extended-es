// SPDX-License-Identifier: GPL-2.0-or-later
/* evergreen_3d_pacotes.c - paquetes PM4, sombreadores y valores de inicio del motor 3D
 * de las Radeon Evergreen y Northern Islands sin Cayman (D427). Lógica pura: se prueba
 * también en el PC (tests/evergreen_3d_unidad.c). Ver evergreen_3d.h (referencias y
 * diferencias con las R6xx/R7xx). Usa los lotes de r600_3d_pacotes.c (r63d_lote, r63d_p)
 * sin cambiarlos. */
#include "evergreen_3d.h"

/* ------------------------------------------------------------------ familias */

static const eg3d_chip_t chips[EG3D_N_FAMILIAS] = {
    /*            nombre     firmware   ses pip tpi sim rb gpr  thr gs pila sx  exp pos smx ctx cf prim   hiz   ez     golden      vc */
    [EG3D_CEDAR]   = { "Cedar",   "CEDAR",   1, 2, 2, 2, 1, 256, 192, 16, 256, 4, 128, 32, 96, 4, 1, 0x40, 0x30, 0x130, 0x02010001u, false },
    [EG3D_REDWOOD] = { "Redwood", "REDWOOD", 1, 4, 4, 5, 2, 256, 248, 32, 256, 4, 256, 64, 192, 8, 2, 0x100, 0x30, 0x130, 0x02010002u, true },
    [EG3D_JUNIPER] = { "Juniper", "JUNIPER", 1, 4, 4, 10, 4, 256, 248, 32, 512, 4, 256, 64, 192, 8, 2, 0x100, 0x30, 0x130, 0x02010002u, true },
    [EG3D_CYPRESS] = { "Cypress", "CYPRESS", 2, 4, 8, 10, 8, 256, 248, 32, 512, 4, 256, 64, 192, 8, 2, 0x100, 0x30, 0x130, 0x02011003u, true },
    [EG3D_BARTS]   = { "Barts",   "BARTS",   2, 4, 8, 7, 8, 256, 248, 32, 512, 4, 256, 64, 192, 8, 2, 0x100, 0x30, 0x130, 0x02011003u, true },
    [EG3D_TURKS]   = { "Turks",   "TURKS",   1, 4, 4, 6, 2, 256, 248, 32, 256, 4, 256, 64, 192, 8, 2, 0x100, 0x30, 0x130, 0x02010002u, true },
    [EG3D_CAICOS]  = { "Caicos",  "CAICOS",  1, 2, 2, 2, 1, 256, 192, 16, 256, 4, 128, 32, 96, 4, 1, 0x40, 0x30, 0x130, 0x02010001u, false },
};

const eg3d_chip_t *eg3d_chip(int f) { return f >= 0 && f < EG3D_N_FAMILIAS ? &chips[f] : NULL; }

static bool comeca(const char *s, const char *p)
{
    while (*p) if (*s++ != *p++) return false;
    return true;
}

int eg3d_familia_por_nome(const char *nome)
{
    if (!nome) return -1;
    if (comeca(nome, "Hemlock")) return EG3D_CYPRESS;
    for (int f = 0; f < EG3D_N_FAMILIAS; f++)
        if (comeca(nome, chips[f].nome)) return f;
    return -1;   /* Cayman y lo que no esté en la lista */
}

/* ------------------------------------------------------------------ lotes */

/* Cabecera de «escribir n registros desde reg», con los rangos de Evergreen (evergreend.h de Linux). */
static void cabeca_regs(r6_lote_t *l, uint32_t reg, int n)
{
    uint32_t op, base;
    if (reg >= 0x8000 && reg < 0xAC00)          { op = R63D_IT_SET_CONFIG_REG;  base = 0x8000; }
    else if (reg >= 0x28000 && reg < 0x29000)   { op = R63D_IT_SET_CONTEXT_REG; base = 0x28000; }
    else if (reg >= 0x30000 && reg < 0x38000)   { op = R63D_IT_SET_RESOURCE;    base = 0x30000; }
    else if (reg >= 0x3C000 && reg < 0x3C600)   { op = R63D_IT_SET_SAMPLER;     base = 0x3C000; }
    else if (reg >= 0x3CFF0 && reg < 0x3FF0C)   { op = R63D_IT_SET_CTL_CONST;   base = 0x3CFF0; }
    else { l->cheio = true; return; }   /* un registro que no va por la cola: error de quien llama */
    r63d_p(l, R63D_PKT3(op, n + 1));
    r63d_p(l, (reg - base) >> 2);
}

void eg3d_reg(r6_lote_t *l, uint32_t reg, uint32_t valor)
{
    cabeca_regs(l, reg, 1);
    r63d_p(l, valor);
}

void eg3d_regs(r6_lote_t *l, uint32_t reg, const uint32_t *v, int n)
{
    cabeca_regs(l, reg, n);
    for (int i = 0; i < n; i++) r63d_p(l, v ? v[i] : 0);
}

void eg3d_sync(r6_lote_t *l, uint32_t acoes)
{
    r63d_p(l, R63D_PKT3(R63D_IT_SURFACE_SYNC, 4));
    r63d_p(l, acoes);
    r63d_p(l, 0xFFFFFFFFu);   /* CP_COHER_SIZE: todo (sin el fallo de los rangos sin alinear de la D402) */
    r63d_p(l, 0);             /* CP_COHER_BASE */
    r63d_p(l, 10);            /* intervalo de sondeo */
}

/* ------------------------------------------------------------------ inicio del motor */

static int bits(uint32_t v) { int n = 0; while (v) { n += (int)(v & 1u); v >>= 1; } return n; }

uint32_t eg3d_mapa_backends(uint32_t n_tub_log2, uint32_t max_rb, uint32_t total_rb, uint32_t desativados)
{
    if (!max_rb || max_rb > 8) return 0;
    uint32_t dados = 0, mascara = 1u << (max_rb - 1);
    uint32_t tmp = desativados | ((0xFFu << max_rb) & 0xFFu);
    if ((tmp & 0xFFu) != 0xFFu) desativados = tmp;   /* que quede al menos uno */
    uint32_t tub = 1u << n_tub_log2, pedidos = total_rb - (uint32_t)bits(desativados & 0xFFu);
    if (!pedidos || tub < pedidos) return 0;
    uint32_t razao = tub / pedidos, resto = tub - razao * pedidos;
    for (uint32_t i = 0; i < max_rb; i++) {
        if (!(mascara & desativados)) {
            for (uint32_t j = 0; j < razao; j++) { dados <<= 4; dados |= max_rb - i - 1; }
            if (resto) { dados <<= 4; dados |= max_rb - i - 1; resto--; }
        }
        mascara >>= 1;
    }
    return dados;
}

uint32_t eg3d_gb_addr_config(int f, const eg3d_leituras_t *l)
{
    const eg3d_chip_t *c = eg3d_chip(f);
    if (!c) return 0;
    return l->gb_addr_config ? l->gb_addr_config : c->gb_addr_config;
}

uint32_t eg3d_gb_backend_map(int f, const eg3d_leituras_t *l)
{
    const eg3d_chip_t *c = eg3d_chip(f);
    if (!c) return 0;
    if (l->gb_backend_map) return l->gb_backend_map;
    uint32_t todos = (1u << c->backends) - 1u, des = l->rb_desativados;
    if ((des & todos) == todos) des &= ~todos;   /* todos apagados: se corrige (como Linux) */
    return eg3d_mapa_backends(eg3d_gb_addr_config(f, l) & 0xFu, (uint32_t)c->backends, 8, des);
}

int eg3d_alinhamento_passo(uint32_t gb)
{
    uint32_t grupo = 256u << ((gb >> 4) & 7u);
    return grupo / 4u > 64u ? (int)(grupo / 4u) : 64;
}

uint32_t eg3d_bancos(uint32_t ramcfg)
{
    switch (ramcfg & 3u) {
    case 0:  return 1;   /* 4 bancos */
    case 1:  return 2;   /* 8 */
    default: return 3;   /* 16 */
    }
}

void eg3d_valores_sq(int f, uint32_t sq_config_lido, uint32_t v[9])
{
    const eg3d_chip_t *c = eg3d_chip(f);
    if (!c) c = &chips[EG3D_TURKS];
    uint32_t sq = sq_config_lido & ~((3u << 24) | (3u << 26) | (3u << 28) | (3u << 30));
    sq |= (1u << 0) | (1u << 1) | (0u << 24) | (1u << 26) | (2u << 28) | (3u << 30);   /* VC_ENABLE, EXPORT_SRC_C, prioridades */
    if (!c->cache_vertices) sq &= ~1u;
    uint32_t g = (uint32_t)c->gprs - 8u;
    uint32_t ps_hilos = f == EG3D_CEDAR ? 96u : 128u;
    uint32_t h = (((uint32_t)c->threads - ps_hilos) / 6u / 8u) * 8u;
    uint32_t p = (uint32_t)c->pilha / 6u;
    v[0] = sq;
    v[1] = (g * 12 / 32) | ((g * 6 / 32) << 16) | (4u << 28);           /* PS, VS, temporales de cláusula */
    v[2] = (g * 4 / 32) | ((g * 4 / 32) << 16);                           /* GS, ES */
    v[3] = (g * 3 / 32) | ((g * 3 / 32) << 16);                           /* HS, LS */
    v[4] = ps_hilos | (h << 8) | (h << 16) | (h << 24);                   /* hilos PS, VS, GS, ES */
    v[5] = h | (h << 8);                                                  /* HS, LS */
    v[6] = p | (p << 16);                                                 /* pila PS, VS */
    v[7] = p | (p << 16);                                                 /* GS, ES */
    v[8] = p | (p << 16);                                                 /* HS, LS */
}

int eg3d_inicio_motor(int f, const eg3d_leituras_t *l, r6_par_t *p, int max, uint32_t *max_contextos)
{
    const eg3d_chip_t *c = eg3d_chip(f);
    if (!c) return -1;
    int n = 0;
#define W(r, v) do { if (n >= max) return -1; p[n].reg = (r); p[n].valor = (v); n++; } while (0)
    if (max_contextos) *max_contextos = (uint32_t)c->contextos;
    /* No se hace (D427, como en las R7xx de la D284): el borrado de las superficies del HDP (0x2C14...),
     * SRBM_INT_*, el tamaño de lectura del PCIe, DMIF/HDP/DMA/UVD_*_ADDR_CONFIG (pantalla, apertura,
     * 2D por DMA y vídeo siguen como los dejó la BIOS), HDP_MISC_CNTL ni HDP_HOST_PATH_CNTL. Las bases de
     * los CB y los tamaños de las constantes son registros de contexto: van por la cola. */
    W(0x8000, 0xFF);                                            /* GRBM_CNTL: plazo de lectura */
    W(EG3D_GB_ADDR_CONFIG, eg3d_gb_addr_config(f, l));          /* el de la BIOS si lo hay (no cambia nada) */
    W(EG3D_GB_BACKEND_MAP, eg3d_gb_backend_map(f, l));
    W(0x3F90, 0); W(0x9148, 0); W(0x3F94, 0); W(0x914C, 0);     /* CGTS_*_TCC_DISABLE */
    W(0x8760, 0x16u | (0x2Bu << 8));                            /* CP_QUEUE_THRESHOLDS: ROQ_IB1_START, ROQ_IB2_START */
    W(0x8764, 0x30);                                            /* CP_MEQ_THRESHOLDS: STQ_SPLIT */
    W(0x9508, (1u << 1) | (1u << 24) | (1u << 25) | (1u << 26)); /* TA_CNTL_AUX: sin anisotropía en cubos, SYNC_* */
    W(EG3D_SX_DEBUG_1, l->sx_debug_1 | (1u << 16));             /* ENABLE_NEW_SMX_ADDRESS */
    W(EG3D_SMX_DC_CTL0, (l->smx_dc_ctl0 & ~(0x1FFu << 1)) | ((uint32_t)c->sx_sets << 1));   /* NUMBER_OF_SETS */
    if (!EG3D_E_NI(f)) W(0xA008, 0x00010000);                   /* SMX_SAR_CTL0 (solo Evergreen: Linux, family <= SUMO2) */
    W(0x900C, (uint32_t)(c->exp / 4 - 1) | ((uint32_t)(c->exp_pos / 4 - 1) << 8) | ((uint32_t)(c->exp_smx / 4 - 1) << 16));   /* SX_EXPORT_BUFFER_SIZES */
    W(0x8BCC, (uint32_t)c->prim_fifo | ((uint32_t)c->hiz_fifo << 12) | ((uint32_t)c->earlyz_fifo << 20));   /* PA_SC_FIFO_SIZE */
    W(0x8974, 1);                                               /* VGT_NUM_INSTANCES */
    W(0x9100, 0);                                               /* SPI_CONFIG_CNTL */
    W(0x913C, 4);                                               /* SPI_CONFIG_CNTL_1: VTX_DONE_DELAY */
    W(0x87FC, 0);                                               /* CP_PERFMON_CNTL */
    W(0x8CF0, (uint32_t)(16 * c->cf_insts) | (0x4u << 8) | (0xE0u << 16) | (0x8u << 24));   /* SQ_MS_FIFO_SIZES */
    uint32_t sq[9];
    eg3d_valores_sq(f, l->sq_config, sq);
    W(0x8C00, sq[0]); W(0x8C04, sq[1]); W(0x8C08, sq[2]); W(0x8C0C, sq[3]);
    W(0x8C18, sq[4]); W(0x8C1C, sq[5]); W(0x8C20, sq[6]); W(0x8C24, sq[7]); W(0x8C28, sq[8]);
    W(0x8D8C, 0);                                               /* SQ_DYN_GPR_CNTL_PS_FLUSH_REQ */
    W(EG3D_SQ_LDS_RESOURCE_MGMT, 0x10001000u);                  /* NUM_PS_LDS, NUM_LS_LDS (como Mesa y xf86-video-ati) */
    W(0x8B24, 4095u | (255u << 16));                            /* PA_SC_FORCE_EOV_MAX_CNTS */
    W(0x88C4, (c->cache_vertices ? 2u : 1u) | (3u << 6));       /* VGT_CACHE_INVALIDATION: VC y TC (o TC), ES_AND_GS_AUTO */
    W(0x88D4, 16);                                              /* VGT_GS_VERTEX_REUSE */
    W(0x8A60, 0);                                               /* PA_SU_LINE_STIPPLE_VALUE */
    W(0x8B10, 0);                                               /* PA_SC_LINE_STIPPLE_STATE */
    W(0x8A14, 1u | (3u << 1));                                  /* PA_CL_ENHANCE: CLIP_VTX_REORDER_ENA, NUM_CLIP_SEQ 3 */
#undef W
    return n;
}

/* ------------------------------------------------------------------ estado */

static const uint32_t zeros[48];

/* El contexto por defecto: lo de evergreen_default_state de Linux (evergreen_blit_shaders.h) y
 * evergreen_set_default_state de xf86-video-ati, más lo propio (tres parámetros interpolados con
 * perspectiva, vértices ya en pantalla, tamaños de las constantes a cero). Solo registros de contexto:
 * va dentro del preámbulo. */
static void contexto_padrao(r6_lote_t *l, uint64_t mc_fs)
{
    uint32_t v[40];
    eg3d_reg(l, 0x288EC, 0);                                    /* SQ_LDS_ALLOC_PS */
    eg3d_regs(l, 0x28900, zeros, 6);                            /* SQ_ESGS_RING_ITEMSIZE ... */
    eg3d_regs(l, 0x2891C, zeros, 4);                            /* SQ_GS_VERT_ITEMSIZE ... */
    eg3d_regs(l, 0x28040, zeros, 2);                            /* DB_Z_INFO, DB_STENCIL_INFO: sin Z hasta eg3d_alvo */
    eg3d_reg(l, 0x28800, 0);                                    /* DB_DEPTH_CONTROL */
    v[0] = (1u << 5) | (1u << 6);                               /* DB_RENDER_CONTROL: sin compresión de Z ni plantilla */
    v[1] = 0;                                                   /* DB_COUNT_CONTROL */
    v[2] = 0;                                                   /* DB_DEPTH_VIEW */
    v[3] = 2u | (2u << 2) | (2u << 4);                          /* DB_RENDER_OVERRIDE: sin HiZ ni HiS */
    v[4] = 0;                                                   /* DB_RENDER_OVERRIDE2 */
    v[5] = 0;                                                   /* DB_HTILE_DATA_BASE */
    eg3d_regs(l, 0x28000, v, 6);
    eg3d_regs(l, 0x28028, zeros, 2);                            /* DB_STENCIL_CLEAR, DB_DEPTH_CLEAR */
    eg3d_reg(l, 0x28B70, 0x0000AA00u);                          /* DB_ALPHA_TO_MASK */
    eg3d_reg(l, 0x28ABC, 0);                                    /* DB_HTILE_SURFACE */
    eg3d_regs(l, 0x28430, zeros, 3);                            /* DB_STENCILREFMASK(_BF), SX_ALPHA_REF */
    eg3d_reg(l, 0x28200, 0);                                    /* PA_SC_WINDOW_OFFSET */
    v[0] = 0xFFFF;                                              /* PA_SC_CLIPRECT_RULE */
    for (int i = 0; i < 4; i++) { v[1 + 2 * i] = 0; v[2 + 2 * i] = 0x20002000u; }   /* PA_SC_CLIPRECT_0..3 */
    v[9] = 0xAAAAAAAAu;                                         /* PA_SC_EDGERULE */
    v[10] = 0;                                                  /* PA_SU_HARDWARE_SCREEN_OFFSET */
    v[11] = 0xF;                                                /* CB_TARGET_MASK */
    v[12] = 0xF;                                                /* CB_SHADER_MASK */
    eg3d_regs(l, 0x2820C, v, 13);
    for (int i = 0; i < 16; i++) { v[2 * i] = 1u << 31; v[2 * i + 1] = 0x20002000u; }   /* PA_SC_VPORT_SCISSOR_0..15 */
    v[32] = R63D_F_0; v[33] = R63D_F_1;                         /* PA_SC_VPORT_ZMIN_0, ZMAX_0 */
    eg3d_regs(l, 0x28250, v, 34);
    eg3d_reg(l, 0x28350, 0);                                    /* SX_MISC */
    eg3d_regs(l, 0x28A48, zeros, 2);                            /* PA_SC_MODE_CNTL_0, _1 */
    v[0] = 0;                                                   /* PA_SC_LINE_CNTL */
    v[1] = 0;                                                   /* PA_SC_AA_CONFIG: sin MSAA */
    v[2] = 1u | (2u << 1) | (5u << 3);                          /* PA_SU_VTX_CNTL: centro en .5, redondeo al par, 1/256 (el de las R7xx, D402) */
    v[3] = v[4] = v[5] = v[6] = R63D_F_1;                       /* PA_CL_GB_*: bandas de guarda */
    for (int i = 7; i < 15; i++) v[i] = 0;                      /* PA_SC_AA_SAMPLE_LOCS_0..7 */
    v[15] = 0xFFFFFFFFu;                                        /* PA_SC_AA_MASK */
    eg3d_regs(l, 0x28C00, v, 16);
    v[0] = (1u << 4) | (0xCCu << 16);                           /* CB_COLOR_CONTROL: CB_NORMAL, copiar */
    v[1] = (1u << 4) | (1u << 9);                               /* DB_SHADER_CONTROL: Z antes y después, exportación doble */
    v[2] = 1u << 16;                                            /* PA_CL_CLIP_CNTL: sin recorte (SaguiGL ya recortó) */
    v[3] = 1u << 2;                                             /* PA_SU_SC_MODE_CNTL: sin descarte de caras */
    v[4] = (1u << 8) | (1u << 9);                               /* PA_CL_VTE_CNTL: x, y, z ya en pantalla; w llega como 1/w */
    for (int i = 5; i < 13; i++) v[i] = 0;                      /* PA_CL_VS_OUT_CNTL ... SQ_DYN_GPR_RESOURCE_LIMIT_1 */
    eg3d_regs(l, 0x28808, v, 13);
    eg3d_regs(l, 0x28B78, zeros, 6);                            /* desplazamiento de polígonos: no */
    eg3d_reg(l, 0x288A4, (uint32_t)(mc_fs >> 8));               /* SQ_PGM_START_FS: memoria nuestra (no se llama) */
    eg3d_reg(l, 0x288A8, 0);                                    /* SQ_PGM_RESOURCES_FS */
    v[0] = 0xFFFFFFu;                                           /* VGT_MAX_VTX_INDX */
    for (int i = 1; i < 9; i++) v[i] = 0;                       /* MIN, INDX_OFFSET, RESET_INDX, SX_ALPHA_TEST_CONTROL, CB_BLEND_RGBA */
    eg3d_regs(l, 0x28400, v, 9);
    eg3d_regs(l, 0x28AA0, zeros, 2);                            /* VGT_INSTANCE_STEP_RATE_0, _1 */
    eg3d_regs(l, 0x28AB4, zeros, 2);                            /* VGT_REUSE_OFF, VGT_VTX_CNT_EN */
    for (int i = 0; i < 17; i++) v[i] = 0;
    v[2] = 8;                                                   /* PA_SU_LINE_CNTL: líneas de 1 píxel */
    eg3d_regs(l, 0x28A00, v, 17);                               /* PA_SU_POINT_SIZE ... VGT_GS_MODE */
    eg3d_reg(l, 0x28A84, 0);                                    /* VGT_PRIMITIVEID_EN */
    eg3d_reg(l, 0x28A94, 0);                                    /* VGT_MULTI_PRIM_IB_RESET_EN */
    eg3d_reg(l, 0x28B54, 0);                                    /* VGT_SHADER_STAGES_EN: solo VS y PS */
    eg3d_regs(l, 0x28B94, zeros, 2);                            /* VGT_STRMOUT_CONFIG, VGT_STRMOUT_BUFFER_CONFIG */
    eg3d_reg(l, 0x28780, 0);                                    /* CB_BLEND0_CONTROL */
    /* Interpolación: tres parámetros (semánticas 1 = color, 2 = especular, 3 = textura; 0 no se usa,
     * como Mesa), con perspectiva y en el centro del píxel: el SPI deja las baricéntricas en R0.xy. */
    eg3d_reg(l, 0x286C4, (3u - 1u) << 1);                       /* SPI_VS_OUT_CONFIG: exporta 3 */
    v[0] = 1u | (2u << 8) | (3u << 16);
    for (int i = 1; i < 10; i++) v[i] = 0;
    eg3d_regs(l, 0x2861C, v, 10);                               /* SPI_VS_OUT_ID_0..9 */
    v[0] = 1u | (1u << 8); v[1] = 2u | (1u << 8); v[2] = 3u | (1u << 8);
    eg3d_regs(l, 0x28644, v, 3);                                /* SPI_PS_INPUT_CNTL_0..2: semántica, por defecto (0,0,0,1) */
    v[0] = 3u | (1u << 28);                                     /* SPI_PS_IN_CONTROL_0: 3 interpolados, PERSP_GRADIENT_ENA */
    v[1] = 0;                                                   /* SPI_PS_IN_CONTROL_1 */
    v[2] = 0;                                                   /* SPI_INTERP_CONTROL_0 */
    v[3] = 0;                                                   /* SPI_INPUT_Z */
    v[4] = 0;                                                   /* SPI_FOG_CNTL */
    v[5] = 1u;                                                  /* SPI_BARYC_CNTL: PERSP_CENTER_ENA */
    v[6] = 0;                                                   /* SPI_PS_IN_CONTROL_2 */
    eg3d_regs(l, 0x286CC, v, 7);
    v[0] = 14; v[1] = 16;
    eg3d_regs(l, 0x28C58, v, 2);                                /* VGT_VERTEX_REUSE_BLOCK_CNTL, VGT_OUT_DEALLOC_CNTL */
    eg3d_regs(l, 0x28140, zeros, 48);                           /* SQ_ALU_CONST_BUFFER_SIZE_PS/VS/GS_*: sin constantes */
    eg3d_regs(l, 0x28F80, zeros, 32);                           /* SQ_ALU_CONST_BUFFER_SIZE_HS/LS_* */
    for (int i = 1; i < 12; i++)                                /* CB_COLOR1..11_INFO: superficies apagadas */
        eg3d_reg(l, i < 8 ? 0x28C70u + (uint32_t)i * 0x3Cu : 0x28E50u + (uint32_t)(i - 8) * 0x1Cu, 0);
}

void eg3d_estado_base(r6_lote_t *l, int f, uint32_t sq_config_lido, uint64_t mc_fs)
{
    r63d_p(l, R63D_PKT3(R63D_IT_CONTEXT_CONTROL, 2));
    r63d_p(l, 0x80000000u);
    r63d_p(l, 0x80000000u);
    /* El estado «limpio» del contexto, como evergreen_cp_start: preámbulo, CLEAR_STATE. */
    r63d_p(l, R63D_PKT3(EG3D_IT_PREAMBLE_CNTL, 1));
    r63d_p(l, EG3D_PREAMBLE_INICIO);
    contexto_padrao(l, mc_fs);
    r63d_p(l, R63D_PKT3(EG3D_IT_PREAMBLE_CNTL, 1));
    r63d_p(l, EG3D_PREAMBLE_FIM);
    r63d_p(l, R63D_PKT3(EG3D_IT_CLEAR_STATE, 1));
    r63d_p(l, 0);
    eg3d_regs(l, 0x3CFF0, zeros, 2);                            /* SQ_VTX_BASE_VTX_LOC, SQ_VTX_START_INST_LOC */
    uint32_t v[9];
    v[0] = v[1] = v[2] = 0xFFFFFFFFu;
    eg3d_regs(l, 0x3FF00, v, 3);                                /* las «constantes» que evergreen_cp_start pone a 1 */
    /* Otra vez, fuera del preámbulo: lo que sigue no depende de que el microcódigo guarde el estado limpio. */
    contexto_padrao(l, mc_fs);
    /* El secuenciador (también lo pone el inicio por MMIO; aquí por si el CP lo pisa). */
    eg3d_reg(l, 0x8D8C, 0);                                     /* SQ_DYN_GPR_CNTL_PS_FLUSH_REQ */
    eg3d_valores_sq(f, sq_config_lido, v);
    eg3d_regs(l, 0x8C00, v, 4);                                 /* SQ_CONFIG, SQ_GPR_RESOURCE_MGMT_1..3 */
    eg3d_regs(l, 0x8C18, v + 4, 5);                             /* SQ_THREAD_RESOURCE_MGMT(_2), SQ_STACK_RESOURCE_MGMT_1..3 */
    eg3d_reg(l, EG3D_SQ_LDS_RESOURCE_MGMT, 0x10001000u);
}

void eg3d_alvo(r6_lote_t *l, uint64_t mc_cor, uint64_t mc_z, int passo, int alt_buf, int larg, int alt, int zbits, uint32_t bancos)
{
    eg3d_alvo_modo(l, mc_cor, mc_z, passo, alt_buf, larg, alt, zbits, bancos, 1);
}

/* D432: lo mismo con el modo de la superficie de color dado (ARRAY_MODE de CB_COLOR0_INFO: 0 lineal
 * general, 1 lineal alineada, como los destinos propios). Para dibujar en la pantalla del CRTC con el
 * modo que le puso la BIOS (GRPH_CONTROL), sin Z (zbits 0: DB_Z_INFO y DB_STENCIL_INFO sin formato). */
void eg3d_alvo_modo(r6_lote_t *l, uint64_t mc_cor, uint64_t mc_z, int passo, int alt_buf, int larg, int alt, int zbits, uint32_t bancos,
                    uint32_t modo)
{
    uint32_t v[15];
    uint32_t base = (uint32_t)(mc_cor >> 8);
    v[0] = base;                                                /* CB_COLOR0_BASE (bits 39:8) */
    v[1] = (uint32_t)(passo / 8 - 1);                           /* CB_COLOR0_PITCH: PITCH_TILE_MAX */
    v[2] = (uint32_t)(passo * alt_buf / 64 - 1);                /* CB_COLOR0_SLICE: SLICE_TILE_MAX */
    v[3] = 0;                                                   /* CB_COLOR0_VIEW */
    /* 8888 sin signo normalizado, lineal alineada, bytes en el orden ARGB (0xAARRGGBB: SWAP_ALT), mezcla
     * saturada, exportación EXPORT_4C_16BPC (EXPORT_NORM: lo que ponen xf86-video-ati y Mesa para 8 bits
     * normalizados, y lo que llevan las R7xx en el dv7). */
    v[4] = (0x1Au << 2) | ((modo & 0xFu) << 8) | (0u << 12) | (1u << 15) | (1u << 19) | (1u << 24);
    v[5] = (1u << 4) | (4u << 5) | ((bancos & 3u) << 10);       /* CB_COLOR0_ATTRIB: NON_DISP_TILING_ORDER (lineal), TILE_SPLIT 1 KB */
    v[6] = 0;                                                   /* CB_COLOR0_DIM: solo para búferes y RAT (Mesa lo deja a 0) */
    v[7] = base;                                                /* CB_COLOR0_CMASK (sin usar: apunta al color) */
    v[8] = 0;                                                   /* CB_COLOR0_CMASK_SLICE */
    v[9] = base;                                                /* CB_COLOR0_FMASK (sin usar) */
    v[10] = 0;                                                  /* CB_COLOR0_FMASK_SLICE */
    v[11] = v[12] = v[13] = v[14] = 0;                          /* CB_COLOR0_CLEAR_WORD0..3 */
    eg3d_regs(l, 0x28C60, v, 15);
    if (zbits) {
        uint32_t zb = (uint32_t)(mc_z >> 8);
        v[0] = (zbits == 16 ? 1u : 2u) | (2u << 4) | (4u << 8) | ((bancos & 3u) << 12);   /* DB_Z_INFO: Z_16/Z_24, mosaico 1D */
        v[1] = 0;                                               /* DB_STENCIL_INFO: sin plantilla */
        v[2] = zb;                                              /* DB_Z_READ_BASE */
        v[3] = zb;                                              /* DB_STENCIL_READ_BASE (sin usar) */
        v[4] = zb;                                              /* DB_Z_WRITE_BASE */
        v[5] = zb;                                              /* DB_STENCIL_WRITE_BASE (sin usar) */
        v[6] = (uint32_t)(passo / 8 - 1) | ((uint32_t)(alt_buf / 8 - 1) << 11);   /* DB_DEPTH_SIZE */
        v[7] = (uint32_t)(passo * alt_buf / 64 - 1);            /* DB_DEPTH_SLICE */
        eg3d_regs(l, 0x28040, v, 8);
        eg3d_reg(l, 0x28008, 0);                                /* DB_DEPTH_VIEW */
    } else {
        eg3d_regs(l, 0x28040, zeros, 2);
    }
    v[0] = 0; v[1] = (uint32_t)larg | ((uint32_t)alt << 16);
    eg3d_regs(l, 0x28030, v, 2);                                /* PA_SC_SCREEN_SCISSOR */
    v[0] = 1u << 31;                                            /* WINDOW_OFFSET_DISABLE */
    eg3d_regs(l, 0x28204, v, 2);                                /* PA_SC_WINDOW_SCISSOR */
    eg3d_regs(l, 0x28240, v, 2);                                /* PA_SC_GENERIC_SCISSOR */
}

void eg3d_programas(r6_lote_t *l, uint64_t mc_vs, uint64_t mc_ps)
{
    uint32_t v[3];
    eg3d_sync(l, EG3D_SYNC_SH);
    eg3d_reg(l, 0x2885C, (uint32_t)(mc_vs >> 8));               /* SQ_PGM_START_VS */
    v[0] = EG3D_VS_GPRS | (1u << 21);                           /* SQ_PGM_RESOURCES_VS: 5 registros, sin pila, DX10_CLAMP */
    v[1] = 0;                                                   /* SQ_PGM_RESOURCES_2_VS */
    eg3d_regs(l, 0x28860, v, 2);
    eg3d_reg(l, 0x28840, (uint32_t)(mc_ps >> 8));               /* SQ_PGM_START_PS */
    v[0] = EG3D_PS_GPRS | (1u << 21) | (1u << 23);              /* SQ_PGM_RESOURCES_PS: R0..R3, DX10_CLAMP, PRIME_CACHE_ON_DRAW */
    v[1] = 0;                                                   /* SQ_PGM_RESOURCES_2_PS */
    v[2] = 1u << 1;                                             /* SQ_PGM_EXPORTS_PS: un color */
    eg3d_regs(l, 0x28844, v, 3);
}

void eg3d_textura(r6_lote_t *l, uint64_t mc, int larg, int alt, int passo, bool borde_s, bool borde_t, bool linear, uint32_t bancos)
{
    uint32_t v[8];
    eg3d_sync(l, EG3D_SYNC_TC);
    v[0] = 1u | ((uint32_t)(passo / 8 - 1) << 6) | ((uint32_t)(larg - 1) << 18);   /* 2D, paso, ancho */
    v[1] = (uint32_t)(alt - 1) | (1u << 28);                    /* alto, lineal alineada */
    v[2] = (uint32_t)(mc >> 8);                                 /* base (bits 39:8) */
    v[3] = (uint32_t)(mc >> 8);                                 /* niveles de detalle: sin ellos */
    v[4] = (2u << 16) | (1u << 19) | (0u << 22) | (3u << 25);   /* 0xAARRGGBB -> R, G, B, A; sin signo, normalizado */
    v[5] = 0;                                                   /* nivel 0, capa 0 */
    v[6] = 0;
    v[7] = 0x1Au | ((bancos & 3u) << 16) | (2u << 30);          /* 8_8_8_8, textura válida */
    eg3d_regs(l, EG3D_RES_PS0, v, 8);                           /* recurso 0 del sombreador de píxeles */
    uint32_t cs = borde_s ? 2u : 0u, ct = borde_t ? 2u : 0u, fl = linear ? 1u : 0u;
    v[0] = cs | (ct << 3) | (2u << 6) | (fl << 9) | (fl << 11); /* repetir o sujetar; vecino o bilineal */
    v[1] = 0;                                                   /* MIN_LOD y MAX_LOD 0: solo el nivel 0 */
    v[2] = (1u << 29) | (1u << 31);                             /* DISABLE_CUBE_WRAP, TYPE (como Mesa) */
    eg3d_regs(l, 0x3C000, v, 3);                                /* muestreador 0 */
}

void eg3d_estado(r6_lote_t *l, bool z_teste, bool z_escrita, uint32_t z_func, bool mz, uint32_t src, uint32_t dst, bool cor_escrita,
                 int rx, int ry, int rl, int ra)
{
    uint32_t z = 0;
    if (z_teste) z = (1u << 1) | (z_escrita ? 1u << 2 : 0u) | ((z_func & 7u) << 4);
    eg3d_reg(l, 0x28800, z);                                    /* DB_DEPTH_CONTROL */
    eg3d_reg(l, 0x28238, cor_escrita ? 0xFu : 0u);              /* CB_TARGET_MASK */
    eg3d_reg(l, 0x28808, (1u << 4) | (0xCCu << 16));            /* CB_COLOR_CONTROL: la mezcla se enciende abajo */
    eg3d_reg(l, 0x28780, (src & 0x1Fu) | ((dst & 0x1Fu) << 8) | (mz ? 1u << 30 : 0u));   /* CB_BLEND0_CONTROL */
    uint32_t v[2];
    v[0] = (uint32_t)rx | ((uint32_t)ry << 16) | (1u << 31);
    v[1] = (uint32_t)(rx + rl) | ((uint32_t)(ry + ra) << 16);
    eg3d_regs(l, 0x28240, v, 2);                                /* PA_SC_GENERIC_SCISSOR */
}

void eg3d_desenhar(r6_lote_t *l, uint64_t mc_vb, int n, int prim, bool cache_vertices)
{
    uint32_t bytes = (uint32_t)n * EG3D_VERT_BYTES, v[8];
    eg3d_sync(l, cache_vertices ? EG3D_SYNC_VC : EG3D_SYNC_TC);
    v[0] = (uint32_t)mc_vb;                                     /* bits 31:0 */
    v[1] = bytes - 1;
    v[2] = (uint32_t)(mc_vb >> 32) & 0xFFu;                     /* bits 39:32; el formato va en la instrucción de lectura */
    v[2] |= (uint32_t)EG3D_VERT_BYTES << 8;                     /* paso */
    v[3] = (0u << 3) | (1u << 6) | (2u << 9) | (3u << 12);      /* X, Y, Z, W */
    v[4] = v[5] = v[6] = 0;
    v[7] = 3u << 30;                                            /* búfer válido */
    eg3d_regs(l, EG3D_RES_VS0, v, 8);                           /* recurso 0 del sombreador de vértices (el 176) */
    eg3d_reg(l, 0x8958, prim == 4 ? 4u : 2u);                   /* VGT_PRIMITIVE_TYPE: triángulos o líneas sueltos */
    r63d_p(l, R63D_PKT3(R63D_IT_INDEX_TYPE, 1));
    r63d_p(l, 0);
    r63d_p(l, R63D_PKT3(R63D_IT_NUM_INSTANCES, 1));
    r63d_p(l, 1);
    r63d_p(l, R63D_PKT3(R63D_IT_DRAW_INDEX_AUTO, 2));
    r63d_p(l, (uint32_t)n);
    r63d_p(l, 2);                                               /* índices automáticos */
}

void eg3d_valla(r6_lote_t *l, uint32_t scratch, uint32_t valor, bool cache_vertices)
{
    /* Como r600_fence_ring_emit (sin escritura de vuelta): cachés de lectura, vaciar todo, esperar al 3D,
     * y después el color y el Z hasta la VRAM. */
    eg3d_sync(l, EG3D_SYNC_TC | EG3D_SYNC_SH | (cache_vertices ? EG3D_SYNC_VC : 0u));
    r63d_p(l, R63D_PKT3(R63D_IT_EVENT_WRITE, 1));
    r63d_p(l, 0x16);                                            /* CACHE_FLUSH_AND_INV_EVENT */
    eg3d_reg(l, R6_WAIT_UNTIL, (1u << 15) | (1u << 17));        /* WAIT_3D_IDLE, WAIT_3D_IDLECLEAN */
    eg3d_sync(l, EG3D_SYNC_CB0 | EG3D_SYNC_DB);
    eg3d_reg(l, scratch, valor);
}

/* ------------------------------------------------------------------ sombreadores (ISA de Evergreen) */

#define SEL_X 0u
#define SEL_Y 1u
#define SEL_Z 2u
#define SEL_W 3u
#define SEL_0 4u
#define SEL_1 5u
#define CF_TC          0x01u
#define CF_VC          0x02u
#define CF_ALU         0x08u
#define CF_EXPORT      0x53u
#define CF_EXPORT_DONE 0x54u
#define EXP_PIXEL 0u
#define EXP_POS   1u
#define EXP_PARAM 2u
#define OP2_ADD       0x00u
#define OP2_MUL       0x01u
#define OP2_INTERP_XY 0xD6u
#define OP2_INTERP_ZW 0xD7u
#define SRC_PARAM     448u     /* ALU_SRC_PARAM_BASE: el parámetro n interpolado está en 448 + n */
#define VEC_210       5u

/* CF que lanza n lecturas (de vértices o de textura) en «ender» (palabras de 64 bits). */
static void cf(uint32_t *p, uint32_t ender, uint32_t n, uint32_t inst)
{
    p[0] = ender;
    p[1] = ((n - 1) << 10) | (inst << 22) | (1u << 31);
}

static void cf_alu(uint32_t *p, uint32_t ender, uint32_t n)
{
    p[0] = ender;
    p[1] = ((n - 1) << 18) | (CF_ALU << 26) | (1u << 31);
}

static void cf_exp(uint32_t *p, uint32_t base, uint32_t tipo, uint32_t gpr, uint32_t inst, bool fim, bool barreira)
{
    p[0] = base | (tipo << 13) | (gpr << 15);
    p[1] = SEL_X | (SEL_Y << 3) | (SEL_Z << 6) | (SEL_W << 9) | ((fim ? 1u : 0u) << 21) | (inst << 22) | ((barreira ? 1u : 0u) << 31);
}

/* Lectura del búfer de vértices 0: formato, desplazamiento y cómo llegan los 4 componentes (igual que en R7xx). */
static void vtx(uint32_t *p, uint32_t gpr, uint32_t fmt, bool norm, uint32_t desloc, uint32_t bytes, bool mega,
                uint32_t sx, uint32_t sy, uint32_t sz, uint32_t sw)
{
    p[0] = ((bytes - 1) << 26);                                   /* lee el vértice cuyo índice está en R0.x */
    p[1] = gpr | (sx << 9) | (sy << 12) | (sz << 15) | (sw << 18) | (fmt << 22) |
           ((norm ? 0u : 2u) << 28) | ((norm ? 0u : 1u) << 30);  /* normalizado sin signo, o a escala con signo */
    p[2] = desloc | ((mega ? 1u : 0u) << 19);
    p[3] = 0;
}

/* dst.comp = sat(src0.comp OP src1.comp). */
static void alu(uint32_t *p, uint32_t inst, uint32_t dst, uint32_t s0, uint32_t s1, uint32_t comp, bool ultima)
{
    p[0] = s0 | (comp << 10) | (s1 << 13) | (comp << 23) | ((ultima ? 1u : 0u) << 31);
    p[1] = (1u << 4) | (inst << 7) | (dst << 21) | (comp << 29) | (1u << 31);
}

/* Interpolación del parámetro «param» a «dst» con las baricéntricas de R0 (como evergreen_interp_alu de
 * Mesa): un grupo INTERP_ZW que escribe z, w y otro INTERP_XY que escribe x, y; los cuatro huecos de cada
 * grupo van siempre (los que no escriben, con la máscara a 0). «xyzw» false: solo el grupo XY. */
static int interp(uint32_t *p, uint32_t param, uint32_t dst, bool xyzw)
{
    int n = 0;
    for (uint32_t g = xyzw ? 0u : 1u; g < 2; g++)
        for (uint32_t c = 0; c < 4; c++) {
            bool escreve = g == 0 ? c >= 2 : c < 2;
            p[n++] = 0u | ((c & 1u ? 0u : 1u) << 10) | ((SRC_PARAM + param) << 13) | (0u << 23) | ((c == 3 ? 1u : 0u) << 31);
            p[n++] = ((escreve ? 1u : 0u) << 4) | ((g == 0 ? OP2_INTERP_ZW : OP2_INTERP_XY) << 7) | (VEC_210 << 18) | (dst << 21) | (c << 29);
        }
    return n;
}

int eg3d_vs(uint32_t *p)
{
    cf(p + 0, 6, 4, CF_VC);                                        /* 0: cuatro lecturas en la palabra 6 */
    cf_exp(p + 2, 60, EXP_POS, 1, CF_EXPORT_DONE, false, true);    /* 1: POS0 = R1 */
    cf_exp(p + 4, 0, EXP_PARAM, 2, CF_EXPORT, false, false);       /* 2: PARAM0 = R2 (color) */
    cf_exp(p + 6, 1, EXP_PARAM, 3, CF_EXPORT, false, false);       /* 3: PARAM1 = R3 (especular) */
    cf_exp(p + 8, 2, EXP_PARAM, 4, CF_EXPORT_DONE, true, false);   /* 4: PARAM2 = R4 (textura); fin */
    p[10] = p[11] = 0;                                             /* 5: relleno (las lecturas van alineadas a 16 bytes) */
    vtx(p + 12, 1, 35, false, 0, EG3D_VERT_BYTES, true, SEL_X, SEL_Y, SEL_Z, SEL_W);   /* 4 flotantes */
    vtx(p + 16, 2, 26, true, 16, 4, false, SEL_Z, SEL_Y, SEL_X, SEL_W);               /* 0xAARRGGBB -> RGBA */
    vtx(p + 20, 3, 26, true, 20, 4, false, SEL_Z, SEL_Y, SEL_X, SEL_W);               /* especular 0x00RRGGBB */
    vtx(p + 24, 4, 30, false, 24, 8, false, SEL_X, SEL_Y, SEL_0, SEL_1);              /* 2 flotantes */
    return 28;
}

int eg3d_ps_cor(uint32_t *p)
{
    cf_alu(p + 0, 2, 20);                                          /* 0: 16 interpolaciones y 4 sumas en la palabra 2 */
    cf_exp(p + 2, 0, EXP_PIXEL, 1, CF_EXPORT_DONE, true, true);    /* 1: color 0 = R1; fin */
    int n = 4;
    n += interp(p + n, 0, 1, true);                                /* R1 = color */
    n += interp(p + n, 1, 2, true);                                /* R2 = especular */
    for (uint32_t c = 0; c < 4; c++, n += 2) alu(p + n, OP2_ADD, 1, 1, 2, c, c == 3);   /* R1 = sat(R1 + R2) */
    return n;                                                      /* 44 */
}

int eg3d_ps_tex(uint32_t *p)
{
    cf_alu(p + 0, 4, 20);                                          /* 0: 20 interpolaciones en la palabra 4 */
    cf(p + 2, 24, 1, CF_TC);                                       /* 1: una muestra en la palabra 24 */
    cf_alu(p + 4, 26, 8);                                          /* 2: ocho operaciones en la palabra 26 */
    cf_exp(p + 6, 0, EXP_PIXEL, 1, CF_EXPORT_DONE, true, true);    /* 3: color 0 = R1; fin */
    int n = 8;
    n += interp(p + n, 0, 1, true);                                /* R1 = color */
    n += interp(p + n, 1, 2, true);                                /* R2 = especular */
    n += interp(p + n, 2, 3, false);                               /* R3.xy = uv */
    /* n = 48: la muestra (128 bits, alineada a 16 bytes). */
    p[n++] = 0x10u | (0u << 8) | (3u << 16);                       /* SAMPLE, recurso 0, coordenadas en R3 */
    p[n++] = 3u | (SEL_X << 9) | (SEL_Y << 12) | (SEL_Z << 15) | (SEL_W << 18) | (0xFu << 28);   /* a R3; coordenadas 0..1 */
    p[n++] = (0u << 15) | (SEL_X << 20) | (SEL_Y << 23) | (SEL_0 << 26) | (SEL_1 << 29);         /* muestreador 0; (u, v, 0, 1) */
    p[n++] = 0;
    for (uint32_t c = 0; c < 4; c++, n += 2) alu(p + n, OP2_MUL, 1, 1, 3, c, c == 3);   /* R1 = sat(R1 * R3) */
    for (uint32_t c = 0; c < 4; c++, n += 2) alu(p + n, OP2_ADD, 1, 1, 2, c, c == 3);   /* R1 = sat(R1 + R2) */
    return n;                                                      /* 68 */
}
