// SPDX-License-Identifier: GPL-2.0-or-later
/* evergreen_dma_unidad.c - prueba de unidad en el PC de lo propio de las Radeon
 * Evergreen y Northern Islands (D286): la lista exacta de modelos (detección
 * por identificador PCI), el paquete de copia del motor DMA, la comprobación
 * del controlador de pantalla y el reparto de rectángulos con ese paquete.
 *
 *   gcc -O2 -DR600_ANFITRION -I kernel/inc -o evergreen_dma_unidad \
 *       tests/evergreen_dma_unidad.c kernel/dispositivos/evergreen_dma.c \
 *       kernel/dispositivos/r600_dma_pacotes.c
 *
 * El motor simulado decodifica cada paquete con el formato de evergreend.h
 * (orden en 31:28, subcódigo en 27:20, cuenta en 19:0) y copia en un orden
 * cualquiera: el resultado tiene que ser el de memmove.
 *
 * D408: los rectángulos se prueban dos veces: con la pantalla en el MC por
 * debajo de 4 GB y como en la HD 7600M del Toshiba (VRAM del MC en
 * 0xF_0000_0000): el motor lee la dirección entera de 40 bits (bits 39:32 en
 * las palabras 3 y 4) y comprueba que cae en la memoria simulada. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evergreen_dma.h"
#include "r600_dma.h"

#define L        200
#define PASSO    208
#define A        120
#define BASE_MC  0x10000000u
#define BASE_ALTA 0xF00000000ull   /* D408: la VRAM del MC del Toshiba (MC_VM_FB_LOCATION 0x0f3f0f00) */
#define RASC_F   5
#define TOTAL    (PASSO * (A + 1 + RASC_F + 1))

static uint32_t vram[TOTAL], ref[PASSO * A];
static int falhas, verificacoes, modo_ordem, pacotes;
static uint64_t base_sim = BASE_MC;   /* dirección entera en el MC del principio de vram[] */
static uint8_t alto_sim;              /* el byte alto que pone el driver en los paquetes */
static uint32_t semente = 2861;
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
    if (np != 5) { falha("la copia de Evergreen son 5 palabras"); return false; }
    uint32_t op = p[0] >> 28, sub = (p[0] >> 20) & 0xFF, n = p[0] & 0xFFFFF;
    if (op != 3 || sub != 0) { falha("orden o subcódigo del paquete"); return false; }
    if (p[3] > 0xFF || p[4] > 0xFF || (p[1] & 3) || (p[2] & 3)) { falha("bits altos o alineación"); return false; }
    uint64_t dst = ((uint64_t)p[3] << 32) | p[1], src = ((uint64_t)p[4] << 32) | p[2];   /* 40 bits */
    if (dst < base_sim || src < base_sim) { falha("fuera de la memoria"); return false; }
    uint64_t d64 = (dst - base_sim) / 4, o64 = (src - base_sim) / 4;
    if (d64 > TOTAL || o64 > TOTAL) { falha("fuera de la memoria (40 bits)"); return false; }
    uint32_t d = (uint32_t)d64, o = (uint32_t)o64;
    if (n == 0 || d + n > TOTAL || o + n > TOTAL) { falha("fuera de la memoria"); return false; }
    if (d < o + n && o < d + n) falha("origen y destino de un paquete se tocan");
    switch (modo_ordem) {
    case 0: for (uint32_t i = 0; i < n; i++) vram[d + i] = vram[o + i]; break;
    case 1: for (uint32_t i = n; i-- > 0;) vram[d + i] = vram[o + i]; break;
    default: for (uint32_t i = 0; i < n; i++) { uint32_t k = (i * 7919u) % n; vram[d + k] = vram[o + k]; } break;
    }
    pacotes++;
    return true;
}

static bool sim_copia(void *ctx, uint32_t dst, uint32_t src, uint32_t n)
{
    (void)ctx;
    uint32_t p[8] = { 0 };
    return motor(p, alto_sim ? evg_pkt_copiar_alto(p, dst, src, n, alto_sim) : evg_pkt_copiar(p, dst, src, n));
}
static bool sim_barreira(void *ctx) { (void)ctx; return true; }

static r600_geo_t geo = { BASE_MC, PASSO * 4, BASE_MC + PASSO * 4 * (A + 1), RASC_F };
static const r600_saida_t saida = { NULL, sim_copia, sim_barreira };

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

static int contar(int gen, const char *prefixo)
{
    int n = 0;
    for (int i = 0; i < evg_n_modelos(); i++) {
        const evg_modelo_t *m = evg_modelo_n(i);
        if (m->gen == gen && (!prefixo || !strncmp(m->nome, prefixo, strlen(prefixo)))) n++;
    }
    return n;
}

int main(void)
{
    uint32_t p[8];

    /* 1. Paquetes (evergreend.h: DMA_PACKET(cmd, sub_cmd, n)). */
    COMPROBAR(evg_pkt_copiar(p, 0x1003u, 0x2002u, 3) == 5 && p[0] == 0x30000003u && p[1] == 0x1000u && p[2] == 0x2000u &&
              p[3] == 0 && p[4] == 0, "copia: 5 palabras, direcciones alineadas");
    COMPROBAR(evg_pkt_copiar(p, 0x1000u, 0x2000u, 0x10000u) == 5 && p[0] == 0x30010000u, "copia: la cuenta pasa de 16 bits");
    COMPROBAR(evg_pkt_copiar(p, 0x1000u, 0x2000u, 0xFFFFFu) == 5 && p[0] == 0x300FFFFFu, "copia: cuenta máxima de 20 bits");
    COMPROBAR(evg_pkt_copiar(p, 0x1000u, 0x2000u, 0x100000u) == 0, "copia: más de 20 bits se rechaza (pisaría el subcódigo)");
    COMPROBAR(evg_pkt_copiar(p, 0x1000u, 0x2000u, 0) == 0, "copia: cuenta 0 se rechaza");
    COMPROBAR(EVG_DMA_CABECA(0xF, 0, 0) == R600_DMA_NOP, "el NOP es el mismo que en las R6xx/R7xx");
    /* Escribir y valla son los de r600 (Linux usa r600_dma_ring_test en las dos): mismas cabeceras. */
    r600_pkt_escrever(p, 0x1000u, 0xCAFEDEADu);
    COMPROBAR(p[0] == EVG_DMA_CABECA(2, 0, 1), "escribir: la cabecera de r600 vale en Evergreen");
    r600_pkt_valla(p, 0x1000u, 9);
    COMPROBAR(p[0] == EVG_DMA_CABECA(6, 0, 0), "valla: la cabecera de r600 vale en Evergreen");
    /* Hasta 0xFFFE palabras (lo que parte r600_dma_pacotes.c) las dos cabeceras de copia coinciden. */
    {
        uint32_t q[8];
        r600_pkt_copiar(q, 0x1000u, 0x2000u, R600_DMA_MAX_DW, true);
        evg_pkt_copiar(p, 0x1000u, 0x2000u, R600_DMA_MAX_DW);
        COMPROBAR(!memcmp(p, q, 5 * sizeof p[0]), "la copia de R7xx y la de Evergreen coinciden hasta 0xFFFE palabras");
    }

    /* 2. Detección por identificador PCI. */
    const evg_modelo_t *m = evg_modelo(0x68E0);
    COMPROBAR(m && m->gen == EVG_GEN_EVERGREEN && m->crtcs == 4 && !strncmp(m->nome, "Cedar", 5),
              "1002:68e0 (Toshiba L635) es Cedar, generación 8, 4 controladores");
    COMPROBAR(contar(EVG_GEN_EVERGREEN, "Cedar") == 12 && contar(EVG_GEN_EVERGREEN, "Redwood") == 9 &&
              contar(EVG_GEN_EVERGREEN, "Juniper") == 10 && contar(EVG_GEN_EVERGREEN, "Cypress") == 10 &&
              contar(EVG_GEN_EVERGREEN, "Hemlock") == 2, "Evergreen: 12 Cedar, 9 Redwood, 10 Juniper, 10 Cypress, 2 Hemlock");
    COMPROBAR(contar(EVG_GEN_EVERGREEN, "Barts") == 13 && contar(EVG_GEN_EVERGREEN, "Turks") == 26 &&
              contar(EVG_GEN_EVERGREEN, "Caicos") == 15, "Northern Islands: 13 Barts, 26 Turks, 15 Caicos");
    COMPROBAR(contar(EVG_GEN_CAYMAN, "Cayman") == 15 && contar(EVG_GEN_CAYMAN, NULL) == 15, "15 Cayman, generación 9");
    COMPROBAR(evg_n_modelos() == 112, "112 modelos en total");
    {
        bool bien = true;
        for (int i = 0; i < evg_n_modelos(); i++) {
            const evg_modelo_t *a = evg_modelo_n(i);
            if (evg_modelo(a->id) != a) bien = false;                     /* sin repetidos */
            if (a->crtcs != 4 && a->crtcs != 6) bien = false;
            if (a->gen != EVG_GEN_EVERGREEN && a->gen != EVG_GEN_CAYMAN) bien = false;
            bool faixa = (a->id >= 0x6700 && a->id <= 0x677B) || (a->id >= 0x6840 && a->id <= 0x6859) ||
                         (a->id >= 0x6880 && a->id <= 0x68FE);
            if (!faixa) bien = false;
            if ((a->gen == EVG_GEN_CAYMAN) != (a->id >= 0x6700 && a->id <= 0x671F)) bien = false;
        }
        COMPROBAR(bien, "sin repetidos, cada modelo en el intervalo de su familia, Cayman solo en 0x6700-0x671F");
    }
    /* Las que NO son: APU (Palm, Sumo, Aruba), Southern Islands y posteriores, R7xx, y huecos de los intervalos. */
    {
        static const uint16_t fora[] = { 0x9802, 0x9806, 0x9640, 0x9647, 0x9900, 0x9990, 0x6798, 0x6818, 0x6820, 0x683D,
                                         0x6860, 0x6863, 0x6600, 0x6660, 0x9553, 0x9440, 0x68E2, 0x68FF, 0x6710, 0x67DF,
                                         0x6844, 0x685F, 0x0000, 0xFFFF };
        bool bien = true;
        for (unsigned i = 0; i < sizeof fora / sizeof fora[0]; i++)
            if (evg_modelo(fora[i])) bien = false;
        COMPROBAR(bien, "ni las APU, ni Southern Islands/Vega, ni las R7xx, ni los huecos están en la lista");
    }
    COMPROBAR(evg_modelo_n(-1) == NULL && evg_modelo_n(evg_n_modelos()) == NULL, "evg_modelo_n fuera de rango");

    /* 3. Controladores de pantalla. */
    COMPROBAR(evg_crtc_desloc(0) == 0 && evg_crtc_desloc(1) == 0xC00 && evg_crtc_desloc(2) == 0x9800 &&
              evg_crtc_desloc(3) == 0xA400 && evg_crtc_desloc(4) == 0xB000 && evg_crtc_desloc(5) == 0xBC00 &&
              0x6E70 + evg_crtc_desloc(5) < 0x20000, "desplazamientos de los 6 controladores (dentro de 128 KB)");
    {
        const evg_crtc_t bom = { 1, 1, 0x00100002u, 0xD0000000u, 0, 1376, 0, 0 };   /* 32 bits, lineal alineado */
        evg_crtc_t c = bom;
        COMPROBAR(evg_crtc_mostra(&c, 0xD0000000u, 1376), "controlador correcto");
        c = bom; c.superficie |= 1;                         /* bits bajos: no son dirección */
        COMPROBAR(evg_crtc_mostra(&c, 0xD0000000u, 1376), "los 8 bits bajos de la superficie no cuentan");
        c = bom; c.crtc_control = 0;      COMPROBAR(!evg_crtc_mostra(&c, 0xD0000000u, 1376), "apagado");
        c = bom; c.grph_enable = 0;       COMPROBAR(!evg_crtc_mostra(&c, 0xD0000000u, 1376), "GRPH apagado");
        c = bom; c.grph_control = 1;      COMPROBAR(!evg_crtc_mostra(&c, 0xD0000000u, 1376), "16 bits");
        c = bom; c.grph_control = 0x102;  COMPROBAR(!evg_crtc_mostra(&c, 0xD0000000u, 1376), "ARGB2101010");
        c = bom; c.grph_control = 0x00200002u; COMPROBAR(!evg_crtc_mostra(&c, 0xD0000000u, 1376), "mosaico 1D");
        c = bom; c.grph_control = 0x00400002u; COMPROBAR(!evg_crtc_mostra(&c, 0xD0000000u, 1376), "mosaico 2D");
        c = bom; c.pitch = 1366;          COMPROBAR(!evg_crtc_mostra(&c, 0xD0000000u, 1376), "otro pitch");
        c = bom; c.off_y = 8;             COMPROBAR(!evg_crtc_mostra(&c, 0xD0000000u, 1376), "con desplazamiento");
        c = bom; c.superficie_alta = 1;   COMPROBAR(!evg_crtc_mostra(&c, 0xD0000000u, 1376), "por encima de 4 GB");
        c = bom;                          COMPROBAR(!evg_crtc_mostra(&c, 0xD0100000u, 1376), "otra superficie");
        c = bom; c.superficie = 0xD0000080u; COMPROBAR(!evg_crtc_mostra(&c, 0xD0000080u, 1376), "pantalla sin alinear a 256 bytes");
        /* D408: la superficie por encima de 4 GB (la del Toshiba: 0xF_0000_0000). */
        const evg_crtc_t alta = { 1, 1, 0x00100002u, 0x00000000u, 0x0F, 1024, 0, 0 };
        c = alta;                         COMPROBAR(evg_crtc_mostra(&c, BASE_ALTA, 1024), "40 bits: pantalla en 0xF_0000_0000");
        c = alta;                         COMPROBAR(!evg_crtc_mostra(&c, 0x00000000u, 1024), "40 bits: el byte alto cuenta");
        c = alta; c.superficie_alta = 0x0E; COMPROBAR(!evg_crtc_mostra(&c, BASE_ALTA, 1024), "40 bits: otro byte alto");
        c = alta; c.superficie_alta = 0x10F; COMPROBAR(evg_crtc_mostra(&c, BASE_ALTA, 1024), "40 bits: solo los bits 7:0 del registro alto");
        c = alta;                         COMPROBAR(!evg_crtc_mostra(&c, 0x10000000000ull, 1024), "más de 40 bits se rechaza");
        c = bom;                          COMPROBAR(!evg_crtc_mostra(&c, 0x1D0000000ull, 1376), "bits altos que el registro no tiene");
    }
    /* D408: los paquetes con byte alto 0 son los de siempre; con byte alto, en las palabras 3 y 4. */
    {
        uint32_t q[8];
        evg_pkt_copiar(q, 0x1003u, 0x2002u, 77);
        COMPROBAR(evg_pkt_copiar_alto(p, 0x1003u, 0x2002u, 77, 0) == 5 && !memcmp(p, q, 5 * sizeof p[0]),
                  "copia con byte alto 0 = la de siempre");
        COMPROBAR(evg_pkt_copiar_alto(p, 0x1003u, 0x2002u, 77, 0x0F) == 5 && p[0] == q[0] && p[1] == 0x1000u &&
                  p[2] == 0x2000u && p[3] == 0x0Fu && p[4] == 0x0Fu, "copia con byte alto 0x0F (Toshiba)");
        COMPROBAR(evg_pkt_copiar_alto(p, 0x1000u, 0x2000u, 0x100000u, 0x0F) == 0, "copia con byte alto: la cuenta sigue limitada");
        r600_pkt_valla_alto(p, 0x1000u, 9, 0x0F);
        COMPROBAR(p[0] == EVG_DMA_CABECA(6, 0, 0) && p[1] == 0x1000u && p[2] == 0x0Fu && p[3] == 9, "valla con byte alto");
        r600_pkt_escrever_alto(p, 0x1004u, 0xCAFEDEADu, 0x0F);
        COMPROBAR(p[0] == EVG_DMA_CABECA(2, 0, 1) && p[1] == 0x1004u && p[2] == 0x0Fu && p[3] == 0xCAFEDEADu,
                  "escribir con byte alto");
    }
    COMPROBAR(evg_memsize_bytes(512) == (512u << 20) && evg_memsize_bytes(1024) == (1u << 30) && evg_memsize_bytes(0) == 0 &&
              evg_memsize_bytes(4096) == 0 && evg_memsize_bytes(0xFFFFFFFFu) == 0 && evg_memsize_bytes(4095) == 0xFFF00000u,
              "CONFIG_MEMSIZE en MB");

    /* 4. Rectángulos con el paquete de Evergreen: como memmove con el motor copiando en cualquier orden.
     * D408: primero por debajo de 4 GB y después con la VRAM del MC en 0xF_0000_0000 (byte alto 0x0F). */
    for (int vez = 0; vez < 2; vez++) {
    base_sim = vez ? BASE_ALTA : BASE_MC;
    alto_sim = (uint8_t)(base_sim >> 32);
    geo.mc_tela = (uint32_t)base_sim;
    geo.mc_rascunho = (uint32_t)base_sim + PASSO * 4 * (A + 1);
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
        }
        uint32_t fila = geo.mc_rascunho + (uint32_t)RASC_F * geo.pitch;
        for (int i = 0; i < L; i++) vram[(fila - (uint32_t)base_sim) / 4 + (uint32_t)i] = 0x00A1B2C3u;
        if (!r600_preencher_ret(&geo, &saida, fila, 10, 20, 100, 50)) falha("relleno");
        for (int j = 0; j < 50; j++)
            for (int i = 0; i < 100; i++) ref[(20 + j) * PASSO + 10 + i] = 0x00A1B2C3u;
        COMPROBAR(!memcmp(vram, ref, sizeof ref), "relleno");
    }
    }
    printf("%d comprobaciones, %d paquetes\n", verificacoes, pacotes);
    if (falhas) { printf("%d fallos\n", falhas); return 1; }
    printf("todo bien\n");
    return 0;
}
