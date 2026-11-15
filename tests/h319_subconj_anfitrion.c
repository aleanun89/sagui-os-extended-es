// SPDX-License-Identifier: GPL-2.0-or-later
/* Programa del PC para la prueba de unidad del subconjunto TrueType (tests/h319_subconjunto.py):
 *   subconj fuente.ttf salida.ttf "texto"   -> escribe el subconjunto y muestra la info de PDF */
#define SUBCONJ_ANFITRION 1
#include <stdio.h>
#include "../userland/libfontes/subconjunto.c"

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 3;
    static uint8_t buf[8 << 20];
    unsigned n = (unsigned)fread(buf, 1, sizeof buf, f);
    fclose(f);
    pdfw_finfo_t i;
    if (!fonte_info_pdf(buf, n, argv[1], &i)) return 4;
    printf("info %s flags=%d bbox=%d,%d,%d,%d ang=%d asc=%d desc=%d cap=%d cff=%d\n", i.psname, i.flags, i.bbox[0], i.bbox[1], i.bbox[2], i.bbox[3], i.angulo, i.ascent, i.descent, i.capheight, i.cff);
    uint8_t usadas[32] = { 0 };
    for (const unsigned char *p = (const unsigned char *)argv[3]; *p; p++) usadas[*p >> 3] |= (uint8_t)(1u << (*p & 7));
    uint8_t *o; unsigned no;
    if (!fonte_subconjunto_ttf(buf, n, usadas, &o, &no)) return 5;
    f = fopen(argv[2], "wb"); fwrite(o, 1, no, f); fclose(f);
    printf("subconjunto %u -> %u\n", n, no);
    return 0;
}
