// SPDX-License-Identifier: GPL-2.0-or-later
/* xargs - construye y ejecuta órdenes con lo que llega por la entrada (D097).
 *   find . -name "*.o" | xargs rm      ls | xargs -n 1 echo > cada nombre por separado */
#include "sagui.h"

static const char *AYUDA =
    "uso: xargs [-n N] [-I MARCA] [-0] [-t] [orden [argumentos...]]\n"
    "  Lee palabras de la entrada y ejecuta la orden con ellas como argumentos (por defecto echo).\n"
    "  -n N  como mucho N argumentos por ejecución    -I M  ejecuta una vez por línea, sustituyendo M\n"
    "  -0    los datos vienen separados por bytes 0   -t  muestra cada orden antes de ejecutarla\n"
    "  Si no llega nada, no ejecuta la orden. La orden se busca en PATH (los internos del shell, como cd, no sirven).\n";

#define MAX_ARGS 120

static bool traza;
static int erros;

static int ejecutar_orden(char **args)
{
    char prog[200];
    if (acha_char(args[0], '/')) copia_texto(prog, args[0], sizeof prog);
    else {
        formatar_texto(prog, sizeof prog, "/bin/%s", args[0]);
        const char *path = obter_env("PATH");
        if (!path || !*path) path = SAGUI_PATH_PADRAO;   /* D194: también /apps/bin */
        {
            const char *p = path;
            for (;;) {
                const char *f = p;
                while (*f && *f != ':') f++;
                char d[120], c[200];
                size_t n = (size_t)(f - p);
                if (n >= sizeof d) n = sizeof d - 1;
                memcpy(d, p, n);
                d[n] = 0;
                caminho_juntar(c, sizeof c, n ? d : ".", args[0]);
                info_arquivo_t inf;
                if (info_arquivo(c, &inf) == 0 && inf.tipo != TIPO_PASTA) { copia_texto(prog, c, sizeof prog); break; }
                if (!*f) break;
                p = f + 1;
            }
        }
    }
    if (traza) {
        for (int i = 0; args[i]; i++) imprimir_em(ERROS, "%s%s", i ? " " : "", args[i]);
        imprimir_em(ERROS, "\n");
    }
    int pid = executar(prog, args), c = 1;
    if (pid <= 0) { imprimir_em(ERROS, "xargs: %s: orden no encontrada\n", args[0]); erros = 127; return -1; }
    esperar(pid, &c);
    if (c != 0 && !erros) erros = 123;
    return 0;
}

int principal(int argc, char **argv)
{
    int por_tanda = 0;
    const char *marca = NULL;
    bool nulo = false;
    int i = 1;
    for (; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--help") == 0 || compara_texto(a, "--ayuda") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        if (compara_texto(a, "-t") == 0) traza = true;
        else if (compara_texto(a, "-0") == 0) nulo = true;
        else if (compara_texto(a, "-r") == 0) continue;
        else if (compara_texto(a, "-n") == 0 && i + 1 < argc) {
            if (!texto_para_numero(argv[++i], &por_tanda) || por_tanda < 1) { imprimir_em(ERROS, "xargs: -n: número no válido\n"); return 2; }
        } else if (compara_texto(a, "-I") == 0 && i + 1 < argc) marca = argv[++i];
        else if (compara_texto_n(a, "-I", 2) == 0 && a[2]) marca = a + 2;
        else if (a[0] == '-' && a[1] && compara_texto(a, "--") != 0) { imprimir_em(ERROS, "xargs: opción desconocida: %s (xargs --help)\n", a); return 2; }
        else { if (compara_texto(a, "--") == 0) i++; break; }
    }
    char *base[] = { "echo", NULL };
    char **cmd = i < argc ? &argv[i] : base;
    int ncmd = 0;
    while (cmd[ncmd]) ncmd++;
    if (ncmd > 20) { imprimir_em(ERROS, "xargs: la orden tiene demasiados argumentos\n"); return 2; }
    unsigned n;
    char *datos = ler_todo_fd(ENTRADA, &n);
    if (!datos) { imprimir_em(ERROS, "xargs: sin memoria\n"); return 1; }
    /* corta en palabras (con comillas simples y dobles) o en líneas con -I y en \0 con -0 */
    char *pal[2048];
    int np = 0;
    unsigned p = 0;
    while (p < n && np < 2048) {
        if (nulo) {
            pal[np++] = datos + p;
            while (p < n && datos[p]) p++;
            p++;
            continue;
        }
        if (marca) {
            while (p < n && datos[p] == '\n') p++;
            if (p >= n) break;
            pal[np++] = datos + p;
            while (p < n && datos[p] != '\n') p++;
            datos[p++] = 0;
            continue;
        }
        while (p < n && eh_espaco(datos[p])) p++;
        if (p >= n) break;
        if (datos[p] == '"' || datos[p] == '\'') {
            char q = datos[p++];
            pal[np++] = datos + p;
            while (p < n && datos[p] != q) p++;
            if (p < n) datos[p++] = 0;
        } else {
            pal[np++] = datos + p;
            while (p < n && !eh_espaco(datos[p])) p++;
            if (p < n) datos[p++] = 0;
        }
    }
    if (np == 0) return 0;
    if (marca) {
        for (int k = 0; k < np; k++) {
            char *args[MAX_ARGS];
            char sub[20][300];
            int a = 0;
            for (int c = 0; c < ncmd; c++) {
                if (compara_texto(cmd[c], marca) == 0) args[a++] = pal[k];
                else if (acha_char(cmd[c], marca[0]) && tam_texto(cmd[c]) < 280) {
                    /* sustituye la marca dentro de un argumento más largo */
                    char *d = sub[c];
                    size_t lm = tam_texto(marca);
                    int pos = 0;
                    for (const char *s = cmd[c]; *s && pos < 280;) {
                        if (compara_texto_n(s, marca, lm) == 0) {
                            for (const char *t = pal[k]; *t && pos < 299; t++) d[pos++] = *t;
                            s += lm;
                        } else d[pos++] = *s++;
                    }
                    d[pos] = 0;
                    args[a++] = d;
                } else args[a++] = cmd[c];
            }
            args[a] = NULL;
            if (ejecutar_orden(args) < 0) break;
        }
        return erros;
    }
    int tanda = por_tanda ? por_tanda : MAX_ARGS - ncmd - 1;
    if (tanda > MAX_ARGS - ncmd - 1) tanda = MAX_ARGS - ncmd - 1;
    for (int k = 0; k < np; k += tanda) {
        char *args[MAX_ARGS];
        int a = 0;
        for (int c = 0; c < ncmd; c++) args[a++] = cmd[c];
        for (int j = k; j < np && j < k + tanda; j++) args[a++] = pal[j];
        args[a] = NULL;
        if (ejecutar_orden(args) < 0) break;
    }
    return erros;
}
