// SPDX-License-Identifier: GPL-2.0-or-later
/* gcn_dma_unidad.c - prueba de unidad en el PC de lo propio de las Radeon GCN
 * (D360): la lista exacta de modelos, los paquetes del DMA de las Southern
 * Islands, la comprobación del controlador de pantalla (DCE 6) y los
 * rectángulos (copias con el reparto de r600_dma_pacotes.c y el paquete de SI;
 * rellenos con CONSTANT_FILL).
 *
 *   gcc -O2 -DR600_ANFITRION -I kernel/inc -o gcn_dma_unidad \
 *       tests/gcn_dma_unidad.c kernel/dispositivos/gcn_dma.c \
 *       kernel/dispositivos/r600_dma_pacotes.c
 *
 * El motor simulado decodifica cada paquete con el formato de sid.h
 * (DMA_PACKET(cmd, b, t, s, n)) y copia en un orden cualquiera: el resultado
 * tiene que ser el de memmove. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcn_dma.h"
#include "r600_dma.h"

#define L        200
#define PASSO    208
#define A        120
#define ALTO     0xF4u              /* byte alto del MC (bits 39:32), como en una VRAM puesta arriba */
#define BASE_MC  0x10000000u
#define RASC_F   5
#define TOTAL    (PASSO * (A + 1 + RASC_F + 1))

static uint32_t vram[TOTAL], ref[PASSO * A];
static int falhas, verificacoes, modo_ordem, pacotes;
static uint32_t semente = 3601;
static uint32_t aleat(uint32_t n) { semente = semente * 1103515245u + 12345u; return (semente >> 8) % n; }

static void falha(const char *m)
{
    if (falhas < 12) printf("FALLO: %s\n", m);
    falhas++;
}
#define COMPROBAR(c, m) do { verificacoes++; if (!(c)) falha(m); } while (0)

/* El motor: recibe las palabras del paquete tal como irían al anillo. */
static bool motor(const uint32_t *p, int np)
{
    uint32_t op = p[0] >> 28, b = (p[0] >> 26) & 1, t = (p[0] >> 23) & 1, s = (p[0] >> 22) & 1, n = p[0] & 0xFFFFF;
    if (t || s || (p[0] & 0x0B300000u)) { falha("bits de la cabecera que deberían ir a 0"); return false; }
    if (op == SI_DMA_COPIAR) {
        if (np != 5 || b != 1) { falha("la copia de SI son 5 palabras con b = 1 (bytes)"); return false; }
        if (p[3] != ALTO || p[4] != ALTO) { falha("bits 39:32 de la copia"); return false; }
        if ((p[1] & 3) || (p[2] & 3) || (n & 3)) { falha("copia sin alinear a palabras"); return false; }
        if (n == 0 || n > SI_DMA_MAX_BYTES) { falha("cuenta de bytes de la copia"); return false; }
        if (p[1] < BASE_MC || p[2] < BASE_MC) { falha("fuera de la memoria"); return false; }
        uint32_t d = (p[1] - BASE_MC) / 4, o = (p[2] - BASE_MC) / 4, w = n / 4;
        if (d + w > TOTAL || o + w > TOTAL) { falha("fuera de la memoria"); return false; }
        if (d < o + w && o < d + w) falha("origen y destino de un paquete se tocan");
        switch (modo_ordem) {
        case 0: for (uint32_t i = 0; i < w; i++) vram[d + i] = vram[o + i]; break;
        case 1: for (uint32_t i = w; i-- > 0;) vram[d + i] = vram[o + i]; break;
        default: for (uint32_t i = 0; i < w; i++) { uint32_t k = (i * 7919u) % w; vram[d + k] = vram[o + k]; } break;
        }
    } else if (op == SI_DMA_ENCHER) {
        if (np != 4 || b != 0) { falha("CONSTANT_FILL son 4 palabras con b = 0"); return false; }
        if (p[3] != (ALTO << 16)) { falha("bits altos del CONSTANT_FILL (<< 16, como amdgpu)"); return false; }
        if (p[1] & 3) { falha("relleno sin alinear"); return false; }
        if (n == 0 || n * 4 > SI_DMA_MAX_BYTES) { falha("cuenta del relleno (en palabras)"); return false; }
        if (p[1] < BASE_MC) { falha("relleno fuera de la memoria"); return false; }
        uint32_t d = (p[1] - BASE_MC) / 4;
        if (d + n > TOTAL) { falha("relleno fuera de la memoria"); return false; }
        for (uint32_t i = 0; i < n; i++) vram[d + i] = p[2];
    } else {
        falha("orden inesperada");
        return false;
    }
    pacotes++;
    return true;
}

static bool sim_copia(void *ctx, uint32_t dst, uint32_t src, uint32_t n)
{
    (void)ctx;
    uint32_t p[8] = { 0 };
    return motor(p, si_pkt_copiar(p, dst, src, n * 4, ALTO));
}
static bool sim_barreira(void *ctx) { (void)ctx; return true; }
static bool sim_encher(void *ctx, uint32_t dst, uint32_t bytes, uint32_t cor)
{
    (void)ctx;
    uint32_t p[8] = { 0 };
    return motor(p, si_pkt_encher(p, dst, cor, bytes, ALTO));
}

static const r600_geo_t geo = { BASE_MC, PASSO * 4, BASE_MC + PASSO * 4 * (A + 1), RASC_F };
static const r600_saida_t saida = { NULL, sim_copia, sim_barreira };
static const gcn_saida_encher_t saida_e = { NULL, sim_encher };

static void copia(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!r600_copiar_ret(&geo, &saida, ox, oy, dx, dy, l, a)) falha("r600_copiar_ret devolvió false");
    if (dy > oy)
        for (int j = a - 1; j >= 0; j--) memmove(ref + (dy + j) * PASSO + dx, ref + (oy + j) * PASSO + ox, (size_t)l * 4);
    else
        for (int j = 0; j < a; j++) memmove(ref + (dy + j) * PASSO + dx, ref + (oy + j) * PASSO + ox, (size_t)l * 4);
    verificacoes++;
    if (memcmp(vram, ref, sizeof ref)) falha("la copia no da lo mismo que memmove");
}

static void enche(int x, int y, int l, int a, uint32_t cor)
{
    if (!gcn_preencher_ret(&geo, &saida_e, x, y, l, a, cor)) falha("gcn_preencher_ret devolvió false");
    for (int j = 0; j < a; j++)
        for (int i = 0; i < l; i++) ref[(y + j) * PASSO + x + i] = cor;
    verificacoes++;
    if (memcmp(vram, ref, sizeof ref)) falha("el relleno no da lo mismo que la CPU");
}

static int trozos;
static bool trozo_mal;
static bool conta_trozo(void *ctx, uint32_t dst, uint32_t bytes, uint32_t cor)
{
    (void)ctx; (void)dst; (void)cor;
    if (bytes > SI_DMA_MAX_BYTES || (bytes & 3)) trozo_mal = true;
    trozos++;
    return true;
}

static int contar(int gen, const char *prefixo)
{
    int n = 0;
    for (int i = 0; i < gcn_n_modelos(); i++) {
        const gcn_modelo_t *m = gcn_modelo_n(i);
        if (m->gen == gen && (!prefixo || !strncmp(m->nome, prefixo, strlen(prefixo)))) n++;
    }
    return n;
}

int main(void)
{
    uint32_t p[8];

    /* 1. Paquetes (sid.h: DMA_PACKET(cmd, b, t, s, n); si_dma.c de radeon y amdgpu). */
    COMPROBAR(si_pkt_copiar(p, 0x1000u, 0x2000u, 0x100u, 0) == 5 && p[0] == 0x34000100u && p[1] == 0x1000u &&
              p[2] == 0x2000u && p[3] == 0 && p[4] == 0, "copia: 5 palabras, b = 1, cuenta en bytes");
    COMPROBAR(si_pkt_copiar(p, 0x1000u, 0x2000u, 0x40u, 0xF4) == 5 && p[3] == 0xF4u && p[4] == 0xF4u, "copia: bits 39:32");
    COMPROBAR(si_pkt_copiar(p, 0x1000u, 0x2000u, SI_DMA_MAX_BYTES, 0) == 5 && p[0] == 0x340FFFF8u, "copia: máximo de amdgpu");
    COMPROBAR(si_pkt_copiar(p, 0x1000u, 0x2000u, SI_DMA_MAX_BYTES + 1, 0) == 0, "copia: más del máximo se rechaza");
    COMPROBAR(si_pkt_copiar(p, 0x1000u, 0x2000u, 0, 0) == 0, "copia: cuenta 0 se rechaza");
    COMPROBAR(si_pkt_encher(p, 0x1000u, 0x00ABCDEFu, 64, 0xF4) == 4 && p[0] == 0xD0000010u && p[1] == 0x1000u &&
              p[2] == 0x00ABCDEFu && p[3] == 0x00F40000u, "CONSTANT_FILL: 4 palabras, cuenta en palabras, alto << 16");
    COMPROBAR(si_pkt_encher(p, 0x1000u, 0, 6, 0) == 0 && si_pkt_encher(p, 0x1002u, 0, 8, 0) == 0 &&
              si_pkt_encher(p, 0x1000u, 0, 0, 0) == 0 && si_pkt_encher(p, 0x1000u, 0, SI_DMA_MAX_BYTES + 8, 0) == 0,
              "CONSTANT_FILL: sin alinear, vacío o demasiado grande se rechaza");
    COMPROBAR(si_pkt_escrever(p, 0x1003u, 0xCAFEDEADu, 0xF4) == 4 && p[0] == 0x20000001u && p[1] == 0x1000u &&
              p[2] == 0xF4u && p[3] == 0xCAFEDEADu, "escribir: como r600_dma_ring_test");
    COMPROBAR(si_pkt_valla(p, 0x2000u, 7, 0) == 4 && p[0] == 0x60000000u && p[1] == 0x2000u && p[2] == 0 && p[3] == 7,
              "valla: como evergreen_dma_fence_ring_emit");
    COMPROBAR(SI_DMA_NOP == 0xF0000000u && SI_DMA_NOP == R600_DMA_NOP, "NOP");
    {
        uint32_t q[8];
        r600_pkt_escrever(q, 0x1000u, 1);
        si_pkt_escrever(p, 0x1000u, 1, 0);
        COMPROBAR(!memcmp(p, q, 4 * sizeof p[0]), "con el byte alto a 0, escribir es el de r600");
    }

    /* 2. Detección por identificador PCI. */
    COMPROBAR(contar(GCN_GEN_SI, "Tahiti") == 13 && contar(GCN_GEN_SI, "Pitcairn") == 12 &&
              contar(GCN_GEN_SI, "Cape Verde") == 24 && contar(GCN_GEN_SI, "Oland") == 17 && contar(GCN_GEN_SI, NULL) == 66,
              "SI: 13 Tahiti, 12 Pitcairn (sin 684C), 24 Cape Verde, 17 Oland");
    COMPROBAR(contar(GCN_GEN_CIK, "Bonaire") == 11 && contar(GCN_GEN_CIK, "Hawaii") == 12, "CIK: 11 Bonaire, 12 Hawaii");
    COMPROBAR(contar(GCN_GEN_VI, "Tonga") == 9 && contar(GCN_GEN_VI, "Fiji") == 2 && contar(GCN_GEN_VI, "Polaris 10") == 13 &&
              contar(GCN_GEN_VI, "Polaris 11") == 9 && contar(GCN_GEN_VI, "Polaris 12") == 8 && contar(GCN_GEN_VI, "Vega M") == 3,
              "VI: Tonga, Fiji, Polaris 10/11/12 y Vega M");
    COMPROBAR(contar(GCN_GEN_GFX9, "Vega 10") == 15 && contar(GCN_GEN_GFX9, "Vega 12") == 5 && contar(GCN_GEN_GFX9, "Vega 20") == 7,
              "Vega 10/12/20");
    COMPROBAR(gcn_n_modelos() == 160, "160 modelos en total");
    {
        const gcn_modelo_t *m = gcn_modelo(0x6818);
        COMPROBAR(m && m->gen == GCN_GEN_SI && m->crtcs == 6 && !strcmp(m->nome, "Pitcairn"), "1002:6818 (HD 7870) es Pitcairn");
        m = gcn_modelo(0x6611);
        COMPROBAR(m && m->gen == GCN_GEN_SI && m->crtcs == 2, "1002:6611 (R7 240) es Oland con 2 controladores");
        m = gcn_modelo(0x67DF);
        COMPROBAR(m && m->gen == GCN_GEN_VI && m->crtcs == 0 && gcn_por_que_nao(m->gen), "1002:67df (RX 470/480/570/580): VI, sin 2D");
        COMPROBAR(gcn_por_que_nao(GCN_GEN_SI) == NULL && gcn_por_que_nao(GCN_GEN_CIK) && gcn_por_que_nao(GCN_GEN_GFX9),
                  "motivo solo fuera de SI");
        for (int g = GCN_GEN_CIK; g <= GCN_GEN_GFX9; g++)
            COMPROBAR(strlen(gcn_por_que_nao(g)) < 90, "el motivo cabe en el mensaje de la capa común (96)");
    }
    {
        bool bien = true;
        for (int i = 0; i < gcn_n_modelos(); i++) {
            const gcn_modelo_t *a = gcn_modelo_n(i);
            if (gcn_modelo(a->id) != a) bien = false;                     /* sin repetidos */
            if (a->gen == GCN_GEN_SI ? (a->crtcs != 2 && a->crtcs != 6) : a->crtcs != 0) bien = false;
            if (a->gen < GCN_GEN_SI || a->gen > GCN_GEN_GFX9) bien = false;
        }
        COMPROBAR(bien, "sin repetidos, controladores y generaciones coherentes");
    }
    {
        /* Ni Hainan ni Topaz (sin pantalla), ni las APU, ni Evergreen/NI, ni 684C. */
        static const uint16_t fora[] = { 0x6660, 0x6663, 0x666F, 0x6900, 0x6907, 0x9830, 0x9850, 0x1304, 0x9874, 0x98E4,
                                         0x15DD, 0x15D8, 0x684C, 0x68E0, 0x6738, 0x6700, 0x9553, 0x0000, 0xFFFF };
        bool bien = true;
        for (unsigned i = 0; i < sizeof fora / sizeof fora[0]; i++)
            if (gcn_modelo(fora[i])) bien = false;
        COMPROBAR(bien, "ni Hainan/Topaz, ni APU, ni HD 5000-6000, ni 684C");
    }
    COMPROBAR(gcn_modelo_n(-1) == NULL && gcn_modelo_n(gcn_n_modelos()) == NULL, "gcn_modelo_n fuera de rango");

    /* 3. Controladores de pantalla (DCE 6). */
    COMPROBAR(gcn_crtc_desloc(0) == 0 && gcn_crtc_desloc(1) == 0xC00 && gcn_crtc_desloc(2) == 0x9800 &&
              gcn_crtc_desloc(3) == 0xA400 && gcn_crtc_desloc(4) == 0xB000 && gcn_crtc_desloc(5) == 0xBC00 &&
              0x6E70 + gcn_crtc_desloc(5) < 0x20000, "desplazamientos de los 6 controladores (dentro de 128 KB)");
    {
        const gcn_crtc_t bom = { 1, 1, 0x00100002u, 0x00000000u, 0xF4, 1920, 0, 0 };
        const uint64_t mc = 0xF400000000ull;
        gcn_crtc_t c = bom;
        COMPROBAR(gcn_crtc_mostra(&c, mc, 1920), "controlador correcto con la VRAM por encima de 4 GB del MC");
        c = bom; c.superficie_alta = 0; COMPROBAR(!gcn_crtc_mostra(&c, mc, 1920), "otro byte alto");
        c = bom; c.superficie_alta = 0; COMPROBAR(gcn_crtc_mostra(&c, 0, 1920), "superficie en 0");
        c = bom; c.superficie |= 0x7F;  COMPROBAR(gcn_crtc_mostra(&c, mc, 1920), "los 8 bits bajos de la superficie no cuentan");
        c = bom; c.crtc_control = 0;    COMPROBAR(!gcn_crtc_mostra(&c, mc, 1920), "apagado");
        c = bom; c.grph_enable = 0;     COMPROBAR(!gcn_crtc_mostra(&c, mc, 1920), "GRPH apagado");
        c = bom; c.grph_control = 1;    COMPROBAR(!gcn_crtc_mostra(&c, mc, 1920), "16 bits");
        c = bom; c.grph_control = 0x102; COMPROBAR(!gcn_crtc_mostra(&c, mc, 1920), "ARGB2101010");
        c = bom; c.grph_control = 0x00200002u; COMPROBAR(!gcn_crtc_mostra(&c, mc, 1920), "mosaico 1D");
        c = bom; c.grph_control = 0x00400002u; COMPROBAR(!gcn_crtc_mostra(&c, mc, 1920), "mosaico 2D");
        c = bom; c.pitch = 1366;        COMPROBAR(!gcn_crtc_mostra(&c, mc, 1920), "otro pitch");
        c = bom; c.off_x = 8;           COMPROBAR(!gcn_crtc_mostra(&c, mc, 1920), "con desplazamiento");
        c = bom;                        COMPROBAR(!gcn_crtc_mostra(&c, mc + 0x100000, 1920), "otra superficie");
        c = bom; c.superficie = 0x80;   COMPROBAR(!gcn_crtc_mostra(&c, mc + 0x80, 1920), "pantalla sin alinear a 256 bytes");
        COMPROBAR(!gcn_crtc_mostra(&bom, 1ull << 40, 1920), "más de 40 bits");
    }
    COMPROBAR(gcn_memsize_bytes(2048) == (2048ull << 20) && gcn_memsize_bytes(6144) == (6144ull << 20) &&
              gcn_memsize_bytes(0) == 0 && gcn_memsize_bytes(0xFFFFFFFFu) == 0, "CONFIG_MEMSIZE en MB (también > 4 GB)");

    /* 4. Rectángulos con el paquete de SI: como memmove con el motor copiando en cualquier orden. */
    for (modo_ordem = 0; modo_ordem < 3; modo_ordem++) {
        for (uint32_t i = 0; i < TOTAL; i++) vram[i] = i * 2654435761u;
        memcpy(ref, vram, sizeof ref);
        copia(0, 0, 7, 3, 60, 30);
        copia(20, 15, 11, 9, 70, 25);
        copia(30, 36, 36, 36, 60, 8);
        copia(40, 20, 33, 20, 50, 10);
        copia(0, 0, 0, 1, PASSO, A - 1);
        copia(0, 1, 0, 0, PASSO, A - 1);
        copia(0, 0, 0, 60, PASSO, 60);
        for (int k = 0; k < 1500; k++) {
            int l = 1 + (int)aleat(L), a = 1 + (int)aleat(A);
            int ox = (int)aleat((uint32_t)(L - l + 1)), oy = (int)aleat((uint32_t)(A - a + 1));
            int dx = (int)aleat((uint32_t)(L - l + 1)), dy = aleat(3) ? (int)aleat((uint32_t)(A - a + 1)) : oy;
            copia(ox, oy, dx, dy, l, a);
            if (k % 3 == 0) enche((int)aleat((uint32_t)(L - l + 1)), (int)aleat((uint32_t)(A - a + 1)), l, a, aleat(0x1000000));
        }
        enche(10, 20, 100, 50, 0x00A1B2C3u);
        enche(0, 5, PASSO, 40, 0x00123456u);          /* ancho completo: un bloque */
        enche(0, 0, PASSO, A, 0x00654321u);           /* toda la memoria visible */
        enche(L - 1, A - 1, 1, 1, 0x00FFFFFFu);       /* un píxel */
    }
    {
        /* Un bloque de más de SI_DMA_MAX_BYTES se parte en trozos. */
        r600_geo_t g = { BASE_MC, 0x2000u, 0, 0 };   /* filas de 8 KB: 400 filas = 3,1 MB */
        gcn_saida_encher_t s = { NULL, conta_trozo };
        trozos = 0;
        COMPROBAR(gcn_preencher_ret(&g, &s, 0, 0, 0x800, 400, 0) && trozos == 4 && !trozo_mal, "3,1 MB de relleno en 4 paquetes");
    }
    printf("%d comprobaciones, %d paquetes\n", verificacoes, pacotes);
    if (falhas) { printf("%d fallos\n", falhas); return 1; }
    printf("todo bien\n");
    return 0;
}
