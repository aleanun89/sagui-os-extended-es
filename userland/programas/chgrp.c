// SPDX-License-Identifier: GPL-2.0-or-later
/* chgrp - cambia el grupo de archivos y carpetas (D282, mejora 098): root, o el
 * dueño a su propio grupo. */
#include "permisos_comum.h"

static const char *AYUDA =
    "uso: chgrp [-R] GRUPO archivo...\n"
    "  GRUPO: nombre (root, sagui) o número\n"
    "  -R  también lo que hay dentro de las carpetas\n";

static int aplicar(const char *c, void *dados)
{
    return mudar_dono(c, -1, *(const int *)dados);
}

int principal(int argc, char **argv)
{
    bool recursivo = false;
    const char *grupo = NULL;
    int primeiro = 0, erros = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--help") == 0 || compara_texto(argv[i], "--ayuda") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        if (compara_texto(argv[i], "-R") == 0 && !grupo) { recursivo = true; continue; }
        if (!grupo) { grupo = argv[i]; continue; }
        if (!primeiro) primeiro = i;
    }
    if (!grupo || !primeiro) { imprimir_em(ERROS, "chgrp: falta el grupo o el archivo (chgrp --help)\n"); return 2; }
    int gid = id_de(grupo, true);
    if (gid < 0) { imprimir_em(ERROS, "chgrp: no existe el grupo %s\n", grupo); return 1; }
    for (int i = primeiro; i < argc; i++)
        erros += aplicar_em("chgrp", argv[i], recursivo, aplicar, &gid, 0);
    return erros ? 1 : 0;
}
