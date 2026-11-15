// SPDX-License-Identifier: GPL-2.0-or-later
/* evergreen_dma.c - lista de modelos, paquete de copia y comprobación de la
 * pantalla de las Radeon Evergreen y Northern Islands (D286). Lógica pura: se
 * prueba también en el PC (tests/evergreen_dma_unidad.c). Ver evergreen_dma.h. */
#include "evergreen_dma.h"

#define E EVG_GEN_EVERGREEN
#define C EVG_GEN_CAYMAN

/* Los modelos exactos: todos los de esas familias en drm_pciids.h de Linux 6.6.
 * No están (a propósito) las integradas de las APU (Palm, Sumo, Aruba: 0x98xx,
 * 0x964x, 0x99xx), que tienen otro mapa de memoria, ni nada de Southern
 * Islands en adelante (otro motor DMA). */
static const evg_modelo_t modelos[] = {
    /* Cedar (HD 5450 y Mobility HD 5400: 0x68E0 es la del Toshiba L635) */
    { 0x68E0, E, 4, "Cedar (Mobility HD 5400)" }, { 0x68E1, E, 4, "Cedar (Mobility HD 5400)" },
    { 0x68E4, E, 4, "Cedar (Mobility HD 6300)" }, { 0x68E5, E, 4, "Cedar (Mobility HD 6300)" },
    { 0x68E8, E, 4, "Cedar" }, { 0x68E9, E, 4, "Cedar" }, { 0x68F1, E, 4, "Cedar" }, { 0x68F2, E, 4, "Cedar" },
    { 0x68F8, E, 4, "Cedar" }, { 0x68F9, E, 4, "Cedar" }, { 0x68FA, E, 4, "Cedar" }, { 0x68FE, E, 4, "Cedar" },
    /* Redwood (HD 5550/5570/5670 y Mobility HD 5600/5700) */
    { 0x68C0, E, 6, "Redwood (Mobility)" }, { 0x68C1, E, 6, "Redwood (Mobility)" }, { 0x68C7, E, 6, "Redwood (Mobility)" },
    { 0x68C8, E, 6, "Redwood" }, { 0x68C9, E, 6, "Redwood" }, { 0x68D8, E, 6, "Redwood" }, { 0x68D9, E, 6, "Redwood" },
    { 0x68DA, E, 6, "Redwood" }, { 0x68DE, E, 6, "Redwood" },
    /* Juniper (HD 5750/5770 y Mobility HD 5800) */
    { 0x68A0, E, 6, "Juniper (Mobility)" }, { 0x68A1, E, 6, "Juniper (Mobility)" }, { 0x68A8, E, 6, "Juniper" },
    { 0x68A9, E, 6, "Juniper" }, { 0x68B0, E, 6, "Juniper (Mobility)" }, { 0x68B8, E, 6, "Juniper" },
    { 0x68B9, E, 6, "Juniper" }, { 0x68BA, E, 6, "Juniper" }, { 0x68BE, E, 6, "Juniper" }, { 0x68BF, E, 6, "Juniper" },
    /* Cypress (HD 5830/5850/5870) y Hemlock (HD 5970) */
    { 0x6880, E, 6, "Cypress (Mobility)" }, { 0x6888, E, 6, "Cypress" }, { 0x6889, E, 6, "Cypress" },
    { 0x688A, E, 6, "Cypress" }, { 0x688C, E, 6, "Cypress" }, { 0x688D, E, 6, "Cypress" }, { 0x6898, E, 6, "Cypress" },
    { 0x6899, E, 6, "Cypress" }, { 0x689B, E, 6, "Cypress" }, { 0x689E, E, 6, "Cypress" },
    { 0x689C, E, 6, "Hemlock" }, { 0x689D, E, 6, "Hemlock" },
    /* Barts (HD 6790/6850/6870 y HD 6900M) */
    { 0x6720, E, 6, "Barts (Mobility)" }, { 0x6721, E, 6, "Barts (Mobility)" }, { 0x6722, E, 6, "Barts" },
    { 0x6723, E, 6, "Barts" }, { 0x6724, E, 6, "Barts (Mobility)" }, { 0x6725, E, 6, "Barts (Mobility)" },
    { 0x6726, E, 6, "Barts" }, { 0x6727, E, 6, "Barts" }, { 0x6728, E, 6, "Barts" }, { 0x6729, E, 6, "Barts" },
    { 0x6738, E, 6, "Barts" }, { 0x6739, E, 6, "Barts" }, { 0x673E, E, 6, "Barts" },
    /* Turks (HD 6570/6670, HD 6600M/6700M y las HD 7500M/7600M «Thames») */
    { 0x6740, E, 6, "Turks (Mobility)" }, { 0x6741, E, 6, "Turks (Mobility)" }, { 0x6742, E, 6, "Turks (Mobility)" },
    { 0x6743, E, 6, "Turks (Mobility)" }, { 0x6744, E, 6, "Turks (Mobility)" }, { 0x6745, E, 6, "Turks (Mobility)" },
    { 0x6746, E, 6, "Turks" }, { 0x6747, E, 6, "Turks" }, { 0x6748, E, 6, "Turks" }, { 0x6749, E, 6, "Turks" },
    { 0x674A, E, 6, "Turks" }, { 0x6750, E, 6, "Turks" }, { 0x6751, E, 6, "Turks" }, { 0x6758, E, 6, "Turks" },
    { 0x6759, E, 6, "Turks" }, { 0x675B, E, 6, "Turks" }, { 0x675D, E, 6, "Turks" }, { 0x675F, E, 6, "Turks" },
    { 0x6840, E, 6, "Turks (Mobility)" }, { 0x6841, E, 6, "Turks (Mobility)" }, { 0x6842, E, 6, "Turks (Mobility)" },
    { 0x6843, E, 6, "Turks (Mobility)" }, { 0x6849, E, 6, "Turks" }, { 0x6850, E, 6, "Turks" }, { 0x6858, E, 6, "Turks" },
    { 0x6859, E, 6, "Turks" },
    /* Caicos (HD 6450 y HD 6400M) */
    { 0x6760, E, 4, "Caicos (Mobility)" }, { 0x6761, E, 4, "Caicos (Mobility)" }, { 0x6762, E, 4, "Caicos" },
    { 0x6763, E, 4, "Caicos" }, { 0x6764, E, 4, "Caicos (Mobility)" }, { 0x6765, E, 4, "Caicos (Mobility)" },
    { 0x6766, E, 4, "Caicos" }, { 0x6767, E, 4, "Caicos" }, { 0x6768, E, 4, "Caicos" }, { 0x6770, E, 4, "Caicos" },
    { 0x6771, E, 4, "Caicos" }, { 0x6772, E, 4, "Caicos" }, { 0x6778, E, 4, "Caicos" }, { 0x6779, E, 4, "Caicos" },
    { 0x677B, E, 4, "Caicos" },
    /* Cayman (HD 6930/6950/6970) y Antilles (HD 6990) */
    { 0x6700, C, 6, "Cayman" }, { 0x6701, C, 6, "Cayman" }, { 0x6702, C, 6, "Cayman" }, { 0x6703, C, 6, "Cayman" },
    { 0x6704, C, 6, "Cayman" }, { 0x6705, C, 6, "Cayman" }, { 0x6706, C, 6, "Cayman" }, { 0x6707, C, 6, "Cayman" },
    { 0x6708, C, 6, "Cayman" }, { 0x6709, C, 6, "Cayman" }, { 0x6718, C, 6, "Cayman" }, { 0x6719, C, 6, "Cayman" },
    { 0x671C, C, 6, "Cayman (Antilles)" }, { 0x671D, C, 6, "Cayman (Antilles)" }, { 0x671F, C, 6, "Cayman" },
};
#undef E
#undef C
#define N_MODELOS (int)(sizeof modelos / sizeof modelos[0])

const evg_modelo_t *evg_modelo(uint16_t id)
{
    for (int i = 0; i < N_MODELOS; i++)
        if (modelos[i].id == id) return &modelos[i];
    return NULL;
}

int evg_n_modelos(void) { return N_MODELOS; }
const evg_modelo_t *evg_modelo_n(int i) { return i >= 0 && i < N_MODELOS ? &modelos[i] : NULL; }

int evg_pkt_copiar(uint32_t *p, uint32_t dst, uint32_t src, uint32_t ndw)
{
    if (ndw == 0 || ndw > EVG_DMA_MAX_DW) return 0;
    p[0] = EVG_DMA_CABECA(EVG_DMA_COPIAR, 0, ndw);
    p[1] = dst & 0xFFFFFFFCu;
    p[2] = src & 0xFFFFFFFCu;
    p[3] = 0;                      /* bits 39:32 del destino: siempre por debajo de 4 GB */
    p[4] = 0;                      /* y los del origen */
    return EVG_PKT_COPIAR_DW;
}

/* EVERGREEN_CRTCn_REGISTER_OFFSET: los dos primeros en los 64 KB bajos, los
 * otros cuatro por encima (el MMIO de estas tarjetas es de 128 KB). */
uint32_t evg_crtc_desloc(int n)
{
    static const uint32_t d[EVG_MAX_CRTC] = { 0x0000, 0x0C00, 0x9800, 0xA400, 0xB000, 0xBC00 };
    return n >= 0 && n < EVG_MAX_CRTC ? d[n] : 0;
}

bool evg_crtc_mostra(const evg_crtc_t *c, uint32_t mc_tela, uint32_t pitch_px)
{
    if (!(c->crtc_control & 1) || !(c->grph_enable & 1)) return false;
    uint32_t gc = c->grph_control;
    if ((gc & 3) != 2) return false;               /* 32 bits */
    if ((gc >> 8) & 7) return false;               /* ARGB8888 */
    if (((gc >> 20) & 7) > 1) return false;        /* lineal (general o alineado) */
    if (c->pitch != pitch_px || c->off_x || c->off_y) return false;
    if (c->superficie_alta & 0xFF) return false;   /* por debajo de 4 GB */
    if (mc_tela & 0xFFu) return false;             /* la superficie va en múltiplos de 256 bytes */
    return (c->superficie & 0xFFFFFF00u) == mc_tela;
}

uint32_t evg_memsize_bytes(uint32_t reg)
{
    if (reg == 0 || reg >= 4096) return 0;
    return reg << 20;
}
