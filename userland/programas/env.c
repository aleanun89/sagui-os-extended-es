// SPDX-License-Identifier: GPL-2.0-or-later
/* env - muestra el ambiente o ejecuta una orden con un ambiente modificado (D097).
 *   env                         una línea NOMBRE=valor por variable
 *   env NOMBRE                  el valor de esa variable (si existe)
 *   env [-i] [-u NOMBRE] [NOMBRE=valor...] orden [argumentos...] */
#include "sagui.h"

static const char *AYUDA =
    "uso: env [-i] [-u NOMBRE] [NOMBRE=valor...] [orden [argumentos...]]\n"
    "  sin orden: muestra el ambiente           env NOMBRE: muestra solo el valor de esa variable\n"
    "  -i  empieza con el ambiente vacío        -u N  quita la variable N\n"
    "  NOMBRE=valor  define variables solo para la orden (ej.: env LANG=es ls)\n";

#define MAX_ENV 256

int principal(int argc, char **argv)
{
    static char *nuevo[MAX_ENV + 1];
    static char *quitar[16];
    int nn = 0, nq = 0, i = 1;
    bool vacio = false;
    for (; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--help") == 0 || compara_texto(a, "--ayuda") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        if (compara_texto(a, "-i") == 0) { vacio = true; continue; }
        if (compara_texto(a, "-u") == 0) {
            if (i + 1 >= argc) { imprimir_em(ERROS, "env: -u necesita un nombre\n"); return 2; }
            if (nq < 16) quitar[nq++] = argv[++i];
            continue;
        }
        if (a[0] == '-' && a[1] && compara_texto(a, "--") != 0) { imprimir_em(ERROS, "env: opción desconocida: %s (env --help)\n", a); return 2; }
        if (compara_texto(a, "--") == 0) { i++; break; }
        break;
    }
    int primero_nueva = i;
    while (i < argc && acha_char(argv[i], '=') && argv[i][0] != '=') i++;
    int fin_vars = i;
    bool hay_orden = i < argc;
    /* compatibilidad: «env NOMBRE» muestra el valor */
    if (hay_orden && fin_vars == primero_nueva && primero_nueva == 1 && argc == 2) {
        const char *v = obter_env(argv[1]);
        if (v) { saida_linha(v, -1); return 0; }
    }
    /* ambiente de partida */
    if (!vacio)
        for (char **e = environ; e && *e && nn < MAX_ENV; e++) {
            bool fuera = false;
            for (int q = 0; q < nq; q++) {
                size_t l = tam_texto(quitar[q]);
                if (compara_texto_n(*e, quitar[q], l) == 0 && (*e)[l] == '=') fuera = true;
            }
            if (!fuera) nuevo[nn++] = *e;
        }
    for (int k = primero_nueva; k < fin_vars; k++) {
        size_t l = (size_t)(acha_char(argv[k], '=') - argv[k]);
        int pos = -1;
        for (int j = 0; j < nn; j++) if (compara_texto_n(nuevo[j], argv[k], l + 1) == 0) pos = j;
        if (pos >= 0) nuevo[pos] = argv[k];
        else if (nn < MAX_ENV) nuevo[nn++] = argv[k];
    }
    nuevo[nn] = NULL;
    if (!hay_orden) {
        for (int j = 0; j < nn; j++) if (saida_linha(nuevo[j], -1) < 0) break;
        return 0;
    }
    char prog[200];
    if (acha_char(argv[fin_vars], '/')) copia_texto(prog, argv[fin_vars], sizeof prog);
    else {
        const char *path = obter_env("PATH");
        if (!path || !*path) path = SAGUI_PATH_PADRAO;
        prog[0] = 0;
        const char *p = path;
        for (;;) {
            const char *f = p;
            while (*f && *f != ':') f++;
            char d[120];
            size_t n = (size_t)(f - p);
            if (n >= sizeof d) n = sizeof d - 1;
            memcpy(d, p, n);
            d[n] = 0;
            char c[200];
            caminho_juntar(c, sizeof c, n ? d : ".", argv[fin_vars]);
            info_arquivo_t inf;
            if (info_arquivo(c, &inf) == 0 && inf.tipo != TIPO_PASTA) { copia_texto(prog, c, sizeof prog); break; }
            if (!*f) break;
            p = f + 1;
        }
        if (!prog[0] && argc == 2) return 1;   /* «env NOMBRE» sin esa variable: como siempre, sin mensaje */
        if (!prog[0]) { imprimir_em(ERROS, "env: %s: orden no encontrada\n", argv[fin_vars]); return 127; }
    }
    int pid = executar_env(prog, &argv[fin_vars], nuevo);
    if (pid <= 0) { imprimir_em(ERROS, "env: %s: %s\n", argv[fin_vars], nome_erro(pid)); return 126; }
    int codigo = 1;
    esperar(pid, &codigo);
    return codigo < 0 ? 143 : codigo & 0xFF;
}
