// SPDX-License-Identifier: GPL-2.0-or-later
/* chown - cambia el dueño (y el grupo) de archivos y carpetas (D282, mejora 098).
 * El dueño solo lo cambia root; el grupo, root o el dueño (al suyo). */
#include "permisos_comum.h"

static const char *AYUDA =
    "uso: chown [-R] DUEÑO[:GRUPO] archivo...\n"
    "     chown [-R] :GRUPO archivo...\n"
    "  DUEÑO y GRUPO: nombre de cuenta (root, sagui) o número\n"
    "  -R  también lo que hay dentro de las carpetas\n"
    "Solo root cambia el dueño (usa «admin chown ...»).\n";

typedef struct { int uid, gid; } dono_t;

static int aplicar(const char *c, void *dados)
{
    const dono_t *d = dados;
    return mudar_dono(c, d->uid, d->gid);
}

int principal(int argc, char **argv)
{
    bool recursivo = false;
    const char *quem = NULL;
    int primeiro = 0, erros = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--help") == 0 || compara_texto(argv[i], "--ayuda") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        if (compara_texto(argv[i], "-R") == 0 && !quem) { recursivo = true; continue; }
        if (!quem) { quem = argv[i]; continue; }
        if (!primeiro) primeiro = i;
    }
    if (!quem || !primeiro) { imprimir_em(ERROS, "chown: falta el dueño o el archivo (chown --help)\n"); return 2; }
    char u[CONTA_NOME_MAX * 2];
    copia_texto(u, quem, sizeof u);
    char *g = acha_char(u, ':');
    if (g) *g++ = 0;
    dono_t d = { -1, -1 };
    if (*u && (d.uid = id_de(u, false)) < 0) { imprimir_em(ERROS, "chown: no existe la cuenta %s\n", u); return 1; }
    if (g && *g && (d.gid = id_de(g, true)) < 0) { imprimir_em(ERROS, "chown: no existe el grupo %s\n", g); return 1; }
    if (d.uid < 0 && d.gid < 0) { imprimir_em(ERROS, "chown: falta el dueño o el grupo\n"); return 2; }
    for (int i = primeiro; i < argc; i++)
        erros += aplicar_em("chown", argv[i], recursivo, aplicar, &d, 0);
    return erros ? 1 : 0;
}
