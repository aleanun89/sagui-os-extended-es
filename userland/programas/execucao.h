// SPDX-License-Identifier: GPL-2.0-or-later
/* execucao.h - lo común de time, timeout y watch (D232): buscar un programa en PATH y lanzarlo. */
#ifndef EXECUCAO_H
#define EXECUCAO_H
#include "sagui.h"

/* Resuelve "orden" a una ruta de programa (con barra: tal cual; si no, se busca en PATH). 0 si la encuentra. */
static int achar_programa(const char *nome, char *prog, size_t max)
{
    if (acha_char(nome, '/')) { copia_texto(prog, nome, max); return 0; }
    const char *path = obter_env("PATH");
    if (!path || !*path) path = SAGUI_PATH_PADRAO;
    const char *p = path;
    for (;;) {
        const char *f = p;
        while (*f && *f != ':') f++;
        char d[120], c[200];
        size_t n = (size_t)(f - p);
        if (n >= sizeof d) n = sizeof d - 1;
        memcpy(d, p, n);
        d[n] = 0;
        caminho_juntar(c, sizeof c, n ? d : ".", nome);
        info_arquivo_t inf;
        if (info_arquivo(c, &inf) == 0 && inf.tipo != TIPO_PASTA) { copia_texto(prog, c, max); return 0; }
        if (!*f) return -1;
        p = f + 1;
    }
}

/* Lanza la orden (argv terminado en NULL); devuelve el pid o -1 tras avisar con el nombre del comando "cmd". */
static int lanzar_orden(const char *cmd, char **args)
{
    char prog[200];
    if (achar_programa(args[0], prog, sizeof prog) != 0) {
        imprimir_em(ERROS, "%s: %s: orden no encontrada (los comandos internos del shell, como cd, no se pueden lanzar así)\n", cmd, args[0]);
        return -1;
    }
    int pid = executar(prog, args);
    if (pid <= 0) { imprimir_em(ERROS, "%s: %s: no se pudo ejecutar (%s)\n", cmd, args[0], nome_erro(pid)); return -1; }
    return pid;
}

#endif
