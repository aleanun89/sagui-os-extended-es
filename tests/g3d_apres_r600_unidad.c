// SPDX-License-Identifier: GPL-2.0-or-later
/* g3d_apres_r600_unidad.c - prueba en el PC de la pantalla como destino de color en las R6xx/R7xx
 * (r600_3d_pacotes.c) y en Evergreen (evergreen_3d_pacotes.c, con direcciones de 40 bits), para
 * «presentar sin volver a la RAM» (D432): base, paso, modo de superficie, sin Z, recorte; y que los
 * destinos de siempre no cambian ni una palabra.
 *
 *   gcc -O2 -Wall -DR600_ANFITRION -Ikernel/inc tests/g3d_apres_r600_unidad.c \
 *       kernel/dispositivos/r600_3d_pacotes.c kernel/dispositivos/evergreen_3d_pacotes.c -lm */
#include <stdio.h>
#include <string.h>
#include "r600_3d.h"
#include "evergreen_3d.h"

static int n_ok, falhas;
#define VERIFICA(c, ...) do { if (c) n_ok++; else { falhas++; printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ R6xx/R7xx y Evergreen */

/* El último valor que el lote escribe en un registro (SET_CONFIG/CONTEXT/RESOURCE/SAMPLER). */
static int reg_de(const r6_lote_t *l, bool evergreen, uint32_t reg, uint32_t *valor)
{
    int i = 0, achou = 0;
    while (i < l->n) {
        uint32_t h = l->p[i];
        if (h == 0x80000000u) { i++; continue; }
        int n = (int)((h >> 16) & 0x3FFF) + 1;
        unsigned op = (h >> 8) & 0xFF;
        uint32_t base = op == 0x68 ? 0x8000 : op == 0x69 ? 0x28000 : op == 0x6D ? (evergreen ? 0x30000 : 0x38000) : op == 0x6E ? 0x3C000 : 0;
        if (base)
            for (int k = 0; k < n - 1; k++)
                if (base + (l->p[i + 1] + (uint32_t)k) * 4 == reg) { *valor = l->p[i + 2 + k]; achou = 1; }
        i += 1 + n;
    }
    return achou;
}

static void r600(void)
{
    uint32_t a[160], b[160], v = 0;
    r6_lote_t la, lb;
    /* Los destinos de siempre no cambian: r63d_alvo = r63d_alvo_modo con lineal general. */
    r63d_lote(&la, a, 160); r63d_lote(&lb, b, 160);
    r63d_alvo(&la, 0x10300000u, 0x10400000u, 640, 512, 640, 480, 24, true);
    r63d_alvo_modo(&lb, 0x10300000u, 0x10400000u, 640, 512, 640, 480, 24, true, 0);
    VERIFICA(la.n == lb.n && !memcmp(a, b, (size_t)la.n * 4) && !la.cheio, "R7xx: el destino propio no cambia (%d palabras)", la.n);
    /* La pantalla del dv7: 1600x900, paso 1600, en MC 0x10000000, modo lineal general (0) o alineado (1). */
    for (uint32_t modo = 0; modo <= 1; modo++) {
        r63d_lote(&la, a, 160);
        r63d_alvo_modo(&la, 0x10000000u, 0, 1600, 904, 1600, 900, 0, false, modo);
        VERIFICA(!la.cheio, "R7xx: el lote de la pantalla cabe");
        VERIFICA(reg_de(&la, false, 0x28040, &v) && v == 0x100000u, "R7xx: CB_COLOR0_BASE = pantalla >> 8 (%x)", v);
        VERIFICA(reg_de(&la, false, 0x28060, &v) && v == ((1600u / 8 - 1) | ((1600u * 904 / 64 - 1) << 10)), "R7xx: CB_COLOR0_SIZE (%x)", v);
        VERIFICA(reg_de(&la, false, 0x280A0, &v) && ((v >> 8) & 0xF) == modo && ((v >> 2) & 0x3F) == 0x1A, "R7xx: CB_COLOR0_INFO: 8888, modo %u (%x)", modo, v);
        VERIFICA(reg_de(&la, false, 0x28010, &v) && v == 0, "R7xx: sin Z (DB_DEPTH_INFO sin formato)");
        VERIFICA(!reg_de(&la, false, 0x2800C, &v), "R7xx: la base del Z no se toca");
        VERIFICA(reg_de(&la, false, 0x28034, &v) && v == (1600u | (900u << 16)), "R7xx: recorte a la pantalla (%x)", v);
    }
    /* La copia: sin Z ni mezcla, color entero, tijera a la pantalla. */
    r63d_lote(&la, a, 160);
    r63d_estado(&la, false, false, 7, false, 1, 0, true, 0, 0, 1600, 900, false);
    VERIFICA(reg_de(&la, false, 0x28800, &v) && v == 0, "R7xx: DB_DEPTH_CONTROL 0");
    VERIFICA(reg_de(&la, false, 0x28238, &v) && v == 0xF, "R7xx: CB_TARGET_MASK");
    VERIFICA(reg_de(&la, false, 0x28244, &v) && v == (1600u | (900u << 16)), "R7xx: tijera genérica (%x)", v);
}

static void evergreen(void)
{
    uint32_t a[160], b[160], v = 0;
    r6_lote_t la, lb;
    r63d_lote(&la, a, 160); r63d_lote(&lb, b, 160);
    eg3d_alvo(&la, 0xF00123400ull, 0xF00800000ull, 128, 128, 100, 90, 24, 2);
    eg3d_alvo_modo(&lb, 0xF00123400ull, 0xF00800000ull, 128, 128, 100, 90, 24, 2, 1);
    VERIFICA(la.n == lb.n && !memcmp(a, b, (size_t)la.n * 4) && !la.cheio, "Evergreen: el destino propio no cambia (%d palabras)", la.n);
    /* La pantalla del Toshiba (D408): 1366x768 con paso 1408, en MC f:00000000, lineal alineada. */
    for (uint32_t modo = 0; modo <= 1; modo++) {
        r63d_lote(&la, a, 160);
        eg3d_alvo_modo(&la, 0xF00000000ull, 0, 1408, 768, 1366, 768, 0, 2, modo);
        VERIFICA(!la.cheio, "Evergreen: el lote de la pantalla cabe");
        VERIFICA(reg_de(&la, true, 0x28C60, &v) && v == 0x0F000000u, "Evergreen: CB_COLOR0_BASE lleva los bits 39:8 (%x)", v);
        VERIFICA(reg_de(&la, true, 0x28C64, &v) && v == 1408u / 8 - 1, "Evergreen: CB_COLOR0_PITCH (%x)", v);
        VERIFICA(reg_de(&la, true, 0x28C68, &v) && v == 1408u * 768 / 64 - 1, "Evergreen: CB_COLOR0_SLICE (%x)", v);
        VERIFICA(reg_de(&la, true, 0x28C70, &v) && ((v >> 8) & 0xF) == modo && ((v >> 2) & 0x3F) == 0x1A, "Evergreen: CB_COLOR0_INFO: 8888, modo %u (%x)", modo, v);
        VERIFICA(reg_de(&la, true, 0x28040, &v) && v == 0 && reg_de(&la, true, 0x28044, &v) && v == 0, "Evergreen: sin Z ni plantilla");
        VERIFICA(!reg_de(&la, true, 0x28048, &v), "Evergreen: las bases del Z no se tocan");
        VERIFICA(reg_de(&la, true, 0x28034, &v) && v == (1366u | (768u << 16)), "Evergreen: recorte a la pantalla (%x)", v);
    }
}

int main(void)
{
    r600();
    evergreen();
    printf("g3d_apres_r600_unidad: %d comprobaciones, %d fallos\n", n_ok + falhas, falhas);
    return falhas ? 1 : 0;
}
