// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_intel_anfitriao.c - pruebas en el anfitrión de la lógica del blitter de
 * Intel (D195, D359): modelos exactos de Gen7/7.5/8, familias reconocidas sin
 * 2D (Atom, Gen9, Gen11-12), formato de las órdenes por generación (las de
 * Gen3-6 tienen que salir igual que antes de la D359), el plano de la pantalla,
 * las entradas del GTT, la zona reservada y el sitio del anillo. Las compila y
 * ejecuta tests/h361_acel2d_intel_moderna.py con el MISMO
 * kernel/dispositivos/acel_intel_logica.h que usa el kernel.
 * Imprime "ok ...", "FALHA ..." e "id XXXX G" (cada modelo con 2D y su
 * generación, para que la prueba lo cruce con la tabla de acel2d.c) y, al
 * final, "TUDO OK". */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../kernel/dispositivos/acel_intel_logica.h"

static int falhas, oks;

static void confere(bool c, const char *o_que)
{
    if (c) { oks++; printf("ok %s\n", o_que); }
    else { falhas++; printf("FALHA %s\n", o_que); }
}

/* Listas de i915_pciids.h (Linux), escritas a mano para cruzarlas con las reglas del driver. */
static const uint16_t hsw[] = {
    0x0402, 0x040A, 0x040B, 0x040E, 0x0C02, 0x0C0A, 0x0C0B, 0x0C0E, 0x0A02, 0x0A0A, 0x0A0B, 0x0D02, 0x0D0A,
    0x0D0B, 0x0D0E, 0x0406, 0x0C06, 0x0A06, 0x0A0E, 0x0D06,
    0x0412, 0x041A, 0x041B, 0x041E, 0x0C12, 0x0C1A, 0x0C1B, 0x0C1E, 0x0A12, 0x0A1A, 0x0A1B, 0x0D12, 0x0D1A,
    0x0D1B, 0x0D1E, 0x0416, 0x0426, 0x0C16, 0x0A16, 0x0A1E, 0x0D16,
    0x0422, 0x042A, 0x042B, 0x042E, 0x0C22, 0x0C2A, 0x0C2B, 0x0C2E, 0x0A22, 0x0A2A, 0x0A2B, 0x0D22, 0x0D2A,
    0x0D2B, 0x0D2E, 0x0C26, 0x0A26, 0x0A2E, 0x0D26,
};
static const uint16_t bdw[] = {
    0x1602, 0x1606, 0x160B, 0x160E, 0x160A, 0x160D, 0x1612, 0x1616, 0x161B, 0x161E, 0x161A, 0x161D,
    0x1622, 0x1626, 0x162B, 0x162E, 0x162A, 0x162D, 0x1632, 0x1636, 0x163B, 0x163E, 0x163A, 0x163D,
};
static const uint16_t ivb[] = { 0x0152, 0x0156, 0x015A, 0x0162, 0x0166, 0x016A };

static void modelos(void)
{
    const char *n = NULL;
    confere(intel_variante(0x0166, &n) == INTEL_VAR_IVB && n && strstr(n, "HD 4000"),
            "0x0166 (HD 4000 del L850): Ivy Bridge");
    confere(intel_var_geracao(INTEL_VAR_IVB) == 7 && intel_var_geracao(INTEL_VAR_HSW) == 7 &&
            intel_var_geracao(INTEL_VAR_BDW) == 8 && intel_var_geracao(INTEL_VAR_NADA) == 0,
            "generaciones: IVB 7, HSW 7, BDW 8");
    bool ok = true;
    for (size_t i = 0; i < sizeof ivb / sizeof ivb[0]; i++) ok &= intel_variante(ivb[i], NULL) == INTEL_VAR_IVB;
    confere(ok, "los 6 Ivy Bridge de i915_pciids.h");
    ok = true;
    for (size_t i = 0; i < sizeof hsw / sizeof hsw[0]; i++) ok &= intel_variante(hsw[i], NULL) == INTEL_VAR_HSW;
    confere(ok, "los 60 Haswell de i915_pciids.h");
    ok = true;
    for (size_t i = 0; i < sizeof bdw / sizeof bdw[0]; i++) ok &= intel_variante(bdw[i], NULL) == INTEL_VAR_BDW;
    confere(ok, "los 24 Broadwell de i915_pciids.h");
    /* Y ningún otro: el total de modelos aceptados es el de las tres listas. */
    int total = 0, sem = 0, ambos = 0;
    for (uint32_t m = 0; m <= 0xFFFF; m++) {
        int v = intel_variante((uint16_t)m, NULL);
        const char *f = NULL, *p = NULL;
        int g = intel_familia_sem_2d((uint16_t)m, &f, &p);
        if (v) { total++; printf("id %04X %d\n", m, intel_var_geracao(v)); }
        if (g) sem++;
        if (v && g) ambos++;
    }
    confere(total == 6 + 60 + 24, "solo los 90 modelos exactos tienen 2D en Gen7/7.5/8");
    confere(ambos == 0, "ningún modelo con 2D está también entre las familias sin 2D");
    confere(sem > 0, "hay familias reconocidas sin 2D");
    confere(!intel_variante(0x0F31, NULL) && !intel_variante(0x22B0, NULL) && !intel_variante(0x0155, NULL),
            "las Atom (Valleyview 0x0F31/0x0155, Cherryview 0x22B0) no entran");
    confere(!intel_variante(0x0403, NULL) && !intel_variante(0x0432, NULL) && !intel_variante(0x1642, NULL) &&
            !intel_variante(0x0126, NULL) && !intel_variante(0x46A8, NULL), "huecos de los intervalos y otras: no");
    const char *f = NULL, *p = NULL;
    confere(intel_familia_sem_2d(0x46A8, &f, &p) == 12 && f && strstr(f, "Alder Lake-P") && p && strstr(p, "execlists"),
            "0x46A8 (i3-1220P): Gen12 Alder Lake-P, sin 2D por execlists");
    confere(intel_familia_sem_2d(0x46B3, NULL, NULL) == 12 && intel_familia_sem_2d(0x9A49, NULL, NULL) == 12,
            "Alder Lake-P GT1 y Tiger Lake: Gen12");
    confere(intel_familia_sem_2d(0x1916, NULL, NULL) == 9 && intel_familia_sem_2d(0x5916, NULL, NULL) == 9 &&
            intel_familia_sem_2d(0x3E92, NULL, NULL) == 9, "Skylake, Kaby Lake y Coffee Lake: Gen9");
    confere(intel_familia_sem_2d(0x8A52, NULL, NULL) == 11, "Ice Lake: Gen11");
    confere(intel_familia_sem_2d(0x0F31, &f, &p) == 7 && strstr(p, "Atom"), "Bay Trail: Atom, sin 2D");
    confere(intel_familia_sem_2d(0x0166, NULL, NULL) == 0 && intel_familia_sem_2d(0x1111, NULL, NULL) == 0,
            "la HD 4000 y otras no son «sin 2D»");
}

static int len_campo(uint32_t cab) { return (int)(cab & 0xFF) + 2; }

static void ordens(void)
{
    uint32_t b[INTEL_PAL_MAX + 4];
    /* Gen6 (y Gen3-5): las palabras de antes de la D359. */
    int n = intel_cmd_relleno(b, 6, 4096, 0x10000, 3, 5, 10, 20, 0x00ABCDEF);
    uint32_t r6[] = { (2u << 29) | (0x50u << 22) | (3u << 20) | 4, (3u << 24) | (0xF0u << 16) | 4096,
                      (5u << 16) | 3, (25u << 16) | 13, 0x10000, 0x00ABCDEF };
    confere(n == 6 && !memcmp(b, r6, sizeof r6), "Gen6: XY_COLOR_BLT igual que antes (6 palabras)");
    n = intel_cmd_copia(b, 6, 4096, 0x10000, 1, 2, 30, 40, 7, 8);
    uint32_t c6[] = { (2u << 29) | (0x53u << 22) | (3u << 20) | 6, (3u << 24) | (0xCCu << 16) | 4096,
                      (40u << 16) | 30, (48u << 16) | 37, 0x10000, (2u << 16) | 1, 4096, 0x10000 };
    confere(n == 8 && !memcmp(b, c6, sizeof c6), "Gen6: XY_SRC_COPY_BLT igual que antes (8 palabras)");
    n = intel_cmd_flush(b, 6);
    confere(n == 4 && b[0] == ((0x26u << 23) | 1) && !b[1] && !b[2] && b[3] == 0, "Gen6: MI_FLUSH_DW + NOOP");
    n = intel_cmd_flush(b, 5);
    confere(n == 2 && b[0] == 0x02000000u && b[1] == 0, "Gen3-5: MI_FLUSH + NOOP");
    n = intel_cmd_flush(b, 7);
    confere(n == 4 && b[0] == ((0x26u << 23) | 1), "Gen7: MI_FLUSH_DW de 3 palabras + NOOP");
    n = intel_cmd_flush(b, 8);
    confere(n == 4 && b[0] == ((0x26u << 23) | 2) && !b[1] && !b[2] && !b[3], "Gen8: MI_FLUSH_DW de 4 palabras");
    for (int g = 3; g <= 8; g++) {
        int nr = intel_cmd_relleno(b, g, 4096, 0, 0, 0, 1, 1, 0);
        int lr = len_campo(b[0]);
        int nc = intel_cmd_copia(b, g, 4096, 0, 0, 0, 1, 1, 1, 1);
        int lc = len_campo(b[0]);
        char t[96];
        snprintf(t, sizeof t, "Gen%d: la longitud de la cabecera cuadra (relleno %d, copia %d)", g, nr, nc);
        confere(nr == lr && nc == lc && nc <= INTEL_PAL_MAX && (nc + 4) % 2 == 0, t);
    }
    n = intel_cmd_copia(b, 8, 5504, 0x00200000, 1, 2, 30, 40, 7, 8);
    confere(n == 10 && b[4] == 0x00200000 && b[5] == 0 && b[6] == ((2u << 16) | 1) && b[7] == 5504 &&
            b[8] == 0x00200000 && b[9] == 0, "Gen8: copia con direcciones de 64 bits (destino 4-5, origen 8-9)");
    n = intel_cmd_relleno(b, 8, 5504, 0x00200000, 0, 0, 1, 1, 0x112233);
    confere(n == 7 && b[4] == 0x00200000 && b[5] == 0 && b[6] == 0x112233, "Gen8: relleno de 7 palabras, color al final");
    confere(intel_cmd_relleno(b, 7, 4096, 0, 0, 0, 1, 1, 0) == 6 && intel_cmd_copia(b, 7, 4096, 0, 0, 0, 1, 1, 1, 1) == 8,
            "Gen7: mismas longitudes que Gen6");
}

static void plano(void)
{
    uint32_t fb = 0x00000000, pt = 1366 * 4;   /* 5464: no es múltiplo de 64 */
    intel_plano_t p = { (1u << 31) | (6u << 26), 0, 5504, fb, 0 };
    confere(intel_plano_conferir(&p, INTEL_VAR_IVB, fb, 5504) == INTEL_PLANO_OK, "IVB: plano lineal XRGB8888 con el mismo pitch");
    confere(intel_plano_conferir(&p, INTEL_VAR_IVB, fb, pt) == INTEL_PLANO_PITCH_MAU, "otro pitch: no");
    p.cntr |= INTEL_PLANO_TILED;
    confere(intel_plano_conferir(&p, INTEL_VAR_IVB, fb, 5504) == INTEL_PLANO_EM_TILES, "en tiles: no");
    p.cntr = (1u << 31) | (5u << 26);
    confere(intel_plano_conferir(&p, INTEL_VAR_IVB, fb, 5504) == INTEL_PLANO_FORMATO_MAU, "RGB565: no");
    p.cntr = (1u << 31) | (7u << 26);
    confere(intel_plano_conferir(&p, INTEL_VAR_IVB, fb, 5504) == INTEL_PLANO_OK, "BGRA8888 (alfa ignorado): sí");
    p.cntr = (6u << 26);
    confere(intel_plano_conferir(&p, INTEL_VAR_IVB, fb, 5504) == INTEL_PLANO_OUTRO, "plano apagado: no muestra la pantalla");
    intel_plano_t q = { (1u << 31) | (6u << 26), 0x00100000, 4096, 0, 0 };
    confere(intel_plano_conferir(&q, INTEL_VAR_IVB, 0x00100000, 4096) == INTEL_PLANO_OK,
            "IVB: SURF 0 + LINOFF = la pantalla");
    confere(intel_plano_conferir(&q, INTEL_VAR_HSW, 0x00100000, 4096) == INTEL_PLANO_OUTRO,
            "HSW: LINOFF no cuenta (DSPOFFSET)");
    intel_plano_t h = { (1u << 31) | (6u << 26), 0, 4096, 0x00100000 | 4u, (2u << 16) | 8u };
    confere(intel_plano_inicio(&h, INTEL_VAR_HSW) == 0x00100000 + 2 * 4096 + 32,
            "HSW/BDW: inicio = SURF + y*stride + x*4 (bits bajos de SURF fuera)");
    confere(intel_plano_conferir(&h, INTEL_VAR_BDW, 0x00100000 + 2 * 4096 + 32, 4096) == INTEL_PLANO_OK, "BDW con desplazamiento");
    confere(INTEL_DSPCNTR(0) == 0x70180 && INTEL_DSPSURF(1) == 0x7119C && INTEL_DSPSTRIDE(2) == 0x72188 &&
            INTEL_DSPTILEOFF(0) == 0x701A4 && INTEL_DSPLINOFF(0) == 0x70184, "registros del plano de los pipes A, B y C");
}

static void gtt(void)
{
    uint64_t f = 0;
    confere(intel_pte(INTEL_VAR_IVB, 0x12345001u, 0, &f) && f == 0x12345000ull, "IVB: entrada válida de 32 bits");
    confere(intel_pte(INTEL_VAR_IVB, 0x12345031u, 0, &f) && f == 0x312345000ull, "IVB: bits 39:32 en 11:4");
    confere(intel_pte(INTEL_VAR_HSW, 0x12345839u, 0, &f) && f == 0x312345000ull, "HSW: el bit 11 es de caché, no de dirección");
    confere(!intel_pte(INTEL_VAR_IVB, 0x12345000u, 0, &f), "sin el bit 0: no válida");
    confere(intel_pte(INTEL_VAR_BDW, 0x87654003u, 0x2u, &f) && f == 0x287654000ull, "BDW: entrada de 64 bits");
    confere(intel_gtt_desloc(INTEL_VAR_IVB) == 0x200000 && intel_gtt_desloc(INTEL_VAR_HSW) == 0x200000 &&
            intel_gtt_desloc(INTEL_VAR_BDW) == 0x800000 && intel_pte_bytes(INTEL_VAR_BDW) == 8 &&
            intel_pte_bytes(INTEL_VAR_IVB) == 4, "GTT a 2 MB (Gen7, 4 bytes) y a 8 MB (Gen8, 8 bytes) del BAR 0");
    uint64_t b = 0; uint32_t t = 0;
    confere(intel_reservado(INTEL_VAR_IVB, 0xDF000001u, 0, &b, &t) && b == 0xDF000000ull && t == 1u << 20,
            "reservada Gen7: base y 1 MB");
    confere(intel_reservado(INTEL_VAR_HSW, 0xDF0C0021u, 0, &b, &t) && b == 0xDF0C0000ull && t == 256u << 10,
            "reservada Gen7: base de 256 KB y 256 KB");
    /* Broadwell: el tamaño es hasta el final de la memoria robada, no los bits 8:7 (eso es Cherryview). */
    confere(intel_reservado(INTEL_VAR_BDW, 0xDF000001u, 0xE0000000ull, &b, &t) && b == 0xDF000000ull && t == 16u << 20,
            "reservada Gen8: de la base al final de la robada (16 MB, sin campo de tamaño)");
    confere(intel_reservado(INTEL_VAR_BDW, 0xDF000181u, 0xE0000000ull, &b, &t) && t == 16u << 20,
            "reservada Gen8: los bits 8:7 no cuentan");
    confere(intel_reservado(INTEL_VAR_BDW, 0xDF000001u, 0, &b, &t) && t == 0x21000000u,
            "reservada Gen8 sin final conocido: hasta los 4 GB");
    confere(!intel_reservado(INTEL_VAR_BDW, 0x00000001u, 0xE0000000ull, &b, &t), "reservada Gen8 con base 0: no hay");
    confere(!intel_reservado(INTEL_VAR_IVB, 0xDF000000u, 0, &b, &t), "sin el bit de activa: no hay reservada");
    uint64_t rb = 0, rt = 0;
    confere(intel_robada(INTEL_VAR_IVB, (2u << 3) | 1u, 0xDB000001u, &rb, &rt) && rb == 0xDB000000ull &&
            rt == 64u << 20, "robada Gen7: BDSM y GMS en los bits 7:3 (2 = 64 MB)");
    confere(intel_robada(INTEL_VAR_BDW, 0x0400u | 0x10u, 0xCF800000u, &rb, &rt) && rb == 0xCF800000ull && rt == 128u << 20,
            "robada Gen8: GMS en los bits 15:8 (4 = 128 MB)");
    confere(!intel_robada(INTEL_VAR_IVB, 0, 0xDB000000u, &rb, &rt), "robada sin GMS: no se sabe");
    confere(intel_dentro_robada(0xDB000000ull, 16, 0xDB000000ull, 64u << 20) &&
            !intel_dentro_robada(0xDB000000ull + (64u << 20) - 4096, 2, 0xDB000000ull, 64u << 20) &&
            !intel_dentro_robada(0xDAFFF000ull, 1, 0xDB000000ull, 64u << 20), "páginas dentro y fuera de la robada");
    confere(intel_solapa(0x500000, 16384, 0x4F0000, 0x510000) && !intel_solapa(0x4E0000, 0x10000, 0x4F0000, 0x510000) &&
            intel_solapa(0x4E0000, 0x10001, 0x4F0000, 0x510000) && !intel_solapa(0x510000, 4096, 0x4F0000, 0x510000),
            "solape de otra superficie con la zona del 2D");

    uint32_t e[64], ruim = 0;
    uint64_t f0 = 0;
    for (int i = 0; i < 32; i++) e[i] = (0xD0000000u + (uint32_t)i * 4096u) | 1u;
    confere(intel_gtt_conferir(INTEL_VAR_IVB, e, 32, false, 0, 0, &ruim, &f0) == INTEL_GTT_OK && f0 == 0xD0000000ull,
            "32 páginas seguidas: bien");
    confere(intel_gtt_conferir(INTEL_VAR_IVB, e, 32, true, 0xD0010000ull, 1u << 20, &ruim, &f0) == INTEL_GTT_RESERVADA &&
            ruim == 16, "la página 16 pisa la zona reservada");
    confere(intel_gtt_conferir(INTEL_VAR_IVB, e, 32, true, 0xD0020000ull, 1u << 20, &ruim, &f0) == INTEL_GTT_OK,
            "reservada justo detrás: bien");
    e[7] &= ~1u;
    confere(intel_gtt_conferir(INTEL_VAR_IVB, e, 32, false, 0, 0, &ruim, &f0) == INTEL_GTT_INVALIDA && ruim == 7,
            "una entrada sin mapear");
    e[7] = e[6];
    confere(intel_gtt_conferir(INTEL_VAR_IVB, e, 32, false, 0, 0, &ruim, &f0) == INTEL_GTT_SALTO && ruim == 7,
            "dos entradas a la misma página (relleno): no");
    for (int i = 0; i < 32; i++) e[i] = 0xDEAD0001u;
    confere(intel_gtt_conferir(INTEL_VAR_HSW, e, 32, false, 0, 0, &ruim, &f0) == INTEL_GTT_SALTO && ruim == 1,
            "todo apuntando a una página de relleno: no");
    for (int i = 0; i < 32; i++) { e[2 * i] = (0x80000000u + (uint32_t)i * 4096u) | 3u; e[2 * i + 1] = 1; }
    confere(intel_gtt_conferir(INTEL_VAR_BDW, e, 32, false, 0, 0, &ruim, &f0) == INTEL_GTT_OK && f0 == 0x180000000ull,
            "BDW: 32 entradas de 64 bits seguidas por encima de 4 GB");
    e[2 * 5 + 1] = 2;
    confere(intel_gtt_conferir(INTEL_VAR_BDW, e, 32, false, 0, 0, &ruim, &f0) == INTEL_GTT_SALTO && ruim == 5,
            "BDW: una con los bits altos distintos");
}

static void anel(void)
{
    confere(intel_anel_desloc(4096, 768, 64) == 4096u * 832u, "anillo tras 1024x768 + 64 filas (ya alineado)");
    uint32_t d = intel_anel_desloc(5504, 768, 64);
    confere(d >= 5504u * 832u && d < 5504u * 832u + 4096u && !(d & 0xFFF), "anillo tras 1366x768 + 64 filas, alineado a 4 KB");
}

int main(void)
{
    modelos();
    ordens();
    plano();
    gtt();
    anel();
    printf("%d comprobaciones, %d fallos\n", oks + falhas, falhas);
    if (falhas) return 1;
    printf("TUDO OK\n");
    return 0;
}
