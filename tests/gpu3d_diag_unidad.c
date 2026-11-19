// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_diag_unidad.c - prueba de unidad en el PC del diagnóstico común del 3D (D390).
 *
 *   gcc -O1 -fno-builtin -I kernel/inc -o gpu3d_diag_unidad tests/gpu3d_diag_unidad.c
 *
 * Comprueba kernel/inc/gpu3d_diag.h, lo que la capa usa con todos los drivers:
 *   - las vistas: llenar con la sentinela y leer con 32 y 16 bits y con paso mayor
 *     que la fila;
 *   - el análisis: píxeles intactos, colores distintos (los cuatro primeros y el
 *     resto), Z intacto y al máximo con el Z24 arriba (R5xx), abajo (Intel) y Z16;
 *   - el código del Z para «gpu3d probar --ver» (0xFFFF = sentinela);
 *   - el lector de /sistema/conf/gpu3d-<driver>.conf con varios rangos, sin rangos
 *     (el driver no admite ajustes) y con el máximo de líneas.
 * Usa las cabeceras del kernel (con sus tipos): no incluye las de la libc. */
#include "gpu3d_diag.h"

int printf(const char *, ...);

static int falhas, feitas;
#define VER(c, ...) do { feitas++; if (!(c)) { falhas++; printf("FALLO: " __VA_ARGS__); printf("\n"); } } while (0)

static uint32_t cor[24 * 10], z32[24 * 10];
static uint16_t z16[32 * 10];

static void prova_vistas(void)
{
    /* Color de 20x10 con filas de 24 píxeles: lo de fuera no se toca. */
    for (int i = 0; i < 24 * 10; i++) cor[i] = 0x11111111u;
    g3d_vista_t c = { (volatile uint8_t *)cor, 24 * 4, 20, 10, 32, 0, 0, false };
    g3d_vista_encher(&c, G3D_SENTINELA_COR);
    VER(cor[0] == G3D_SENTINELA_COR && cor[19] == G3D_SENTINELA_COR && cor[20] == 0x11111111u && cor[9 * 24 + 19] == G3D_SENTINELA_COR,
        "la sentinela solo dentro de la vista (paso mayor que la fila)");
    /* La GPU «pinta» un rectángulo de 10x5 de 102030 (con alfa) y unos píxeles de otros colores. */
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 10; x++) cor[y * 24 + x] = 0xFF102030u;
    cor[9 * 24 + 0] = 0x00FF0000u; cor[9 * 24 + 1] = 0x0000FF00u; cor[9 * 24 + 2] = 0x000000FFu;
    cor[9 * 24 + 3] = 0x00010101u; cor[9 * 24 + 4] = 0x00020202u;
    g3d_analise_t an;
    g3d_analisar(&c, NULL, &an);
    VER(an.total == 200 && an.cor_intacta == 200 - 50 - 5, "intactos: 145 (%u de %u)", an.cor_intacta, an.total);
    VER(an.cores[0] == 0x102030 && an.n_cores[0] == 50, "el primer color sin el alfa (%x x%u)", an.cores[0], an.n_cores[0]);
    VER(an.cores[1] == 0xFF0000 && an.cores[3] == 0x0000FF && an.resto == 2, "los cuatro primeros y el resto (%u)", an.resto);
    VER(!an.tem_z, "sin Z: no se cuenta");

    /* Z24 arriba (R5xx: plantilla abajo). */
    g3d_vista_t z = { (volatile uint8_t *)z32, 24 * 4, 20, 10, 32, 24, 8, false };
    g3d_vista_encher(&z, G3D_SENTINELA_Z);
    z32[0] = 0xFFFFFF00u; z32[1] = 0xFFFFFF7Fu; z32[2] = 0x80000000u; z32[3] = 0x00FFFFFFu;
    g3d_analisar(&c, &z, &an);
    VER(an.tem_z && an.z_intacto == 196 && an.z_maximo == 2, "Z24 arriba: 196 intactos y 2 al máximo (%u, %u)", an.z_intacto, an.z_maximo);
    VER(g3d_z_codigo(&z, z32[2]) == 0x80 && g3d_z_codigo(&z, G3D_SENTINELA_Z) == 0xFFFF && g3d_z_codigo(&z, z32[0]) == 0xFF,
        "código del Z24 arriba");
    /* Z24 abajo (Intel: 24_FIXED_8_OTHER). */
    z.zdesloc = 0;
    g3d_analisar(&c, &z, &an);
    VER(an.z_maximo == 1 && g3d_z_codigo(&z, 0x00800000u) == 0x80, "Z24 abajo: solo 00ffffff está al máximo (%u)", an.z_maximo);
    /* Z16 con filas de 32. */
    g3d_vista_t z2 = { (volatile uint8_t *)z16, 32 * 2, 20, 10, 16, 16, 0, false };
    for (int i = 0; i < 32 * 10; i++) z16[i] = 0x1234;
    g3d_vista_encher(&z2, G3D_SENTINELA_Z);
    VER(z16[0] == 0x5A5A && z16[19] == 0x5A5A && z16[20] == 0x1234, "sentinela de 16 bits");
    z16[5] = 0xFFFF; z16[6] = 0x4000;
    g3d_analisar(&c, &z2, &an);
    VER(an.z_intacto == 198 && an.z_maximo == 1, "Z16: 198 intactos, 1 al máximo (%u, %u)", an.z_intacto, an.z_maximo);
    VER(g3d_z_codigo(&z2, 0x4000) == 0x40 && g3d_z_codigo(&z2, 0x5A5A) == 0xFFFF, "código del Z16");
}

static void prova_ajustes(void)
{
    /* Como el r600: el contexto y un trozo de la configuración. */
    static const g3d_faixa_t fx[] = { { 0x28000u, 0x28FFCu }, { 0x8800u, 0x8DFCu } };
    static const char t[] = "inicio 28040 c0500000\n"     /* CB_COLOR0_BASE: dentro */
                            "dibujo 0x8C00 1\n"           /* SQ_CONFIG: dentro */
                            "dibujo 0x8DFC 2\n"           /* el último del rango */
                            "inicio 0x8E00 3\n"           /* fuera */
                            "inicio 0x86D8 0\n"           /* CP_ME_CNTL: fuera (pararía el CP) */
                            "inicio 0x2000 0\n"           /* MC: fuera */
                            "inicio 0x28042 0\n"          /* sin alinear */
                            "dibujo\n";                   /* sin números */
    g3d_ajustes_t a;
    g3d_ajustes_ler(t, sizeof t - 1, fx, 2, &a);
    VER(a.n == 3 && a.malas == 5 && a.fora == 4, "varios rangos: 3 buenos, 5 malos, 4 fuera (%d, %d, %d)", a.n, a.malas, a.fora);
    VER(a.reg[0] == 0x28040 && a.val[0] == 0xC0500000u && a.quando[0] == G3D_AJ_INICIO, "registros de más de 16 bits");
    VER(a.reg[2] == 0x8DFC && a.quando[2] == G3D_AJ_DIBUJO, "el extremo del rango vale");
    /* Sin rangos (Intel): nada vale. */
    g3d_ajustes_ler(t, sizeof t - 1, NULL, 0, &a);
    VER(a.n == 0 && a.malas == 8 && a.fora == 7, "sin rangos ningún ajuste (%d, %d, %d)", a.n, a.malas, a.fora);
    /* Más líneas que el máximo: las de más se cuentan como malas. */
    static char g[100 * 20];
    int k = 0;
    for (int i = 0; i < G3D_AJUSTES_MAX + 5; i++) {
        const char *l = "dibujo 28000 1\n";
        while (*l) g[k++] = *l++;
    }
    g3d_ajustes_ler(g, (uint32_t)k, fx, 2, &a);
    VER(a.n == G3D_AJUSTES_MAX && a.malas == 5, "como mucho %d ajustes (%d, %d)", G3D_AJUSTES_MAX, a.n, a.malas);
}

int main(void)
{
    prova_vistas();
    prova_ajustes();
    printf("gpu3d_diag_unidad: %d comprobaciones, %d fallos\n", feitas, falhas);
    return falhas != 0;
}
