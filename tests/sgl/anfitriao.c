// SPDX-License-Identifier: GPL-2.0-or-later
/* anfitriao.c - compila o mesmo motor libsgl no anfitrião e imprime as somas
 * esperadas de tests/h77_sgl.py (e, com -p, grava PPMs). */
#include <stdio.h>
#include <string.h>
#include "../../userland/libsgl/cenas.h"

static uint32_t px[CENA_L * CENA_A];

int main(int argc, char **argv)
{
    const char *nomes[] = { "triangulo", "cubo", "zcruzado" };
    int ppm = argc > 1 && !strcmp(argv[1], "-p");
    for (int z = 16; z <= 32; z += 16)
        for (int i = 0; i < 3; i++) {
            sgl_ctx_t *c = sgl_criar_contexto_mem(px, CENA_L, CENA_A, z);
            cena_por_nome(nomes[i]);
            printf("%s z%d %08x\n", nomes[i], z, sgl_soma(px, CENA_L * CENA_A));
            if (ppm) {
                char n[64]; snprintf(n, sizeof n, "/tmp/sgl_%s_z%d.ppm", nomes[i], z);
                FILE *f = fopen(n, "wb"); fprintf(f, "P6\n%d %d\n255\n", CENA_L, CENA_A);
                for (int k = 0; k < CENA_L * CENA_A; k++) { fputc(px[k] >> 16, f); fputc(px[k] >> 8, f); fputc(px[k], f); }
                fclose(f);
            }
            sgl_destruir_contexto(c);
        }
    /* o -t 60 do sgldemo: 60 quadros do cubo em 320x240, soma do ultimo */
    static uint32_t grande[320 * 240];
    for (int z = 16; z <= 32; z += 16) {
        sgl_ctx_t *c = sgl_criar_contexto_mem(grande, 320, 240, z);
        for (int i = 0; i < 60; i++) cena_cubo_tam((float)i * 3.0f, 320, 240);
        printf("demo60 z%d %08x\n", z, sgl_soma(grande, 320 * 240));
        sgl_destruir_contexto(c);
    }
    return 0;
}
