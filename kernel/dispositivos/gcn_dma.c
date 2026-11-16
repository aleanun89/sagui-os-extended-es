// SPDX-License-Identifier: GPL-2.0-or-later
/* gcn_dma.c - lista de modelos GCN, paquetes del DMA de las Southern Islands,
 * comprobación de la pantalla (DCE 6) y relleno con CONSTANT_FILL (D360).
 * Lógica pura: se prueba también en el PC (tests/gcn_dma_unidad.c). Ver gcn_dma.h. */
#include "gcn_dma.h"

#define S GCN_GEN_SI
#define C GCN_GEN_CIK
#define V GCN_GEN_VI
#define G GCN_GEN_GFX9

/* Los modelos exactos, de drm_pciids.h (SI) y amdgpu_drv.c (el resto) de
 * Linux 6.6, sacados mecánicamente. No están (a propósito):
 *   - Hainan (0x666x) y Topaz/Iceland (0x690x): no tienen pantalla;
 *   - las integradas de las APU (Kaveri, Kabini, Mullins, Carrizo, Stoney,
 *     Raven...): otro mapa de memoria (la VRAM es RAM del PC);
 *   - 1002:684C (Pitcairn, FirePro): cae dentro del intervalo de la fila de
 *     Turks de acel2d.c (0x6840-0x6859, D286) y partir esa fila tocaría lo de
 *     la rama de Evergreen; queda en la CPU.
 * Solo las SI (generación 10) tienen 2D por la GPU; las demás están para que
 * el log diga qué tarjeta es y por qué copia la CPU. */
static const gcn_modelo_t modelos[] = {
    /* Tahiti (HD 7870 XT/7950/7970, R9 280/280X) */
    { 0x6780, S, 6, "Tahiti" }, { 0x6784, S, 6, "Tahiti" }, { 0x6788, S, 6, "Tahiti" }, { 0x678A, S, 6, "Tahiti" },
    { 0x6790, S, 6, "Tahiti" }, { 0x6791, S, 6, "Tahiti" }, { 0x6792, S, 6, "Tahiti" }, { 0x6798, S, 6, "Tahiti" },
    { 0x6799, S, 6, "Tahiti" }, { 0x679A, S, 6, "Tahiti" }, { 0x679B, S, 6, "Tahiti" }, { 0x679E, S, 6, "Tahiti" },
    { 0x679F, S, 6, "Tahiti" },
    /* Pitcairn (HD 7850/7870, R7 265, R9 270/270X/370) */
    { 0x6800, S, 6, "Pitcairn (Mobility)" }, { 0x6801, S, 6, "Pitcairn (Mobility)" }, { 0x6802, S, 6, "Pitcairn (Mobility)" }, { 0x6806, S, 6, "Pitcairn" },
    { 0x6808, S, 6, "Pitcairn" }, { 0x6809, S, 6, "Pitcairn" }, { 0x6810, S, 6, "Pitcairn" }, { 0x6811, S, 6, "Pitcairn" },
    { 0x6816, S, 6, "Pitcairn" }, { 0x6817, S, 6, "Pitcairn" }, { 0x6818, S, 6, "Pitcairn" }, { 0x6819, S, 6, "Pitcairn" },
    /* Cape Verde (HD 7730-7770, R7 250X, HD 7700M/8800M) */
    { 0x6820, S, 6, "Cape Verde (Mobility)" }, { 0x6821, S, 6, "Cape Verde (Mobility)" }, { 0x6822, S, 6, "Cape Verde (Mobility)" }, { 0x6823, S, 6, "Cape Verde (Mobility)" },
    { 0x6824, S, 6, "Cape Verde (Mobility)" }, { 0x6825, S, 6, "Cape Verde (Mobility)" }, { 0x6826, S, 6, "Cape Verde (Mobility)" }, { 0x6827, S, 6, "Cape Verde (Mobility)" },
    { 0x6828, S, 6, "Cape Verde" }, { 0x6829, S, 6, "Cape Verde" }, { 0x682A, S, 6, "Cape Verde (Mobility)" }, { 0x682B, S, 6, "Cape Verde (Mobility)" },
    { 0x682C, S, 6, "Cape Verde" }, { 0x682D, S, 6, "Cape Verde (Mobility)" }, { 0x682F, S, 6, "Cape Verde (Mobility)" }, { 0x6830, S, 6, "Cape Verde (Mobility)" },
    { 0x6831, S, 6, "Cape Verde (Mobility)" }, { 0x6835, S, 6, "Cape Verde" }, { 0x6837, S, 6, "Cape Verde" }, { 0x6838, S, 6, "Cape Verde" },
    { 0x6839, S, 6, "Cape Verde" }, { 0x683B, S, 6, "Cape Verde" }, { 0x683D, S, 6, "Cape Verde" }, { 0x683F, S, 6, "Cape Verde" },
    /* Oland (GCN 1.0: R7 240/250, HD 8570, R5 M2xx) */
    { 0x6600, S, 2, "Oland (Mobility)" }, { 0x6601, S, 2, "Oland (Mobility)" }, { 0x6602, S, 2, "Oland (Mobility)" }, { 0x6603, S, 2, "Oland (Mobility)" },
    { 0x6604, S, 2, "Oland (Mobility)" }, { 0x6605, S, 2, "Oland (Mobility)" }, { 0x6606, S, 2, "Oland (Mobility)" }, { 0x6607, S, 2, "Oland (Mobility)" },
    { 0x6608, S, 2, "Oland" }, { 0x6610, S, 2, "Oland" }, { 0x6611, S, 2, "Oland" }, { 0x6613, S, 2, "Oland" },
    { 0x6617, S, 2, "Oland (Mobility)" }, { 0x6620, S, 2, "Oland (Mobility)" }, { 0x6621, S, 2, "Oland (Mobility)" }, { 0x6623, S, 2, "Oland (Mobility)" },
    { 0x6631, S, 2, "Oland" },
    /* Bonaire (HD 7790, R7 260/260X/360) */
    { 0x6640, C, 0, "Bonaire" }, { 0x6641, C, 0, "Bonaire" }, { 0x6646, C, 0, "Bonaire" }, { 0x6647, C, 0, "Bonaire" },
    { 0x6649, C, 0, "Bonaire" }, { 0x6650, C, 0, "Bonaire" }, { 0x6651, C, 0, "Bonaire" }, { 0x6658, C, 0, "Bonaire" },
    { 0x665C, C, 0, "Bonaire" }, { 0x665D, C, 0, "Bonaire" }, { 0x665F, C, 0, "Bonaire" },
    /* Hawaii (R9 290/290X/390/390X) */
    { 0x67A0, C, 0, "Hawaii" }, { 0x67A1, C, 0, "Hawaii" }, { 0x67A2, C, 0, "Hawaii" }, { 0x67A8, C, 0, "Hawaii" },
    { 0x67A9, C, 0, "Hawaii" }, { 0x67AA, C, 0, "Hawaii" }, { 0x67B0, C, 0, "Hawaii" }, { 0x67B1, C, 0, "Hawaii" },
    { 0x67B8, C, 0, "Hawaii" }, { 0x67B9, C, 0, "Hawaii" }, { 0x67BA, C, 0, "Hawaii" }, { 0x67BE, C, 0, "Hawaii" },
    /* Tonga (R9 285/380/380X) */
    { 0x6920, V, 0, "Tonga" }, { 0x6921, V, 0, "Tonga" }, { 0x6928, V, 0, "Tonga" }, { 0x6929, V, 0, "Tonga" },
    { 0x692B, V, 0, "Tonga" }, { 0x692F, V, 0, "Tonga" }, { 0x6930, V, 0, "Tonga" }, { 0x6938, V, 0, "Tonga" },
    { 0x6939, V, 0, "Tonga" },
    /* Fiji (R9 Fury/Nano) */
    { 0x7300, V, 0, "Fiji" }, { 0x730F, V, 0, "Fiji" },
    /* Polaris 10 (RX 470/480/570/580) */
    { 0x67C0, V, 0, "Polaris 10" }, { 0x67C1, V, 0, "Polaris 10" }, { 0x67C2, V, 0, "Polaris 10" }, { 0x67C4, V, 0, "Polaris 10" },
    { 0x67C7, V, 0, "Polaris 10" }, { 0x67C8, V, 0, "Polaris 10" }, { 0x67C9, V, 0, "Polaris 10" }, { 0x67CA, V, 0, "Polaris 10" },
    { 0x67CC, V, 0, "Polaris 10" }, { 0x67CF, V, 0, "Polaris 10" }, { 0x67D0, V, 0, "Polaris 10" }, { 0x67DF, V, 0, "Polaris 10" },
    { 0x6FDF, V, 0, "Polaris 10" },
    /* Polaris 11 (RX 460/560) */
    { 0x67E0, V, 0, "Polaris 11" }, { 0x67E1, V, 0, "Polaris 11" }, { 0x67E3, V, 0, "Polaris 11" }, { 0x67E7, V, 0, "Polaris 11" },
    { 0x67E8, V, 0, "Polaris 11" }, { 0x67E9, V, 0, "Polaris 11" }, { 0x67EB, V, 0, "Polaris 11" }, { 0x67EF, V, 0, "Polaris 11" },
    { 0x67FF, V, 0, "Polaris 11" },
    /* Polaris 12 (RX 540/550) */
    { 0x6980, V, 0, "Polaris 12" }, { 0x6981, V, 0, "Polaris 12" }, { 0x6985, V, 0, "Polaris 12" }, { 0x6986, V, 0, "Polaris 12" },
    { 0x6987, V, 0, "Polaris 12" }, { 0x6995, V, 0, "Polaris 12" }, { 0x6997, V, 0, "Polaris 12" }, { 0x699F, V, 0, "Polaris 12" },
    /* Vega M (Kaby Lake G) */
    { 0x694C, V, 0, "Vega M" }, { 0x694E, V, 0, "Vega M" }, { 0x694F, V, 0, "Vega M" },
    /* Vega 10 (RX Vega 56/64) */
    { 0x6860, G, 0, "Vega 10" }, { 0x6861, G, 0, "Vega 10" }, { 0x6862, G, 0, "Vega 10" }, { 0x6863, G, 0, "Vega 10" },
    { 0x6864, G, 0, "Vega 10" }, { 0x6867, G, 0, "Vega 10" }, { 0x6868, G, 0, "Vega 10" }, { 0x6869, G, 0, "Vega 10" },
    { 0x686A, G, 0, "Vega 10" }, { 0x686B, G, 0, "Vega 10" }, { 0x686C, G, 0, "Vega 10" }, { 0x686D, G, 0, "Vega 10" },
    { 0x686E, G, 0, "Vega 10" }, { 0x686F, G, 0, "Vega 10" }, { 0x687F, G, 0, "Vega 10" },
    /* Vega 12 (Pro Vega 20) */
    { 0x69A0, G, 0, "Vega 12" }, { 0x69A1, G, 0, "Vega 12" }, { 0x69A2, G, 0, "Vega 12" }, { 0x69A3, G, 0, "Vega 12" },
    { 0x69AF, G, 0, "Vega 12" },
    /* Vega 20 (Radeon VII) */
    { 0x66A0, G, 0, "Vega 20" }, { 0x66A1, G, 0, "Vega 20" }, { 0x66A2, G, 0, "Vega 20" }, { 0x66A3, G, 0, "Vega 20" },
    { 0x66A4, G, 0, "Vega 20" }, { 0x66A7, G, 0, "Vega 20" }, { 0x66AF, G, 0, "Vega 20" },
};
#undef S
#undef C
#undef V
#undef G
#define N_MODELOS (int)(sizeof modelos / sizeof modelos[0])

const gcn_modelo_t *gcn_modelo(uint16_t id)
{
    for (int i = 0; i < N_MODELOS; i++)
        if (modelos[i].id == id) return &modelos[i];
    return NULL;
}

int gcn_n_modelos(void) { return N_MODELOS; }
const gcn_modelo_t *gcn_modelo_n(int i) { return i >= 0 && i < N_MODELOS ? &modelos[i] : NULL; }

const char *gcn_nome_geracao(int gen)
{
    switch (gen) {
    case GCN_GEN_SI:   return "GCN 1.0 (Southern Islands)";
    case GCN_GEN_CIK:  return "GCN 1.1 (Sea Islands)";
    case GCN_GEN_VI:   return "GCN 1.2-4 (Volcanic Islands/Polaris)";
    case GCN_GEN_GFX9: return "GCN 5 (Vega)";
    default:           return "?";
    }
}

/* Cortos: van dentro del motivo de infopc (m[96] de la capa común). */
const char *gcn_por_que_nao(int gen)
{
    switch (gen) {
    case GCN_GEN_CIK:  return "GCN 1.1: el SDMA necesita microcodigo y el 2D arranca antes de los archivos (D360)";
    case GCN_GEN_VI:   return "GCN 1.2-4: el microcodigo del SDMA solo lo carga el SMU (D360)";
    case GCN_GEN_GFX9: return "Vega: el firmware lo carga el PSP, firmado (D360)";
    default:           return NULL;
    }
}

int si_pkt_copiar(uint32_t *p, uint32_t dst, uint32_t src, uint32_t bytes, uint8_t alto)
{
    if (bytes == 0 || bytes > SI_DMA_MAX_BYTES) return 0;
    p[0] = SI_DMA_CABECA(SI_DMA_COPIAR, 1, bytes);   /* b = 1: copia lineal con la cuenta en bytes */
    p[1] = dst;
    p[2] = src;
    p[3] = alto;                   /* bits 39:32 del destino */
    p[4] = alto;                   /* y los del origen */
    return SI_PKT_COPIAR_DW;
}

int si_pkt_encher(uint32_t *p, uint32_t dst, uint32_t valor, uint32_t bytes, uint8_t alto)
{
    if (bytes == 0 || bytes > SI_DMA_MAX_BYTES || (bytes & 3) || (dst & 3)) return 0;
    p[0] = SI_DMA_CABECA(SI_DMA_ENCHER, 0, bytes / 4);   /* la cuenta en palabras */
    p[1] = dst;
    p[2] = valor;
    p[3] = (uint32_t)alto << 16;   /* así lo pone amdgpu (si_dma_emit_fill_buffer) */
    return SI_PKT_ENCHER_DW;
}

int si_pkt_escrever(uint32_t *p, uint32_t dst, uint32_t valor, uint8_t alto)
{
    p[0] = SI_DMA_CABECA(SI_DMA_ESCREVER, 0, 1);
    p[1] = dst & 0xFFFFFFFCu;
    p[2] = alto;
    p[3] = valor;
    return 4;
}

int si_pkt_valla(uint32_t *p, uint32_t dst, uint32_t valor, uint8_t alto)
{
    p[0] = SI_DMA_CABECA(SI_DMA_VALLA, 0, 0);
    p[1] = dst & 0xFFFFFFFCu;
    p[2] = alto;
    p[3] = valor;
    return 4;
}

/* Los de EVERGREEN_CRTCn_REGISTER_OFFSET (si.c los usa tal cual en DCE 6). */
uint32_t gcn_crtc_desloc(int n)
{
    static const uint32_t d[GCN_MAX_CRTC] = { 0x0000, 0x0C00, 0x9800, 0xA400, 0xB000, 0xBC00 };
    return n >= 0 && n < GCN_MAX_CRTC ? d[n] : 0;
}

bool gcn_crtc_mostra(const gcn_crtc_t *c, uint64_t mc_tela, uint32_t pitch_px)
{
    if (!(c->crtc_control & 1) || !(c->grph_enable & 1)) return false;
    uint32_t gc = c->grph_control;
    if ((gc & 3) != 2) return false;               /* 32 bits */
    if ((gc >> 8) & 7) return false;               /* ARGB8888 */
    if (((gc >> 20) & 7) > 1) return false;        /* lineal (general o alineado) */
    if (c->pitch != pitch_px || c->off_x || c->off_y) return false;
    if (mc_tela & 0xFFu) return false;             /* la superficie va en múltiplos de 256 bytes */
    if (mc_tela >> 40) return false;
    uint64_t sup = ((uint64_t)(c->superficie_alta & 0xFFu) << 32) | (c->superficie & 0xFFFFFF00u);
    return sup == mc_tela;
}

uint64_t gcn_memsize_bytes(uint32_t reg)
{
    if (reg == 0 || reg > 65536u) return 0;
    return (uint64_t)reg << 20;
}

static bool encher_linear(const gcn_saida_encher_t *s, uint32_t dst, uint32_t bytes, uint32_t cor)
{
    while (bytes) {
        uint32_t k = bytes > SI_DMA_MAX_BYTES ? SI_DMA_MAX_BYTES : bytes;
        if (!s->encher(s->ctx, dst, k, cor)) return false;
        dst += k;
        bytes -= k;
    }
    return true;
}

bool gcn_preencher_ret(const r600_geo_t *g, const gcn_saida_encher_t *s, int x, int y, int l, int a, uint32_t cor)
{
    if (l <= 0 || a <= 0) return true;
    uint32_t fila = (uint32_t)l * 4u;
    uint32_t ini = g->mc_tela + (uint32_t)y * g->pitch + (uint32_t)x * 4u;
    if (x == 0 && fila == g->pitch)                 /* filas contiguas: un bloque */
        return encher_linear(s, ini, fila * (uint32_t)a, cor);
    for (int j = 0; j < a; j++)
        if (!encher_linear(s, ini + (uint32_t)j * g->pitch, fila, cor)) return false;
    return true;
}
