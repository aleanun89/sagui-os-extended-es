// SPDX-License-Identifier: GPL-3.0-only
/* es20_ri.c - compila un sombreador de GLSL ES 1.00 y escribe su representación intermedia (D475).
 *
 *   es20_ri v|f fichero.glsl     el volcado de la RI (lo que traducirá cada backend), o el log de error
 *
 * Sirve para depurar el compilador y para quien escriba un backend de GPU: es lo que recibirá. Solo en el
 * anfitrión (-DSGL_ANFITRIAO). Sale con 1 si no compila o si la RI no pasa ri_validar(). */
#include "sgl_ri.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: es20_ri v|f fichero.glsl\n"); return 2; }
    FILE *f = fopen(argv[2], "rb");
    if (!f) { perror(argv[2]); return 2; }
    static char fonte[1 << 18];
    int n = (int)fread(fonte, 1, sizeof fonte - 1, f);
    fclose(f);
    fonte[n] = 0;
    char erro[512];
    ri_prog_t *p = sgl_glsl_compilar(fonte, n, argv[1][0] == 'f', erro, sizeof erro);
    if (!p) { printf("%s\n", erro); return 1; }
    static char txt[1 << 20];
    ri_texto(p, txt, sizeof txt);
    fputs(txt, stdout);
    for (int i = 0; i < p->nunif; i++) printf("; uniforme %s tipo 0x%x arr %d reg %d\n", p->unif[i].nome, p->unif[i].tipo_gl, p->unif[i].arr, p->unif[i].reg);
    for (int i = 0; i < p->nattr; i++) printf("; atributo %s tipo 0x%x reg %d\n", p->attr[i].nome, p->attr[i].tipo_gl, p->attr[i].reg);
    for (int i = 0; i < p->nvary; i++) printf("; varying %s tipo 0x%x reg %d\n", p->vary[i].nome, p->vary[i].tipo_gl, p->vary[i].reg);
    char e2[256];
    int ok = ri_validar(p, e2, sizeof e2);
    if (!ok) printf("; NO VALIDA: %s\n", e2);
    ri_liberar(p);
    return ok ? 0 : 1;
}
