// SPDX-License-Identifier: GPL-2.0-or-later
/* r600_dma_unidad.c - prueba de unidad en el PC de los paquetes del motor DMA de
 * las Radeon R6xx/R7xx y del reparto de rectángulos (D223).
 *
 *   gcc -O2 -DR600_ANFITRION -I kernel/inc -o r600_dma_unidad \
 *       tests/r600_dma_unidad.c kernel/dispositivos/r600_dma_pacotes.c
 *
 * Simula un motor que hace los paquetes en orden pero, dentro de cada paquete,
 * copia palabra a palabra en un orden cualquiera (hacia delante, hacia atrás o
 * por la mitad): el resultado tiene que ser el de memmove en cualquier caso.
 * Además comprueba en cada paquete que origen y destino no se tocan y que
 * ningún paquete lee lo que escribió otro desde la última barrera. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "r600_dma.h"

#define L        300            /* píxeles visibles por fila */
#define PASSO    320            /* palabras por fila (pitch / 4) */
#define A        200            /* filas */
#define BASE_MC  0x00400000u    /* la pantalla en el MC */
#define RASC_F   7              /* filas de la zona de paso (pocas: así hay varias franjas) */
#define TOTAL    (PASSO * (A + 1 + RASC_F + 1))   /* pantalla, una fila libre, paso y la fila del color */

static uint32_t vram[TOTAL];
static int falhas, verificacoes, modo_ordem, r7xx;
static uint32_t semente = 12345;
static uint32_t aleat(uint32_t n) { semente = semente * 1103515245u + 12345u; return (semente >> 8) % n; }

/* Lo escrito desde la última barrera (intervalos de palabras). */
#define MAX_ESC 8192
static uint32_t esc_ini[MAX_ESC], esc_fim[MAX_ESC];
static int n_esc, pacotes, barreiras;

static void falha(const char *m)
{
    if (falhas < 10) printf("FALLO: %s\n", m);
    falhas++;
}

static uint32_t idx(uint32_t mc) { return (mc - BASE_MC) / 4; }

static bool sim_copia(void *ctx, uint32_t dst, uint32_t src, uint32_t n)
{
    (void)ctx;
    uint32_t p[R600_PKT_MAX] = { 0 };
    int np = r600_pkt_copiar(p, dst, src, n, r7xx);
    if (np != (r7xx ? 5 : 4) || p[0] != (0x30000000u | n) || p[1] != dst || p[2] != src || p[3] != 0 || p[4] != 0)
        falha("formato del paquete de copia");
    if (n == 0 || n > R600_DMA_MAX_DW) falha("tamaño del paquete");
    uint32_t d = idx(dst), o = idx(src);
    if (d + n > TOTAL || o + n > TOTAL || dst < BASE_MC || src < BASE_MC) { falha("fuera de la memoria"); return false; }
    if (d < o + n && o < d + n) falha("origen y destino de un paquete se tocan");
    for (int k = 0; k < n_esc; k++)
        if (o < esc_fim[k] && esc_ini[k] < o + n) { falha("un paquete lee lo escrito sin barrera"); break; }
    if (n_esc < MAX_ESC) { esc_ini[n_esc] = d; esc_fim[n_esc] = d + n; n_esc++; }
    /* El motor copia en un orden cualquiera dentro del paquete. */
    switch (modo_ordem) {
    case 0: for (uint32_t i = 0; i < n; i++) vram[d + i] = vram[o + i]; break;
    case 1: for (uint32_t i = n; i-- > 0;) vram[d + i] = vram[o + i]; break;
    default: for (uint32_t i = 0; i < n; i++) { uint32_t k = (i * 7919u) % n; vram[d + k] = vram[o + k]; } break;
    }
    pacotes++;
    return true;
}

static bool sim_barreira(void *ctx)
{
    (void)ctx;
    n_esc = 0;
    barreiras++;
    return true;
}

static const r600_geo_t geo = { BASE_MC, PASSO * 4, BASE_MC + PASSO * 4 * (A + 1), RASC_F };
static const r600_saida_t saida = { NULL, sim_copia, sim_barreira };

static uint32_t ref[PASSO * A];

static void padrao(void)
{
    for (uint32_t i = 0; i < TOTAL; i++) vram[i] = i * 2654435761u;
    memcpy(ref, vram, sizeof ref);
}

static void ref_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (dy > oy)
        for (int j = a - 1; j >= 0; j--) memmove(ref + (dy + j) * PASSO + dx, ref + (oy + j) * PASSO + ox, (size_t)l * 4);
    else
        for (int j = 0; j < a; j++) memmove(ref + (dy + j) * PASSO + dx, ref + (oy + j) * PASSO + ox, (size_t)l * 4);
}

static void comparar(const char *nome)
{
    verificacoes++;
    for (int y = 0; y < A; y++)
        for (int x = 0; x < PASSO; x++)
            if (vram[y * PASSO + x] != ref[y * PASSO + x]) {
                char m[160];
                snprintf(m, sizeof m, "%s: palabra distinta en %d,%d", nome, x, y);
                falha(m);
                return;
            }
}

static void copia(int ox, int oy, int dx, int dy, int l, int a, const char *nome)
{
    n_esc = 0;
    if (!r600_copiar_ret(&geo, &saida, ox, oy, dx, dy, l, a)) falha("r600_copiar_ret devolvió false");
    ref_copiar(ox, oy, dx, dy, l, a);
    sim_barreira(NULL);
    comparar(nome);
}

int main(void)
{
    uint32_t p[R600_PKT_MAX];
    /* Formato de los paquetes (r600d.h: DMA_PACKET(cmd, t, s, n)). */
    r600_pkt_valla(p, 0x12345678u, 7);
    if (p[0] != 0x60000000u || p[1] != 0x12345678u || p[2] != 0 || p[3] != 7) falha("valla");
    r600_pkt_escrever(p, 0x1000u, 0xCAFEDEADu);
    if (p[0] != 0x20000001u || p[1] != 0x1000u || p[2] != 0 || p[3] != 0xCAFEDEADu) falha("escribir");
    if (R600_DMA_NOP != 0xF0000000u) falha("nop");
    if (r600_pkt_copiar(p, 0x1003u, 0x2002u, 0x10000u, false) != 4 || p[0] != 0x30000000u || p[1] != 0x1000u ||
        p[2] != 0x2000u || p[3] != 0)
        falha("copiar R6xx: 4 palabras, alineado y n de 16 bits");
    if (r600_pkt_copiar(p, 0x1000u, 0x2000u, 3, true) != 5 || p[0] != 0x30000003u || p[3] != 0 || p[4] != 0)
        falha("copiar R7xx: 5 palabras (bits altos del destino y del origen aparte)");
    verificacoes += 5;
    /* D408: con byte alto 0 salen las mismas palabras que siempre; con byte alto, donde lo pone Linux
     * (r600_copy_dma: destino << 16 | origen en R6xx; rv770_copy_dma: palabras 3 y 4; escribir y valla: palabra 2). */
    {
        uint32_t q[R600_PKT_MAX];
        for (int r7 = 0; r7 < 2; r7++) {
            memset(p, 0, sizeof p); memset(q, 0, sizeof q);
            int a1 = r600_pkt_copiar(q, 0x1003u, 0x2002u, 99, r7), a2 = r600_pkt_copiar_alto(p, 0x1003u, 0x2002u, 99, r7, 0);
            if (a1 != a2 || memcmp(p, q, sizeof p)) falha("copiar con byte alto 0 = la de siempre");
        }
        memset(p, 0, sizeof p); memset(q, 0, sizeof q);
        r600_pkt_valla(q, 0x12345678u, 7); r600_pkt_valla_alto(p, 0x12345678u, 7, 0);
        if (memcmp(p, q, 4 * sizeof p[0])) falha("valla con byte alto 0 = la de siempre");
        r600_pkt_escrever(q, 0x1000u, 5); r600_pkt_escrever_alto(p, 0x1000u, 5, 0);
        if (memcmp(p, q, 4 * sizeof p[0])) falha("escribir con byte alto 0 = la de siempre");
        if (r600_pkt_copiar_alto(p, 0x1000u, 0x2000u, 3, false, 0x0F) != 4 || p[3] != 0x000F000Fu)
            falha("copiar R6xx con byte alto: destino << 16 | origen");
        if (r600_pkt_copiar_alto(p, 0x1000u, 0x2000u, 3, true, 0x0F) != 5 || p[0] != 0x30000003u || p[3] != 0x0Fu || p[4] != 0x0Fu)
            falha("copiar R7xx con byte alto: palabras 3 y 4");
        r600_pkt_valla_alto(p, 0x1000u, 7, 0xAB);
        if (p[0] != 0x60000000u || p[1] != 0x1000u || p[2] != 0xABu || p[3] != 7) falha("valla con byte alto");
        r600_pkt_escrever_alto(p, 0x1000u, 0xCAFEDEADu, 0xAB);
        if (p[0] != 0x20000001u || p[1] != 0x1000u || p[2] != 0xABu || p[3] != 0xCAFEDEADu) falha("escribir con byte alto");
        verificacoes += 7;
    }

    for (int caso = 0; caso < 6; caso++) {
        modo_ordem = caso % 3;
        r7xx = caso >= 3;
        padrao();
        /* Las de la autoprueba del kernel (acel2d.c) y los casos límite. */
        copia(0, 0, 7, 3, 60, 30, "abajo a la derecha");
        copia(20, 15, 11, 9, 70, 25, "arriba a la izquierda");
        copia(30, 36, 36, 36, 60, 8, "misma fila a la derecha");
        copia(40, 20, 33, 20, 50, 10, "misma fila a la izquierda");
        copia(100, 2, 100, 6, 20, A - 8, "misma columna hacia abajo");
        copia(130, 9, 130, 1, 20, A - 10, "misma columna hacia arriba");
        copia(1, 0, 0, 0, L - 1, A, "todo un píxel a la izquierda");
        copia(0, 0, 1, 0, L - 1, A, "todo un píxel a la derecha");
        copia(0, 0, 0, 1, L, A - 1, "todo una fila hacia abajo");
        copia(0, 1, 0, 0, L, A - 1, "todo una fila hacia arriba");
        copia(0, 5, 150, 5, 150, 40, "misma fila sin solape");
        copia(5, 5, 5, 5, 40, 40, "sobre sí mismo");
        copia(L - 1, A - 1, 0, 0, 1, 1, "un píxel");
        /* D246: ancho completo (l * 4 == pitch): bloques de filas contiguas. */
        {
            int pq = pacotes;
            copia(0, 0, 0, 40, PASSO, 100, "ancho completo, desplazamiento menor que la altura (abajo)");
            copia(0, 50, 0, 47, PASSO, 100, "ancho completo, sube 3 filas");
            copia(0, 10, 0, 100, PASSO, 50, "ancho completo, un solo paquete");
            copia(0, 100, 0, 10, PASSO, 50, "ancho completo, un solo paquete hacia arriba");
            copia(0, 0, 0, 1, PASSO, A - 1, "ancho completo, una fila abajo");
            copia(0, 1, 0, 0, PASSO, A - 1, "ancho completo, una fila arriba");
            if (pq + 6 > pacotes) falha("ancho completo: faltan paquetes");   /* no menos de uno por llamada */
            pq = pacotes;
            copia(0, 20, 0, 120, PASSO, 60, "ancho completo, un paquete");
            if (pacotes - pq != 1) falha("ancho completo sin solape tiene que ser un paquete");
            pq = pacotes;
            copia(0, 0, 0, 100, PASSO, 100, "ancho completo, desplazamiento igual a la altura");
            if (pacotes - pq != 1) falha("ancho completo, desplazamiento igual a la altura: un paquete");
            pq = pacotes;
            copia(0, 0, 0, 2, PASSO, 100, "ancho completo, 2 filas");
            if (pacotes - pq != 50) falha("ancho completo con solape: un paquete por bloque de |dy-oy| filas");
        }
        /* Al azar, muchas cerca del origen. */
        for (int k = 0; k < 3000; k++) {
            int l = 1 + (int)aleat(L), a = 1 + (int)aleat(A);
            int ox = (int)aleat((uint32_t)(L - l + 1)), oy = (int)aleat((uint32_t)(A - a + 1));
            int dx, dy;
            if (aleat(2)) {
                dx = ox + (int)aleat(17) - 8; dy = aleat(3) ? oy + (int)aleat(9) - 4 : oy;
                if (dx < 0) dx = 0;
                if (dy < 0) dy = 0;
                if (dx > L - l) dx = L - l;
                if (dy > A - a) dy = A - a;
            } else {
                dx = (int)aleat((uint32_t)(L - l + 1)); dy = (int)aleat((uint32_t)(A - a + 1));
            }
            copia(ox, oy, dx, dy, l, a, "al azar");
        }
        /* Relleno: desde una fila pintada en la zona de paso. */
        uint32_t fila = geo.mc_rascunho + (uint32_t)RASC_F * geo.pitch;
        for (int i = 0; i < L; i++) vram[idx(fila) + (uint32_t)i] = 0x00A1B2C3u;
        n_esc = 0;
        if (!r600_preencher_ret(&geo, &saida, fila, 10, 20, 100, 50)) falha("relleno");
        for (int j = 0; j < 50; j++)
            for (int i = 0; i < 100; i++) ref[(20 + j) * PASSO + 10 + i] = 0x00A1B2C3u;
        sim_barreira(NULL);
        comparar("relleno");
    }
    printf("%d comprobaciones, %d paquetes, %d barreras\n", verificacoes, pacotes, barreiras);
    if (falhas) { printf("%d fallos\n", falhas); return 1; }
    printf("todo bien\n");
    return 0;
}
