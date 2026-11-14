// SPDX-License-Identifier: GPL-2.0-or-later
/* gpuid.c - prova no anfitrião a tabela de userland/inc/gpuid.h: "fab:mod" -> família. */
#include <stdio.h>
#include <stdlib.h>
#include "../../userland/inc/gpuid.h"

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        unsigned f, m;
        sscanf(argv[i], "%x:%x", &f, &m);
        gpu_id_t g = gpu_identificar((unsigned short)f, (unsigned short)m);
        printf("%s|%s|%s\n", argv[i], g.familia, g.motor);
    }
    return 0;
}
