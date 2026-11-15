// SPDX-License-Identifier: GPL-2.0-or-later
/* permisos_comum.h - lo que comparten chmod, chown y chgrp (D282): recorrer una
 * carpeta con -R y entender nombres de cuenta o números. */
#ifndef PERMISOS_COMUM_H
#define PERMISOS_COMUM_H
#include "sagui.h"
#include "contas.h"

typedef int (*aplicar_fn)(const char *caminho, void *dados);

/* Aplica "f" a "caminho" y, con "recursivo", a todo lo que haya dentro (8 niveles
 * como mucho: no hay enlaces, pero así nunca da vueltas). Devuelve los errores. */
static inline int aplicar_em(const char *prog, const char *caminho, bool recursivo, aplicar_fn f, void *dados, int nivel)
{
    int erros = 0;
    int r = f(caminho, dados);
    if (r < 0) { imprimir_em(ERROS, "%s: %s: %s\n", prog, caminho, nome_erro(r)); erros++; }
    info_arquivo_t inf;
    if (!recursivo || nivel >= 32 || info_arquivo(caminho, &inf) < 0 || inf.tipo != TIPO_PASTA) return erros;
    entrada_pasta_t *v;
    int n = listar_tudo(caminho, &v);
    if (n < 0) { imprimir_em(ERROS, "%s: %s: %s\n", prog, caminho, nome_erro(n)); return erros + 1; }
    for (int i = 0; i < n; i++) {
        char c[256];
        caminho_juntar(c, sizeof c, caminho, v[i].nome);
        erros += aplicar_em(prog, c, true, f, dados, nivel + 1);
    }
    libera(v);
    return erros;
}

/* "1000", "sagui" o "root" -> uid (o gid: el de la cuenta con ese nombre). -1 si no existe. */
static inline int id_de(const char *t, bool grupo)
{
    if (*t >= '0' && *t <= '9') {
        int x = 0;
        for (const char *p = t; *p; p++) {
            if (*p < '0' || *p > '9' || x > UID_MAX) return -1;
            x = x * 10 + (*p - '0');
        }
        return x <= UID_MAX ? x : -1;
    }
    conta_t c;
    if (!conta_por_nome(t, &c)) return -1;
    return (int)(grupo ? c.gid : c.uid);
}
#endif
