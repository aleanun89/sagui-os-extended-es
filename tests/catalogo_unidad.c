// SPDX-License-Identifier: GPL-2.0-or-later
/* catalogo_unidad.c - prueba en el anfitrión del lector de catálogos (D313, h315).
 * Uso: catalogo_unidad ARCHIVO.cat < claves   (una clave por línea; \n escrito como «\n»).
 * Imprime una línea por clave: la traducción o «(null)». Con -n: normaliza nombres de idioma. */
#define CATALOGO_ANFITRION
#include "../kernel/inc/catalogo.h"
#include "../kernel/lib/catalogo.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char linea[1024];
    if (argc == 2 && !strcmp(argv[1], "-n")) {
        while (fgets(linea, sizeof linea, stdin)) {
            linea[strcspn(linea, "\n")] = 0;
            char o[IDIOMA_MAX];
            catalogo_idioma_norm(linea, o);
            printf("%s\n", o);
        }
        return 0;
    }
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 3;
    static uint8_t buf[1 << 20];
    uint32_t n = (uint32_t)fread(buf, 1, sizeof buf, f);
    fclose(f);
    catalogo_t c;
    if (catalogo_abrir(&c, buf, n) < 0) { puts("CATALOGO INVALIDO"); return 4; }
    while (fgets(linea, sizeof linea, stdin)) {
        linea[strcspn(linea, "\n")] = 0;
        char clave[1024];
        size_t k = 0;
        for (char *p = linea; *p && k < sizeof clave - 1; p++) {   /* «\n» escrito -> salto de línea */
            if (p[0] == '\\' && p[1] == 'n') { clave[k++] = '\n'; p++; } else clave[k++] = *p;
        }
        clave[k] = 0;
        const char *t = catalogo_buscar(&c, clave);
        if (!t) puts("(null)");
        else {
            for (const char *q = t; *q; q++) { if (*q == '\n') fputs("\\n", stdout); else putchar(*q); }
            putchar('\n');
        }
    }
    /* un archivo truncado o con la tabla fuera de rango se rechaza sin romper nada */
    for (uint32_t cut = 0; cut < 40 && cut < n; cut += 3) {
        catalogo_t d;
        if (catalogo_abrir(&d, buf, cut) == 0) { catalogo_buscar(&d, "x"); }
    }
    return 0;
}
