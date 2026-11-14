// SPDX-License-Identifier: GPL-2.0-or-later
/* afinidad - las CPU donde puede correr un proceso (D174).
 *
 *   afinidad PID              la máscara actual
 *   afinidad PID MASCARA      la cambia: en hexadecimal, como taskset (0x5 = CPU 0 y 2; 0 = todas)
 *   afinidad PID -c LISTA     la cambia con una lista de CPU: 0,2  o  1-3
 *
 * Vale para todo el proceso (sus hilos) y la heredan los hijos que lance. */
#include "sagui.h"

static int hex(const char *s, unsigned *v)
{
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) return 0;
    unsigned r = 0;
    for (; *s; s++) {
        int d = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 :
                *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
        if (d < 0) return 0;
        r = r * 16 + (unsigned)d;
    }
    *v = r;
    return 1;
}

/* "0,2" o "1-3" o "0,2-3" */
static int lista(const char *s, unsigned *v)
{
    unsigned r = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        int a = 0, b;
        while (*s >= '0' && *s <= '9') a = a * 10 + (*s++ - '0');
        b = a;
        if (*s == '-') {
            s++;
            if (*s < '0' || *s > '9') return 0;
            b = 0;
            while (*s >= '0' && *s <= '9') b = b * 10 + (*s++ - '0');
        }
        if (a > 31 || b > 31 || b < a) return 0;
        for (int c = a; c <= b; c++) r |= 1u << c;
        if (*s == ',') s++;
        else if (*s) return 0;
    }
    *v = r;
    return 1;
}

static void mostrar(const char *que, int pid, unsigned m)
{
    char t[64];
    unsigned k = 0;
    t[0] = 0;
    if (!m) formatar_texto(t, sizeof t, "todas");
    for (int c = 0; c < 32 && m; c++)
        if (m & (1u << c)) k += (unsigned)formatar_texto(t + k, sizeof t - k, k ? ",%d" : "%d", c);
    imprimir("Proceso %d: %s 0x%x (CPU %s)\n", pid, que, m, t);
}

int principal(int argc, char **argv)
{
    if (argc < 2 || argc > 4) {
        imprimir_em(ERROS, "uso: afinidad <pid> [mascara hex | -c lista]\n");
        return 1;
    }
    int pid = texto_para_int(argv[1]);
    if (argc == 2) {
        int r = afinidade_consultar(pid);
        if (r < 0 && r >= ERRO_ACESSO) { imprimir_em(ERROS, "afinidad: %d: %s\n", pid, nome_erro(r)); return 1; }
        mostrar("máscara", pid, (unsigned)r);
        return 0;
    }
    unsigned m = 0;
    int ok = argc == 4 ? compara_texto(argv[2], "-c") == 0 && lista(argv[3], &m) : hex(argv[2], &m);
    if (!ok) { imprimir_em(ERROS, "afinidad: máscara no válida\n"); return 1; }
    int r = afinidade_cpu(pid, m);
    if (r < 0 && r >= ERRO_ACESSO) {
        imprimir_em(ERROS, "afinidad: %d: %s\n", pid, r == ERRO_INVALIDO ? "ninguna de esas CPU trabaja" : nome_erro(r));
        return 1;
    }
    mostrar("máscara nueva", pid, m);
    registrar("afinidad: proceso %d mascara 0x%x (antes 0x%x)", pid, m, (unsigned)r);
    return 0;
}
